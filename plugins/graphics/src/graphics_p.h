/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
#define BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD

#include <graphics.h>
#include <device.h>
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bimg/bimg.h>
#include <bx/error.h>

#include <charset.h> // bgfx's built-in bitmap font, retained with its upstream license.

#include <frost/math.h>
#include <frost/bounds.h>
#include <frost/handlealloc.h>
#include <frost/readerwriter.h>
#include <frost/bufferstorage.h>
#include <frost/sort.h>

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
		// Validate every mip before handing the container to the backend.
		for (uint8_t ii = 0; ii < image.m_numMips; ++ii)
		{
			bimg::ImageMip mip;
			if (!bimg::imageGetRawData(image, 0, ii, data, size, mip))
			{
				FROST_ERROR_SET(_err, kErrorTextureFormat, "Texture mip data is truncated.");
				return handle;
			}
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
		MaterialDesc desc;
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

	static bool validateMaterial(const MaterialDesc& _desc, Error* _err)
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

	MaterialHandle createMaterial(const MaterialDesc& _desc, Error* _err)
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
		MaterialDesc desc;
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

	struct MeshRef
	{
		Guid       guid;
		MeshGroup* groups = NULL;
		MeshMaterialSlot* materials = NULL;
		uint16_t numMaterials = 0;
		uint32_t   numGroups = 0;
		uint32_t   refCount = 0;
	};

	struct MeshContext
	{
		AllocatorI*  allocator;
		HandleAlloc* handles;
		MeshRef*     refs;
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
		for (uint32_t ii = 0; NULL != _mesh.groups && ii < _mesh.numGroups; ++ii)
		{
			MeshGroup& group = _mesh.groups[ii];
			if (bgfx::isValid(group.vertexBuffer) )
			{
				bgfx::destroy(group.vertexBuffer);
			}

			if (bgfx::isValid(group.indexBuffer) )
			{
				bgfx::destroy(group.indexBuffer);
			}

			for (uint16_t jj = 0; jj < group.numPrimitives; ++jj)
			{
				free(allocator, group.primitives[jj].name);
			}

			free(allocator, group.primitives);
		}

		for (uint16_t ii = 0; ii < _mesh.numMaterials; ++ii)
		{
			free(allocator, _mesh.materials[ii].name);
			release(_mesh.materials[ii].material);
		}
		free(allocator, _mesh.materials);
		free(allocator, _mesh.groups);
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

		readHE(&reader, mesh.numGroups, true, _err);

		if (!_err->isOk() || 0 == mesh.numGroups || UINT16_MAX < mesh.numGroups || mesh.numGroups > reader.remaining() / 140)
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh group count is invalid.");
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

		mesh.groups = (MeshGroup*)alloc(s_meshContext->allocator, sizeof(MeshGroup) * mesh.numGroups);

		if (NULL == mesh.groups)
		{
			mesh.numGroups = 0;
			destroyMesh(mesh);
			FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate mesh groups.");
			return handle;
		}

		for (uint32_t ii = 0; ii < mesh.numGroups; ++ii)
		{
			FROST_PLACEMENT_NEW(&mesh.groups[ii], MeshGroup)();
		}

		for (uint32_t ii = 0; ii < mesh.numGroups && _err->isOk(); ++ii)
		{
			MeshGroup& group = mesh.groups[ii];
			if (!readMeshGroup(reader, group, _err)) { break; }
			if (group.materialSlot >= mesh.numMaterials)
			{
				FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh group material slot is invalid.");
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
		static void startup(World& _world, ResT<GraphicsResource> _graphics)
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
		static void startup(World& _world, ResT<GraphicsResource> _graphics)
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
		static void startup(World& _world, ResT<GraphicsResource> _graphics)
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

} // namespace frost

// Renderer
#include "vs_deferred_geom.bin.h"
#include "fs_deferred_geom.bin.h"
#include "vs_deferred_light.bin.h"
#include "fs_deferred_light.bin.h"
#include "vs_sky.bin.h"
#include "fs_sky.bin.h"
#include "vs_deferred_combine.bin.h"
#include "fs_deferred_combine.bin.h"
#include "vs_ui.bin.h"
#include "fs_ui.bin.h"

namespace frost
{
	constexpr bgfx::ViewId kRenderPassGeometry = 0;
	constexpr bgfx::ViewId kRenderPassLight = 1;
	constexpr bgfx::ViewId kRenderPassSky = 2;
	constexpr bgfx::ViewId kRenderPassToneMap = 3;
	constexpr bgfx::ViewId kRenderPassDebug = 4;
	constexpr bgfx::ViewId kRenderPassUi = 5;

	static const bgfx::EmbeddedShader s_embeddedShaders[] =
	{
		BGFX_EMBEDDED_SHADER(vs_deferred_geom),
		BGFX_EMBEDDED_SHADER(fs_deferred_geom),
		BGFX_EMBEDDED_SHADER(vs_deferred_light),
		BGFX_EMBEDDED_SHADER(fs_deferred_light),
		BGFX_EMBEDDED_SHADER(vs_sky),
		BGFX_EMBEDDED_SHADER(fs_sky),
		BGFX_EMBEDDED_SHADER(vs_deferred_combine),
		BGFX_EMBEDDED_SHADER(fs_deferred_combine),
		BGFX_EMBEDDED_SHADER_END()
	};

} // namespace frost

// Geometry pass
namespace frost
{
	struct RenderFrame
	{
		uint16_t width = 0;
		uint16_t height = 0;
		bool active = false;
		float view[16] = {};
		float projection[16] = {};
		float screenProjection[16] = {};
		float inverseViewProjection[16] = {};
		float exposure = 1.0f;
		float cameraPosition[4] = {};
		float cameraDirection[4] = {};
	};

	struct RenderFrameSystem
	{
		static void update(ResT<DisplayResource> _display, ResT<ActiveCameraResource> _activeCamera,
			QueryT<const CameraComponent&> _cameras, ResMutT<RenderFrame> _frame)
		{
			_frame->width = uint16_t(_display->width);
			_frame->height = uint16_t(_display->height);
			_frame->active = false;
			if (0 == _frame->width || 0 == _frame->height)
			{
				return;
			}

			_frame->active = getCameraMatrices(*_activeCamera, _cameras, _frame->width, _frame->height,
				_frame->view, _frame->projection);
			if (_frame->active)
			{
				float viewProjection[16];
				mtxMul(viewProjection, _frame->view, _frame->projection);
				mtxInverse(_frame->inverseViewProjection, viewProjection);
				_cameras.get(_activeCamera->entity, [&](const CameraComponent& _camera)
				{
					store(_frame->cameraPosition, _camera.position);
					_frame->cameraPosition[3] = CameraProjection::Orthographic == _camera.projection ? 1.0f : 0.0f;
					store(_frame->cameraDirection, toZAxis(normalize(_camera.rotation)));
					_frame->exposure = exp2(isFinite(_camera.exposure) ? clamp(_camera.exposure, -20.0f, 20.0f) : 0.0f);
				});
				mtxOrtho(_frame->screenProjection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f,
					0.0f, bgfx::getCaps()->homogeneousDepth);
			}
		}
	};

	struct GBufferResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
		// Base color, normal/coverage, depth, metallic/roughness/AO, HDR emissive.
		bgfx::TextureHandle textures[5] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
			BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		bgfx::UniformHandle samplers[MaterialTexture::Count];
		bgfx::UniformHandle u_materialBaseColor = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_materialParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_materialEmissive = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_materialFlags = BGFX_INVALID_HANDLE;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct GBufferSystem
	{
		static void startup(ResMutT<GBufferResource> _gbuffer)
		{
			bgfx::setViewName(kRenderPassGeometry, "Geometry");

			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_gbuffer->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_geom"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_deferred_geom"), true);

			FROST_ASSERT(bgfx::isValid(_gbuffer->program), "Failed to create the geometry program.");

			const char* samplers[] = { "s_baseColorMap", "s_normalMap", "s_metallicRoughnessMap", "s_occlusionMap", "s_emissiveMap" };
			for (uint32_t ii = 0; ii < MaterialTexture::Count; ++ii)
			{
				_gbuffer->samplers[ii] = bgfx::createUniform(samplers[ii], bgfx::UniformType::Sampler);
			}
			_gbuffer->u_materialBaseColor = bgfx::createUniform("u_materialBaseColor", bgfx::UniformType::Vec4);
			_gbuffer->u_materialParameters = bgfx::createUniform("u_materialParameters", bgfx::UniformType::Vec4);
			_gbuffer->u_materialEmissive = bgfx::createUniform("u_materialEmissive", bgfx::UniformType::Vec4);
			_gbuffer->u_materialFlags = bgfx::createUniform("u_materialFlags", bgfx::UniformType::Vec4);
			bgfx::setPaletteColor(0, UINT32_C(0x00000000));
			bgfx::setPaletteColor(1, UINT32_C(0xffffffff));
		}

		static void destroyTargets(GBufferResource& _gbuffer)
		{
			if (bgfx::isValid(_gbuffer.frameBuffer))
			{
				bgfx::destroy(_gbuffer.frameBuffer);
				_gbuffer.frameBuffer = BGFX_INVALID_HANDLE;
			}
			for (bgfx::TextureHandle& texture : _gbuffer.textures)
			{
				if (bgfx::isValid(texture)) { bgfx::destroy(texture); }
				texture = BGFX_INVALID_HANDLE;
			}
			_gbuffer.width = _gbuffer.height = 0;
		}

		static void resize(GBufferResource& _gbuffer, uint16_t _width, uint16_t _height)
		{
			destroyTargets(_gbuffer);
			const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
				| BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

			const bgfx::TextureFormat::Enum depthFormat =
				bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D32F, flags)
				? bgfx::TextureFormat::D32F : bgfx::TextureFormat::D24;
			FROST_ASSERT(bgfx::getCaps()->limits.maxFBAttachments >= 4
				&& bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::BGRA8, flags)
				&& bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA16F, flags)
				&& bgfx::isTextureValid(0, false, 1, depthFormat, flags), "Required G-buffer formats are unsupported.");

			_gbuffer.textures[0] = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::BGRA8, flags);
			_gbuffer.textures[1] = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::BGRA8, flags);
			_gbuffer.textures[2] = bgfx::createTexture2D(_width, _height, false, 1, depthFormat, flags);

			_gbuffer.textures[3] = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::BGRA8, flags);
			_gbuffer.textures[4] = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::RGBA16F, flags);

			// Textures are owned here; framebuffers retain their attachments.
			_gbuffer.frameBuffer = bgfx::createFrameBuffer(5, _gbuffer.textures, false);
			FROST_ASSERT(bgfx::isValid(_gbuffer.frameBuffer), "Failed to create the G-buffer.");

			_gbuffer.width = _width;
			_gbuffer.height = _height;
		}

		static void bindMaterial(const MaterialDesc& _material, const GBufferResource& _gbuffer)
		{
			for (uint8_t ii = 0; ii < MaterialTexture::Count; ++ii)
			{
				const TextureRef* texture = getTextureRef(_material.textures[ii]);
				FROST_ASSERT(!isValid(_material.textures[ii]) || NULL != texture, "Material must reference live textures.");
				const bgfx::TextureHandle fallback = MaterialTexture::Normal == ii ? s_textureContext->normal : s_textureContext->white;
				const uint32_t flags = _material.samplers[ii];
				uint32_t sampler = 0;
				if (0 != (flags & TextureSampler::ClampU)) { sampler |= BGFX_SAMPLER_U_CLAMP; }
				if (0 != (flags & TextureSampler::ClampV)) { sampler |= BGFX_SAMPLER_V_CLAMP; }
				if (0 != (flags & TextureSampler::MirrorU)) { sampler |= BGFX_SAMPLER_U_MIRROR; }
				if (0 != (flags & TextureSampler::MirrorV)) { sampler |= BGFX_SAMPLER_V_MIRROR; }
				if (0 != (flags & TextureSampler::Point)) { sampler |= BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT; }
				if (0 != (flags & TextureSampler::Anisotropic)) { sampler |= BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC; }
				bgfx::setTexture(ii, _gbuffer.samplers[ii], NULL != texture ? texture->texture : fallback, sampler);
			}
			const float parameters[] = { _material.metallic, _material.roughness, _material.normalScale, _material.occlusionStrength };
			const float emissive[] = { _material.emissive.x * _material.emissiveStrength,
				_material.emissive.y * _material.emissiveStrength, _material.emissive.z * _material.emissiveStrength, _material.alphaCutoff };
			const float flags[] = { 0 != (_material.flags & MaterialFlags::Masked) ? 1.0f : 0.0f,
				0 != (_material.flags & MaterialFlags::DoubleSided) ? 1.0f : 0.0f, 0.0f, 0.0f };
			bgfx::setUniform(_gbuffer.u_materialBaseColor, _material.baseColor);
			bgfx::setUniform(_gbuffer.u_materialParameters, parameters);
			bgfx::setUniform(_gbuffer.u_materialEmissive, emissive);
			bgfx::setUniform(_gbuffer.u_materialFlags, flags);
		}

		static void update(ResT<RenderFrame> _frame,
			QueryT<const MeshComponent&, OptionalT<const TransformComponent>> _meshes, ResMutT<GBufferResource> _gbuffer)
		{
			if (!_frame->active)
			{
				return;
			}
			if (_gbuffer->width != _frame->width || _gbuffer->height != _frame->height)
			{
				resize(*_gbuffer, _frame->width, _frame->height);
			}

			bgfx::setViewRect(kRenderPassGeometry, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassGeometry, _gbuffer->frameBuffer);
			bgfx::setViewClear(kRenderPassGeometry, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 1.0f, 0, 0, 0, 0, 0);
			bgfx::setViewTransform(kRenderPassGeometry, _frame->view, _frame->projection);
			bgfx::touch(kRenderPassGeometry);

			_meshes.each([&](const MeshComponent& _mesh, const TransformComponent* _transform)
			{
				const MeshRef* mesh = getMeshRef(_mesh.handle);

				FROST_ASSERT(NULL != mesh, "MeshComponent must reference a live mesh.");

				float mtx[16];
				mtxIdentity(mtx);

				if (NULL != _transform)
				{
					const Vec3& scale = _transform->scale;
					if (0.0f == scale.x || 0.0f == scale.y || 0.0f == scale.z)
					{
						// Dont render anything that has zero scale
						return;
					}

					const Quaternion rotation = normalize(_transform->rotation);
					store(&mtx[0], mul(toXAxis(rotation), scale.x));
					store(&mtx[4], mul(toYAxis(rotation), scale.y));
					store(&mtx[8], mul(toZAxis(rotation), scale.z));
					store(&mtx[12], _transform->position);
				}

				for (uint32_t ii = 0; ii < mesh->numGroups; ++ii)
				{
					const MeshGroup& group = mesh->groups[ii];
					MaterialHandle materialHandle = mesh->materials[group.materialSlot].material;
					if (group.materialSlot < _mesh.numMaterials && NULL != _mesh.materials && isValid(_mesh.materials[group.materialSlot]))
					{
						materialHandle = _mesh.materials[group.materialSlot];
					}
					const MaterialRef* material = getMaterialRef(materialHandle);
					FROST_ASSERT(!isValid(materialHandle) || NULL != material, "Mesh must reference live materials.");
					const MaterialDesc fallback;
					const MaterialDesc& desc = NULL != material ? material->desc : fallback;
					uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
					const Vec3 scale = NULL != _transform ? _transform->scale : Vec3{ 1.0f, 1.0f, 1.0f };
					if (scale.x * scale.y * scale.z < 0.0f) { state |= BGFX_STATE_FRONT_CCW; }
					if (0 == (desc.flags & MaterialFlags::DoubleSided)) { state |= BGFX_STATE_CULL_CCW; }
					for (uint16_t jj = 0; jj < group.numPrimitives; ++jj)
					{
						const MeshPrimitive& primitive = group.primitives[jj];
						bgfx::setTransform(mtx);
						bgfx::setVertexBuffer(0, group.vertexBuffer);
						bgfx::setIndexBuffer(group.indexBuffer, primitive.startIndex, primitive.numIndices);
						bindMaterial(desc, *_gbuffer);
						bgfx::setState(state);
						bgfx::submit(kRenderPassGeometry, _gbuffer->program);
					}
				}
			});
		}

		static void shutdown(ResMutT<GBufferResource> _gbuffer)
		{
			destroyTargets(*_gbuffer);
			bgfx::destroy(_gbuffer->program);
			for (bgfx::UniformHandle sampler : _gbuffer->samplers) { bgfx::destroy(sampler); }
			bgfx::destroy(_gbuffer->u_materialBaseColor);
			bgfx::destroy(_gbuffer->u_materialParameters);
			bgfx::destroy(_gbuffer->u_materialEmissive);
			bgfx::destroy(_gbuffer->u_materialFlags);
		}
	};

} // namespace frost

