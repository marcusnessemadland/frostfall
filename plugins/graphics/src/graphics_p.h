/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
#define BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>

#include <frost/math.h>
#include <frost/bounds.h>
#include <frost/handlealloc.h>
#include <frost/readerwriter.h>

#include <meshoptimizer/src/meshoptimizer.h>

/// Helpers
namespace frost
{
	struct ScreenVertex
	{
		float x, y, z, u, v;
	};

	static bool screenSpaceTriangle(const bgfx::VertexLayout& _layout)
	{
		if (3 != bgfx::getAvailTransientVertexBuffer(3, _layout))
		{
			return false;
		}
		bgfx::TransientVertexBuffer vb;
		bgfx::allocTransientVertexBuffer(&vb, 3, _layout);
		ScreenVertex* vertices = (ScreenVertex*)vb.data;
		const bool bottomLeft = bgfx::getCaps()->originBottomLeft;
		const float minv = bottomLeft ? 1.0f : 0.0f;
		const float maxv = bottomLeft ? -1.0f : 2.0f;
		vertices[0] = { -1.0f, 0.0f, 0.0f, -1.0f, minv };
		vertices[1] = {  1.0f, 0.0f, 0.0f,  1.0f, minv };
		vertices[2] = {  1.0f, 2.0f, 0.0f,  1.0f, maxv };
		bgfx::setVertexBuffer(0, &vb);
		return true;
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
		bgfx::VertexLayout      layout;
		MeshBounds              bounds;
		MeshPrimitive*          primitives = NULL;
		char*                   material = NULL;
		uint32_t                numVertices = 0;
		uint32_t                numIndices = 0;
		uint16_t                numPrimitives = 0;
		uint16_t                materialSlot = 0;
	};

	struct MeshRef
	{
		Guid       guid;
		MeshGroup* groups = NULL;
		uint32_t   numGroups = 0;
		uint32_t   refCount = 0;
	};

	struct MeshContext
	{
		AllocatorI* allocator;
		HandleAlloc* handles;
		MeshRef* refs;
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
		for (uint32_t ii = 0; ii < _mesh.numGroups; ++ii)
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
			free(allocator, group.material);
		}
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
		_group.material = readMeshString(_reader, _err);
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

