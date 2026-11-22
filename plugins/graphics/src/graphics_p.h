/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
#define BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD

#include <graphics.h>

#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bx/error.h> //bx::Error

#include <frost/math.h>
#include <frost/bounds.h>
#include <frost/handlealloc.h>
#include <frost/readerwriter.h>
#include <frost/sort.h>
#include <frost/hash.h>

#include <meshoptimizer/src/meshoptimizer.h>

/// Helpers
namespace frost
{
	struct ScreenVertex
	{
		float x, y, z, u, v;
	};

	static void screenSpaceTriangle(const bgfx::VertexLayout& _layout)
	{
		FROST_ASSERT(3 == bgfx::getAvailTransientVertexBuffer(3, _layout), "Transient vertex buffer exhausted.");

		bgfx::TransientVertexBuffer vb;
		bgfx::allocTransientVertexBuffer(&vb, 3, _layout);

		const bool bottomLeft = bgfx::getCaps()->originBottomLeft;

		const float minv = bottomLeft ? 1.0f : 0.0f;
		const float maxv = bottomLeft ? -1.0f : 2.0f;

		ScreenVertex* vertices = (ScreenVertex*)vb.data;
		vertices[0] = { -1.0f, 0.0f, 0.0f, -1.0f, minv };
		vertices[1] = {  1.0f, 0.0f, 0.0f,  1.0f, minv };
		vertices[2] = {  1.0f, 2.0f, 0.0f,  1.0f, maxv };

		bgfx::setVertexBuffer(0, &vb);
	}

	static bool getCameraMatrices(const ActiveCameraResource& _activeCamera, QueryT<const CameraComponent&> _cameras,
		uint32_t _width, uint32_t _height, float* _view, float* _proj)
	{
		CameraComponent camera;
		if (!_cameras.get(_activeCamera.entity, [&](const CameraComponent& _camera) { camera = _camera; }))
		{
			return false;
		}

		mtxFromQuaternion(_view, camera.rotation, camera.position);

		const float aspect = float(_width) / float(_height);
		const bool homogeneousDepth = bgfx::getCaps()->homogeneousDepth;

		if (CameraProjection::Orthographic == camera.projection)
		{
			const float halfHeight = camera.orthographicHeight * 0.5f;

			mtxOrtho(_proj, -halfHeight * aspect, halfHeight * aspect, -halfHeight, halfHeight,
				camera.nearPlane, camera.farPlane, 0.0f, homogeneousDepth);
		}
		else
		{
			mtxProj(_proj, camera.fov, aspect, camera.nearPlane, camera.farPlane, homogeneousDepth);
		}

		return true;
	}

} // namespace frost

/// Asset system
namespace frost
{
	struct TextureRef
	{
		Guid guid;
		bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
		bgfx::TextureInfo info = {};
		uint32_t refCount = 0;
		bool srgb = false;
		uint8_t minAlpha = 0; // Conservative lower bound across all mip levels.
	};

	struct TextureContext
	{
		AllocatorI* allocator;
		HandleAlloc* handles;
		TextureRef* refs;
		bgfx::TextureHandle white = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle normal = BGFX_INVALID_HANDLE;
	};

	static TextureContext* s_textureContext = NULL;

	static TextureRef* getTextureRef(TextureHandle _handle)
	{
		return NULL != s_textureContext
			&& _handle.idx < s_textureContext->handles->getMaxHandles()
			&& s_textureContext->handles->isValid(_handle.idx)
			? &s_textureContext->refs[_handle.idx] : NULL;
	}

	Guid getGuid(TextureHandle _handle)
	{
		const TextureRef* texture = getTextureRef(_handle);
		return NULL != texture ? texture->guid : Guid{};
	}

	void addRef(TextureHandle _handle)
	{
		TextureRef* texture = getTextureRef(_handle);
		FROST_ASSERT(NULL != texture && UINT32_MAX != texture->refCount, "Cannot retain an invalid or overflowing texture reference.");
		++texture->refCount;
	}

	void release(TextureHandle _handle)
	{
		if (!isValid(_handle)) { return; }
		TextureRef* texture = getTextureRef(_handle);
		FROST_ASSERT(NULL != texture && 0 != texture->refCount, "Cannot release an invalid texture reference.");
		if (0 == --texture->refCount)
		{
			bgfx::destroy(texture->texture);
			*texture = TextureRef{};
			s_textureContext->handles->free(_handle.idx);
		}
	}

	static uint8_t minimumAlpha(const bimg::ImageMip& _mip, bimg::TextureFormat::Enum _format)
	{
		uint8_t result = 255;
		if (bimg::TextureFormat::RGBA8 == _format)
		{
			for (uint32_t ii = 3; ii < _mip.m_size; ii += 4)
			{
				result = min(result, _mip.m_data[ii]);
			}
		}
		else if (bimg::TextureFormat::BC3 == _format)
		{
			for (uint32_t ii = 0; ii < _mip.m_size && result > 0; ii += 16)
			{
				const uint8_t* block = _mip.m_data + ii;
				uint8_t alpha[8] = { block[0], block[1], 0, 0, 0, 0, 0, 255 };
				uint32_t count = 4;
				if (alpha[0] > alpha[1])
				{
					count = 6;
				}
				for (uint32_t jj = 0; jj < count; ++jj)
				{
					alpha[jj + 2] = uint8_t(((count - jj) * alpha[0] + (jj + 1) * alpha[1]) / (count + 1));
				}
				uint64_t selectors = 0;
				for (uint32_t jj = 0; jj < 6; ++jj)
				{
					selectors |= uint64_t(block[jj + 2]) << (jj * 8);
				}
				for (uint32_t jj = 0; jj < 16; ++jj)
				{
					result = min(result, alpha[selectors & 7]);
					selectors >>= 3;
				}
			}
		}
		else
		{
			// Unknown alpha encodings keep their alpha test.
			return 0;
		}
		return result;
	}