// Light pass
namespace frost
{
	struct LightResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_normal = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_albedo = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sunDirection = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sunLuminance = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout layout;
		bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle depth = BGFX_INVALID_HANDLE; // Borrowed from the G-buffer.
		uint32_t width = 0;
		uint32_t height = 0;
		bgfx::UniformHandle s_material = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_emissive = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_invSceneViewProj = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sceneCamera = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sceneDirection = BGFX_INVALID_HANDLE;
	};

	struct LightSystem
	{
		static void startup(ResMutT<LightResource> _light)
		{
			bgfx::setViewName(kRenderPassLight, "Directional Light");
			bgfx::setViewMode(kRenderPassLight, bgfx::ViewMode::Sequential);

			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_light->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_light"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_deferred_light"), true);

			FROST_ASSERT(bgfx::isValid(_light->program), "Failed to create the lighting program.");

			_light->s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
			_light->s_albedo = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
			_light->s_material = bgfx::createUniform("s_material", bgfx::UniformType::Sampler);
			_light->s_emissive = bgfx::createUniform("s_emissive", bgfx::UniformType::Sampler);
			_light->s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
			_light->u_invSceneViewProj = bgfx::createUniform("u_invSceneViewProj", bgfx::UniformType::Mat4);
			_light->u_sceneCamera = bgfx::createUniform("u_sceneCamera", bgfx::UniformType::Vec4);
			_light->u_sceneDirection = bgfx::createUniform("u_sceneDirection", bgfx::UniformType::Vec4);
			_light->u_sunDirection = bgfx::createUniform("u_sunDirection", bgfx::UniformType::Vec4);
			_light->u_sunLuminance = bgfx::createUniform("u_sunLuminance", bgfx::UniformType::Vec4);

			_light->layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
		}

		static void destroyTargets(LightResource& _light)
		{
			if (bgfx::isValid(_light.frameBuffer))
			{
				bgfx::destroy(_light.frameBuffer);
				_light.frameBuffer = BGFX_INVALID_HANDLE;
			}
			if (bgfx::isValid(_light.texture)) { bgfx::destroy(_light.texture); }
			_light.texture = BGFX_INVALID_HANDLE;
			_light.depth = BGFX_INVALID_HANDLE;
			_light.width = _light.height = 0;
		}

		static void resize(LightResource& _light, const GBufferResource& _gbuffer)
		{
			destroyTargets(_light);
			const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
				| BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
			FROST_ASSERT(bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA16F, flags),
				"RGBA16F scene rendering is unsupported.");

			_light.texture = bgfx::createTexture2D(uint16_t(_gbuffer.width), uint16_t(_gbuffer.height),
				false, 1, bgfx::TextureFormat::RGBA16F, flags);

			bgfx::Attachment attachments[2];
			attachments[0].init(_light.texture);
			attachments[1].init(_gbuffer.textures[2], bgfx::Access::Read, 0, 1, 0, BGFX_ATTACHMENT_READ_ONLY_DEPTH);

			// Own scene color, retain shared depth through the framebuffer only.
			_light.frameBuffer = bgfx::createFrameBuffer(2, attachments, false);

			FROST_ASSERT(bgfx::isValid(_light.frameBuffer), "Failed to create the lighting framebuffer.");
			_light.width = _gbuffer.width;
			_light.height = _gbuffer.height;
			_light.depth = _gbuffer.textures[2];
		}

		static void bindScene(const RenderFrame& _frame, const GBufferResource& _gbuffer, const LightResource& _light)
		{
			bgfx::setTexture(0, _light.s_normal, _gbuffer.textures[1]);
			bgfx::setTexture(1, _light.s_albedo, _gbuffer.textures[0]);
			bgfx::setTexture(2, _light.s_material, _gbuffer.textures[3]);
			bgfx::setTexture(3, _light.s_emissive, _gbuffer.textures[4]);
			bgfx::setTexture(4, _light.s_depth, _gbuffer.textures[2]);
			bgfx::setUniform(_light.u_invSceneViewProj, _frame.inverseViewProjection);
			bgfx::setUniform(_light.u_sceneCamera, _frame.cameraPosition);
			bgfx::setUniform(_light.u_sceneDirection, _frame.cameraDirection);
		}

		static void update(ResT<RenderFrame> _frame, ResT<GBufferResource> _gbuffer,
			QueryT<const DirectionalLightComponent&> _lights, ResMutT<LightResource> _light)
		{
			if (!_frame->active)
			{
				return;
			}
			if (_light->width != _frame->width || _light->height != _frame->height
				|| _light->depth.idx != _gbuffer->textures[2].idx)
			{
				resize(*_light, *_gbuffer);
			}

			bgfx::setViewRect(kRenderPassLight, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassLight, _light->frameBuffer);
			bgfx::setViewClear(kRenderPassLight, BGFX_CLEAR_COLOR, UINT32_C(0x00000000));
			bgfx::setViewTransform(kRenderPassLight, NULL, _frame->screenProjection);
			bgfx::touch(kRenderPassLight);

			// Seed scene color with emission once, including frames without a light.
			const float emission[4] = {};
			bgfx::setUniform(_light->u_sunLuminance, emission);
			bgfx::setUniform(_light->u_sunDirection, emission);
			bindScene(*_frame, *_gbuffer, *_light);
			screenSpaceTriangle(_light->layout);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			bgfx::submit(kRenderPassLight, _light->program);

			_lights.each([&](const DirectionalLightComponent& _component)
			{
				if (0.0f == _component.intensity)
				{
					return;
				}

				const float lengthSq = dot(_component.direction, _component.direction);

				FROST_ASSERT(isFinite(lengthSq) && 0.0f < lengthSq
					&& isFinite(_component.intensity) && 0.0f < _component.intensity,
					"Directional lights require a nonzero finite direction and nonnegative finite intensity.");

				float direction[4] = {};
				store(direction, mul(_component.direction, rsqrt(lengthSq)));

				const Vec3 color = {
					max(0.0f, _component.color.x), max(0.0f, _component.color.y), max(0.0f, _component.color.z)
				};
				float luminance[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				store(luminance, mul(color, _component.intensity));

				screenSpaceTriangle(_light->layout);

				bgfx::setUniform(_light->u_sunDirection, direction);
				bgfx::setUniform(_light->u_sunLuminance, luminance);
				bindScene(*_frame, *_gbuffer, *_light);
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ADD);
				bgfx::submit(kRenderPassLight, _light->program);
			});
		}

		static void shutdown(ResMutT<LightResource> _light)
		{
			destroyTargets(*_light);
			bgfx::destroy(_light->program);
			bgfx::destroy(_light->s_normal);
			bgfx::destroy(_light->s_albedo);
			bgfx::destroy(_light->s_material);
			bgfx::destroy(_light->s_emissive);
			bgfx::destroy(_light->s_depth);
			bgfx::destroy(_light->u_invSceneViewProj);
			bgfx::destroy(_light->u_sceneCamera);
			bgfx::destroy(_light->u_sceneDirection);
			bgfx::destroy(_light->u_sunDirection);
			bgfx::destroy(_light->u_sunLuminance);
		}
	};

} // namespace frost