	static MeshHandle meshCreate(const Memory* _mem, Error* _err)
	{
		MeshHandle handle;
		if (!_err->isOk() )
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
		if (kAssetTypeMesh != header.type || kMeshVersion != header.payloadVersion || 0 != header.dependencyCount)
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
		mesh.groups = (MeshGroup*)alloc(s_meshContext->allocator, sizeof(MeshGroup) * mesh.numGroups);
		if (NULL == mesh.groups)
		{
			FROST_ERROR_SET(_err, kErrorMeshCreate, "Unable to allocate mesh groups.");
			return handle;
		}
		for (uint32_t ii = 0; ii < mesh.numGroups; ++ii)
		{
			FROST_PLACEMENT_NEW(&mesh.groups[ii], MeshGroup)();
		}
		uint16_t numMaterialSlots = 0;
		for (uint32_t ii = 0; ii < mesh.numGroups && _err->isOk(); ++ii)
		{
			MeshGroup& group = mesh.groups[ii];
			if (!readMeshGroup(reader, group, _err) )
			{
				break;
			}
			group.materialSlot = numMaterialSlots;
			for (uint32_t jj = 0; jj < ii; ++jj)
			{
				if (0 == strCmp(group.material, mesh.groups[jj].material) )
				{
					group.materialSlot = mesh.groups[jj].materialSlot;
					break;
				}
			}
			if (group.materialSlot == numMaterialSlots)
			{
				++numMaterialSlots;
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

	struct MeshSystem
	{
		static void startup(World& _world, ResT<GraphicsResource> _graphics)
		{
			FROST_ASSERT(NULL == s_meshContext, "Only one graphics mesh context can be active.");
			if (NULL != s_meshContext || 0 == _graphics->maxMeshes)
			{
				return;
			}
			AllocatorI* allocator = _world.getAllocator();
			MeshContext* context = (MeshContext*)alloc(allocator, sizeof(MeshContext) );
			MeshRef* refs = (MeshRef*)alloc(allocator, sizeof(MeshRef) * _graphics->maxMeshes);
			void* handles = alloc(allocator, sizeof(HandleAlloc) + 2 * _graphics->maxMeshes * sizeof(uint16_t) );
			if (NULL == context || NULL == refs || NULL == handles)
			{
				free(allocator, context);
				free(allocator, refs);
				free(allocator, handles);
				debugPrintf("Unable to allocate graphics mesh storage.\n");
				return;
			}
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
			if (NULL == s_meshContext)
			{
				return;
			}
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

namespace frost
{
	constexpr bgfx::ViewId kRenderPassGeometry = 0;
	constexpr bgfx::ViewId kRenderPassLight = 1;
	constexpr bgfx::ViewId kRenderPassSky = 2;
	constexpr bgfx::ViewId kRenderPassCombine = 3;

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
	struct GBufferResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle textures[3] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		uint32_t width = 0;
		uint32_t height = 0;
		bool ready = false;
		bool active = false;
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

			bgfx::setPaletteColor(0, UINT32_C(0x00000000));
			bgfx::setPaletteColor(1, UINT32_C(0xffffffff));

			_gbuffer->ready = bgfx::getCaps()->limits.maxFBAttachments >= 2 && bgfx::isValid(_gbuffer->program);

			if (!_gbuffer->ready)
			{
				debugPrintf("Geometry rendering requires two color attachments and valid shaders.\n");
			}
		}

		static bool resize(GBufferResource& _gbuffer, uint32_t _width, uint32_t _height)
		{
			if (bgfx::isValid(_gbuffer.frameBuffer))
			{
				// Destroy current framebuffer if it exists
				bgfx::destroy(_gbuffer.frameBuffer);
			}

			const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
				| BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

			// Not all platforms support D32F
			const bgfx::TextureFormat::Enum depthFormat =
				bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D32F, flags)
				? bgfx::TextureFormat::D32F : bgfx::TextureFormat::D24;

			_gbuffer.textures[0] = bgfx::createTexture2D(uint16_t(_width), uint16_t(_height), false, 1, bgfx::TextureFormat::BGRA8, flags);
			_gbuffer.textures[1] = bgfx::createTexture2D(uint16_t(_width), uint16_t(_height), false, 1, bgfx::TextureFormat::BGRA8, flags);
			_gbuffer.textures[2] = bgfx::createTexture2D(uint16_t(_width), uint16_t(_height), false, 1, depthFormat, flags);

			_gbuffer.frameBuffer = bgfx::createFrameBuffer(3, _gbuffer.textures, false);

			_gbuffer.width = _width;
			_gbuffer.height = _height;

			return true;
		}

		static void update(
			  ResT<DisplayResource> _display
			, QueryT<const MeshComponent&, OptionalT<const TransformComponent>> _meshes
			, ResT<ActiveCameraResource> _activeCamera
			, QueryT<const CameraComponent&> _cameras
			, ResMutT<GBufferResource> _gbuffer
			)
		{
			_gbuffer->active = false;
			if (!_gbuffer->ready || 0 == _display->width || 0 == _display->height)
			{
				return;
			}

			float view[16], proj[16];
			if (!getCameraMatrices(*_activeCamera, _cameras, _display->width, _display->height, view, proj))
			{
				return;
			}

			if (_gbuffer->width != _display->width || _gbuffer->height != _display->height)
			{
				if (!resize(*_gbuffer, _display->width, _display->height))
				{
					debugPrintf("Unable to create deferred render targets.\n");
					_gbuffer->ready = false;
					return;
				}
			}

			const uint16_t width = uint16_t(_gbuffer->width);
			const uint16_t height = uint16_t(_gbuffer->height);

			bgfx::setViewRect(kRenderPassGeometry, 0, 0, width, height);
			bgfx::setViewFrameBuffer(kRenderPassGeometry, _gbuffer->frameBuffer);
			bgfx::setViewClear(kRenderPassGeometry, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 1.0f, 0, 1, 0);
			bgfx::setViewTransform(kRenderPassGeometry, view, proj);
			bgfx::touch(kRenderPassGeometry);

			_meshes.each([&](const MeshComponent& _mesh, const TransformComponent* _transform)
			{
				const MeshRef* mesh = getMeshRef(_mesh.handle);

				if (NULL == mesh)
				{
					return;
				}

				float mtx[16];
				mtxIdentity(mtx);

				if (NULL != _transform)
				{
					const Vec3& scale = _transform->scale;
					if (0.0f == scale.x || 0.0f == scale.y || 0.0f == scale.z)
					{
						// Zero scale invalidates transform
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
					for (uint16_t jj = 0; jj < group.numPrimitives; ++jj)
					{
						const MeshPrimitive& primitive = group.primitives[jj];

						bgfx::setTransform(mtx);
						bgfx::setVertexBuffer(0, group.vertexBuffer);
						bgfx::setIndexBuffer(group.indexBuffer, primitive.startIndex, primitive.numIndices);
						bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);

						bgfx::submit(kRenderPassGeometry, _gbuffer->program);
					}
				}
			});

			_gbuffer->active = true;
		}

		static void shutdown(ResMutT<GBufferResource> _gbuffer)
		{
			if (bgfx::isValid(_gbuffer->frameBuffer))
			{
				bgfx::destroy(_gbuffer->frameBuffer);
			}

			if (bgfx::isValid(_gbuffer->program))
			{
				bgfx::destroy(_gbuffer->program);
			}
		}
	};

} // namespace frost

// Sky model
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

	struct SunState
	{
		float direction[4];
		float luminance[4];
		float skyLuminanceXYZ[4];
		float tint[4];
		float parameters[4];
		float perez[20];
	};

	static void evaluateSun(const SkyComponent& _sky, SunState& _sun)
	{
		const float hour = isFinite(_sky.time) ? wrap(_sky.time, 24.0f) : 12.0f;
		const float latitude = toRad(clamp(_sky.latitude, -90.0f, 90.0f));
		const float day = 30.0f * float(clamp(int32_t(_sky.month), 0, 11)) + 15.0f;
		const float lambda = toRad(280.46f + 0.9856474f * day);
		const float delta = asin(sin(toRad(23.4f)) * sin(lambda));
		const float hh = (hour - 12.0f) * kPi / 12.0f;
		const float azimuth = atan2(sin(hh), cos(hh) * sin(latitude) - tan(delta) * cos(latitude));
		const float altitude = asin(clamp(sin(latitude) * sin(delta)
			+ cos(latitude) * cos(delta) * cos(hh), -1.0f, 1.0f));
		// +X is north and +Y is up, matching example 36.
		const Vec3 direction = { cos(azimuth) * cos(altitude), sin(altitude), sin(azimuth) * cos(altitude) };
		store(_sun.direction, direction);
		_sun.direction[3] = 0.0f;

		const Vec3 xyz = sampleSkyLuminance(s_sunLuminance, hour);
		const Vec3 rgb = {
			max(0.0f,  3.240479f * xyz.x - 1.537150f * xyz.y - 0.498530f * xyz.z),
			max(0.0f, -0.969256f * xyz.x + 1.875991f * xyz.y + 0.041556f * xyz.z),
			max(0.0f,  0.055648f * xyz.x - 0.204043f * xyz.y + 1.057311f * xyz.z),
		};
		const Vec3 tint = {
			max(0.0f, _sky.sunTint.x), max(0.0f, _sky.sunTint.y), max(0.0f, _sky.sunTint.z)
		};
		const float intensity = max(0.0f, _sky.sunIntensity);
		const float exposure = max(0.0f, _sky.exposure);
		store(_sun.luminance, mul(mul(rgb, tint), intensity * exposure));
		_sun.luminance[3] = 0.0f;
		store(_sun.skyLuminanceXYZ, sampleSkyLuminance(s_skyLuminance, hour));
		_sun.skyLuminanceXYZ[3] = 0.0f;
		store(_sun.tint, mul(tint, intensity));
		_sun.tint[3] = 0.0f;
		_sun.parameters[0] = max(0.0001f, _sky.sunSize);
		_sun.parameters[1] = max(0.0001f, _sky.sunBloom);
		_sun.parameters[2] = exposure;
		_sun.parameters[3] = hour;
		const float turbidity = clamp(_sky.turbidity, 1.9f, 10.0f);
		for (uint32_t ii = 0; ii < 5; ++ii)
		{
			store(&_sun.perez[ii * 4], mad(s_perezTurbidity[ii], turbidity, s_perezBase[ii]));
			_sun.perez[ii * 4 + 3] = 0.0f;
		}
	}

} // namespace frost

// Light pass
namespace frost
{
	struct LightResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_normal = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sunDirection = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sunLuminance = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout layout;
		bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle depth = BGFX_INVALID_HANDLE; // Borrowed from the G-buffer.
		uint32_t width = 0;
		uint32_t height = 0;
		bool ready = false;
		bool active = false;
	};

	struct LightSystem
	{
		static void startup(ResMutT<LightResource> _light)
		{
			bgfx::setViewName(kRenderPassLight, "Directional Light");
			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_light->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_light"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_deferred_light"), true);
			_light->s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
			_light->u_sunDirection = bgfx::createUniform("u_sunDirection", bgfx::UniformType::Vec4);
			_light->u_sunLuminance = bgfx::createUniform("u_sunLuminance", bgfx::UniformType::Vec4);
			_light->layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
			_light->ready = bgfx::isValid(_light->program)
				&& bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT);
			if (!_light->ready)
			{
				debugPrintf("Directional lighting requires RGBA16F rendering and valid shaders.\n");
			}
		}

		static void destroyTargets(LightResource& _light)
		{
			if (bgfx::isValid(_light.frameBuffer)) bgfx::destroy(_light.frameBuffer);
			if (bgfx::isValid(_light.texture)) bgfx::destroy(_light.texture);
			_light.frameBuffer = BGFX_INVALID_HANDLE;
			_light.texture = BGFX_INVALID_HANDLE;
			_light.depth = BGFX_INVALID_HANDLE;
			_light.width = _light.height = 0;
		}

		static bool resize(LightResource& _light, const GBufferResource& _gbuffer)
		{
			destroyTargets(_light);
			const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
				| BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
			_light.texture = bgfx::createTexture2D(uint16_t(_gbuffer.width), uint16_t(_gbuffer.height),
				false, 1, bgfx::TextureFormat::RGBA16F, flags);
			if (!bgfx::isValid(_light.texture))
			{
				return false;
			}
			bgfx::Attachment attachments[2];
			attachments[0].init(_light.texture);
			attachments[1].init(_gbuffer.textures[2], bgfx::Access::Read, 0, 1, 0, BGFX_ATTACHMENT_READ_ONLY_DEPTH);
			_light.frameBuffer = bgfx::createFrameBuffer(2, attachments, false);
			if (!bgfx::isValid(_light.frameBuffer))
			{
				destroyTargets(_light);
				return false;
			}
			_light.depth = _gbuffer.textures[2];
			_light.width = _gbuffer.width;
			_light.height = _gbuffer.height;
			return true;
		}

		static void update(ResT<GBufferResource> _gbuffer, QueryT<const SkyComponent&> _skies, ResMutT<LightResource> _light)
		{
			_light->active = false;
			if (!_light->ready || !_gbuffer->active)
			{
				return;
			}
			if (_light->width != _gbuffer->width || _light->height != _gbuffer->height
			|| _light->depth.idx != _gbuffer->textures[2].idx)
			{
				if (!resize(*_light, *_gbuffer))
				{
					debugPrintf("Unable to create lighting render targets.\n");
					_light->ready = false;
					return;
				}
			}
			float projection[16];
			mtxOrtho(projection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
			bgfx::setViewRect(kRenderPassLight, 0, 0, uint16_t(_light->width), uint16_t(_light->height));
			bgfx::setViewFrameBuffer(kRenderPassLight, _light->frameBuffer);
			bgfx::setViewClear(kRenderPassLight, BGFX_CLEAR_COLOR, UINT32_C(0x00000000));
			bgfx::setViewTransform(kRenderPassLight, NULL, projection);
			bgfx::touch(kRenderPassLight);
			_light->active = true;
			SunState sun = {};
			if (!_skies.single([&](const SkyComponent& _sky) { evaluateSun(_sky, sun); }) || !screenSpaceTriangle(_light->layout))
			{
				return;
			}
			bgfx::setUniform(_light->u_sunDirection, sun.direction);
			bgfx::setUniform(_light->u_sunLuminance, sun.luminance);
			bgfx::setTexture(0, _light->s_normal, _gbuffer->textures[1]);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			bgfx::submit(kRenderPassLight, _light->program);
		}

		static void shutdown(ResMutT<LightResource> _light)
		{
			destroyTargets(*_light);
			if (bgfx::isValid(_light->program)) bgfx::destroy(_light->program);
			bgfx::destroy(_light->s_normal);
			bgfx::destroy(_light->u_sunDirection);
			bgfx::destroy(_light->u_sunLuminance);
		}
	};
} // namespace frost