	static TextureHandle textureCreate(const Memory* _mem, Error* _err)
	{
		TextureHandle handle;
		if (!_err->isOk()) { return handle; }
		if (NULL == s_textureContext)
		{
			FROST_ERROR_SET(_err, kErrorTextureCreate, "Texture creation requires graphics startup.");
			return handle;
		}
		AssetHeader header;
		if (!readAssetHeader(_mem, &header, _err)) { return handle; }
		if (kAssetTypeTexture != header.type || kTextureVersion != header.payloadVersion || 0 != header.dependencyCount)
		{
			FROST_ERROR_SET(_err, kErrorTextureFormat, "Texture asset type, version, or dependencies are unsupported.");
			return handle;
		}
		MemoryReader reader((const uint8_t*)_mem->data + header.payloadOffset, header.payloadSize);
		uint32_t flags = 0;
		readHE(&reader, flags, true, _err);
		if (!_err->isOk() || 0 != (flags & ~uint32_t(TextureAssetFlags::Srgb)))
		{
			FROST_ERROR_SET(_err, kErrorTextureFormat, "Texture flags are invalid.");
			return handle;
		}
		const uint8_t* data = (const uint8_t*)_mem->data + header.payloadOffset + 4;
		const uint32_t size = header.payloadSize - 4;
		const bool srgb = 0 != (flags & TextureAssetFlags::Srgb);
		bimg::ImageContainer image = {};
		bx::Error imageError;
		if (size < 128 || 0 != memCmp(data, "DDS ", 4)
			|| !bimg::imageParse(image, data, size, &imageError)
			|| 0 == image.m_width || 0 == image.m_height || UINT16_MAX < image.m_width || UINT16_MAX < image.m_height
			|| image.m_cubeMap || 1 != image.m_numLayers || 1 < image.m_depth
			|| !bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::Enum(image.m_format), srgb ? BGFX_TEXTURE_SRGB : 0))
		{
			FROST_ERROR_SET(_err, kErrorTextureFormat, "Texture requires a supported 2D DDS image.");
			return handle;
		}
		uint8_t minAlpha = 255;
		// Validate every mip before handing the container to the backend.
		for (uint8_t ii = 0; ii < image.m_numMips; ++ii)
		{
			bimg::ImageMip mip;
			if (!bimg::imageGetRawData(image, 0, ii, data, size, mip))
			{
				FROST_ERROR_SET(_err, kErrorTextureFormat, "Texture mip data is truncated.");
				return handle;
			}
			minAlpha = min(minAlpha, minimumAlpha(mip, image.m_format));
		}
		handle.idx = s_textureContext->handles->alloc();
		if (!isValid(handle))
		{
			FROST_ERROR_SET(_err, kErrorTextureCreate, "Texture handle capacity has been reached.");
			return handle;
		}
		TextureRef texture;
		texture.guid = header.guid;
		texture.srgb = srgb;
		texture.minAlpha = minAlpha;
		texture.texture = bgfx::createTexture(bgfx::copy(data, size), srgb ? BGFX_TEXTURE_SRGB : 0, 0, &texture.info);
		if (!bgfx::isValid(texture.texture))
		{
			s_textureContext->handles->free(handle.idx);
			FROST_ERROR_SET(_err, kErrorTextureCreate, "Unable to create the texture.");
			return TextureHandle{};
		}
		texture.refCount = 1;
		s_textureContext->refs[handle.idx] = texture;
		return handle;
	}

	TextureHandle createTexture(const Memory* _mem, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		const TextureHandle handle = textureCreate(_mem, _err);
		release(_mem);
		return handle;
	}

	struct MaterialRef
	{
		Guid guid;
		MaterialCreate desc;
		uint32_t refCount = 0;
	};

	struct MaterialContext
	{
		AllocatorI* allocator;
		HandleAlloc* handles;
		MaterialRef* refs;
	};

	static MaterialContext* s_materialContext = NULL;

	static MaterialRef* getMaterialRef(MaterialHandle _handle)
	{
		return NULL != s_materialContext
			&& _handle.idx < s_materialContext->handles->getMaxHandles()
			&& s_materialContext->handles->isValid(_handle.idx)
			? &s_materialContext->refs[_handle.idx] : NULL;
	}

	static bool validateMaterial(const MaterialCreate& _desc, Error* _err)
	{
		for (float value : _desc.baseColor)
		{
			if (!isFinite(value) || value < 0.0f || value > 1.0f)
			{
				FROST_ERROR_SET(_err, kErrorMaterialFormat, "Base color factors must be in [0, 1].");
				return false;
			}
		}
		const float positive[] = { _desc.emissive.x, _desc.emissive.y, _desc.emissive.z, _desc.emissiveStrength, _desc.normalScale };
		for (float value : positive)
		{
			if (!isFinite(value) || value < 0.0f)
			{
				FROST_ERROR_SET(_err, kErrorMaterialFormat, "Emissive and normal factors must be finite and nonnegative.");
				return false;
			}
		}
		const float unit[] = { _desc.metallic, _desc.roughness, _desc.occlusionStrength, _desc.alphaCutoff };
		for (float value : unit)
		{
			if (!isFinite(value) || value < 0.0f || value > 1.0f)
			{
				FROST_ERROR_SET(_err, kErrorMaterialFormat, "Material parameters must be in [0, 1].");
				return false;
			}
		}
		if (0 != (_desc.flags & ~uint32_t(MaterialFlags::Masked | MaterialFlags::DoubleSided)))
		{
			FROST_ERROR_SET(_err, kErrorMaterialFormat, "Material flags are unsupported.");
			return false;
		}
		for (uint32_t ii = 0; ii < MaterialTexture::Count; ++ii)
		{
			const uint32_t sampler = _desc.samplers[ii];
			if (0 != (sampler & ~0x3fu) || (0 != (sampler & TextureSampler::ClampU) && 0 != (sampler & TextureSampler::MirrorU))
				|| (0 != (sampler & TextureSampler::ClampV) && 0 != (sampler & TextureSampler::MirrorV))
				|| (0 != (sampler & TextureSampler::Point) && 0 != (sampler & TextureSampler::Anisotropic)))
			{
				FROST_ERROR_SET(_err, kErrorMaterialFormat, "Material sampler flags are invalid.");
				return false;
			}
			if (!isValid(_desc.textures[ii])) { continue; }
			const TextureRef* texture = getTextureRef(_desc.textures[ii]);
			const bool color = MaterialTexture::BaseColor == ii || MaterialTexture::Emissive == ii;
			if (NULL == texture || (!color && texture->srgb))
			{
				FROST_ERROR_SET(_err, kErrorMaterialFormat, "Material requires live textures; normal and parameter textures must be linear.");
				return false;
			}
		}
		return true;
	}

	MaterialHandle createMaterial(const MaterialCreate& _desc, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		MaterialHandle handle;
		if (!_err->isOk()) { return handle; }
		if (NULL == s_materialContext)
		{
			FROST_ERROR_SET(_err, kErrorMaterialCreate, "Material creation requires graphics startup.");
			return handle;
		}
		if (!validateMaterial(_desc, _err)) { return handle; }
		handle.idx = s_materialContext->handles->alloc();
		if (!isValid(handle))
		{
			FROST_ERROR_SET(_err, kErrorMaterialCreate, "Material handle capacity has been reached.");
			return handle;
		}
		MaterialRef& material = s_materialContext->refs[handle.idx];
		material.desc = _desc;
		material.refCount = 1;
		for (TextureHandle texture : _desc.textures)
		{
			if (isValid(texture)) { addRef(texture); }
		}
		return handle;
	}

	static float readAssetFloat(MemoryReader& _reader, Error* _err)
	{
		uint32_t bits = 0;
		readHE(&_reader, bits, true, _err);
		float value;
		memCopy(&value, &bits, sizeof(value));
		return value;
	}

	static MaterialHandle materialCreate(const Memory* _mem, const TextureHandle* _textures, uint16_t _numTextures, Error* _err)
	{
		MaterialHandle handle;
		if (!_err->isOk()) { return handle; }
		AssetHeader header;
		if (!readAssetHeader(_mem, &header, _err)) { return handle; }
		if (kAssetTypeMaterial != header.type || kMaterialVersion != header.payloadVersion || kMaterialPayloadSize != header.payloadSize
			|| _numTextures != header.dependencyCount || (0 != _numTextures && NULL == _textures))
		{
			FROST_ERROR_SET(_err, kErrorMaterialFormat, "Material asset type, version, payload, or dependency count is invalid.");
			return handle;
		}
		for (uint16_t ii = 0; ii < _numTextures; ++ii)
		{
			AssetDependency dependency;
			if (!readAssetDependency(_mem, ii, &dependency, _err)) { return handle; }
			const TextureRef* texture = getTextureRef(_textures[ii]);
			if (kAssetTypeTexture != dependency.type || NULL == texture || texture->guid != dependency.guid)
			{
				FROST_ERROR_SET(_err, kErrorMaterialFormat, "Resolved material texture does not match its dependency GUID.");
				return handle;
			}
		}
		MemoryReader reader((const uint8_t*)_mem->data + header.payloadOffset, header.payloadSize);
		MaterialCreate desc;
		for (float& value : desc.baseColor) { value = readAssetFloat(reader, _err); }
		desc.emissive.x = readAssetFloat(reader, _err);
		desc.emissive.y = readAssetFloat(reader, _err);
		desc.emissive.z = readAssetFloat(reader, _err);
		desc.emissiveStrength = readAssetFloat(reader, _err);
		desc.metallic = readAssetFloat(reader, _err);
		desc.roughness = readAssetFloat(reader, _err);
		desc.normalScale = readAssetFloat(reader, _err);
		desc.occlusionStrength = readAssetFloat(reader, _err);
		readHE(&reader, desc.flags, true, _err);
		desc.alphaCutoff = readAssetFloat(reader, _err);
		for (uint32_t ii = 0; ii < MaterialTexture::Count; ++ii)
		{
			uint32_t index = UINT32_MAX;
			readHE(&reader, index, true, _err);
			readHE(&reader, desc.samplers[ii], true, _err);
			if (UINT32_MAX == index) { continue; }
			if (index >= _numTextures)
			{
				FROST_ERROR_SET(_err, kErrorMaterialFormat, "Material texture dependency index is invalid.");
				return handle;
			}
			desc.textures[ii] = _textures[index];
		}
		handle = createMaterial(desc, _err);
		if (isValid(handle)) { getMaterialRef(handle)->guid = header.guid; }
		return handle;
	}

	MaterialHandle createMaterial(const Memory* _mem, const TextureHandle* _textures, uint16_t _numTextures, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		const MaterialHandle handle = materialCreate(_mem, _textures, _numTextures, _err);
		release(_mem);
		return handle;
	}

	Guid getGuid(MaterialHandle _handle)
	{
		const MaterialRef* material = getMaterialRef(_handle);
		return NULL != material ? material->guid : Guid{};
	}

	void addRef(MaterialHandle _handle)
	{
		MaterialRef* material = getMaterialRef(_handle);
		FROST_ASSERT(NULL != material && UINT32_MAX != material->refCount, "Cannot retain an invalid or overflowing material reference.");
		++material->refCount;
	}

	static void destroyMaterial(MaterialRef& _material)
	{
		for (TextureHandle texture : _material.desc.textures)
		{
			if (isValid(texture)) { release(texture); }
		}
		_material = MaterialRef{};
	}

	void release(MaterialHandle _handle)
	{
		if (!isValid(_handle)) { return; }
		MaterialRef* material = getMaterialRef(_handle);
		FROST_ASSERT(NULL != material && 0 != material->refCount, "Cannot release an invalid material reference.");
		if (0 == --material->refCount)
		{
			destroyMaterial(*material);
			s_materialContext->handles->free(_handle.idx);
		}
	}


	struct MeshBounds
	{
		Sphere sphere;
		Aabb   aabb;
		Obb    obb;
	};

	struct MeshPrimitive
	{
		char*      name = NULL;
		uint32_t   startIndex = 0;
		uint32_t   numIndices = 0;
		uint32_t   startVertex = 0;
		uint32_t   numVertices = 0;
		MeshBounds bounds;
	};

	struct MeshGroup
	{
		bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
		bgfx::IndexBufferHandle  indexBuffer  = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout       layout;
		MeshBounds               bounds;
		MeshPrimitive*           primitives = NULL;
		uint32_t                 numVertices = 0;
		uint32_t                 numIndices = 0;
		uint16_t                 numPrimitives = 0;
		uint16_t                 materialSlot = 0;
	};

	struct MeshMaterialSlot
	{
		char* name = NULL;
		MaterialHandle material;
	};

	struct MeshLod
	{
		MeshGroup* groups = NULL;
		uint32_t numGroups = 0;
	};

	struct MeshGeometry
	{
		MeshGeometry* next = NULL;
		const Memory* data = NULL; // Serialized geometry for collision-checked sharing.
		MeshLod* lods = NULL;
		Aabb bounds;
		uint32_t hash = 0;
		uint32_t refCount = 0;
		uint16_t numLods = 0;
		uint16_t numMaterials = 0;
	};

	struct MeshRef
	{
		Guid       guid;
		MeshGeometry* geometry = NULL;
		MeshLod* lods = NULL; // Highest detail first; all levels share material slots.
		MeshMaterialSlot* materials = NULL;
		uint16_t numMaterials = 0;
		uint16_t numLods = 0;
		uint32_t   refCount = 0;
	};

	struct MeshContext
	{
		AllocatorI*  allocator;
		HandleAlloc* handles;
		MeshRef*     refs;
		MeshGeometry* geometries[256];
	};

	static MeshContext* s_meshContext = NULL;

	static MeshRef* getMeshRef(MeshHandle _handle)
	{
		return NULL != s_meshContext
			&& _handle.idx < s_meshContext->handles->getMaxHandles()
			&& s_meshContext->handles->isValid(_handle.idx)
			? &s_meshContext->refs[_handle.idx] : NULL;
	}

	static void destroyMesh(MeshRef& _mesh)
	{
		AllocatorI* allocator = s_meshContext->allocator;
		// Material slots belong to the asset; geometry can outlive any one asset.
		bool destroyGeometry = true;
		if (NULL != _mesh.geometry)
		{
			MeshGeometry* geometry = _mesh.geometry;
			destroyGeometry = 0 == --geometry->refCount;
			if (destroyGeometry)
			{
				MeshGeometry** entry = &s_meshContext->geometries[geometry->hash & 255];
				while (*entry != geometry)
				{
					entry = &(*entry)->next;
				}
				*entry = geometry->next;
				release(geometry->data);
				free(allocator, geometry);
			}
		}
		for (uint16_t lodIndex = 0; destroyGeometry && NULL != _mesh.lods && lodIndex < _mesh.numLods; ++lodIndex)
		{
			MeshLod& lod = _mesh.lods[lodIndex];
			for (uint32_t ii = 0; NULL != lod.groups && ii < lod.numGroups; ++ii)
			{
				MeshGroup& group = lod.groups[ii];
				if (bgfx::isValid(group.vertexBuffer)) { bgfx::destroy(group.vertexBuffer); }
				if (bgfx::isValid(group.indexBuffer)) { bgfx::destroy(group.indexBuffer); }
				for (uint16_t jj = 0; jj < group.numPrimitives; ++jj)
				{
					free(allocator, group.primitives[jj].name);
				}
				free(allocator, group.primitives);
			}
			free(allocator, lod.groups);
		}

		for (uint16_t ii = 0; ii < _mesh.numMaterials; ++ii)
		{
			free(allocator, _mesh.materials[ii].name);
			release(_mesh.materials[ii].material);
		}
		free(allocator, _mesh.materials);
		if (destroyGeometry)
		{
			free(allocator, _mesh.lods);
		}
		_mesh = MeshRef{};
	}

	static bool readMeshBounds(MemoryReader& _reader, MeshBounds& _bounds, Error* _err)
	{
		float values[26] = {};

		for (uint32_t ii = 0; ii < FROST_COUNTOF(values); ++ii)
		{
			uint32_t bits = 0;
			readHE(&_reader, bits, true, _err);
			memCopy(&values[ii], &bits, sizeof(bits) );
			if (!isFinite(values[ii]) )
			{
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh bounds contain a non-finite value.");
				return false;
			}
		}

		if (0.0f > values[3] || values[4] > values[7] || values[5] > values[8] || values[6] > values[9])
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh bounds are invalid.");
			return false;
		}

		_bounds.sphere.center = { values[0], values[1], values[2] };
		_bounds.sphere.radius = values[3];
		_bounds.aabb.min = { values[4], values[5], values[6] };
		_bounds.aabb.max = { values[7], values[8], values[9] };

		memCopy(_bounds.obb.mtx, &values[10], sizeof(_bounds.obb.mtx) );

		return _err->isOk();
	}

	static char* readMeshString(MemoryReader& _reader, Error* _err)
	{
		uint16_t size = 0;

		readHE(&_reader, size, true, _err);

		if (!_err->isOk() || size > _reader.remaining() )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh string is truncated.");
			return NULL;
		}

		char* str = (char*)alloc(s_meshContext->allocator, size_t(size) + 1);
		if (NULL == str)
		{
			FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate a mesh string.");
			return NULL;
		}

		read(&_reader, str, size, _err);
		str[size] = '\0';

		for (uint16_t ii = 0; ii < size; ++ii)
		{
			if ('\0' == str[ii])
			{
				free(s_meshContext->allocator, str);
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh string contains an embedded zero.");
				return NULL;
			}
		}

		return str;
	}

	static void meshSkipLayout(bgfx::VertexLayout& _layout, uint16_t _size)
	{
		while (0 != _size)
		{
			const uint8_t step = uint8_t(min<uint16_t>(_size, UINT8_MAX) );
			_layout.skip(step);
			_size -= step;
		}
	}

	static bool readMeshLayout(MemoryReader& _reader, bgfx::VertexLayout& _layout, Error* _err)
	{
		uint16_t stride = 0;
		uint16_t numAttributes = 0;

		readHE(&_reader, stride, true, _err);
		readHE(&_reader, numAttributes, true, _err);

		if (!_err->isOk() || 0 == stride || 0 == numAttributes || bgfx::Attrib::Count < numAttributes
		|| uint32_t(numAttributes) * 8 > _reader.remaining() )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh vertex layout is invalid.");
			return false;
		}

		_layout.begin(bgfx::RendererType::Noop);
		for (uint16_t ii = 0; ii < numAttributes; ++ii)
		{
			uint8_t attr = 0;
			uint8_t num = 0;
			uint8_t type = 0;
			uint8_t flags = 0;
			uint16_t offset = 0;
			uint16_t reserved = 0;

			read(&_reader, attr, _err);
			read(&_reader, num, _err);
			read(&_reader, type, _err);
			read(&_reader, flags, _err);
			readHE(&_reader, offset, true, _err);
			readHE(&_reader, reserved, true, _err);

			if (bgfx::Attrib::Count <= attr || bgfx::AttribType::Count <= type
			|| 0 == num || 4 < num || 0 != (flags & ~3) || 0 != reserved
			|| offset < _layout.getStride() || offset >= stride
			|| _layout.has(bgfx::Attrib::Enum(attr) ) )
			{
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh vertex attribute is invalid.");
				return false;
			}

			bgfx::VertexLayout attribute;
			attribute.begin(bgfx::RendererType::Noop)
				.add(bgfx::Attrib::Enum(attr), num, bgfx::AttribType::Enum(type), 0 != (flags & 1), 0 != (flags & 2) )
				.end();

			if (attribute.getStride() > stride - offset)
			{
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh vertex attribute exceeds its stride.");
				return false;
			}

			meshSkipLayout(_layout, offset - _layout.getStride() );
			_layout.add(bgfx::Attrib::Enum(attr), num, bgfx::AttribType::Enum(type), 0 != (flags & 1), 0 != (flags & 2) );

			if (_layout.getStride() > stride)
			{
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh vertex attribute exceeds its stride.");
				return false;
			}
		}

		meshSkipLayout(_layout, stride - _layout.getStride() );
		_layout.end();

		// Match the packed inputs of vs_deferred_geom, as produced by meshc
		// with --packnormal 1 --tangent.
		if (!_layout.has(bgfx::Attrib::Position) || !_layout.has(bgfx::Attrib::Normal)
		|| !_layout.has(bgfx::Attrib::Tangent) || !_layout.has(bgfx::Attrib::TexCoord0) )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Deferred meshes require positions, packed normals/tangents, and UVs.");
			return false;
		}

		uint8_t num;
		bgfx::AttribType::Enum type;
		bool normalized;
		bool asInt;
		_layout.decode(bgfx::Attrib::Position, num, type, normalized, asInt);

		if (3 != num || bgfx::AttribType::Float != type)
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh position must contain three floats.");
			return false;
		}

		const bgfx::Attrib::Enum packedAttributes[] = { bgfx::Attrib::Normal, bgfx::Attrib::Tangent };
		for (bgfx::Attrib::Enum attr : packedAttributes)
		{
			_layout.decode(attr, num, type, normalized, asInt);
			if (4 != num || bgfx::AttribType::Uint8 != type || !normalized || !asInt)
			{
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Deferred meshes require meshc --packnormal 1 --tangent.");
				return false;
			}
		}

		_layout.decode(bgfx::Attrib::TexCoord0, num, type, normalized, asInt);

		if (2 != num || (bgfx::AttribType::Float != type && bgfx::AttribType::Half != type))
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh UVs must contain two floats or half floats.");
			return false;
		}

		return _err->isOk();
	}

	static const Memory* readMeshBuffer(MemoryReader& _reader, uint32_t _size, bool _compressed, bool _vertex, uint32_t _count, uint16_t _stride, Error* _err)
	{
		uint32_t storedSize = 0;
		readHE(&_reader, storedSize, true, _err);

		if (!_err->isOk() || 0 == storedSize || storedSize > _reader.remaining()
		|| (!_compressed && storedSize != _size) )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh buffer size is invalid.");
			return NULL;
		}

		const Memory* mem = allocMemory(s_meshContext->allocator, _size);
		if (NULL == mem)
		{
			FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate mesh upload memory.");
			return NULL;
		}

		if (_compressed)
		{
			const int result = _vertex
				? meshopt_decodeVertexBuffer(mem->data, _count, _stride, _reader.getDataPtr(), storedSize)
				: meshopt_decodeIndexBuffer(mem->data, _count, _stride, _reader.getDataPtr(), storedSize);

			if (0 != result)
			{
				release(mem);
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh buffer decompression failed.");
				return NULL;
			}
		}
		else
		{
			memCopy(mem->data, _reader.getDataPtr(), _size);
		}

		skip(&_reader, storedSize);
		return mem;
	}