// Sky
namespace frost
{
	// Perez coefficients, luminance tables, solar orbit, and grid adapted from
	// bgfx/examples/36-sky. Copyright 2017 Stanislav Pidhorskyi.
	// License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
	struct SkyLuminance
	{
		float hour;
		Vec3 xyz;
	};

	static const SkyLuminance s_sunLuminance[] =
	{
		{  5.0f, {  0.000000f,  0.000000f,  0.000000f } },
		{  7.0f, { 12.703322f, 12.989393f,  9.100411f } },
		{  8.0f, { 13.202644f, 13.597814f, 11.524929f } },
		{  9.0f, { 13.192974f, 13.597458f, 12.264488f } },
		{ 10.0f, { 13.132943f, 13.535914f, 12.560032f } },
		{ 11.0f, { 13.088722f, 13.489535f, 12.692996f } },
		{ 12.0f, { 13.067827f, 13.467483f, 12.745179f } },
		{ 13.0f, { 13.069653f, 13.469413f, 12.740822f } },
		{ 14.0f, { 13.094319f, 13.495428f, 12.678066f } },
		{ 15.0f, { 13.142133f, 13.545483f, 12.526785f } },
		{ 16.0f, { 13.201734f, 13.606017f, 12.188001f } },
		{ 17.0f, { 13.182774f, 13.572725f, 11.311157f } },
		{ 18.0f, { 12.448635f, 12.672520f,  8.267771f } },
		{ 20.0f, {  0.000000f,  0.000000f,  0.000000f } },
	};