// Sky pass
namespace frost
{
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
		uint32_t skyCount = 0;
		bool ready = false;
	};

	struct SkySystem
	{
		static void advanceTime(ResT<TimeResource> _time, QueryT<SkyComponent&> _skies)
		{
			const float delta = float(min(_time->delta, 0.1));
			_skies.each([&](SkyComponent& _sky)
				{
					if (0.0f != _sky.hps)
					{
						_sky.time = wrap(_sky.time + delta * _sky.hps, 24.0f);
					}
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
			_sky->ready = bgfx::isValid(_sky->program) && bgfx::isValid(_sky->vertices) && bgfx::isValid(_sky->indices);
			if (!_sky->ready)
			{
				debugPrintf("Sky rendering requires valid shaders and grid buffers.\n");
			}
		}

		static void update(ResT<LightResource> _light, ResT<ActiveCameraResource> _activeCamera,
			QueryT<const CameraComponent&> _cameras, QueryT<const SkyComponent&> _skies, ResMutT<SkyResource> _sky)
		{
			const uint32_t skyCount = _skies.getCount();
			if (skyCount != _sky->skyCount)
			{
				if (skyCount > 1) debugPrintf("The active view requires at most one Sky component; sunlight and sky are disabled.\n");
				_sky->skyCount = skyCount;
			}
			if (!_sky->ready || !_light->active)
			{
				return;
			}
			SunState sun = {};
			if (!_skies.single([&](const SkyComponent& _component) { evaluateSun(_component, sun); }))
			{
				return;
			}
			float view[16], proj[16];
			if (!getCameraMatrices(*_activeCamera, _cameras, _light->width, _light->height, view, proj))
			{
				return;
			}
			bgfx::setViewRect(kRenderPassSky, 0, 0, uint16_t(_light->width), uint16_t(_light->height));
			bgfx::setViewFrameBuffer(kRenderPassSky, _light->frameBuffer);
			bgfx::setViewClear(kRenderPassSky, BGFX_CLEAR_NONE);
			bgfx::setViewTransform(kRenderPassSky, view, proj);
			bgfx::setUniform(_sky->u_sunDirection, sun.direction);
			bgfx::setUniform(_sky->u_skyLuminanceXYZ, sun.skyLuminanceXYZ);
			bgfx::setUniform(_sky->u_sunTint, sun.tint);
			bgfx::setUniform(_sky->u_parameters, sun.parameters);
			bgfx::setUniform(_sky->u_perezCoeff, sun.perez, 5);
			bgfx::setVertexBuffer(0, _sky->vertices);
			bgfx::setIndexBuffer(_sky->indices);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_EQUAL);
			bgfx::submit(kRenderPassSky, _sky->program);
		}

		static void shutdown(ResMutT<SkyResource> _sky)
		{
			if (bgfx::isValid(_sky->vertices)) bgfx::destroy(_sky->vertices);
			if (bgfx::isValid(_sky->indices)) bgfx::destroy(_sky->indices);
			if (bgfx::isValid(_sky->program)) bgfx::destroy(_sky->program);
			bgfx::destroy(_sky->u_sunDirection);
			bgfx::destroy(_sky->u_skyLuminanceXYZ);
			bgfx::destroy(_sky->u_sunTint);
			bgfx::destroy(_sky->u_parameters);
			bgfx::destroy(_sky->u_perezCoeff);
		}
	};
} // namespace frost