#if FROST_CPU_ENDIAN_BIG
	static void swapMeshVertices(void* _data, uint32_t _count, const bgfx::VertexLayout& _layout)
	{
		for (uint32_t ii = 0; ii < bgfx::Attrib::Count; ++ii)
		{
			const bgfx::Attrib::Enum attr = bgfx::Attrib::Enum(ii);
			if (!_layout.has(attr) )
			{
				continue;
			}

			uint8_t num;
			bgfx::AttribType::Enum type;
			bool normalized;
			bool asInt;
			_layout.decode(attr, num, type, normalized, asInt);

			if (bgfx::AttribType::Int8 == type || bgfx::AttribType::Uint8 == type)
			{
				continue;
			}

			const uint32_t width = bgfx::AttribType::Int16 == type || bgfx::AttribType::Uint16 == type || bgfx::AttribType::Half == type ? 2 : 4;
			const uint32_t components = bgfx::AttribType::Uint10 == type ? 1 : num;

			for (uint32_t jj = 0; jj < _count; ++jj)
			{
				uint8_t* data = (uint8_t*)_data + jj * _layout.getStride() + _layout.getOffset(attr);
				for (uint32_t kk = 0; kk < components; ++kk, data += width)
				{
					if (2 == width)
					{
						uint16_t value;
						memCopy(&value, data, sizeof(value) );
						value = endianSwap(value);
						memCopy(data, &value, sizeof(value) );
					}
					else
					{
						uint32_t value;
						memCopy(&value, data, sizeof(value) );
						value = endianSwap(value);
						memCopy(data, &value, sizeof(value) );
					}
				}
			}
		}
	}