	static const SkyLuminance s_skyLuminance[] =
	{
		{  0.0f, { 0.308f,    0.308f,    0.411f    } },
		{  1.0f, { 0.308f,    0.308f,    0.410f    } },
		{  2.0f, { 0.301f,    0.301f,    0.402f    } },
		{  3.0f, { 0.287f,    0.287f,    0.382f    } },
		{  4.0f, { 0.258f,    0.258f,    0.344f    } },
		{  5.0f, { 0.258f,    0.258f,    0.344f    } },
		{  7.0f, { 0.962851f, 1.000000f, 1.747835f } },
		{  8.0f, { 0.967787f, 1.000000f, 1.776762f } },
		{  9.0f, { 0.970173f, 1.000000f, 1.788413f } },
		{ 10.0f, { 0.971431f, 1.000000f, 1.794102f } },
		{ 11.0f, { 0.972099f, 1.000000f, 1.797096f } },
		{ 12.0f, { 0.972385f, 1.000000f, 1.798389f } },
		{ 13.0f, { 0.972361f, 1.000000f, 1.798278f } },
		{ 14.0f, { 0.972020f, 1.000000f, 1.796740f } },
		{ 15.0f, { 0.971275f, 1.000000f, 1.793407f } },
		{ 16.0f, { 0.969885f, 1.000000f, 1.787078f } },
		{ 17.0f, { 0.967216f, 1.000000f, 1.773758f } },
		{ 18.0f, { 0.961668f, 1.000000f, 1.739891f } },
		{ 20.0f, { 0.264f,    0.264f,    0.352f    } },
		{ 21.0f, { 0.264f,    0.264f,    0.352f    } },
		{ 22.0f, { 0.290f,    0.290f,    0.386f    } },
		{ 23.0f, { 0.303f,    0.303f,    0.404f    } },
		{ 24.0f, { 0.308f, 0.308f, 0.411f } },
	};

	static constexpr Vec3 s_perezBase[] =
	{
		{ -0.2592f, -0.2608f, -1.4630f },
		{  0.0008f,  0.0092f,  0.4275f },
		{  0.2125f,  0.2102f,  5.3251f },
		{ -0.8989f, -1.6537f, -2.5771f },
		{  0.0452f,  0.0529f,  0.3703f },
	};

	static constexpr Vec3 s_perezTurbidity[] =
	{
		{ -0.0193f, -0.0167f,  0.1787f },
		{ -0.0665f, -0.0950f, -0.3554f },
		{ -0.0004f, -0.0079f, -0.0227f },
		{ -0.0641f, -0.0441f,  0.1206f },
		{ -0.0033f, -0.0109f, -0.0670f },
	};

	template<uint32_t Count>
	static Vec3 sampleSkyLuminance(const SkyLuminance (&_table)[Count], float _hour)
	{
		if (_hour <= _table[0].hour)
		{
			return _table[0].xyz;
		}
		for (uint32_t ii = 1; ii < Count; ++ii)
		{
			if (_hour <= _table[ii].hour)
			{
				const SkyLuminance& a = _table[ii - 1];
				const SkyLuminance& b = _table[ii];
				return lerp(a.xyz, b.xyz, (_hour - a.hour) / (b.hour - a.hour));
			}
		}
		return _table[Count - 1].xyz;
	}