// Combine pass
namespace frost
{
	struct CombineResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_albedo = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_light = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_outputParameters = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout layout;
	};

	struct CombineSystem
	{
		static void startup(ResMutT<CombineResource> _combine)
		{
			bgfx::setViewName(kRenderPassCombine, "Combine");

			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_combine->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_combine"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_deferred_combine"), true);

			_combine->s_albedo = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
			_combine->s_light = bgfx::createUniform("s_light", bgfx::UniformType::Sampler);
			_combine->u_outputParameters = bgfx::createUniform("u_outputParameters", bgfx::UniformType::Vec4);

			_combine->layout.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
		}

		static void update(ResT<DisplayResource> _display, ResT<GBufferResource> _gbuffer,
			ResT<LightResource> _light, QueryT<const SkyComponent&> _skies, ResT<CombineResource> _combine)
		{
			if (0 == _display->width || 0 == _display->height)
			{
				return;
			}

			// Presentation owns the backbuffer clear, including frames without an active camera.
			bgfx::setViewRect(kRenderPassCombine, 0, 0, uint16_t(_display->width), uint16_t(_display->height));
			bgfx::setViewFrameBuffer(kRenderPassCombine, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kRenderPassCombine, BGFX_CLEAR_COLOR, UINT32_C(0x000000ff));
			bgfx::touch(kRenderPassCombine);

			if (!_gbuffer->active || !_light->active || !bgfx::isValid(_combine->program) || !screenSpaceTriangle(_combine->layout))
			{
				return;
			}

			float projection[16];
			mtxOrtho(projection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
			float output[4] = {};
			_skies.single([&](const SkyComponent& _sky)
				{
					output[0] = _sky.preventBanding ? 1.0f : 0.0f;
					output[1] = isFinite(_sky.time) ? wrap(_sky.time, 24.0f) : 12.0f;
				});

			bgfx::setViewTransform(kRenderPassCombine, NULL, projection);

			bgfx::setUniform(_combine->u_outputParameters, output);

			bgfx::setTexture(0, _combine->s_albedo, _gbuffer->textures[0]);
			bgfx::setTexture(1, _combine->s_light, _light->texture);

			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

			bgfx::submit(kRenderPassCombine, _combine->program);
		}

		static void shutdown(ResMutT<CombineResource> _combine)
		{
			if (bgfx::isValid(_combine->program))
			{
				bgfx::destroy(_combine->program);
			}

			bgfx::destroy(_combine->s_albedo);
			bgfx::destroy(_combine->s_light);
			bgfx::destroy(_combine->u_outputParameters);
		}
	};

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