#endif // FROST_CPU_ENDIAN_BIG

	static bool readMeshGroup(MemoryReader& _reader, MeshGroup& _group, Error* _err)
	{
		if (!readMeshBounds(_reader, _group.bounds, _err) || !readMeshLayout(_reader, _group.layout, _err) )
		{
			return false;
		}

		uint32_t flags = 0;
		readHE(&_reader, _group.numVertices, true, _err);
		readHE(&_reader, _group.numIndices, true, _err);
		readHE(&_reader, flags, true, _err);

		const uint16_t stride = _group.layout.getStride();
		const uint16_t indexSize = 0 != (flags & MeshFormat::Index32) ? 4 : 2;

		if (!_err->isOk() || 0 == _group.numVertices || 0 == _group.numIndices || 0 != _group.numIndices % 3
		|| _group.numVertices > UINT32_MAX / stride || _group.numIndices > UINT32_MAX / indexSize
		|| (2 == indexSize && UINT16_MAX < _group.numVertices) || 0 != (flags & ~7)
		|| (0 != (flags & MeshFormat::VertexCompressed) && (0 != stride % 4 || 256 < stride) )
		|| (4 == indexSize && 0 == (bgfx::getCaps()->supported & BGFX_CAPS_INDEX32) ) )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh group counts or encoding flags are invalid.");
			return false;
		}

		readHE(&_reader, _group.materialSlot, true, _err);
		readHE(&_reader, _group.numPrimitives, true, _err);
		if (!_err->isOk() || 0 == _group.numPrimitives || uint32_t(_group.numPrimitives) * 122 > _reader.remaining() )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh primitive count is invalid.");
			_group.numPrimitives = 0;
			return false;
		}

		_group.primitives = (MeshPrimitive*)alloc(s_meshContext->allocator, sizeof(MeshPrimitive) * _group.numPrimitives);
		if (NULL == _group.primitives)
		{
			_group.numPrimitives = 0;
			FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate mesh primitives.");
			return false;
		}

		for (uint16_t ii = 0; ii < _group.numPrimitives; ++ii)
		{
			FROST_PLACEMENT_NEW(&_group.primitives[ii], MeshPrimitive)();
		}

		const Memory* vertices = readMeshBuffer(_reader, _group.numVertices * stride,
			0 != (flags & MeshFormat::VertexCompressed), true, _group.numVertices, stride, _err);
		if (NULL == vertices)
		{
			return false;
		}

		const Memory* indices = readMeshBuffer(_reader, _group.numIndices * indexSize,
			0 != (flags & MeshFormat::IndexCompressed), false, _group.numIndices, indexSize, _err);
		if (NULL == indices)
		{
			release(vertices);
			return false;
		}