	struct SkyResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::VertexBufferHandle vertices = BGFX_INVALID_HANDLE;
		bgfx::IndexBufferHandle indices = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sunDirection = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_skyLuminanceXYZ = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sunTint = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_parameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_perezCoeff = BGFX_INVALID_HANDLE;
	};

	struct SkySystem
	{
		static void prepare(ResT<TimeResource> _time, QueryT<SkyComponent&> _skies)
		{
			const float deltaTime = float(min(_time->delta, 0.1));
			_skies.each([&](SkyComponent& _sky)
			{
				if (0.0f != _sky.hps)
				{
					_sky.time = wrap(_sky.time + deltaTime * _sky.hps, 24.0f);
				}

				const float hour = isFinite(_sky.time) ? wrap(_sky.time, 24.0f) : 12.0f;
				const float latitude = toRad(clamp(_sky.latitude, -90.0f, 90.0f));
				const float day = 30.0f * float(clamp(int32_t(_sky.month), 0, 11)) + 15.0f;
				const float lambda = toRad(280.46f + 0.9856474f * day);
				const float delta = asin(sin(toRad(23.4f)) * sin(lambda));
				const float hh = (hour - 12.0f) * kPi / 12.0f;
				const float azimuth = atan2(sin(hh), cos(hh) * sin(latitude) - tan(delta) * cos(latitude));
				const float altitude = asin(clamp(sin(latitude) * sin(delta)
					+ cos(latitude) * cos(delta) * cos(hh), -1.0f, 1.0f));

				// +X is north and +Y is up
				_sky.sunDirection = { cos(azimuth) * cos(altitude), sin(altitude), sin(azimuth) * cos(altitude) };

				const Vec3 xyz = sampleSkyLuminance(s_sunLuminance, hour);
				const Vec3 rgb = {
					max(0.0f,  3.240479f * xyz.x - 1.537150f * xyz.y - 0.498530f * xyz.z),
					max(0.0f, -0.969256f * xyz.x + 1.875991f * xyz.y + 0.041556f * xyz.z),
					max(0.0f,  0.055648f * xyz.x - 0.204043f * xyz.y + 1.057311f * xyz.z),
				};

				const Vec3 tint = {
					max(0.0f, _sky.sunTint.x), max(0.0f, _sky.sunTint.y), max(0.0f, _sky.sunTint.z)
				};

				_sky.sunColor = mul(mul(rgb, tint), max(0.0f, _sky.sunIntensity));
			});
		}

		static void startup(ResMutT<SkyResource> _sky)
		{
			bgfx::setViewName(kRenderPassSky, "Sky");

			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_sky->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_sky"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_sky"), true);

			_sky->u_sunDirection = bgfx::createUniform("u_sunDirection", bgfx::UniformType::Vec4);
			_sky->u_skyLuminanceXYZ = bgfx::createUniform("u_skyLuminanceXYZ", bgfx::UniformType::Vec4);
			_sky->u_sunTint = bgfx::createUniform("u_sunTint", bgfx::UniformType::Vec4);
			_sky->u_parameters = bgfx::createUniform("u_parameters", bgfx::UniformType::Vec4);
			_sky->u_perezCoeff = bgfx::createUniform("u_perezCoeff", bgfx::UniformType::Vec4, 5);

			struct Vertex { float x, y; };
			Vertex vertices[32 * 32];
			uint16_t indices[31 * 31 * 6];
			for (uint32_t yy = 0; yy < 32; ++yy)
			{
				for (uint32_t xx = 0; xx < 32; ++xx)
				{
					vertices[yy * 32 + xx] = { float(xx) / 31.0f * 2.0f - 1.0f, float(yy) / 31.0f * 2.0f - 1.0f };
				}
			}

			uint32_t index = 0;
			for (uint32_t yy = 0; yy < 31; ++yy)
			{
				for (uint32_t xx = 0; xx < 31; ++xx)
				{
					const uint16_t base = uint16_t(yy * 32 + xx);
					indices[index++] = base;
					indices[index++] = base + 1;
					indices[index++] = base + 32;
					indices[index++] = base + 1;
					indices[index++] = base + 33;
					indices[index++] = base + 32;
				}
			}

			bgfx::VertexLayout layout;
			layout.begin().add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float).end();
			_sky->vertices = bgfx::createVertexBuffer(bgfx::copy(vertices, sizeof(vertices)), layout);
			_sky->indices = bgfx::createIndexBuffer(bgfx::copy(indices, sizeof(indices)));

			FROST_ASSERT(bgfx::isValid(_sky->program) && bgfx::isValid(_sky->vertices) && bgfx::isValid(_sky->indices),
				"Failed to create sky rendering resources.");
		}

		static void update(ResT<RenderFrame> _frame, ResT<LightResource> _light,
			QueryT<const SkyComponent&> _skies, ResT<SkyResource> _sky)
		{
			if (!_frame->active)
			{
				return;
			}

			bgfx::setViewRect(kRenderPassSky, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassSky, _light->frameBuffer);
			bgfx::setViewClear(kRenderPassSky, BGFX_CLEAR_NONE);
			bgfx::setViewTransform(kRenderPassSky, _frame->view, _frame->projection);

			_skies.single([&](const SkyComponent& _component)
			{
				const float hour = isFinite(_component.time) ? wrap(_component.time, 24.0f) : 12.0f;

				float direction[4] = {};
				store(direction, _component.sunDirection);

				float luminance[4] = {};
				store(luminance, sampleSkyLuminance(s_skyLuminance, hour));

				const Vec3 tint = {
					max(0.0f, _component.sunTint.x), max(0.0f, _component.sunTint.y), max(0.0f, _component.sunTint.z)
				};
				float sunTint[4] = {};
				store(sunTint, mul(tint, max(0.0f, _component.sunIntensity)));

				const float parameters[4] = {
					max(0.0001f, _component.sunSize), max(0.0001f, _component.sunBloom),
					0.0f, hour
				};

				float perez[20] = {};
				const float turbidity = clamp(_component.turbidity, 1.9f, 10.0f);

				for (uint32_t ii = 0; ii < 5; ++ii)
				{
					store(&perez[ii * 4], mad(s_perezTurbidity[ii], turbidity, s_perezBase[ii]));
				}

				bgfx::setUniform(_sky->u_sunDirection, direction);
				bgfx::setUniform(_sky->u_skyLuminanceXYZ, luminance);
				bgfx::setUniform(_sky->u_sunTint, sunTint);
				bgfx::setUniform(_sky->u_parameters, parameters);
				bgfx::setUniform(_sky->u_perezCoeff, perez, 5);

				bgfx::setVertexBuffer(0, _sky->vertices);
				bgfx::setIndexBuffer(_sky->indices);

				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_EQUAL);

				bgfx::submit(kRenderPassSky, _sky->program);
			});
		}

		static void shutdown(ResMutT<SkyResource> _sky)
		{
			bgfx::destroy(_sky->vertices);
			bgfx::destroy(_sky->indices);
			bgfx::destroy(_sky->program);
			bgfx::destroy(_sky->u_sunDirection);
			bgfx::destroy(_sky->u_skyLuminanceXYZ);
			bgfx::destroy(_sky->u_sunTint);
			bgfx::destroy(_sky->u_parameters);
			bgfx::destroy(_sky->u_perezCoeff);
		}
	};

} // namespace frost

