#ifndef BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
#define BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>

#include <frost/math.h>
#include <frost/bounds.h>
#include <frost/handlealloc.h>
#include <frost/readerwriter.h>

#include <meshoptimizer/src/meshoptimizer.h>

namespace frost
{
	constexpr bgfx::ViewId kRenderPassGeometry = 0;

} // namespace frost

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

		// The initial geometry pass requires float positions and a supported normal encoding.
		if (!_layout.has(bgfx::Attrib::Position) || !_layout.has(bgfx::Attrib::Normal) )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh requires position and normal attributes.");
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
		_layout.decode(bgfx::Attrib::Normal, num, type, normalized, asInt);
		if (3 > num || (bgfx::AttribType::Float != type && bgfx::AttribType::Half != type && !normalized)
		|| bgfx::AttribType::Int32 == type || bgfx::AttribType::Uint32 == type
		|| (bgfx::AttribType::Uint10 == type && 0 == (bgfx::getCaps()->supported & BGFX_CAPS_VERTEX_ATTRIB_UINT10) ) )
		{
			FROST_ERROR_SET(_err, kErrorMeshFormat, "Mesh normal encoding is unsupported.");
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
			bgfx::vertexUnpack(position, bgfx::Attrib::Position, _group.layout, vertices->data, ii);
			bgfx::vertexUnpack(normal, bgfx::Attrib::Normal, _group.layout, vertices->data, ii);
			valid &= isFinite(position[0]) && isFinite(position[1]) && isFinite(position[2])
				&& isFinite(normal[0]) && isFinite(normal[1]) && isFinite(normal[2]);
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


#include "vs_deferred_geom.bin.h"
#include "fs_deferred_geom.bin.h"

namespace frost
{
	static const bgfx::EmbeddedShader s_embeddedShaders[] =
	{
		BGFX_EMBEDDED_SHADER(vs_deferred_geom),
		BGFX_EMBEDDED_SHADER(fs_deferred_geom),

		BGFX_EMBEDDED_SHADER_END()
	};

	struct GBufferResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_normalMatrix = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_normalParams = BGFX_INVALID_HANDLE;

		bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle textures[3] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct GBufferSystem
	{
		static void startup(ResMutT<GBufferResource> _gbuffer)
		{
			const bgfx::RendererType::Enum type = bgfx::getRendererType();

			_gbuffer->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_geom"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_deferred_geom"),
				true
			);

			_gbuffer->u_normalMatrix = bgfx::createUniform("u_normalMatrix", bgfx::UniformType::Mat4);
			_gbuffer->u_normalParams = bgfx::createUniform("u_normalParams", bgfx::UniformType::Vec4);

			// Invalidate framebuffer so it gets created
			_gbuffer->frameBuffer = BGFX_INVALID_HANDLE;
		}

		static void update(
			  ResT<Display> _display
			, QueryT<const MeshHandle&, OptionalT<const Transform>> _meshes
			, ResT<ActiveCamera> _activeCamera
			, QueryT<const Camera&> _cameras
			, ResMutT<GBufferResource> _gbuffer
			)
		{
			if (0 == _display->width || 0 == _display->height)
			{
				return;
			}

			// Find active camera
			Camera camera;
			if (!_cameras.get(_activeCamera->entity, [&](const Camera& _camera)
				{
					camera = _camera;
				}) )
			{
				return;
			}

			// Recreate framebuffer if needed
			if (_gbuffer->width != _display->width || _gbuffer->height != _display->height)
			{
				if (bgfx::isValid(_gbuffer->frameBuffer))
				{
					// Textures are destroyed when framebuffer is destroyed
					bgfx::destroy(_gbuffer->frameBuffer);
				}

				const uint64_t tsFlags = 0
					| BGFX_SAMPLER_MIN_POINT
					| BGFX_SAMPLER_MAG_POINT
					| BGFX_SAMPLER_MIP_POINT
					| BGFX_SAMPLER_U_CLAMP
					| BGFX_SAMPLER_V_CLAMP
					;

				_gbuffer->width = _display->width;
				_gbuffer->height = _display->height;

				// Create framebuffer
				bgfx::Attachment gbufferAt[3];
				{
					_gbuffer->textures[0] = bgfx::createTexture2D(uint16_t(_gbuffer->width), uint16_t(_gbuffer->height), false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | tsFlags);
					_gbuffer->textures[1] = bgfx::createTexture2D(uint16_t(_gbuffer->width), uint16_t(_gbuffer->height), false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | tsFlags);
					gbufferAt[0].init(_gbuffer->textures[0], bgfx::Access::Write, 0, 1, 0, BGFX_ATTACHMENT_NONE);
					gbufferAt[1].init(_gbuffer->textures[1], bgfx::Access::Write, 0, 1, 0, BGFX_ATTACHMENT_NONE);

					// Use D32F if available
					const bgfx::TextureFormat::Enum depthFormat =
						  bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D32F, BGFX_TEXTURE_RT | tsFlags)
						? bgfx::TextureFormat::D32F
						: bgfx::TextureFormat::D24;

					_gbuffer->textures[2] = bgfx::createTexture2D(uint16_t(_gbuffer->width), uint16_t(_gbuffer->height), false, 1, depthFormat, BGFX_TEXTURE_RT | tsFlags);
					gbufferAt[2].init(_gbuffer->textures[2], bgfx::Access::Write, 0, 1, 0, BGFX_ATTACHMENT_NONE);
				}
				_gbuffer->frameBuffer = bgfx::createFrameBuffer(FROST_COUNTOF(gbufferAt), gbufferAt, true);
			}

			// Set views
			bgfx::setViewClear(kRenderPassGeometry
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 1.0f
			, 0
			, 1
			, 0
			);
			bgfx::setViewRect(kRenderPassGeometry, 0, 0, uint16_t(_gbuffer->width), uint16_t(_gbuffer->height));
			bgfx::setViewFrameBuffer(kRenderPassGeometry, BGFX_INVALID_HANDLE); // test presenting
			//bgfx::setViewFrameBuffer(kRenderPassGeometry, _gbuffer->frameBuffer);

			float view[16];
			float proj[16];
			{
				// Calculate view and projection matrix from camera
				mtxFromQuaternion(view, camera.rotation, camera.position);

				const float aspect = float(_gbuffer->width) / float(_gbuffer->height);
				const bool homogeneousDepth = bgfx::getCaps()->homogeneousDepth;

				if (CameraProjection::Orthographic == camera.projection)
				{
					const float halfHeight = camera.orthographicHeight * 0.5f;
					const float halfWidth = halfHeight * aspect;

					mtxOrtho(proj, -halfWidth, halfWidth, -halfHeight, halfHeight,
						camera.nearPlane, camera.farPlane, 0.0f, homogeneousDepth);
				}
				else
				{
					mtxProj(proj, camera.fov, aspect, camera.nearPlane, camera.farPlane, homogeneousDepth);
				}
			}
			bgfx::setViewTransform(kRenderPassGeometry, view, proj);

			// Submit all mesh instances. A missing transform means identity.
			_meshes.each([&](const MeshHandle& _handle, const Transform* _transform)
				{
					const MeshRef* mesh = getMeshRef(_handle);
					if (NULL == mesh)
					{
						return;
					}

					float model[16];
					float normal[16];
					mtxIdentity(model);
					mtxIdentity(normal);
					if (NULL != _transform)
					{
						const Vec3& scale = _transform->scale;
						if (0.0f == scale.x || 0.0f == scale.y || 0.0f == scale.z)
						{
							return;
						}
						const Quaternion rotation = normalize(_transform->rotation);
						const Vec3 x = toXAxis(rotation);
						const Vec3 y = toYAxis(rotation);
						const Vec3 z = toZAxis(rotation);
						store(&model[0], mul(x, scale.x) );
						store(&model[4], mul(y, scale.y) );
						store(&model[8], mul(z, scale.z) );
						store(&model[12], _transform->position);
						store(&normal[0], mul(x, 1.0f / scale.x) );
						store(&normal[4], mul(y, 1.0f / scale.y) );
						store(&normal[8], mul(z, 1.0f / scale.z) );
					}

					for (uint32_t ii = 0; ii < mesh->numGroups; ++ii)
					{
						const MeshGroup& group = mesh->groups[ii];
						uint8_t num;
						bgfx::AttribType::Enum type;
						bool normalized;
						bool asInt;
						group.layout.decode(bgfx::Attrib::Normal, num, type, normalized, asInt);
						const bool unsignedNormal = bgfx::AttribType::Uint8 == type
							|| bgfx::AttribType::Uint10 == type || bgfx::AttribType::Uint16 == type;
						const float normalParams[] = { unsignedNormal ? 2.0f : 1.0f, unsignedNormal ? -1.0f : 0.0f, 0.0f, 0.0f };

						for (uint16_t jj = 0; jj < group.numPrimitives; ++jj)
						{
							const MeshPrimitive& primitive = group.primitives[jj];
							bgfx::setTransform(model);
							bgfx::setUniform(_gbuffer->u_normalMatrix, normal);
							bgfx::setUniform(_gbuffer->u_normalParams, normalParams);
							bgfx::setVertexBuffer(0, group.vertexBuffer);
							bgfx::setIndexBuffer(group.indexBuffer, primitive.startIndex, primitive.numIndices);
							bgfx::setState(0
								| BGFX_STATE_WRITE_RGB
								| BGFX_STATE_WRITE_A
								| BGFX_STATE_WRITE_Z
								| BGFX_STATE_DEPTH_TEST_LESS
								| BGFX_STATE_MSAA
							);
							bgfx::submit(kRenderPassGeometry, _gbuffer->program);
						}
					}
				});
		}

		static void shutdown(ResMutT<GBufferResource> _gbuffer)
		{
			bgfx::destroy(_gbuffer->u_normalMatrix);
			bgfx::destroy(_gbuffer->u_normalParams);

			// Shaders are destroyed when program is destroyed
			bgfx::destroy(_gbuffer->program);

			// Textures are destroyed when framebuffer is destroyed
			if (bgfx::isValid(_gbuffer->frameBuffer) )
			{
				bgfx::destroy(_gbuffer->frameBuffer);
			}
		}
	};

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