#if FROST_CPU_ENDIAN_BIG
		swapMeshVertices(vertices->data, _group.numVertices, _group.layout);
#endif // FROST_CPU_ENDIAN_BIG

		bool valid = true;
		for (uint32_t ii = 0; ii < _group.numIndices; ++ii)
		{
			uint32_t index = 0;
			uint8_t* data = (uint8_t*)indices->data + ii * indexSize;

			if (2 == indexSize)
			{
				uint16_t value;
				memCopy(&value, data, sizeof(value) );
				value = 0 != (flags & MeshFormat::IndexCompressed) ? value : toHostEndian(value, true);
				index = value;
				memCopy(data, &value, sizeof(value) );
			}
			else
			{
				memCopy(&index, data, sizeof(index) );
				index = 0 != (flags & MeshFormat::IndexCompressed) ? index : toHostEndian(index, true);
				memCopy(data, &index, sizeof(index) );
			}

			valid &= index < _group.numVertices;
		}
		for (uint32_t ii = 0; ii < _group.numVertices; ++ii)
		{
			float position[4];
			float normal[4];
			float tangent[4];
			float uv[4];
			bgfx::vertexUnpack(position, bgfx::Attrib::Position, _group.layout, vertices->data, ii);
			bgfx::vertexUnpack(normal, bgfx::Attrib::Normal, _group.layout, vertices->data, ii);
			bgfx::vertexUnpack(tangent, bgfx::Attrib::Tangent, _group.layout, vertices->data, ii);
			bgfx::vertexUnpack(uv, bgfx::Attrib::TexCoord0, _group.layout, vertices->data, ii);
			valid &= isFinite(position[0]) && isFinite(position[1]) && isFinite(position[2])
				&& isFinite(normal[0]) && isFinite(normal[1]) && isFinite(normal[2])
				&& isFinite(tangent[0]) && isFinite(tangent[1]) && isFinite(tangent[2]) && isFinite(tangent[3])
				&& isFinite(uv[0]) && isFinite(uv[1])
				&& 0.5f < normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]
				&& 0.5f < tangent[0]*tangent[0] + tangent[1]*tangent[1] + tangent[2]*tangent[2]
				&& 0.5f < abs(tangent[3]);
		}

		for (uint16_t ii = 0; ii < _group.numPrimitives && _err->isOk(); ++ii)
		{
			MeshPrimitive& primitive = _group.primitives[ii];
			primitive.name = readMeshString(_reader, _err);
			readHE(&_reader, primitive.startIndex, true, _err);
			readHE(&_reader, primitive.numIndices, true, _err);
			readHE(&_reader, primitive.startVertex, true, _err);
			readHE(&_reader, primitive.numVertices, true, _err);
			valid &= readMeshBounds(_reader, primitive.bounds, _err);
			valid &= primitive.startIndex <= _group.numIndices
				&& primitive.numIndices <= _group.numIndices - primitive.startIndex
				&& 0 != primitive.numIndices && 0 == primitive.numIndices % 3
				&& primitive.startVertex <= _group.numVertices
				&& primitive.numVertices <= _group.numVertices - primitive.startVertex
				&& 0 != primitive.numVertices;
		}

		if (!valid || !_err->isOk() )
		{
			release(vertices);
			release(indices);
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh indices, vertices, or primitive ranges are invalid.");
			return false;
		}

		_group.vertexBuffer = bgfx::createVertexBuffer(bgfx::copy(vertices->data, vertices->size), _group.layout);
		_group.indexBuffer = bgfx::createIndexBuffer(bgfx::copy(indices->data, indices->size), 4 == indexSize ? BGFX_BUFFER_INDEX32 : BGFX_BUFFER_NONE);
		release(vertices);
		release(indices);

		if (!bgfx::isValid(_group.vertexBuffer) || !bgfx::isValid(_group.indexBuffer) )
		{
			FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to create mesh GPU buffers.");
			return false;
		}

		return true;
	}

	static MeshHandle meshCreate(const Memory* _mem, const MaterialHandle* _materials, uint16_t _numMaterials, Error* _err)
	{
		MeshHandle handle;
		if (!_err->isOk())
		{
			return handle;
		}

		if (NULL == s_meshContext)
		{
			FROST_ERROR_SET(_err, kErrorMeshCreate, "Mesh creation requires graphics startup.");
			return handle;
		}

		AssetHeader header;
		if (!readAssetHeader(_mem, &header, _err) )
		{
			return handle;
		}
		if (kAssetTypeMesh != header.type || kMeshVersion != header.payloadVersion
			|| header.dependencyCount != _numMaterials || (0 != _numMaterials && NULL == _materials))
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh asset type, version, or dependencies are unsupported.");
			return handle;
		}

		MemoryReader reader((const uint8_t*)_mem->data + header.payloadOffset, header.payloadSize);

		MeshRef mesh;
		mesh.guid = header.guid;

		readHE(&reader, mesh.numLods, true, _err);
		if (!_err->isOk() || 0 == mesh.numLods || mesh.numLods > reader.remaining() / 140)
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh LOD count is invalid.");
			return handle;
		}

		for (uint16_t ii = 0; ii < _numMaterials; ++ii)
		{
			AssetDependency dependency;
			if (!readAssetDependency(_mem, ii, &dependency, _err)) { return handle; }
			const MaterialRef* material = getMaterialRef(_materials[ii]);
			if (kAssetTypeMaterial != dependency.type || NULL == material || material->guid != dependency.guid)
			{
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Resolved mesh material does not match its dependency GUID.");
				return handle;
			}
		}
		uint16_t numSlots = 0;
		readHE(&reader, numSlots, true, _err);
		if (!_err->isOk() || 0 == numSlots || numSlots > reader.remaining() / 6)
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh material slot count is invalid.");
			return handle;
		}
		mesh.materials = (MeshMaterialSlot*)alloc(s_meshContext->allocator, sizeof(MeshMaterialSlot) * numSlots);
		if (NULL == mesh.materials)
		{
			FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate mesh material slots.");
			return handle;
		}
		for (uint16_t ii = 0; ii < numSlots; ++ii) { FROST_PLACEMENT_NEW(&mesh.materials[ii], MeshMaterialSlot)(); }
		mesh.numMaterials = numSlots;
		for (uint16_t ii = 0; ii < numSlots && _err->isOk(); ++ii)
		{
			MeshMaterialSlot& slot = mesh.materials[ii];
			slot.name = readMeshString(reader, _err);
			uint32_t index = UINT32_MAX;
			readHE(&reader, index, true, _err);
			if (!_err->isOk()) { break; }
			if (UINT32_MAX != index)
			{
				if (index >= _numMaterials)
				{
					FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh material dependency index is invalid.");
					break;
				}
				slot.material = _materials[index];
				addRef(slot.material);
			}
			for (uint16_t jj = 0; jj < ii; ++jj)
			{
				if (0 == strCmp(slot.name, mesh.materials[jj].name))
				{
					FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh contains duplicate material slot names.");
					break;
				}
			}
		}
		if (!_err->isOk()) { destroyMesh(mesh); return handle; }

		const uint32_t geometrySize = reader.remaining();
		const uint8_t* geometryData = (const uint8_t*)_mem->data + header.payloadOffset + header.payloadSize - geometrySize;
		HashMurmur2A hash;
		hash.begin();
		hash.add(mesh.numLods);
		hash.add(mesh.numMaterials);
		hash.add(geometryData, geometrySize);
		const uint32_t geometryHash = hash.end();
		for (MeshGeometry* geometry = s_meshContext->geometries[geometryHash & 255]; NULL != geometry; geometry = geometry->next)
		{
			if (geometryHash == geometry->hash && mesh.numLods == geometry->numLods
				&& mesh.numMaterials == geometry->numMaterials && geometrySize == geometry->data->size
				&& 0 == memCmp(geometryData, geometry->data->data, geometrySize))
			{
				mesh.geometry = geometry;
				mesh.lods = geometry->lods;
				++geometry->refCount;
				reader.seek(geometrySize, Whence::Current);
				break;
			}
		}

		if (NULL == mesh.geometry)
		{
			mesh.lods = (MeshLod*)alloc(s_meshContext->allocator, sizeof(MeshLod) * mesh.numLods);
			if (NULL == mesh.lods)
			{
				destroyMesh(mesh);
				FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate mesh LODs.");
				return handle;
			}
			for (uint16_t ii = 0; ii < mesh.numLods; ++ii)
			{
				FROST_PLACEMENT_NEW(&mesh.lods[ii], MeshLod)();
			}
			for (uint16_t lodIndex = 0; lodIndex < mesh.numLods && _err->isOk(); ++lodIndex)
			{
				MeshLod& lod = mesh.lods[lodIndex];
				readHE(&reader, lod.numGroups, true, _err);
				if (!_err->isOk() || 0 == lod.numGroups || UINT16_MAX < lod.numGroups || lod.numGroups > reader.remaining() / 140)
				{
					FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh LOD group count is invalid.");
					break;
				}
				lod.groups = (MeshGroup*)alloc(s_meshContext->allocator, sizeof(MeshGroup) * lod.numGroups);
				if (NULL == lod.groups)
				{
					FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate mesh groups.");
					break;
				}
				for (uint32_t ii = 0; ii < lod.numGroups; ++ii)
				{
					FROST_PLACEMENT_NEW(&lod.groups[ii], MeshGroup)();
				}
				for (uint32_t ii = 0; ii < lod.numGroups && _err->isOk(); ++ii)
				{
					MeshGroup& group = lod.groups[ii];
					if (!readMeshGroup(reader, group, _err)) { break; }
					if (group.materialSlot >= mesh.numMaterials)
					{
						FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh group material slot is invalid.");
					}
				}
			}

		}

		if (_err->isOk() && 0 != reader.remaining() )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh payload contains trailing data.");
		}

		if (_err->isOk() )
		{
			handle.idx = s_meshContext->handles->alloc();
			if (!isValid(handle) )
			{
				FROST_ERROR_SET(_err, kErrorMeshCreate, "Mesh handle capacity has been reached.");
			}
		}

		if (!_err->isOk() )
		{
			destroyMesh(mesh);
			return handle;
		}

		if (NULL == mesh.geometry)
		{
			MeshGeometry* geometry = (MeshGeometry*)alloc(s_meshContext->allocator, sizeof(MeshGeometry));
			const Memory* data = copyMemory(s_meshContext->allocator, geometryData, geometrySize);
			if (NULL == geometry || NULL == data)
			{
				free(s_meshContext->allocator, geometry);
				release(data);
				s_meshContext->handles->free(handle.idx);
				destroyMesh(mesh);
				FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate shared mesh geometry.");
				return {};
			}
			FROST_PLACEMENT_NEW(geometry, MeshGeometry)();
			geometry->data = data;
			geometry->lods = mesh.lods;
			geometry->bounds = mesh.lods[0].groups[0].bounds.aabb;
			for (uint32_t ii = 1; ii < mesh.lods[0].numGroups; ++ii)
			{
				geometry->bounds.min = min(geometry->bounds.min, mesh.lods[0].groups[ii].bounds.aabb.min);
				geometry->bounds.max = max(geometry->bounds.max, mesh.lods[0].groups[ii].bounds.aabb.max);
			}
			geometry->hash = geometryHash;
			geometry->refCount = 1;
			geometry->numLods = mesh.numLods;
			geometry->numMaterials = mesh.numMaterials;
			geometry->next = s_meshContext->geometries[geometryHash & 255];
			s_meshContext->geometries[geometryHash & 255] = geometry;
			mesh.geometry = geometry;
		}
		mesh.refCount = 1;
		s_meshContext->refs[handle.idx] = mesh;
		return handle;
	}

	MeshHandle createMesh(const Memory* _mem, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		const MeshHandle handle = meshCreate(_mem, NULL, 0, _err);
		release(_mem);
		return handle;
	}

	MeshHandle createMesh(const Memory* _mem, const MaterialHandle* _materials, uint16_t _numMaterials, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		const MeshHandle handle = meshCreate(_mem, _materials, _numMaterials, _err);
		release(_mem);
		return handle;
	}

	uint16_t getMeshMaterialCount(MeshHandle _handle)
	{
		const MeshRef* mesh = getMeshRef(_handle);
		return NULL != mesh ? mesh->numMaterials : 0;
	}

	StringView getMeshMaterialName(MeshHandle _handle, uint16_t _slot)
	{
		const MeshRef* mesh = getMeshRef(_handle);
		return NULL != mesh && _slot < mesh->numMaterials ? StringView(mesh->materials[_slot].name) : StringView();
	}

	Guid getGuid(MeshHandle _handle)
	{
		const MeshRef* mesh = getMeshRef(_handle);
		return NULL != mesh ? mesh->guid : Guid{};
	}

	void addRef(MeshHandle _handle)
	{
		MeshRef* mesh = getMeshRef(_handle);
		FROST_ASSERT(NULL != mesh && UINT32_MAX != mesh->refCount, "Cannot retain an invalid or overflowing mesh reference.");
		++mesh->refCount;
	}

	void release(MeshHandle _handle)
	{
		if (!isValid(_handle) )
		{
			return;
		}
		MeshRef* mesh = getMeshRef(_handle);
		FROST_ASSERT(NULL != mesh && 0 != mesh->refCount, "Cannot release an invalid mesh reference.");
		if (0 == --mesh->refCount)
		{
			destroyMesh(*mesh);
			s_meshContext->handles->free(_handle.idx);
		}
	}

	struct TextureSystem
	{
		static void startup(World& _world, ResT<GraphicsDesc> _graphics)
		{
			FROST_ASSERT(NULL == s_textureContext && 0 < _graphics->maxTextures, "Graphics requires one texture context with nonzero capacity.");

			AllocatorI* allocator = _world.getAllocator();
			TextureContext* context = (TextureContext*)alloc(allocator, sizeof(TextureContext) );
			TextureRef* refs = (TextureRef*)alloc(allocator, sizeof(TextureRef) * _graphics->maxTextures);

			void* handles = alloc(allocator, sizeof(HandleAlloc) + 2 * _graphics->maxTextures * sizeof(uint16_t) );
			FROST_ASSERT(NULL != context && NULL != refs && NULL != handles, "Failed to allocate texture storage.");

			context->allocator = allocator;
			context->handles = FROST_PLACEMENT_NEW(handles, HandleAlloc)(_graphics->maxTextures);
			context->refs = refs;

			for (uint16_t ii = 0; ii < _graphics->maxTextures; ++ii)
			{
				FROST_PLACEMENT_NEW(&refs[ii], TextureRef)();
			}
			const uint32_t white = UINT32_C(0xffffffff);
			const uint8_t normal[] = { 128, 128, 255, 255 };
			context->white = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(&white, sizeof(white)));
			context->normal = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(normal, sizeof(normal)));
			FROST_ASSERT(bgfx::isValid(context->white) && bgfx::isValid(context->normal), "Failed to create fallback textures.");
			s_textureContext = context;
		}

		static void shutdown()
		{
			while (0 != s_textureContext->handles->getNumHandles() )
			{
				const uint16_t idx = s_textureContext->handles->getHandleAt(0);
				bgfx::destroy(s_textureContext->refs[idx].texture);
				s_textureContext->handles->free(idx);
			}

			bgfx::destroy(s_textureContext->white);
			bgfx::destroy(s_textureContext->normal);
			AllocatorI* allocator = s_textureContext->allocator;
			destroyHandleAlloc(allocator, s_textureContext->handles);
			free(allocator, s_textureContext->refs);
			free(allocator, s_textureContext);
			s_textureContext = NULL;
		}
	};

	struct MaterialSystem
	{
		static void startup(World& _world, ResT<GraphicsDesc> _graphics)
		{
			FROST_ASSERT(NULL == s_materialContext && 0 < _graphics->maxMaterials, "Graphics requires one material context with nonzero capacity.");

			AllocatorI* allocator = _world.getAllocator();
			MaterialContext* context = (MaterialContext*)alloc(allocator, sizeof(MaterialContext) );
			MaterialRef* refs = (MaterialRef*)alloc(allocator, sizeof(MaterialRef) * _graphics->maxMaterials);

			void* handles = alloc(allocator, sizeof(HandleAlloc) + 2 * _graphics->maxMaterials * sizeof(uint16_t) );
			FROST_ASSERT(NULL != context && NULL != refs && NULL != handles, "Failed to allocate material storage.");

			context->allocator = allocator;
			context->handles = FROST_PLACEMENT_NEW(handles, HandleAlloc)(_graphics->maxMaterials);
			context->refs = refs;

			for (uint16_t ii = 0; ii < _graphics->maxMaterials; ++ii)
			{
				FROST_PLACEMENT_NEW(&refs[ii], MaterialRef)();
			}
			s_materialContext = context;
		}

		static void shutdown()
		{
			while (0 != s_materialContext->handles->getNumHandles() )
			{
				const uint16_t idx = s_materialContext->handles->getHandleAt(0);
				destroyMaterial(s_materialContext->refs[idx]);
				s_materialContext->handles->free(idx);
			}

			AllocatorI* allocator = s_materialContext->allocator;
			destroyHandleAlloc(allocator, s_materialContext->handles);
			free(allocator, s_materialContext->refs);
			free(allocator, s_materialContext);
			s_materialContext = NULL;
		}
	};

	struct MeshSystem
	{
		static void startup(World& _world, ResT<GraphicsDesc> _graphics)
		{
			FROST_ASSERT(NULL == s_meshContext && 0 < _graphics->maxMeshes, "Graphics requires one mesh context with nonzero capacity.");

			AllocatorI* allocator = _world.getAllocator();
			MeshContext* context = (MeshContext*)alloc(allocator, sizeof(MeshContext) );
			MeshRef* refs = (MeshRef*)alloc(allocator, sizeof(MeshRef) * _graphics->maxMeshes);

			void* handles = alloc(allocator, sizeof(HandleAlloc) + 2 * _graphics->maxMeshes * sizeof(uint16_t) );
			FROST_ASSERT(NULL != context && NULL != refs && NULL != handles, "Failed to allocate mesh storage.");

			context->allocator = allocator;
			context->handles = FROST_PLACEMENT_NEW(handles, HandleAlloc)(_graphics->maxMeshes);
			context->refs = refs;
			memSet(context->geometries, 0, sizeof(context->geometries));

			for (uint16_t ii = 0; ii < _graphics->maxMeshes; ++ii)
			{
				FROST_PLACEMENT_NEW(&refs[ii], MeshRef)();
			}
			s_meshContext = context;
		}

		static void shutdown()
		{
			while (0 != s_meshContext->handles->getNumHandles() )
			{
				const uint16_t idx = s_meshContext->handles->getHandleAt(0);
				destroyMesh(s_meshContext->refs[idx]);
				s_meshContext->handles->free(idx);
			}

			AllocatorI* allocator = s_meshContext->allocator;
			destroyHandleAlloc(allocator, s_meshContext->handles);
			free(allocator, s_meshContext->refs);
			free(allocator, s_meshContext);
			s_meshContext = NULL;
		}
	};

	static bool getMeshTransform(float* _matrix, const TransformComponent* _transform)
	{
		mtxIdentity(_matrix);
		if (NULL == _transform) { return true; }
		const Vec3& scale = _transform->scale;
		if (0.0f == scale.x || 0.0f == scale.y || 0.0f == scale.z) { return false; }
		const Quaternion rotation = normalize(_transform->rotation);
		store(&_matrix[0], mul(toXAxis(rotation), scale.x));
		store(&_matrix[4], mul(toYAxis(rotation), scale.y));
		store(&_matrix[8], mul(toZAxis(rotation), scale.z));
		store(&_matrix[12], _transform->position);
		return true;
	}

	static bool getWorldTransform(float* _matrix, const World& _world, Entity _entity,
		const TransformComponent* _transform)
	{
		if (!getMeshTransform(_matrix, _transform)) { return false; }
		for (Entity parent = _world.getParent(_entity); _world.contains(parent); parent = _world.getParent(parent))
		{
			float local[16], combined[16];
			if (!getMeshTransform(local, _world.get<TransformComponent>(parent))) { return false; }
			mtxMul(combined, _matrix, local);
			memCopy(_matrix, combined, sizeof(combined));
		}
		return true;
	}

	static const MaterialCreate& getMeshMaterial(const MeshComponent& _instance, const MeshRef& _mesh, uint16_t _slot)
	{
		MaterialHandle handle = _mesh.materials[_slot].material;
		if (_slot < _instance.numMaterials && NULL != _instance.materials && isValid(_instance.materials[_slot]))
		{
			handle = _instance.materials[_slot];
		}
		const MaterialRef* material = getMaterialRef(handle);
		FROST_ASSERT(!isValid(handle) || NULL != material, "Mesh must reference live materials.");
		static const MaterialCreate fallback;
		return NULL != material ? material->desc : fallback;
	}

	static bool isMaterialMasked(const MaterialCreate& _material)
	{
		if (0 == (_material.flags & MaterialFlags::Masked))
		{
			return false;
		}
		const TextureRef* texture = getTextureRef(_material.textures[MaterialTexture::BaseColor]);
		float alpha = _material.baseColor[3];
		if (NULL != texture)
		{
			alpha *= float(texture->minAlpha) / 255.0f;
		}
		return alpha < _material.alphaCutoff;
	}

	static uint64_t meshState(const MaterialCreate& _material, const float* _matrix)
	{
		uint64_t state = 0;
		const Vec3 x = { _matrix[0], _matrix[1], _matrix[2] };
		const Vec3 y = { _matrix[4], _matrix[5], _matrix[6] };
		const Vec3 z = { _matrix[8], _matrix[9], _matrix[10] };
		if (dot(cross(x, y), z) < 0.0f)
		{
			state |= BGFX_STATE_FRONT_CCW;
		}
		if (0 == (_material.flags & MaterialFlags::DoubleSided)) { state |= BGFX_STATE_CULL_CCW; }
		return state;
	}

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