// Combine (tone mapping and debug views)
namespace frost
{
	struct ToneMapResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_scene = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_albedo = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_normal = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_outputParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sceneView = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_invSceneViewProj = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout layout;
		bgfx::UniformHandle s_material = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_emissive = BGFX_INVALID_HANDLE;
	};

	static void submitOutput(bgfx::ViewId _view, uint32_t _mode, const RenderFrame& _frame,
		const GraphicsResource& _graphics, const GBufferResource& _gbuffer,
		const LightResource& _light, const ToneMapResource& _output)
	{
		const float parameters[4] = { _frame.exposure, _graphics.preventBanding ? 1.0f : 0.0f,
			float(_mode), 1.0f / max(0.001f, _graphics.debugDepth) };
		bgfx::setViewTransform(_view, NULL, _frame.screenProjection);
		bgfx::setUniform(_output.u_outputParameters, parameters);
		bgfx::setUniform(_output.u_sceneView, _frame.view);
		bgfx::setUniform(_output.u_invSceneViewProj, _frame.inverseViewProjection);
		bgfx::setTexture(0, _output.s_scene, _light.texture);
		bgfx::setTexture(1, _output.s_albedo, _gbuffer.textures[0]);
		bgfx::setTexture(2, _output.s_normal, _gbuffer.textures[1]);
		bgfx::setTexture(3, _output.s_depth, _gbuffer.textures[2]);
		bgfx::setTexture(4, _output.s_material, _gbuffer.textures[3]);
		bgfx::setTexture(5, _output.s_emissive, _gbuffer.textures[4]);
		screenSpaceTriangle(_output.layout);
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		bgfx::submit(_view, _output.program);
	}

	struct ToneMapSystem
	{
		static void startup(ResMutT<ToneMapResource> _output)
		{
			bgfx::setViewName(kRenderPassToneMap, "Tone Mapping");
			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_output->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_combine"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_deferred_combine"), true);
			FROST_ASSERT(bgfx::isValid(_output->program), "Failed to create the output program.");
			_output->s_scene = bgfx::createUniform("s_scene", bgfx::UniformType::Sampler);
			_output->s_albedo = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
			_output->s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
			_output->s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
			_output->s_material = bgfx::createUniform("s_material", bgfx::UniformType::Sampler);
			_output->s_emissive = bgfx::createUniform("s_emissive", bgfx::UniformType::Sampler);
			_output->u_outputParameters = bgfx::createUniform("u_outputParameters", bgfx::UniformType::Vec4);
			_output->u_sceneView = bgfx::createUniform("u_sceneView", bgfx::UniformType::Mat4);
			_output->u_invSceneViewProj = bgfx::createUniform("u_invSceneViewProj", bgfx::UniformType::Mat4);
			_output->layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
		}

		static void update(ResT<RenderFrame> _frame, ResT<GraphicsResource> _graphics,
			ResT<GBufferResource> _gbuffer, ResT<LightResource> _light, ResT<ToneMapResource> _output)
		{
			if (0 == _frame->width || 0 == _frame->height) { return; }
			// Presentation and UI still work without a selected camera.
			bgfx::setViewRect(kRenderPassToneMap, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassToneMap, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kRenderPassToneMap, BGFX_CLEAR_COLOR, UINT32_C(0x000000ff));
			bgfx::touch(kRenderPassToneMap);
			if (_frame->active)
			{
				submitOutput(kRenderPassToneMap, GraphicsDebugView::None, *_frame, *_graphics, *_gbuffer, *_light, *_output);
			}
		}

		static void shutdown(ResMutT<ToneMapResource> _output)
		{
			bgfx::destroy(_output->program);
			bgfx::destroy(_output->s_scene);
			bgfx::destroy(_output->s_albedo);
			bgfx::destroy(_output->s_normal);
			bgfx::destroy(_output->s_depth);
			bgfx::destroy(_output->s_material);
			bgfx::destroy(_output->s_emissive);
			bgfx::destroy(_output->u_outputParameters);
			bgfx::destroy(_output->u_sceneView);
			bgfx::destroy(_output->u_invSceneViewProj);
		}
	};

	struct DebugViewSystem
	{
		static void startup()
		{
			bgfx::setViewName(kRenderPassDebug, "Debug View");
		}

		static void update(ResT<RenderFrame> _frame, ResT<GraphicsResource> _graphics,
			ResT<GBufferResource> _gbuffer, ResT<LightResource> _light, ResT<ToneMapResource> _output)
		{
			if (!_frame->active || _graphics->debugView <= GraphicsDebugView::None
				|| _graphics->debugView >= GraphicsDebugView::Count) { return; }
			bgfx::setViewRect(kRenderPassDebug, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassDebug, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kRenderPassDebug, BGFX_CLEAR_NONE);
			submitOutput(kRenderPassDebug, _graphics->debugView, *_frame, *_graphics, *_gbuffer, *_light, *_output);
		}
	};

	struct GraphicsStatsSystem
	{
		static void update(ResT<GraphicsResource> _graphics, ResMutT<GraphicsStatsResource> _stats)
		{
			const bgfx::Stats* stats = bgfx::getStats();
			double* times[] = { &_stats->geometryMs, &_stats->lightMs, &_stats->skyMs,
				&_stats->toneMapMs, &_stats->debugViewMs, &_stats->uiMs };
			for (double* time : times) { *time = -1.0; }
			_stats->gpuMs = -1.0;
			_stats->draws = stats->numDraw;
			_stats->textures = stats->numTextures;
			_stats->renderTargetBytes = stats->rtMemoryUsed;
			if (!_graphics->gpuProfiling || stats->gpuTimerFreq <= 0) { return; }
			const double milliseconds = 1000.0 / double(stats->gpuTimerFreq);
			if (stats->gpuTimeEnd > stats->gpuTimeBegin)
			{
				_stats->gpuMs = double(stats->gpuTimeEnd - stats->gpuTimeBegin) * milliseconds;
			}
			for (uint16_t ii = 0; ii < stats->numViews; ++ii)
			{
				const bgfx::ViewStats& view = stats->viewStats[ii];
				if (view.view < FROST_COUNTOF(times) && view.gpuTimeEnd > view.gpuTimeBegin)
				{
					*times[view.view] = double(view.gpuTimeEnd - view.gpuTimeBegin) * milliseconds;
				}
			}
		}
	};

} // namespace frost

// UI pass
namespace frost
{
	struct UiBox { float x, y, width, height; };
	struct UiVertex { float x, y, z, u, v; uint32_t color; };
	struct UiCanvasEntry { Entity entity; int32_t layer; };
	struct UiTreeEntry { Entity parent, entity; int32_t order; };

	struct UiResource
	{
		UiResource() = default;
		UiResource(UiResource&& _other)
			: program(_other.program), atlas(_other.atlas), sampler(_other.sampler), layout(_other.layout)
			, nodeCount(_other.nodeCount), vertexCount(_other.vertexCount), clip(_other.clip), scale(_other.scale)
		{
			canvases = frost::move(_other.canvases);
			nodes = frost::move(_other.nodes);
			memCopy(vertices, _other.vertices, vertexCount * sizeof(UiVertex));
		}

		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle atlas = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle sampler = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout layout;
		DynamicStorageT<UiCanvasEntry> canvases;
		DynamicStorageT<UiTreeEntry> nodes;
		uint32_t nodeCount = 0;
		UiVertex vertices[512 * 6];
		uint32_t vertexCount = 0;
		UiBox clip = {};
		float scale = 1.0f;
	};

	struct UiSystem
	{
		static void startup(World& _world)
		{
			UiResource& ui = _world.initResource<UiResource>();
			ui.canvases.setAllocator(_world.getAllocator());
			ui.nodes.setAllocator(_world.getAllocator());
			ui.layout.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true).end();
			static const bgfx::EmbeddedShader shaders[] =
			{
				BGFX_EMBEDDED_SHADER(vs_ui), BGFX_EMBEDDED_SHADER(fs_ui), BGFX_EMBEDDED_SHADER_END()
			};
			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			ui.program = bgfx::createProgram(
				bgfx::createEmbeddedShader(shaders, type, "vs_ui"),
				bgfx::createEmbeddedShader(shaders, type, "fs_ui"), true);
			ui.sampler = bgfx::createUniform("s_ui", bgfx::UniformType::Sampler);
			const bgfx::Memory* pixels = bgfx::alloc(128 * 129);
			memSet(pixels->data, 0, pixels->size);
			for (uint32_t ch = 0; ch < 256; ++ch)
			{
				for (uint32_t y = 0; y < 8; ++y)
				{
					for (uint32_t x = 0; x < 8; ++x)
					{
						pixels->data[((ch / 16) * 8 + y) * 128 + (ch % 16) * 8 + x]
							= (vga8x8[ch * 8 + y] & (0x80 >> x)) ? 255 : 0;
					}
				}
			}
			pixels->data[128 * 128] = 255; // White texel for solid rectangles.
			ui.atlas = bgfx::createTexture2D(128, 129, false, 1, bgfx::TextureFormat::R8,
				BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT
				| BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, pixels);
			FROST_ASSERT(bgfx::isValid(ui.program) && bgfx::isValid(ui.atlas) && bgfx::isValid(ui.sampler),
				"Failed to create UI resources.");
			bgfx::setViewName(kRenderPassUi, "UI");
			bgfx::setViewMode(kRenderPassUi, bgfx::ViewMode::Sequential);
		}

		static UiBox intersect(UiBox _a, UiBox _b)
		{
			const float x = max(_a.x, _b.x), y = max(_a.y, _b.y);
			return { x, y, max(0.0f, min(_a.x + _a.width, _b.x + _b.width) - x),
				max(0.0f, min(_a.y + _a.height, _b.y + _b.height) - y) };
		}

		static void flush(UiResource& _ui)
		{
			if (0 == _ui.vertexCount)
			{
				return;
			}
			FROST_ASSERT(_ui.vertexCount == bgfx::getAvailTransientVertexBuffer(_ui.vertexCount, _ui.layout),
				"UI transient vertex buffer exhausted.");
			bgfx::TransientVertexBuffer buffer;
			bgfx::allocTransientVertexBuffer(&buffer, _ui.vertexCount, _ui.layout);
			memCopy(buffer.data, _ui.vertices, _ui.vertexCount * sizeof(UiVertex));
			bgfx::setVertexBuffer(0, &buffer);
			bgfx::setTexture(0, _ui.sampler, _ui.atlas);
			const uint16_t x = uint16_t(floor(_ui.clip.x * _ui.scale));
			const uint16_t y = uint16_t(floor(_ui.clip.y * _ui.scale));
			bgfx::setScissor(x, y,
				uint16_t(ceil((_ui.clip.x + _ui.clip.width) * _ui.scale) - x),
				uint16_t(ceil((_ui.clip.y + _ui.clip.height) * _ui.scale) - y));
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA);
			bgfx::submit(kRenderPassUi, _ui.program);
			_ui.vertexCount = 0;
		}

		static void setClip(UiResource& _ui, UiBox _clip)
		{
			if (_clip.x != _ui.clip.x || _clip.y != _ui.clip.y
				|| _clip.width != _ui.clip.width || _clip.height != _ui.clip.height)
			{
				flush(_ui);
				_ui.clip = _clip;
			}
		}

		static void quad(UiResource& _ui, UiBox _box, uint32_t _rgba,
			float _u0 = 0.5f / 128.0f, float _v0 = 128.5f / 129.0f,
			float _u1 = 0.5f / 128.0f, float _v1 = 128.5f / 129.0f)
		{
			const UiBox visible = intersect(_box, _ui.clip);
			if (0 == (_rgba & 0xff) || 0.0f >= visible.width || 0.0f >= visible.height)
			{
				return;
			}
			if (FROST_COUNTOF(_ui.vertices) == _ui.vertexCount)
			{
				flush(_ui);
			}
			const uint32_t abgr = ((_rgba & 0xff) << 24) | ((_rgba & 0xff00) << 8)
				| ((_rgba & 0xff0000) >> 8) | ((_rgba & 0xff000000) >> 24);
			const float x0 = _box.x * _ui.scale, y0 = _box.y * _ui.scale;
			const float x1 = x0 + _box.width * _ui.scale, y1 = y0 + _box.height * _ui.scale;
			UiVertex* v = &_ui.vertices[_ui.vertexCount];
			v[0] = { x0, y0, 0, _u0, _v0, abgr };
			v[1] = { x1, y0, 0, _u1, _v0, abgr };
			v[2] = { x1, y1, 0, _u1, _v1, abgr };
			v[3] = v[0]; v[4] = v[2];
			v[5] = { x0, y1, 0, _u0, _v1, abgr };
			_ui.vertexCount += 6;
		}

		static uint32_t childBegin(const UiResource& _ui, Entity _parent)
		{
			uint32_t begin = 0, end = _ui.nodeCount;
			while (begin < end)
			{
				const uint32_t mid = begin + (end - begin) / 2;
				if (_ui.nodes.getPtr(mid)->parent.idx < _parent.idx) { begin = mid + 1; }
				else { end = mid; }
			}
			return begin;
		}

		static UiBox measure(World& _world, Entity _entity, const UiResource& _ui)
		{
			const UiNodeComponent* node = _world.get<UiNodeComponent>(_entity);
			if (!node->visible)
			{
				return {};
			}
			float width = 0.0f, height = 0.0f;
			if (const UiTextComponent* text = _world.get<UiTextComponent>(_entity))
			{
				uint32_t column = 0, columns = 0, lines = 1;
				for (const char* ptr = text->text.getCPtr(); '\0' != *ptr; ++ptr)
				{
					const char ch = *ptr;
					if ('\n' == ch) { columns = max(columns, column); column = 0; ++lines; }
					else { ++column; }
				}
				width = float(max(column, columns)) * text->height;
				height = float(lines) * text->height;
			}
			if (const UiLayoutComponent* layout = _world.get<UiLayoutComponent>(_entity))
			{
				float main = 0.0f, cross = 0.0f;
				uint32_t count = 0;
				for (uint32_t ii = childBegin(_ui, _entity);
					ii < _ui.nodeCount && _ui.nodes.getPtr(ii)->parent == _entity; ++ii)
				{
					const Entity child = _ui.nodes.getPtr(ii)->entity;
					const UiBox size = measure(_world, child, _ui);
					main += (UiLayoutComponent::Row == layout->direction ? size.width : size.height);
					cross = max(cross, UiLayoutComponent::Row == layout->direction ? size.height : size.width);
					++count;
				}
				main += 0 == count ? 0.0f : float(count - 1) * layout->spacing;
				width = max(width, (UiLayoutComponent::Row == layout->direction ? main : cross) + 2 * layout->padding);
				height = max(height, (UiLayoutComponent::Row == layout->direction ? cross : main) + 2 * layout->padding);
			}
			return { 0, 0, node->width > 0 ? node->width : width, node->height > 0 ? node->height : height };
		}

		static void draw(World& _world, Entity _entity, UiBox _box, UiBox _clip, UiResource& _ui)
		{
			const UiNodeComponent* node = _world.get<UiNodeComponent>(_entity);
			if (NULL != node && !node->visible) { return; }
			if (NULL != node && node->clip) { _clip = intersect(_clip, _box); }
			if (_clip.width <= 0 || _clip.height <= 0) { return; }
			setClip(_ui, _clip);
			if (const UiRectComponent* rect = _world.get<UiRectComponent>(_entity))
			{
				quad(_ui, _box, rect->color);
				const float edge = min(rect->outlineWidth, min(_box.width, _box.height) * 0.5f);
				quad(_ui, { _box.x, _box.y, _box.width, edge }, rect->outlineColor);
				quad(_ui, { _box.x, _box.y + _box.height - edge, _box.width, edge }, rect->outlineColor);
				quad(_ui, { _box.x, _box.y, edge, _box.height }, rect->outlineColor);
				quad(_ui, { _box.x + _box.width - edge, _box.y, edge, _box.height }, rect->outlineColor);
			}
			if (const UiProgressBarComponent* bar = _world.get<UiProgressBarComponent>(_entity))
			{
				FROST_ASSERT(bar->maximum > bar->minimum, "UI progress range must be positive.");
				quad(_ui, _box, bar->backgroundColor);
				quad(_ui, { _box.x, _box.y, _box.width * clamp((bar->value - bar->minimum)
					/ (bar->maximum - bar->minimum), 0.0f, 1.0f), _box.height }, bar->fillColor);
			}
			if (const UiTextComponent* text = _world.get<UiTextComponent>(_entity))
			{
				float x = _box.x, y = _box.y;
				for (const char* ptr = text->text.getCPtr(); '\0' != *ptr; ++ptr)
				{
					const uint8_t ch = uint8_t(*ptr);
					if ('\n' == ch) { x = _box.x; y += text->height; continue; }
					if (' ' != ch)
					{
						const float u = float((ch % 16) * 8) / 128.0f, v = float((ch / 16) * 8) / 129.0f;
						quad(_ui, { x, y, text->height, text->height }, text->color,
							u, v, u + 8.0f / 128.0f, v + 8.0f / 129.0f);
					}
					x += text->height;
				}
			}
			const UiLayoutComponent* layout = _world.get<UiLayoutComponent>(_entity);
			float cursor = NULL != layout ? layout->padding : 0.0f;
			for (uint32_t ii = childBegin(_ui, _entity);
				ii < _ui.nodeCount && _ui.nodes.getPtr(ii)->parent == _entity; ++ii)
			{
				const Entity child = _ui.nodes.getPtr(ii)->entity;
				const UiNodeComponent* childNode = _world.get<UiNodeComponent>(child);
				UiBox box = measure(_world, child, _ui);
				if (NULL == layout)
				{
					box.x = _box.x + childNode->x + childNode->anchorX * _box.width - childNode->pivotX * box.width;
					box.y = _box.y + childNode->y + childNode->anchorY * _box.height - childNode->pivotY * box.height;
				}
				else
				{
					box.x = _box.x + childNode->x + (UiLayoutComponent::Row == layout->direction ? cursor : layout->padding);
					box.y = _box.y + childNode->y + (UiLayoutComponent::Row == layout->direction ? layout->padding : cursor);
					cursor += (UiLayoutComponent::Row == layout->direction ? box.width : box.height) + layout->spacing;
				}
				draw(_world, child, box, _clip, _ui);
			}
		}

		static void update(World& _world, ResT<RenderFrame> _frame)
		{
			const uint16_t _width = _frame->width;
			const uint16_t _height = _frame->height;
			if (0 == _width || 0 == _height)
			{
				return;
			}

			UiResource& ui = *_world.getResourceMut<UiResource>();
			// Build contiguous, ordered child ranges once; layout and drawing share them.
			ui.nodeCount = 0;
			_world.query<Entity, const UiNodeComponent&>().each([&](Entity _entity, const UiNodeComponent& _node)
			{
				if (!_node.visible || _world.has<UiCanvasComponent>(_entity)) { return; }
				if (ui.nodeCount == ui.nodes.getCapacity())
				{
					ui.nodes.setCapacity(max(64u, ui.nodeCount * 2), ui.nodeCount);
				}
				*ui.nodes.getPtr(ui.nodeCount++) = { _world.getParent(_entity), _entity, _node.order };
			});
			quickSort(ui.nodes.getPtr(), ui.nodeCount, [](const void* _lhs, const void* _rhs)
			{
				const UiTreeEntry& a = *(const UiTreeEntry*)_lhs;
				const UiTreeEntry& b = *(const UiTreeEntry*)_rhs;
				if (a.parent.idx != b.parent.idx) { return a.parent.idx < b.parent.idx ? -1 : 1; }
				if (a.order != b.order) { return a.order < b.order ? -1 : 1; }
				return a.entity.idx < b.entity.idx ? -1 : int(a.entity.idx != b.entity.idx);
			});
			uint32_t count = 0;
			_world.query<Entity, const UiCanvasComponent&>().each([&](Entity _entity, const UiCanvasComponent& _canvas)
			{
				if (!_canvas.visible) { return; }
				FROST_ASSERT(_canvas.scale > 0.0f, "UI canvas scale must be positive.");
				if (count == ui.canvases.getCapacity())
				{
					ui.canvases.setCapacity(max(8u, count * 2), count);
				}
				// Stable layer order; entity ID breaks ties independently of archetype order.
				uint32_t index = count++;
				while (index > 0)
				{
					const UiCanvasEntry prev = *ui.canvases.getPtr(index - 1);
					if (prev.layer < _canvas.layer || (prev.layer == _canvas.layer && prev.entity.idx < _entity.idx)) { break; }
					*ui.canvases.getPtr(index--) = prev;
				}
				*ui.canvases.getPtr(index) = { _entity, _canvas.layer };
			});
			bgfx::setViewRect(kRenderPassUi, 0, 0, _width, _height);
			bgfx::setViewFrameBuffer(kRenderPassUi, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kRenderPassUi, BGFX_CLEAR_NONE);
			// All canvases share the same projection; quads apply the canvas scale.
			float projection[16];
			mtxOrtho(projection, 0.0f, float(_width), float(_height), 0.0f, 0.0f, 100.0f,
				0.0f, bgfx::getCaps()->homogeneousDepth);
			bgfx::setViewTransform(kRenderPassUi, NULL, projection);
			for (uint32_t ii = 0; ii < count; ++ii)
			{
				const Entity entity = ui.canvases.getPtr(ii)->entity;
				ui.scale = _world.get<UiCanvasComponent>(entity)->scale;
				const UiBox viewport = { 0, 0, float(_width) / ui.scale, float(_height) / ui.scale };
				UiBox box = viewport;
				if (_world.has<UiNodeComponent>(entity))
				{
					box = measure(_world, entity, ui);
					const UiNodeComponent& node = *_world.get<UiNodeComponent>(entity);
					box.x = node.x + node.anchorX * viewport.width - node.pivotX * box.width;
					box.y = node.y + node.anchorY * viewport.height - node.pivotY * box.height;
				}
				draw(_world, entity, box, viewport, ui);
				flush(ui);
			}
		}

		static void shutdown(ResMutT<UiResource> _ui)
		{
			bgfx::destroy(_ui->program);
			bgfx::destroy(_ui->atlas);
			bgfx::destroy(_ui->sampler);
		}
	};

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
