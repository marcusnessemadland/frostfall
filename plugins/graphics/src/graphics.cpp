/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#include <frost/os.h>
#include <frost/bufferstorage.h>

#include <graphics.h>
#include <device.h>

#include "graphics_p.h"

// Backend
namespace frost
{
	static bgfx::NativeWindowHandleType::Enum getBgfxWindowHandleType(NativeWindowHandleType::Enum _type)
	{
		// frost::NativeWindowHandleType is a maintained valid copy of bgfx::NativeWindowHandleType
		return static_cast<bgfx::NativeWindowHandleType::Enum>(_type);
	}

	static bgfx::RendererType::Enum getBgfxRendererType(RendererType::Enum _type)
	{
		// frost::RendererType is a maintained valid copy of bgfx::RendererType
		return static_cast<bgfx::RendererType::Enum>(_type);
	}

	struct RendererSystem
	{
		static void startup(ResT<GraphicsDesc> _graphics, ResT<DisplayResource> _display)
		{
			bgfx::Init init;
			init.type = getBgfxRendererType(_graphics->type);
			init.vendorId = BGFX_PCI_ID_NONE; // Autoselect adapter.
			init.swapChain.nwh = _display->nativeWindow;
			init.swapChain.ndt = _display->nativeDisplay;
			init.platformData.type = getBgfxWindowHandleType(_display->nativeWindowHandleType);
			init.swapChain.width = _display->width;
			init.swapChain.height = _display->height;
			init.swapChain.flags = BGFX_SWAP_CHAIN_SRGB_BACKBUFFER;
			init.swapChain.formatDepthStencil = bgfx::TextureFormat::Count;
			init.reset = BGFX_RESET_NONE; // TODO: Expose vsync in GraphicsResource.

			// Initialize bgfx
			if (!bgfx::init(init))
			{
				debugPrintf("Failed to initialize bgfx.\n");
				frost::exit(kExitFailure, false);
			}

			// Set debug TODO: Make it within FROST_DEBUG macros?
			bgfx::setDebug(BGFX_DEBUG_TEXT);
		}

		static void update(ResT<DisplayResource> _display)
		{
			if (0 == _display->width || 0 == _display->height)
			{
				// Minimized or disconnected monitor
				return;
			}

			// Reset on resize
			const bgfx::Stats* stats = bgfx::getStats();
			if (stats->width != _display->width || stats->height != _display->height)
			{
				bgfx::SwapChain swapChain;
				swapChain.width = _display->width;
				swapChain.height = _display->height;
				swapChain.flags = BGFX_SWAP_CHAIN_SRGB_BACKBUFFER;
				swapChain.formatDepthStencil = bgfx::TextureFormat::Count;
				bgfx::reset(BGFX_RESET_NONE, &swapChain);
			}
		}

		static void endFrame()
		{
			// Perfomance stats TODO:
			{
				const bgfx::Stats* stats = bgfx::getStats();

				bgfx::dbgTextClear(0x00, false);

				const double cpuMs = stats->cpuTimerFreq > 0
					? double(stats->cpuTimeEnd - stats->cpuTimeBegin)
						* 1000.0 / double(stats->cpuTimerFreq)
					: 0.0;

				const double cpuToMs = stats->cpuTimerFreq > 0
					? 1000.0 / double(stats->cpuTimerFreq)
					: 0.0;

				const double frameMs = double(stats->cpuTimeFrame) * cpuToMs;
				const double fps = frameMs > 0.0 ? 1000.0 / frameMs : 0.0;

				const double gameMs = stats->numEncoders > 0
					? double(stats->encoderStats[0].cpuTimeEnd
						   - stats->encoderStats[0].cpuTimeBegin) * cpuToMs
					: 0.0;

				const double gpuMs = stats->gpuTimerFreq > 0
					? double(stats->gpuTimeEnd - stats->gpuTimeBegin)
						* 1000.0 / double(stats->gpuTimerFreq)
					: 0.0;

				constexpr uint8_t green = 0x0A;
				constexpr uint16_t padding = 1;
				constexpr uint16_t textWidth = 39; // Width of longest stats line
				const uint16_t x = stats->textWidth > textWidth + padding
					? stats->textWidth - textWidth - padding
					: 0;
				const uint16_t y = 1;

				bgfx::dbgTextPrintf(x, y + 0, 0x0A,
					"cpu(game):   %5.2f ms [ %5.2f ms ]",
					gameMs, 0.0f);

				bgfx::dbgTextPrintf(x, y + 1, 0x0A,
					"cpu(render): %5.2f ms [ %5.2f ms ]",
					cpuMs, 0.0f);

				bgfx::dbgTextPrintf(x, y + 2, 0x0A,
					"gpu:         %5.2f ms [ %5.2f ms ]",
					gpuMs, 0.0f);

				bgfx::dbgTextPrintf(x, y + 3, 0x0A,
					"framerate:   %5.2f fps",
					fps);
			}

			// Build stats TODO:
			{
				const bgfx::Caps* caps = bgfx::getCaps();
				const bgfx::Stats* stats = bgfx::getStats();

				const char* renderer = bgfx::getRendererName(
					bgfx::getRendererType());

				const char* vendor = "Unknown";

				switch (caps->vendorId)
				{
				case 0x10DE: vendor = "NVIDIA"; break;
				case 0x1002: vendor = "AMD";    break;
				case 0x8086: vendor = "Intel";  break;
				case 0x106B: vendor = "Apple";  break;
				case 0x13B5: vendor = "ARM";    break;
				case 0x5143: vendor = "Qualcomm"; break;
				}

				constexpr uint8_t white = 0x0F;
				constexpr uint16_t x = 1;

				const uint16_t y = stats->textHeight > 3
					? stats->textHeight - 3
					: 0;

				bgfx::dbgTextPrintf(x, y, white,
					"%s %s @%s %s",
					renderer,
			#ifdef NDEBUG
					"release",
			#else
					"debug",
			#endif
					__DATE__,
					__TIME__);

				bgfx::dbgTextPrintf(x + 4, y + 1, white,
					"%s GPU [%04X:%04X] / VRAM: %lld MB",
					vendor,
					caps->vendorId,
					caps->deviceId,
					(long long)(stats->gpuMemoryMax / (1024 * 1024)));
			}

			// Swap buffers
			bgfx::frame();
		}

		static void shutdown()
		{
			bgfx::shutdown();
		}
	};

} // namespace frost

// Renderer
#include "vs_deferred_geom.bin.h"
#include "vs_deferred_geom_instanced.bin.h"
#include "fs_deferred_geom.bin.h"
#include "vs_deferred_light.bin.h"
#include "fs_deferred_light.bin.h"
#include "vs_sky.bin.h"
#include "fs_sky.bin.h"
#include "vs_deferred_combine.bin.h"
#include "fs_deferred_combine.bin.h"
#include "vs_ui.bin.h"
#include "fs_ui.bin.h"
#include "vs_shadow.bin.h"
#include "vs_shadow_instanced.bin.h"
#include "fs_shadow.bin.h"
#include "fs_shadow_masked.bin.h"

#include "vs_capture.bin.h"
#include "vs_capture_instanced.bin.h"
#include "fs_capture.bin.h"
#include "vs_local_shadow.bin.h"
#include "vs_local_shadow_instanced.bin.h"
#include "fs_local_shadow.bin.h"
#include "fs_local_shadow_masked.bin.h"
#include "cs_local_shadow_copy.bin.h"
#include "fs_local_light.bin.h"
#include "cs_environment_filter.bin.h"
#include "fs_environment_sky.bin.h"
#include "fs_environment_light.bin.h"

#include <bgfx/embedded_shader.h>

#include <charset.h> // bgfx's built-in bitmap font, retained with its upstream license.

namespace frost
{
	constexpr uint8_t kMaxLocalShadows = 30;
	constexpr uint8_t kLocalShadowColumns = 15;
	constexpr uint8_t kLocalShadowRows = (kMaxLocalShadows * 6 + kLocalShadowColumns - 1) / kLocalShadowColumns;
	constexpr bgfx::ViewId kRenderPassEnvironment = 0;
	constexpr bgfx::ViewId kRenderPassEnvironmentFilter = kRenderPassEnvironment + 1;
	constexpr bgfx::ViewId kRenderPassGeometry = kRenderPassEnvironmentFilter + 1;
	constexpr bgfx::ViewId kRenderPassShadow = kRenderPassGeometry + 1;
	constexpr bgfx::ViewId kRenderPassLocalShadow = kRenderPassShadow + 1;
	constexpr bgfx::ViewId kRenderPassLocalShadowCopy = kRenderPassLocalShadow + 1;
	constexpr bgfx::ViewId kRenderPassLight = kRenderPassLocalShadowCopy + 1;
	constexpr bgfx::ViewId kRenderPassLocalLight = kRenderPassLight + 1;
	constexpr bgfx::ViewId kRenderPassSky = kRenderPassLocalLight + 1;
	constexpr bgfx::ViewId kRenderPassToneMap = kRenderPassSky + 1;
	constexpr bgfx::ViewId kRenderPassUi = kRenderPassToneMap + 1;

	static const bgfx::EmbeddedShader s_embeddedShaders[] =
	{
		BGFX_EMBEDDED_SHADER(vs_deferred_geom),
		BGFX_EMBEDDED_SHADER(vs_deferred_geom_instanced),
		BGFX_EMBEDDED_SHADER(fs_deferred_geom),
		BGFX_EMBEDDED_SHADER(vs_deferred_light),
		BGFX_EMBEDDED_SHADER(fs_deferred_light),
		BGFX_EMBEDDED_SHADER(vs_sky),
		BGFX_EMBEDDED_SHADER(fs_sky),
		BGFX_EMBEDDED_SHADER(vs_deferred_combine),
		BGFX_EMBEDDED_SHADER(fs_deferred_combine),
		BGFX_EMBEDDED_SHADER(vs_shadow),
		BGFX_EMBEDDED_SHADER(vs_shadow_instanced),
		BGFX_EMBEDDED_SHADER(fs_shadow),
		BGFX_EMBEDDED_SHADER(fs_shadow_masked),
		BGFX_EMBEDDED_SHADER(vs_capture),
		BGFX_EMBEDDED_SHADER(vs_capture_instanced),
		BGFX_EMBEDDED_SHADER(fs_capture),
		BGFX_EMBEDDED_SHADER(vs_local_shadow),
		BGFX_EMBEDDED_SHADER(vs_local_shadow_instanced),
		BGFX_EMBEDDED_SHADER(fs_local_shadow),
		BGFX_EMBEDDED_SHADER(fs_local_shadow_masked),
		BGFX_EMBEDDED_SHADER(cs_local_shadow_copy),
		BGFX_EMBEDDED_SHADER(fs_local_light),
		BGFX_EMBEDDED_SHADER(cs_environment_filter),
		BGFX_EMBEDDED_SHADER(fs_environment_sky),
		BGFX_EMBEDDED_SHADER(fs_environment_light),
		BGFX_EMBEDDED_SHADER_END()
	};

} // namespace frost

// Geometry pass
namespace frost
{
	static bgfx::ProgramHandle createProgram(const char* _vertex, const char* _fragment)
	{
		const auto type = bgfx::getRendererType();
		const auto program = bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, _vertex),
			bgfx::createEmbeddedShader(s_embeddedShaders, type, _fragment), true);
		FROST_ASSERT(bgfx::isValid(program), "Failed to create %s / %s.", _vertex, _fragment);
		return program;
	}

	static void cubeView(float* _view, const Vec3& _position, uint8_t _face)
	{
		static const Vec3 directions[] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		static const Vec3 ups[] = { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1, 0 } };
		mtxLookAt(_view, _position, add(_position, directions[_face]), ups[_face]);
	}

	static const char* s_cubeFaces[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };

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
		Plane frustum[6] = { InitNone, InitNone, InitNone, InitNone, InitNone, InitNone };
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
				buildFrustumPlanes(_frame->frustum, viewProjection, bgfx::getCaps()->homogeneousDepth);
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

	static Aabb transformBounds(const Aabb& _bounds, const float* _matrix)
	{
		const Vec3 center = mul(getCenter(_bounds), _matrix);
		const Vec3 extents = getExtents(_bounds);
		const Vec3 x = { _matrix[0], _matrix[1], _matrix[2] };
		const Vec3 y = { _matrix[4], _matrix[5], _matrix[6] };
		const Vec3 z = { _matrix[8], _matrix[9], _matrix[10] };
		const Vec3 radius = add(add(mul(abs(x), extents.x), mul(abs(y), extents.y)), mul(abs(z), extents.z));
		return { sub(center, radius), add(center, radius) };
	}

	static bool visibleBounds(const Aabb& _bounds, const Plane* _planes)
	{
		for (uint8_t ii = 0; ii < 6; ++ii)
		{
			const Plane& plane = _planes[ii];
			const float x = plane.normal.x >= 0.0f ? _bounds.max.x : _bounds.min.x;
			const float y = plane.normal.y >= 0.0f ? _bounds.max.y : _bounds.min.y;
			const float z = plane.normal.z >= 0.0f ? _bounds.max.z : _bounds.min.z;
			if (plane.normal.x * x + plane.normal.y * y + plane.normal.z * z + plane.dist < 0.0f) { return false; }
		}
		return true;
	}
	struct MeshRenderItem
	{
		const MeshRef* mesh;
		MeshComponent instance;
		float model[16];
		Aabb bounds;
		float nearDepth;
		float farDepth;
		uint16_t lod;
		bool visible;
	};

	struct MeshLodState
	{
		uint32_t generation;
		uint16_t mesh;
		uint16_t lod;
	};

	struct MeshBvhNode
	{
		Aabb bounds;
		uint32_t item;
		uint32_t escape;
	};

	struct MeshRenderResource
	{
		MeshRenderResource() = default;
		MeshRenderResource(MeshRenderResource&& _other) : count(_other.count), frame(_other.frame), topology(_other.topology)
		{
			items = frost::move(_other.items);
			lods = frost::move(_other.lods);
			nodes = frost::move(_other.nodes);
			order = frost::move(_other.order);
		}
		DynamicStorageT<MeshRenderItem> items;
		DynamicStorageT<MeshLodState> lods;
		DynamicStorageT<MeshBvhNode> nodes;
		DynamicStorageT<uint32_t> order;
		uint32_t count = 0;
		uint32_t frame = 0;
		uint64_t topology = 0;

		void build(uint32_t _begin, uint32_t _end, uint32_t& _next)
		{
			MeshBvhNode& node = *nodes.getPtr(_next++);
			node.bounds = items.getPtr(*order.getPtr(_begin))->bounds;
			for (uint32_t ii = _begin + 1; ii < _end; ++ii)
			{
				const Aabb& bounds = items.getPtr(*order.getPtr(ii))->bounds;
				node.bounds.min = min(node.bounds.min, bounds.min);
				node.bounds.max = max(node.bounds.max, bounds.max);
			}
			node.item = UINT32_MAX;
			if (_end - _begin == 1) { node.item = *order.getPtr(_begin); }
			else
			{
				const Vec3 extents = getExtents(node.bounds);
				const uint8_t axis = extents.x > extents.y ? (extents.x > extents.z ? 0 : 2) : (extents.y > extents.z ? 1 : 2);
				const auto center = [&](uint32_t _index)
				{
					const Vec3 value = getCenter(items.getPtr(*order.getPtr(_index))->bounds);
					return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
				};
				const int32_t middle = int32_t((_begin + _end) / 2);
				int32_t lo = int32_t(_begin), hi = int32_t(_end - 1);
				// Median partition bounds traversal depth, including coincident instances.
				while (lo < hi)
				{
					const float pivot = center(uint32_t((lo + hi) / 2));
					int32_t left = lo, right = hi;
					while (left <= right)
					{
						while (center(uint32_t(left)) < pivot) { ++left; }
						while (center(uint32_t(right)) > pivot) { --right; }
						if (left <= right)
						{
							swap(*order.getPtr(uint32_t(left++)), *order.getPtr(uint32_t(right--)));
						}
					}
					if (middle <= right) { hi = right; }
					else if (middle >= left) { lo = left; }
					else { break; }
				}
				build(_begin, uint32_t(middle), _next);
				build(uint32_t(middle), _end, _next);
			}
			node.escape = _next;
		}

		void refit()
		{
			for (uint32_t ii = count * 2 - 1; ii-- > 0;)
			{
				MeshBvhNode& node = *nodes.getPtr(ii);
				if (node.item != UINT32_MAX) { node.bounds = items.getPtr(node.item)->bounds; }
				else
				{
					const MeshBvhNode& left = *nodes.getPtr(ii + 1);
					const MeshBvhNode& right = *nodes.getPtr(left.escape);
					node.bounds = { min(left.bounds.min, right.bounds.min), max(left.bounds.max, right.bounds.max) };
				}
			}
		}
	};

	struct MeshRenderSystem
	{
		static void startup(World& _world, ResMutT<MeshRenderResource> _render)
		{
			_render->items.setAllocator(_world.getAllocator());
			_render->lods.setAllocator(_world.getAllocator());
			_render->nodes.setAllocator(_world.getAllocator());
			_render->order.setAllocator(_world.getAllocator());
		}

		static void update(World& _world, ResT<RenderFrame> _frame, ResT<GraphicsSettings> _settings,
			QueryT<Entity, const MeshComponent&, OptionalT<const TransformComponent>> _meshes,
			ResMutT<MeshRenderResource> _render, ResMutT<GraphicsStatsResource> _stats)
		{
			const uint32_t previousCount = _render->count;
			uint64_t topology = 14695981039346656037ull;
			++_render->frame;
			_render->count = 0;
			*_stats = GraphicsStatsResource{};
			for (MeshGeometry* geometry : s_meshContext->geometries)
			{
				for (; NULL != geometry; geometry = geometry->next)
				{
					++_stats->sharedGeometries;
				}
			}
			_meshes.each([&](Entity _entity, const MeshComponent& _instance, const TransformComponent* _transform)
			{
				const MeshRef* mesh = getMeshRef(_instance.handle);
				FROST_ASSERT(NULL != mesh, "MeshComponent must reference a live mesh.");
				if (_render->count == _render->items.getCapacity())
				{
					_render->items.setCapacity(max(128u, _render->count * 2), _render->count);
				}
				MeshRenderItem& item = *_render->items.getPtr(_render->count);
				if (!getWorldTransform(item.model, _world, _entity, _transform))
				{
					return;
				}
				item.mesh = mesh;
				item.instance = _instance;
				item.bounds = transformBounds(mesh->geometry->bounds, item.model);
				topology = (topology ^ _entity.idx) * 1099511628211ull;
				topology = (topology ^ _entity.generation) * 1099511628211ull;
				item.visible = _frame->active && visibleBounds(item.bounds, _frame->frustum);
				const Aabb viewBounds = transformBounds(item.bounds, _frame->view);
				item.nearDepth = viewBounds.min.z;
				item.farDepth = viewBounds.max.z;
				if (_entity.idx >= _render->lods.getCapacity())
				{
					const uint32_t previous = _render->lods.getCapacity();
					_render->lods.setCapacity(max(_entity.idx + 1, max(128u, previous * 2)), previous);
					memSet(_render->lods.getPtr(previous), 0xff, sizeof(MeshLodState) * (_render->lods.getCapacity() - previous));
				}
				MeshLodState& state = *_render->lods.getPtr(_entity.idx);
				if (state.generation != _entity.generation || state.mesh != _instance.handle.idx)
				{
					state = { _entity.generation, _instance.handle.idx, 0 };
				}
				state.lod = min(state.lod, uint16_t(mesh->numLods - 1));
				if (!_frame->active || !_settings->lods)
				{
					state.lod = 0;
				}
				else
				{
					float pixels = length(getExtents(item.bounds)) * abs(_frame->projection[5]) * float(_frame->height);
					if (_frame->cameraPosition[3] < 0.5f)
					{
						pixels /= max(0.001f, item.nearDepth);
					}
					const float threshold = max(1.0f, _settings->lodPixels);
					// A dead band around each threshold prevents toggling as the camera moves.
					while (state.lod + 1 < mesh->numLods && pixels < threshold * exp2(-float(state.lod)) * 0.85f)
					{
						++state.lod;
					}
					while (state.lod > 0 && pixels > threshold * exp2(1.0f - float(state.lod)) * 1.15f)
					{
						--state.lod;
					}
				}
				item.lod = state.lod;
				++_render->count;
				++_stats->meshes;
				if (item.visible)
				{
					++_stats->visibleMeshes;
				}
			});
			if (_render->count > 0)
			{
				if (previousCount != _render->count || topology != _render->topology)
				{
					_render->nodes.setCapacity(_render->count * 2 - 1, 0);
					_render->order.setCapacity(_render->count, 0);
					for (uint32_t ii = 0; ii < _render->count; ++ii) { *_render->order.getPtr(ii) = ii; }
					uint32_t next = 0;
					_render->build(0, _render->count, next);
				}
				else { _render->refit(); }
			}
			_render->topology = topology;
		}
	};

	struct MeshDraw
	{
		const MeshGroup* group;
		const MeshPrimitive* primitive;
		const MaterialCreate* material;
		uint64_t state;
		float matrix[16];
		uint8_t pass;
		uint8_t bucket;
		bool depthOnly;
		bool masked;
	};

	struct MeshDrawResource
	{
		MeshDrawResource() = default;
		MeshDrawResource(MeshDrawResource&& _other) : count(_other.count), frame(_other.frame),
			instances(_other.instances), capacity(_other.capacity), used(_other.used)
		{
			draws = frost::move(_other.draws);
			_other.instances = BGFX_INVALID_HANDLE;
		}
		DynamicStorageT<MeshDraw> draws;
		uint32_t count = 0;
		uint32_t frame = UINT32_MAX;
		bgfx::DynamicVertexBufferHandle instances = BGFX_INVALID_HANDLE;
		uint32_t capacity = 0;
		uint32_t used = 0;
	};

	struct MeshPass
	{
		enum Flags : uint8_t
		{
			DepthOnly = 0x1,
			IndependentView = 0x2, //!< Use this pass's frustum and the coarsest mesh LOD.
		};
		bgfx::ViewId view;
		bgfx::ProgramHandle programs[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		bgfx::ProgramHandle instancedPrograms[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		bgfx::UniformHandle transform = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle tile = BGFX_INVALID_HANDLE;
		const float* sortView = NULL;
		float nearDepth = 0.0f;
		float depthScale = 0.01f;
		const float* viewProjection = NULL;
		const Plane* frustum = NULL;
		Plane planes[6] = { InitNone, InitNone, InitNone, InitNone, InitNone, InitNone };
		const Aabb* bounds = NULL;
		uint64_t state = 0;
		uint16_t resolution = 0;
		uint8_t flags = 0;
		uint8_t cascade = 0;
		bool active = true;
		bgfx::UniformHandle parameters = BGFX_INVALID_HANDLE;
		float values[12] = {};
		uint8_t numParameters = 2;
		bgfx::UniformHandle lights = BGFX_INVALID_HANDLE;
		float lightValues[32] = {};
		Vec3 origin = { 0, 0, 0 };
		float minimumDistance = 0.0f;
		bgfx::UniformHandle camera = BGFX_INVALID_HANDLE;
		float cameraMatrix[16] = {};
		uint16_t scissor[4] = {};
	};

	static bool meshPassVisible(const Aabb& _bounds, const MeshPass& _pass, const GraphicsSettings& _settings)
	{
		if (!_pass.active) { return false; }
		if (_pass.minimumDistance > 0.0f)
		{
			const Vec3 farthest = max(abs(sub(_bounds.min, _pass.origin)), abs(sub(_bounds.max, _pass.origin)));
			if (dot(farthest, farthest) < square(_pass.minimumDistance)) { return false; }
		}
		return !_settings.frustumCulling || ((NULL == _pass.frustum || visibleBounds(_bounds, _pass.frustum))
			&& (NULL == _pass.bounds || overlap(_bounds, *_pass.bounds)));
	}
	static void gatherMeshes(const MeshRenderResource& _render, MeshDrawResource& _draws,
		const MeshPass* _passes, uint8_t _numPasses, const GraphicsSettings& _settings, MeshPassStats& _stats)
	{
		FROST_ASSERT(_numPasses > 0 && _numPasses <= 6, "Mesh submission supports up to six views.");
		_draws.count = 0;
		if (_draws.frame != _render.frame)
		{
			_draws.frame = _render.frame;
			_draws.used = 0;
		}
		const bool depthOnly = 0 != (_passes[0].flags & MeshPass::DepthOnly);
		// Preorder nodes have escape links: reject a whole branch without a stack.
		for (uint32_t ii = 0; _render.count > 0 && ii < _render.count * 2 - 1;)
		{
			const MeshBvhNode& node = *_render.nodes.getPtr(ii);
			uint8_t mask = 0;
			for (uint8_t kk = 0; kk < _numPasses; ++kk)
			{
				if (meshPassVisible(node.bounds, _passes[kk], _settings)) { mask |= uint8_t(1 << kk); }
			}
			if (0 == mask)
			{
				_stats.culled += (node.escape - ii + 1) / 2;
				ii = node.escape;
				continue;
			}
			++ii;
			if (UINT32_MAX == node.item) { continue; }
			const MeshRenderItem& item = *_render.items.getPtr(node.item);
			if (depthOnly && !item.instance.castShadows) { continue; }
			uint16_t lodIndex = item.lod;
			if (depthOnly || 0 != (_passes[0].flags & MeshPass::IndependentView))
			{
				lodIndex = _settings.lods ? uint16_t(item.mesh->numLods - 1) : 0;
			}
			const MeshLod& lod = item.mesh->lods[lodIndex];
			float matrices[6][16];
			bool ready[6] = {};
			for (uint32_t jj = 0; jj < lod.numGroups; ++jj)
			{
				const MeshGroup& group = lod.groups[jj];
				const Aabb groupBounds = transformBounds(group.bounds.aabb, item.model);
				const MaterialCreate& material = getMeshMaterial(item.instance, *item.mesh, group.materialSlot);
				const bool masked = isMaterialMasked(material);
				const uint64_t state = meshState(material, item.model);
				uint8_t groupMask = 0;
				for (uint8_t kk = 0; kk < _numPasses; ++kk)
				{
					if (0 != (mask & (1 << kk)) && meshPassVisible(groupBounds, _passes[kk], _settings)) { groupMask |= uint8_t(1 << kk); }
					else { _stats.culled += group.numPrimitives; }
				}
				if (0 == groupMask) { continue; }
				for (uint16_t pp = 0; pp < group.numPrimitives; ++pp)
				{
					const MeshPrimitive& primitive = group.primitives[pp];
					const Aabb bounds = group.numPrimitives == 1 ? groupBounds : transformBounds(primitive.bounds.aabb, item.model);
					for (uint8_t kk = 0; kk < _numPasses; ++kk)
					{
						if (0 == (groupMask & (1 << kk))) { continue; }
						const MeshPass& pass = _passes[kk];
						if (group.numPrimitives > 1 && !meshPassVisible(bounds, pass, _settings)) { ++_stats.culled; continue; }
						if (!ready[kk])
						{
							if (NULL != pass.viewProjection) { mtxMul(matrices[kk], item.model, pass.viewProjection); }
							else { memCopy(matrices[kk], item.model, sizeof(item.model)); }
							ready[kk] = true;
						}
						if (_draws.count == _draws.draws.getCapacity())
						{
							_draws.draws.setCapacity(max(128u, _draws.count * 2), _draws.count);
						}
						MeshDraw& draw = *_draws.draws.getPtr(_draws.count++);
						draw.group = &group;
						draw.primitive = &primitive;
						draw.masked = masked;
						draw.material = depthOnly && !masked ? NULL : &material;
						draw.state = state | pass.state;
						draw.pass = kk;
						draw.depthOnly = depthOnly;
						float depth = bounds.min.z;
						if (NULL != pass.sortView)
						{
							const float* view = pass.sortView;
							depth = view[2] * (view[2] >= 0.0f ? bounds.min.x : bounds.max.x)
								+ view[6] * (view[6] >= 0.0f ? bounds.min.y : bounds.max.y)
								+ view[10] * (view[10] >= 0.0f ? bounds.min.z : bounds.max.z) + view[14];
						}
						draw.bucket = uint8_t(clamp((depth - pass.nearDepth) * pass.depthScale, 0.0f, 1.0f) * 31.0f);
						memCopy(draw.matrix, matrices[kk], sizeof(draw.matrix));
					}
				}
			}
		}
	}
	struct MaterialUniforms
	{
		bgfx::UniformHandle samplers[MaterialTexture::Count];
		bgfx::UniformHandle u_materialBaseColor = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_materialParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_materialEmissive = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_materialFlags = BGFX_INVALID_HANDLE;
	};

	struct GBufferResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle instancedProgram = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
		// Base color, normal/coverage, depth, metallic/roughness/AO, HDR emissive.
		bgfx::TextureHandle textures[5] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
			BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		MaterialUniforms material;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	static void submitMeshes(MeshDrawResource& _draws, const MaterialUniforms& _materialUniforms,
		const MeshPass* _passes, const GraphicsSettings& _settings, MeshPassStats& _stats);

	static void bindMaterial(const MaterialCreate& _material, const MaterialUniforms& _uniforms, bool _shadow = false)
	{
		const uint8_t count = _shadow ? 1 : MaterialTexture::Count;
		for (uint8_t ii = 0; ii < count; ++ii)
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
			bgfx::setTexture(ii, _uniforms.samplers[ii], NULL != texture ? texture->texture : fallback, sampler);
		}
		const float parameters[] = { _material.metallic, _material.roughness, _material.normalScale, _material.occlusionStrength };
		const float emissive[] = { _material.emissive.x * _material.emissiveStrength,
			_material.emissive.y * _material.emissiveStrength, _material.emissive.z * _material.emissiveStrength, _material.alphaCutoff };
		float flags[4] = {};
		if (isMaterialMasked(_material))
		{
			flags[0] = 1.0f;
		}
		if (0 != (_material.flags & MaterialFlags::DoubleSided))
		{
			flags[1] = 1.0f;
		}
		bgfx::setUniform(_uniforms.u_materialBaseColor, _material.baseColor);
		bgfx::setUniform(_uniforms.u_materialEmissive, emissive);
		if (!_shadow)
		{
			bgfx::setUniform(_uniforms.u_materialParameters, parameters);
			bgfx::setUniform(_uniforms.u_materialFlags, flags);
		}
	}

	struct GBufferSystem
	{
		static void startup(ResMutT<GBufferResource> _gbuffer)
		{
			bgfx::setViewName(kRenderPassGeometry, "Geometry");
			bgfx::setViewMode(kRenderPassGeometry, bgfx::ViewMode::Sequential);

			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_gbuffer->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_geom"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_deferred_geom"), true);

			FROST_ASSERT(bgfx::isValid(_gbuffer->program), "Failed to create the geometry program.");
			_gbuffer->instancedProgram = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_geom_instanced"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_deferred_geom"), true);
			FROST_ASSERT(bgfx::isValid(_gbuffer->instancedProgram), "Failed to create the instanced geometry program.");

			const char* samplers[] = { "s_baseColorMap", "s_normalMap", "s_metallicRoughnessMap", "s_occlusionMap", "s_emissiveMap" };
			for (uint32_t ii = 0; ii < MaterialTexture::Count; ++ii)
			{
				_gbuffer->material.samplers[ii] = bgfx::createUniform(samplers[ii], bgfx::UniformType::Sampler);
			}
			_gbuffer->material.u_materialBaseColor = bgfx::createUniform("u_materialBaseColor", bgfx::UniformType::Vec4);
			_gbuffer->material.u_materialParameters = bgfx::createUniform("u_materialParameters", bgfx::UniformType::Vec4);
			_gbuffer->material.u_materialEmissive = bgfx::createUniform("u_materialEmissive", bgfx::UniformType::Vec4);
			_gbuffer->material.u_materialFlags = bgfx::createUniform("u_materialFlags", bgfx::UniformType::Vec4);
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
				&& bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA8, flags)
				&& bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA8, flags | BGFX_TEXTURE_SRGB)
				&& bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::RGBA16F, flags)
				&& bgfx::isTextureValid(0, false, 1, depthFormat, flags), "Required G-buffer formats are unsupported.");

			_gbuffer.textures[0] = bgfx::createTexture2D(_width, _height, false, 1,
				bgfx::TextureFormat::RGBA8, flags | BGFX_TEXTURE_SRGB);
			_gbuffer.textures[1] = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::RGBA8, flags);
			_gbuffer.textures[2] = bgfx::createTexture2D(_width, _height, false, 1, depthFormat, flags);

			_gbuffer.textures[3] = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::RGBA8, flags);
			_gbuffer.textures[4] = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::RGBA16F, flags);
			const char* names[] = { "G-buffer base color (sRGB)", "G-buffer normal and coverage",
				"G-buffer depth", "G-buffer metallic roughness occlusion", "G-buffer HDR emissive" };
			for (uint8_t ii = 0; ii < FROST_COUNTOF(names); ++ii)
			{
				bgfx::setName(_gbuffer.textures[ii], names[ii]);
			}

			// Textures are owned here; framebuffers retain their attachments.
			_gbuffer.frameBuffer = bgfx::createFrameBuffer(5, _gbuffer.textures, false);
			FROST_ASSERT(bgfx::isValid(_gbuffer.frameBuffer), "Failed to create the G-buffer.");

			_gbuffer.width = _width;
			_gbuffer.height = _height;
		}


		static void update(ResT<RenderFrame> _frame, ResT<MeshRenderResource> _render,
			ResT<GraphicsSettings> _settings, ResMutT<MeshDrawResource> _draws,
			ResMutT<GraphicsStatsResource> _stats, ResMutT<GBufferResource> _gbuffer)
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

			MeshPass pass;
			pass.view = kRenderPassGeometry;
			pass.programs[0] = _gbuffer->program;
			pass.instancedPrograms[0] = _gbuffer->instancedProgram;
			pass.frustum = _frame->frustum;
			pass.sortView = _frame->view;
			pass.state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
			gatherMeshes(*_render, *_draws, &pass, 1, *_settings, _stats->geometry);
			submitMeshes(*_draws, _gbuffer->material, &pass, *_settings, _stats->geometry);
		}

		static void shutdown(ResMutT<GBufferResource> _gbuffer)
		{
			destroyTargets(*_gbuffer);
			bgfx::destroy(_gbuffer->program);
			if (bgfx::isValid(_gbuffer->instancedProgram))
			{
				bgfx::destroy(_gbuffer->instancedProgram);
			}
			for (bgfx::UniformHandle sampler : _gbuffer->material.samplers) { bgfx::destroy(sampler); }
			bgfx::destroy(_gbuffer->material.u_materialBaseColor);
			bgfx::destroy(_gbuffer->material.u_materialParameters);
			bgfx::destroy(_gbuffer->material.u_materialEmissive);
			bgfx::destroy(_gbuffer->material.u_materialFlags);
		}
	};

} // namespace frost

// Mesh submission
namespace frost
{
	static int32_t compareMeshDraws(const void* _lhs, const void* _rhs)
	{
		const MeshDraw& a = *(const MeshDraw*)_lhs;
		const MeshDraw& b = *(const MeshDraw*)_rhs;
		if (a.pass != b.pass)
		{
			return compareAscending<uint8_t>(&a.pass, &b.pass);
		}
		if (a.masked != b.masked) { return a.masked ? 1 : -1; }
		if (a.bucket != b.bucket) { return compareAscending<uint8_t>(&a.bucket, &b.bucket); }
		if (a.group != b.group)
		{
			const uintptr_t lhs = uintptr_t(a.group);
			const uintptr_t rhs = uintptr_t(b.group);
			return compareAscending<uintptr_t>(&lhs, &rhs);
		}
		if (a.primitive != b.primitive)
		{
			const uintptr_t lhs = uintptr_t(a.primitive), rhs = uintptr_t(b.primitive);
			return compareAscending<uintptr_t>(&lhs, &rhs);
		}
		if (a.state != b.state)
		{
			return compareAscending<uint64_t>(&a.state, &b.state);
		}
		if (a.material == b.material) { return 0; }
		if (NULL == a.material || NULL == b.material)
		{
			if (NULL == a.material) { return -1; }
			return 1;
		}
		const MaterialCreate& x = *a.material;
		const MaterialCreate& y = *b.material;
		uint32_t textures = MaterialTexture::Count;
		if (a.depthOnly)
		{
			textures = 1;
		}
		for (uint32_t ii = 0; ii < textures; ++ii)
		{
			if (x.textures[ii].idx != y.textures[ii].idx)
			{
				return compareAscending<uint16_t>(&x.textures[ii].idx, &y.textures[ii].idx);
			}
			if (x.samplers[ii] != y.samplers[ii])
			{
				return compareAscending<uint32_t>(&x.samplers[ii], &y.samplers[ii]);
			}
		}
		if (x.baseColor[3] != y.baseColor[3])
		{
			return compareAscending<float>(&x.baseColor[3], &y.baseColor[3]);
		}
		if (x.alphaCutoff != y.alphaCutoff)
		{
			return compareAscending<float>(&x.alphaCutoff, &y.alphaCutoff);
		}
		if (a.depthOnly)
		{
			return 0;
		}
		if (x.flags != y.flags)
		{
			return compareAscending<uint32_t>(&x.flags, &y.flags);
		}
		const float xv[] = { x.baseColor[0], x.baseColor[1], x.baseColor[2], x.emissive.x, x.emissive.y, x.emissive.z,
			x.emissiveStrength, x.metallic, x.roughness, x.normalScale, x.occlusionStrength };
		const float yv[] = { y.baseColor[0], y.baseColor[1], y.baseColor[2], y.emissive.x, y.emissive.y, y.emissive.z,
			y.emissiveStrength, y.metallic, y.roughness, y.normalScale, y.occlusionStrength };
		for (uint32_t ii = 0; ii < FROST_COUNTOF(xv); ++ii)
		{
			if (xv[ii] != yv[ii])
			{
				return compareAscending<float>(&xv[ii], &yv[ii]);
			}
		}
		return 0;
	}

	static void submitMeshes(MeshDrawResource& _draws, const MaterialUniforms& _materialUniforms,
		const MeshPass* _passes, const GraphicsSettings& _settings, MeshPassStats& _stats)
	{
		// BVH traversal produces ordered runs; first-pivot quicksort can become quadratic.
		stableSort(_draws.draws.getAllocator(), _draws.draws.getPtr(), _draws.count,
			[](const MeshDraw& _a, const MeshDraw& _b) { return compareMeshDraws(&_a, &_b) < 0; });
		const bool instancing = _settings.instancing;
		uint32_t offset = _draws.used;
		if (instancing && _draws.count > 0)
		{
			if (_draws.used + _draws.count > _draws.capacity)
			{
				if (bgfx::isValid(_draws.instances)) { bgfx::destroy(_draws.instances); }
				_draws.capacity = max(4096u, max(_draws.count, _draws.capacity * 2));
				bgfx::VertexLayout layout;
				layout.begin().add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
					.add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
					.add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
					.add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float).end();
				_draws.instances = bgfx::createDynamicVertexBuffer(_draws.capacity, layout);
				_draws.used = offset = 0;
			}
			const bgfx::Memory* memory = bgfx::alloc(_draws.count * 64);
			for (uint32_t ii = 0; ii < _draws.count; ++ii)
			{
				memCopy(memory->data + ii * 64, _draws.draws.getPtr(ii)->matrix, 64);
			}
			bgfx::update(_draws.instances, offset, memory);
			_draws.used += _draws.count;
		}
		for (uint32_t begin = 0; begin < _draws.count;)
		{
			const MeshDraw& draw = *_draws.draws.getPtr(begin);
			uint32_t end = begin + 1;
			while (end < _draws.count && 0 == compareMeshDraws(&draw, _draws.draws.getPtr(end))) { ++end; }
			const MeshPass& pass = _passes[draw.pass];
			const MeshGroup& group = *draw.group;
			const MeshPrimitive& primitive = *draw.primitive;
			const uint8_t program = draw.depthOnly && draw.masked ? 1 : 0;
			while (begin < end)
			{
				const uint32_t count = instancing && bgfx::isValid(pass.instancedPrograms[program]) ? end - begin : 1;
				bgfx::setVertexBuffer(0, group.vertexBuffer);
				bgfx::setIndexBuffer(group.indexBuffer, primitive.startIndex, primitive.numIndices);
				if (bgfx::isValid(pass.tile))
				{
					const float tile[] = { 0.25f, -0.75f + 0.5f * float(pass.cascade), 0.0f, 0.0f };
					bgfx::setScissor(uint16_t(pass.resolution * pass.cascade), 0, pass.resolution, pass.resolution);
					bgfx::setUniform(pass.tile, tile);
				}
				if (NULL != draw.material) { bindMaterial(*draw.material, _materialUniforms, draw.depthOnly); }
				bgfx::ProgramHandle shader = pass.programs[program];
				if (count > 1)
				{
					bgfx::setInstanceDataBuffer(_draws.instances, offset + begin, count);
					shader = pass.instancedPrograms[program];
				}
				else if (bgfx::isValid(pass.transform)) { bgfx::setUniform(pass.transform, _draws.draws.getPtr(begin)->matrix); }
				else { bgfx::setTransform(_draws.draws.getPtr(begin)->matrix); }
				if (bgfx::isValid(pass.parameters)) { bgfx::setUniform(pass.parameters, pass.values, pass.numParameters); }
				if (bgfx::isValid(pass.lights)) { bgfx::setUniform(pass.lights, pass.lightValues, 8); }
				if (bgfx::isValid(pass.camera)) { bgfx::setUniform(pass.camera, pass.cameraMatrix); }
				if (pass.scissor[2] > 0) { bgfx::setScissor(pass.scissor[0], pass.scissor[1], pass.scissor[2], pass.scissor[3]); }
				bgfx::setState(draw.state);
				bgfx::submit(pass.view, shader);
				++_stats.draws;
				_stats.instances += count;
				_stats.triangles += uint64_t(count) * primitive.numIndices / 3;
				if (draw.masked) { ++_stats.masked; }
				begin += count;
			}
		}
	}
} // namespace frost

// Sun shadow resources
namespace frost
{
	struct SunShadowTargets
	{
		bgfx::ProgramHandle programs[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		bgfx::ProgramHandle instancedPrograms[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		bgfx::TextureHandle atlas = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle target = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle sampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowViewProjection = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowTile = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowMatrix = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowSplits = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowTexels = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowInfo = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_shadowDirection = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_sceneView = BGFX_INVALID_HANDLE;
		float matrices[4][16] = {};
		float splits[4] = {};
		float texels[4] = {};
		float direction[4] = {};
		uint16_t resolution = 0;
		bool active = false;
		Entity light;
		float sceneView[16] = {};
		bool rendered = false;
	};

	static void bindShadows(const SunShadowTargets& _shadows, const SunShadowResource& _settings,
		const RenderFrame& _frame, bool _enabled)
	{
		bgfx::setTexture(5, _shadows.sampler, _shadows.atlas);
		const float parameters[] = { max(0.0f, _settings.depthBias), max(0.0f, _settings.normalBias),
			clamp(_settings.blend, 0.0f, 0.3f), max(0.0f, _settings.filterRadius) };
		const float info[] = { 1.0f / float(_shadows.resolution), _shadows.splits[3],
			_shadows.active && _enabled ? 1.0f : 0.0f, 0.0f };
		bgfx::setUniform(_shadows.u_shadowMatrix, _shadows.matrices, 4);
		bgfx::setUniform(_shadows.u_shadowSplits, _shadows.splits);
		bgfx::setUniform(_shadows.u_shadowTexels, _shadows.texels);
		bgfx::setUniform(_shadows.u_shadowDirection, _shadows.direction);
		bgfx::setUniform(_shadows.u_shadowParameters, parameters);
		bgfx::setUniform(_shadows.u_shadowInfo, info);
		const float* sceneView = _frame.view;
		if (_shadows.rendered) { sceneView = _shadows.sceneView; }
		bgfx::setUniform(_shadows.u_sceneView, sceneView);
	}

	struct SunShadowSystem
	{
		static void destroyTargets(SunShadowTargets& _shadows)
		{
			if (bgfx::isValid(_shadows.target)) { bgfx::destroy(_shadows.target); }
			if (bgfx::isValid(_shadows.atlas)) { bgfx::destroy(_shadows.atlas); }
			_shadows.target = BGFX_INVALID_HANDLE;
			_shadows.atlas = BGFX_INVALID_HANDLE;
			_shadows.resolution = 0;
			_shadows.rendered = false;
		}

		static void resize(SunShadowTargets& _shadows, uint16_t _resolution)
		{
			destroyTargets(_shadows);
			const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
				| BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
			// D16 exposes normalized R16 when sampled, without a separate color target.
			FROST_ASSERT(bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D16, flags),
				"16-bit shadow depth textures are unsupported.");
			_shadows.atlas = bgfx::createTexture2D(uint16_t(_resolution * 4), _resolution,
				false, 1, bgfx::TextureFormat::D16, flags);
			_shadows.target = bgfx::createFrameBuffer(1, &_shadows.atlas, false);
			FROST_ASSERT(bgfx::isValid(_shadows.target), "Failed to create the shadow atlas.");
			bgfx::setName(_shadows.atlas, "Global Shadow");
			_shadows.resolution = _resolution;
		}

		static void startup(World& _world, ResMutT<SunShadowTargets> _shadows, ResMutT<MeshDrawResource> _draws)
		{
			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_draws->draws.setAllocator(_world.getAllocator());
			const char* fragments[] = { "fs_shadow", "fs_shadow_masked" };
			for (uint8_t ii = 0; ii < 2; ++ii)
			{
				_shadows->programs[ii] = bgfx::createProgram(
					bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_shadow"),
					bgfx::createEmbeddedShader(s_embeddedShaders, type, fragments[ii]), true);
				FROST_ASSERT(bgfx::isValid(_shadows->programs[ii]), "Failed to create a shadow program.");
				_shadows->instancedPrograms[ii] = bgfx::createProgram(
					bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_shadow_instanced"),
					bgfx::createEmbeddedShader(s_embeddedShaders, type, fragments[ii]), true);
				FROST_ASSERT(bgfx::isValid(_shadows->instancedPrograms[ii]), "Failed to create instanced shadow program.");
			}
			_shadows->sampler = bgfx::createUniform("s_shadowAtlas", bgfx::UniformType::Sampler);
			_shadows->u_shadowViewProjection = bgfx::createUniform("u_shadowViewProjection", bgfx::UniformType::Mat4);
			_shadows->u_shadowTile = bgfx::createUniform("u_shadowTile", bgfx::UniformType::Vec4);
			bgfx::setViewName(kRenderPassShadow, "Global Shadow");
			bgfx::setViewMode(kRenderPassShadow, bgfx::ViewMode::Sequential);
			_shadows->u_shadowMatrix = bgfx::createUniform("u_shadowMatrix", bgfx::UniformType::Mat4, 4);
			_shadows->u_shadowSplits = bgfx::createUniform("u_shadowSplits", bgfx::UniformType::Vec4);
			_shadows->u_shadowTexels = bgfx::createUniform("u_shadowTexels", bgfx::UniformType::Vec4);
			_shadows->u_shadowParameters = bgfx::createUniform("u_shadowParameters", bgfx::UniformType::Vec4);
			_shadows->u_shadowInfo = bgfx::createUniform("u_shadowInfo", bgfx::UniformType::Vec4);
			_shadows->u_shadowDirection = bgfx::createUniform("u_shadowDirection", bgfx::UniformType::Vec4);
			_shadows->u_sceneView = bgfx::createUniform("u_sceneView", bgfx::UniformType::Mat4);
			resize(*_shadows, 1); // Valid bindings even without an active shadow light.
		}


		static void update(ResT<RenderFrame> _frame, ResMutT<SunShadowResource> _settings,
			ResT<ActiveCameraResource> _activeCamera, QueryT<const CameraComponent&> _cameras,
			QueryT<Entity, const DirectionalLightComponent&> _lights, ResT<MeshRenderResource> _render,
			ResT<GraphicsSettings> _graphics, ResMutT<GraphicsStatsResource> _stats,
			ResT<GBufferResource> _gbuffer, ResMutT<SunShadowTargets> _shadows,
			ResMutT<MeshDrawResource> _draws)
		{
			_draws->count = 0;
			_shadows->active = false;
			if (!_frame->active || _settings->distance <= 0.0f || _settings->distanceScale <= 0.0f) { return; }
			DirectionalLightComponent light;
			bool found = _lights.get(_settings->light, [&](Entity, const DirectionalLightComponent& _light) { light = _light; });
			if (!found)
			{
				_lights.each([&](Entity _entity, const DirectionalLightComponent& _light)
				{
					if (!found && _light.intensity > 0.0f)
					{
						_settings->light = _entity;
						light = _light;
						found = true;
					}
				});
			}
			if (!found || light.intensity <= 0.0f || max(light.color.x, max(light.color.y, light.color.z)) <= 0.0f) { return; }
			const uint16_t resolution = uint16_t(clamp(uint32_t(_settings->resolution), 16u,
				min(65535u, bgfx::getCaps()->limits.maxTextureSize) / 4));
			if (_shadows->resolution != resolution) { resize(*_shadows, resolution); }
			if (light.isStatic && _shadows->rendered && _shadows->light == _settings->light)
			{
				_shadows->active = true;
				return;
			}
			const float lengthSq = dot(light.direction, light.direction);
			FROST_ASSERT(isFinite(lengthSq) && lengthSq > 0.0f, "Shadow light direction must be finite and nonzero.");
			const Vec3 direction = mul(light.direction, rsqrt(lengthSq));
			store(_shadows->direction, direction);
			CameraComponent camera;
			_cameras.get(_activeCamera->entity, [&](const CameraComponent& _camera) { camera = _camera; });
			const float nearPlane = camera.nearPlane;
			const float farPlane = min(camera.farPlane, _settings->distance * _settings->distanceScale);
			if (farPlane <= nearPlane) { return; }
			const bool homogeneous = bgfx::getCaps()->homogeneousDepth;
			const float aspect = float(_frame->width) / float(_frame->height);
			const Quaternion rotation = normalize(camera.rotation);
			const Vec3 right = toXAxis(rotation), cameraUp = toYAxis(rotation), forward = toZAxis(rotation);
			const float lambda = CameraProjection::Orthographic == camera.projection ? 0.0f
				: clamp(_settings->splitLambda, 0.0f, 1.0f);
			for (uint8_t ii = 0; ii < 4; ++ii)
			{
				const float fraction = float(ii + 1) / 4.0f;
				_shadows->splits[ii] = lerp(nearPlane + (farPlane - nearPlane) * fraction,
					nearPlane * pow(farPlane / nearPlane, fraction), lambda);
			}
			float lightView[16];
			const Vec3 up = abs(direction.y) > 0.99f ? Vec3{ 0.0f, 0.0f, 1.0f } : Vec3{ 0.0f, 1.0f, 0.0f };
			mtxLookAt(lightView, { 0.0f, 0.0f, 0.0f }, mul(direction, -1.0f), up);
			Aabb cascadeBounds[4];
			MeshPass passes[4];
			for (uint8_t cascade = 0; cascade < 4; ++cascade)
			{
				const float previous = 0 == cascade ? nearPlane : _shadows->splits[cascade - 1];
				const float previousNear = cascade < 2 ? nearPlane : _shadows->splits[cascade - 2];
				const float start = max(nearPlane, previous - (previous - previousNear) * clamp(_settings->blend, 0.0f, 0.3f));
				Vec3 corners[8] = { InitNone, InitNone, InitNone, InitNone, InitNone, InitNone, InitNone, InitNone };
				Vec3 center = { 0.0f, 0.0f, 0.0f };
				for (uint8_t ii = 0; ii < 8; ++ii)
				{
					const float depth = ii & 4 ? _shadows->splits[cascade] : start;
					const float height = CameraProjection::Orthographic == camera.projection
						? camera.orthographicHeight * 0.5f : depth * tan(toRad(camera.fov) * 0.5f);
					corners[ii] = add(camera.position, add(mul(forward, depth),
						add(mul(right, (ii & 1 ? height : -height) * aspect), mul(cameraUp, ii & 2 ? height : -height))));
					center = add(center, corners[ii]);
				}
				center = mul(center, 1.0f / 8.0f);
				float radius = 0.0f;
				float minZ = kFloatLargest, maxZ = -kFloatLargest;
				for (const Vec3& corner : corners)
				{
					const Vec3 delta = sub(corner, center);
					radius = max(radius, sqrt(dot(delta, delta)));
					const float z = mul(corner, lightView).z;
					minZ = min(minZ, z); maxZ = max(maxZ, z);
				}
				// Stable square extents and snapping, as in bgfx/examples/16-shadowmaps.
				radius = ceil(radius * 16.0f) / 16.0f;
				radius *= float(resolution) / float(resolution - 2);
				const float texel = 2.0f * radius / float(resolution);
				_shadows->texels[cascade] = texel;
				const Vec3 lightCenter = mul(center, lightView);
				const float x = floor(lightCenter.x / texel + 0.5f) * texel;
				const float y = floor(lightCenter.y / texel + 0.5f) * texel;
				minZ -= max(0.0f, _settings->casterDistance);
				maxZ += texel;
				float projection[16];
				mtxOrtho(projection, x - radius, x + radius, y - radius, y + radius, minZ, maxZ, 0.0f, homogeneous);
				mtxMul(_shadows->matrices[cascade], lightView, projection);
				cascadeBounds[cascade].min = { x - radius, y - radius, minZ };
				cascadeBounds[cascade].max = { x + radius, y + radius, maxZ };

				MeshPass& pass = passes[cascade];
				pass.view = kRenderPassShadow;
				memCopy(pass.programs, _shadows->programs, sizeof(pass.programs));
				memCopy(pass.instancedPrograms, _shadows->instancedPrograms, sizeof(pass.instancedPrograms));
				pass.transform = _shadows->u_shadowViewProjection;
				pass.tile = _shadows->u_shadowTile;
				pass.sortView = lightView;
				pass.nearDepth = minZ;
				pass.depthScale = 1.0f / max(0.001f, maxZ - minZ);
				pass.viewProjection = _shadows->matrices[cascade];
				pass.state = BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
				pass.resolution = resolution;
				pass.flags = MeshPass::DepthOnly;
				pass.cascade = cascade;

				if (_graphics->frustumCulling)
				{
					// Bound potential receivers, then extrude toward the light. Casters need not be camera-visible.
					Aabb receivers = { { kFloatLargest, kFloatLargest, kFloatLargest }, { -kFloatLargest, -kFloatLargest, -kFloatLargest } };
					for (uint32_t ii = 0; ii < _render->count; ++ii)
					{
						const MeshRenderItem& item = *_render->items.getPtr(ii);
						if (!item.visible || item.farDepth < start || item.nearDepth > _shadows->splits[cascade])
						{
							continue;
						}
						const Aabb bounds = transformBounds(item.bounds, lightView);
						receivers.min = min(receivers.min, bounds.min);
						receivers.max = max(receivers.max, bounds.max);
					}
					const float margin = texel * (1.0f + max(0.0f, _settings->normalBias) + max(0.0f, _settings->filterRadius));
					const Vec3 padding = { margin, margin, margin };
					receivers.min = sub(receivers.min, padding);
					receivers.max = add(receivers.max, padding);
					receivers.min.z -= max(0.0f, _settings->casterDistance);
					cascadeBounds[cascade].min = max(cascadeBounds[cascade].min, receivers.min);
					cascadeBounds[cascade].max = min(cascadeBounds[cascade].max, receivers.max);
					const Aabb& bounds = cascadeBounds[cascade];
					pass.active = bounds.min.x <= bounds.max.x && bounds.min.y <= bounds.max.y && bounds.min.z <= bounds.max.z;
				}
				if (pass.active && _graphics->frustumCulling)
				{
					// Transform six planes once, rather than every BVH/group bound for every cascade.
					const Aabb& bounds = cascadeBounds[cascade];
					float cullProjection[16], cullViewProjection[16];
					mtxOrtho(cullProjection, bounds.min.x, bounds.max.x, bounds.min.y, bounds.max.y,
						bounds.min.z, bounds.max.z, 0.0f, homogeneous);
					mtxMul(cullViewProjection, lightView, cullProjection);
					buildFrustumPlanes(pass.planes, cullViewProjection, homogeneous);
					pass.frustum = pass.planes;
				}
			}
			// One atlas clear; submit the culled caster batches in light-depth order.
			bgfx::setViewRect(kRenderPassShadow, 0, 0, uint16_t(resolution * 4), resolution);
			bgfx::setViewFrameBuffer(kRenderPassShadow, _shadows->target);
			bgfx::setViewClear(kRenderPassShadow, BGFX_CLEAR_DEPTH, 0, 1.0f);
			bgfx::setViewTransform(kRenderPassShadow, NULL, NULL);
			bgfx::touch(kRenderPassShadow);
			gatherMeshes(*_render, *_draws, passes, 4, *_graphics, _stats->shadows);
			submitMeshes(*_draws, _gbuffer->material, passes, *_graphics, _stats->shadows);

			_shadows->active = true;
			_shadows->light = _settings->light;
			_shadows->rendered = light.isStatic;
			memCopy(_shadows->sceneView, _frame->view, sizeof(_shadows->sceneView));
		}

		static void shutdown(ResMutT<SunShadowTargets> _shadows, ResMutT<MeshDrawResource> _draws)
		{
			destroyTargets(*_shadows);
			if (bgfx::isValid(_draws->instances)) { bgfx::destroy(_draws->instances); }
			for (bgfx::ProgramHandle program : _shadows->programs) { bgfx::destroy(program); }
			for (bgfx::ProgramHandle program : _shadows->instancedPrograms)
			{
				if (bgfx::isValid(program)) { bgfx::destroy(program); }
			}
			bgfx::destroy(_shadows->sampler);
			bgfx::destroy(_shadows->u_shadowViewProjection);
			bgfx::destroy(_shadows->u_shadowTile);
			bgfx::destroy(_shadows->u_shadowMatrix);
			bgfx::destroy(_shadows->u_shadowSplits);
			bgfx::destroy(_shadows->u_shadowTexels);
			bgfx::destroy(_shadows->u_shadowParameters);
			bgfx::destroy(_shadows->u_shadowInfo);
			bgfx::destroy(_shadows->u_shadowDirection);
			bgfx::destroy(_shadows->u_sceneView);
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
			bgfx::setViewName(kRenderPassLight, "Global Light");
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
			QueryT<Entity, const DirectionalLightComponent&> _lights, ResMutT<LightResource> _light,
			ResT<SunShadowTargets> _shadows, ResT<SunShadowResource> _shadowSettings)
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

			// Seed emission once, including frames without a directional light.
			bindShadows(*_shadows, *_shadowSettings, *_frame, false);
			const float emission[4] = {};
			bgfx::setUniform(_light->u_sunLuminance, emission);
			bgfx::setUniform(_light->u_sunDirection, emission);
			bindScene(*_frame, *_gbuffer, *_light);
			screenSpaceTriangle(_light->layout);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			bgfx::submit(kRenderPassLight, _light->program);

			_lights.each([&](Entity _entity, const DirectionalLightComponent& _component)
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
				bindShadows(*_shadows, *_shadowSettings, *_frame, _entity == _shadowSettings->light);
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

	// Authored moonlight uses the same relative XYZ units as the sun and sky.
	// The cool profile represents the perceived night lighting at the normal exposure.
	static const SkyLuminance s_moonLuminance[] =
	{
		{  0.0f, { 2.5283f, 2.5125f, 5.6714f } },
		{  2.0f, { 2.3801f, 2.3625f, 5.3775f } },
		{  4.0f, { 1.9604f, 1.9375f, 4.3712f } },
		{  6.0f, { 1.1639f, 1.1500f, 2.5630f } },
		{  8.0f, { 0.9817f, 0.9750f, 2.1404f } },
		{ 16.0f, { 0.9817f, 0.9750f, 2.1404f } },
		{ 18.0f, { 1.3461f, 1.3250f, 2.9857f } },
		{ 20.0f, { 2.1085f, 2.0875f, 4.6932f } },
		{ 22.0f, { 2.4542f, 2.4375f, 5.4916f } },
		{ 24.0f, { 2.5283f, 2.5125f, 5.6714f } },
	};

	static const SkyLuminance s_skyLuminance[] =
	{
		{  0.0f, { 0.2593f, 0.2550f, 0.5841f } },
		{  1.0f, { 0.2593f, 0.2550f, 0.5841f } },
		{  2.0f, { 0.2507f, 0.2463f, 0.5677f } },
		{  3.0f, { 0.2288f, 0.2250f, 0.5178f } },
		{  4.0f, { 0.1957f, 0.1925f, 0.4443f } },
		{  5.0f, { 0.1837f, 0.1812f, 0.4168f } },
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
		{ 20.0f, { 0.2263f, 0.2225f, 0.5124f } },
		{ 21.0f, { 0.2263f, 0.2225f, 0.5124f } },
		{ 22.0f, { 0.2436f, 0.2400f, 0.5510f } },
		{ 23.0f, { 0.2544f, 0.2500f, 0.5734f } },
		{ 24.0f, { 0.2593f, 0.2550f, 0.5841f } },
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

	static constexpr float kMoonCycleDays = 29.53059f;

	static Vec3 skyLuminanceToRgb(const Vec3& _xyz)
	{
		return {
			max(0.0f,  3.240479f * _xyz.x - 1.537150f * _xyz.y - 0.498530f * _xyz.z),
			max(0.0f, -0.969256f * _xyz.x + 1.875991f * _xyz.y + 0.041556f * _xyz.z),
			max(0.0f,  0.055648f * _xyz.x - 0.204043f * _xyz.y + 1.057311f * _xyz.z),
		};
	}

	static float skyDaylight(const SkyAtmosphereComponent& _sky)
	{
		return smoothStep(clamp((_sky.sunDirection.y + 0.207912f) / 0.242812f, 0.0f, 1.0f));
	}

	static Vec3 solarSkyLuminance(const SkyAtmosphereComponent& _sky)
	{
		const float hour = isFinite(_sky.time) ? wrap(_sky.time, 24.0f) : 12.0f;
		// Fade the daylight fit through twilight; never evaluate it
		// around a source below the horizon. Lunar scattering is evaluated separately.
		const float daylight = skyDaylight(_sky);
		return mul(sampleSkyLuminance(s_skyLuminance, clamp(hour, 7.0f, 18.0f)),
			daylight * max(0.0f, _sky.sunIntensity));
	}

	static Vec3 skyDirection(float _hourAngle, float _declination, float _latitude)
	{
		const float azimuth = atan2(sin(_hourAngle), cos(_hourAngle) * sin(_latitude)
			- tan(_declination) * cos(_latitude));
		const float altitude = asin(clamp(sin(_latitude) * sin(_declination)
			+ cos(_latitude) * cos(_declination) * cos(_hourAngle), -1.0f, 1.0f));
		return { cos(azimuth) * cos(altitude), sin(altitude), sin(azimuth) * cos(altitude) };
	}

	static Vec3 moonTransmittance(float _elevation, float _turbidity)
	{
		// Kasten-Young air mass and RGB extinction, relative to a clear zenith.
		const float elevation = max(0.0f, _elevation);
		const float airMass = 1.0f / (sin(toRad(elevation))
			+ 0.50572f * pow(elevation + 6.07995f, -1.6364f));
		const float haze = 0.025f * (clamp(_turbidity, 1.9f, 10.0f) - 2.15f);
		const float path = max(0.0f, airMass - 1.0f);
		return { exp(-(0.08f + haze) * path), exp(-(0.12f + haze) * path), exp(-(0.22f + haze) * path) };
	}

	static float skyLuminance(const Vec3& _rgb)
	{
		return dot(_rgb, { 0.2126f, 0.7152f, 0.0722f });
	}

	static Vec3 lunarSkyLuminance(const SkyAtmosphereComponent& _sky)
	{
		const float hour = isFinite(_sky.time) ? wrap(_sky.time, 24.0f) : 12.0f;
		const float nightHour = hour < 12.0f ? min(hour, 5.0f) : max(hour, 20.0f);
		const Vec3 tint = { max(0.0f, _sky.moonTint.x), max(0.0f, _sky.moonTint.y), max(0.0f, _sky.moonTint.z) };
		const float fullMoon = max(0.0001f, skyLuminance(mul(
			skyLuminanceToRgb(sampleSkyLuminance(s_moonLuminance, hour)), tint)));
		const float illumination = max(0.0f, _sky.moonIlluminance) / fullMoon;
		// Keep a faint sky on moonless nights; lunar phase and elevation provide the rest.
		const float strength = (0.12f * max(0.0f, _sky.moonIntensity) + 0.88f * illumination)
			* (1.0f - skyDaylight(_sky));
		const Vec3 rgb = mul(skyLuminanceToRgb(sampleSkyLuminance(s_skyLuminance, nightHour)), tint);
		const Vec3 xyz = {
			0.412456f * rgb.x + 0.357576f * rgb.y + 0.180438f * rgb.z,
			0.212673f * rgb.x + 0.715152f * rgb.y + 0.072175f * rgb.z,
			0.019334f * rgb.x + 0.119192f * rgb.y + 0.950304f * rgb.z,
		};
		return mul(xyz, strength);
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
		bgfx::UniformHandle u_moonDirection = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_moonUp = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_moonColor = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_moonSkyLuminanceXYZ = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_moonParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_moon = BGFX_INVALID_HANDLE;
	};

	static void bindSky(const SkyResource& _sky, const SkyAtmosphereComponent& _component)
	{
		float direction[4] = {};
		store(direction, _component.sunDirection);
		float luminance[4] = {};
		store(luminance, solarSkyLuminance(_component));
		const float sunTint[] = { max(0.0f, _component.sunTint.x) * max(0.0f, _component.sunIntensity),
			max(0.0f, _component.sunTint.y) * max(0.0f, _component.sunIntensity),
			max(0.0f, _component.sunTint.z) * max(0.0f, _component.sunIntensity), 0.0f };
		const float parameters[] = { max(0.0001f, _component.sunSize), max(0.0001f, _component.sunBloom),
			0.0f, 0.0f };
		float perez[20] = {};
		for (uint8_t ii = 0; ii < 5; ++ii)
		{
			store(&perez[ii * 4], mad(s_perezTurbidity[ii], clamp(_component.turbidity, 1.9f, 10.0f), s_perezBase[ii]));
		}
		float moonDirection[4] = {};
		store(moonDirection, _component.moonDirection);
		float moonUp[4] = {};
		store(moonUp, _component.moonUp);
		float moonColor[4] = {};
		const float hour = isFinite(_component.time) ? wrap(_component.time, 24.0f) : 12.0f;
		const Vec3 transmission = moonTransmittance(_component.moonElevation, _component.turbidity);
		const Vec3 tint = { max(0.0f, _component.moonTint.x), max(0.0f, _component.moonTint.y), max(0.0f, _component.moonTint.z) };
		// The disk stays neutral while the authored moonlight profile supplies the cool scene lighting.
		store(moonColor, mul(mul(transmission, tint),
			sampleSkyLuminance(s_moonLuminance, hour).y * max(0.0f, _component.moonIntensity)));
		float moonSkyLuminance[4] = {};
		store(moonSkyLuminance, lunarSkyLuminance(_component));
		const TextureRef* texture = getTextureRef(_component.moonTexture);
		const float radius = sin(toRad(clamp(_component.moonSize, 0.01f, 10.0f)) * 0.5f);
		const float moonParameters[] = { radius, _component.moonPhaseBrightness * 2.0f,
			NULL != texture ? 1.0f / max(0.0001f, _component.moonAlbedo) : 1.0f, 0.0f };

		bgfx::setUniform(_sky.u_sunDirection, direction);
		bgfx::setUniform(_sky.u_skyLuminanceXYZ, luminance);
		bgfx::setUniform(_sky.u_sunTint, sunTint);
		bgfx::setUniform(_sky.u_parameters, parameters);
		bgfx::setUniform(_sky.u_perezCoeff, perez, 5);
		bgfx::setUniform(_sky.u_moonDirection, moonDirection);
		bgfx::setUniform(_sky.u_moonUp, moonUp);
		bgfx::setUniform(_sky.u_moonColor, moonColor);
		bgfx::setUniform(_sky.u_moonSkyLuminanceXYZ, moonSkyLuminance);
		bgfx::setUniform(_sky.u_moonParameters, moonParameters);
		bgfx::setTexture(0, _sky.s_moon, NULL != texture ? texture->texture : s_textureContext->white,
			BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_U_CLAMP);
	}

	struct SkySystem
	{
		static void prepare(ResT<TimeResource> _time, QueryT<SkyAtmosphereComponent&> _skies)
		{
			const float deltaTime = float(min(_time->delta, 0.1));
			_skies.each([&](SkyAtmosphereComponent& _sky)
			{
				const float hours = isFinite(_sky.hps) ? deltaTime * _sky.hps : 0.0f;
				_sky.time = wrap((isFinite(_sky.time) ? _sky.time : 12.0f) + hours, 24.0f);
				_sky.moonCycle = wrap((isFinite(_sky.moonCycle) ? _sky.moonCycle : 0.5f)
					+ (_sky.pauseMoonCycle ? 0.0f : hours / (24.0f * kMoonCycleDays)), 1.0f);

				const float latitude = toRad(clamp(_sky.latitude, -90.0f, 90.0f));
				const float day = 30.0f * float(clamp(int32_t(_sky.month), 0, 11)) + 15.0f;
				const float longitude = toRad(280.46f + 0.9856474f * day);
				const float obliquity = toRad(23.4f);
				const float declination = asin(sin(obliquity) * sin(longitude));
				const float hourAngle = (_sky.time - 12.0f) * kPi / 12.0f;
				_sky.sunDirection = skyDirection(hourAngle, declination, latitude);

				const Vec3 rgb = skyLuminanceToRgb(sampleSkyLuminance(s_sunLuminance, clamp(_sky.time, 7.0f, 18.0f)));
				const Vec3 sunTint = { max(0.0f, _sky.sunTint.x), max(0.0f, _sky.sunTint.y), max(0.0f, _sky.sunTint.z) };
				// Fade by elevation, so the clock table cannot shut off a sun still above the horizon.
				const float sunlight = smoothStep(clamp(_sky.sunDirection.y / 0.1f, 0.0f, 1.0f));
				_sky.sunColor = mul(mul(rgb, sunTint), max(0.0f, _sky.sunIntensity) * sunlight);

				// Circular ecliptic orbit: phases, rise/set times, and terminator share one geometry.
				// The existing month/latitude solar approximation is retained; this is not an ephemeris.
				const float moonLongitude = longitude + _sky.moonCycle * 2.0f * kPi;
				const float sunAscension = atan2(cos(obliquity) * sin(longitude), cos(longitude));
				const float moonAscension = atan2(cos(obliquity) * sin(moonLongitude), cos(moonLongitude));
				const float moonDeclination = asin(sin(obliquity) * sin(moonLongitude));
				_sky.moonDirection = skyDirection(hourAngle + sunAscension - moonAscension, moonDeclination, latitude);
				const Vec3 pole = { cos(latitude), sin(latitude), 0.0f };
				_sky.moonUp = normalize(sub(pole, mul(_sky.moonDirection, dot(pole, _sky.moonDirection))));
				// Keep the visible orbit separate from the authored nighttime key light.
				_sky.moonLightDirection = normalize({ _sky.moonDirection.x,
					max(_sky.moonDirection.y, 0.25f), _sky.moonDirection.z });
				_sky.moonElevation = toDeg(asin(clamp(_sky.moonDirection.y, -1.0f, 1.0f)));
				const float phaseCosine = clamp(-dot(_sky.sunDirection, _sky.moonDirection), -1.0f, 1.0f);
				const float phaseAngle = acos(phaseCosine);
				// Empirical lunar magnitude law, including the strong increase near full moon.
				const float degrees = toDeg(phaseAngle);
				const float brightness = phaseCosine < -0.99999f ? 0.0f
					: pow(10.0f, -0.4f * (0.026f * degrees + 4.0e-9f * pow(degrees, 4.0f)));
				const float halfAngle = clamp(phaseAngle * 0.5f, 0.0001f, kPi * 0.5f - 0.0001f);
				const float diskIntegral = max(0.000001f, 1.0f - sin(halfAngle) * tan(halfAngle) * log(1.0f / tan(halfAngle * 0.5f)));
				_sky.moonPhaseBrightness = brightness / diskIntegral;
				const float visible = smoothStep(clamp(_sky.moonElevation / max(0.01f, _sky.moonSize * 0.5f), 0.0f, 1.0f));
				const Vec3 transmission = moonTransmittance(max(15.0f, _sky.moonElevation), _sky.turbidity);
				const Vec3 moonTint = { max(0.0f, _sky.moonTint.x), max(0.0f, _sky.moonTint.y), max(0.0f, _sky.moonTint.z) };
				// As sunlight fades, keep a weak nighttime key even before moonrise or at new moon.
				// Horizon extinction still applies to the visible disk through bindSky.
				const float moonlight = lerp(visible * brightness, max(brightness, 0.08f), 1.0f - sunlight);
				const float intensity = max(0.0f, _sky.moonIntensity) * moonlight;
				const Vec3 moonRgb = skyLuminanceToRgb(sampleSkyLuminance(s_moonLuminance, _sky.time));
				_sky.moonColor = mul(mul(mul(moonRgb, transmission), moonTint), intensity);
				_sky.moonIlluminance = skyLuminance(_sky.moonColor);
			});
		}

		static void startup(ResMutT<SkyResource> _sky)
		{
			bgfx::setViewName(kRenderPassSky, "Sky Atmosphere");

			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_sky->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_sky"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_sky"), true);

			_sky->u_sunDirection = bgfx::createUniform("u_sunDirection", bgfx::UniformType::Vec4);
			_sky->u_skyLuminanceXYZ = bgfx::createUniform("u_skyLuminanceXYZ", bgfx::UniformType::Vec4);
			_sky->u_sunTint = bgfx::createUniform("u_sunTint", bgfx::UniformType::Vec4);
			_sky->u_parameters = bgfx::createUniform("u_parameters", bgfx::UniformType::Vec4);
			_sky->u_perezCoeff = bgfx::createUniform("u_perezCoeff", bgfx::UniformType::Vec4, 5);
			_sky->u_moonDirection = bgfx::createUniform("u_moonDirection", bgfx::UniformType::Vec4);
			_sky->u_moonUp = bgfx::createUniform("u_moonUp", bgfx::UniformType::Vec4);
			_sky->u_moonColor = bgfx::createUniform("u_moonColor", bgfx::UniformType::Vec4);
			_sky->u_moonSkyLuminanceXYZ = bgfx::createUniform("u_moonSkyLuminanceXYZ", bgfx::UniformType::Vec4);
			_sky->u_moonParameters = bgfx::createUniform("u_moonParameters", bgfx::UniformType::Vec4);
			_sky->s_moon = bgfx::createUniform("s_moon", bgfx::UniformType::Sampler);

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
			QueryT<const SkyAtmosphereComponent&> _skies, ResT<SkyResource> _sky)
		{
			if (!_frame->active)
			{
				return;
			}

			bgfx::setViewRect(kRenderPassSky, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassSky, _light->frameBuffer);
			bgfx::setViewClear(kRenderPassSky, BGFX_CLEAR_NONE);
			bgfx::setViewTransform(kRenderPassSky, _frame->view, _frame->projection);

			_skies.single([&](const SkyAtmosphereComponent& _component)
			{
				bindSky(*_sky, _component);

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
			bgfx::destroy(_sky->u_moonDirection);
			bgfx::destroy(_sky->u_moonUp);
			bgfx::destroy(_sky->u_moonColor);
			bgfx::destroy(_sky->u_moonSkyLuminanceXYZ);
			bgfx::destroy(_sky->u_moonParameters);
			bgfx::destroy(_sky->s_moon);
		}
	};

} // namespace frost

// Local shadows and light
namespace frost
{
	struct LocalLight
	{
		Entity entity;
		// Position/range, radiance/type, direction/cone, right/width, up/height,
		// inner cone/radius/two sided, shadow slice/bias/texel size, reserved.
		float data[8][4] = {};
		float projection[16] = {};
		bool castShadows = true;
		int16_t slot = -1;
		bool isStatic = false;
		bool visible = false;
	};

	struct LocalShadowState
	{
		Entity entity;
		uint8_t type = 0;
		bool rendered = false;
	};

	struct LocalLightResource
	{
		LocalLightResource() = default;
		LocalLightResource(LocalLightResource&& _other) { lights = frost::move(_other.lights); }
		DynamicStorageT<LocalLight> lights;
		uint32_t count = 0;
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle shadows[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		bgfx::ProgramHandle instanced[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		bgfx::UniformHandle parameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle projection = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle shadowParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle pointSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle spotSampler = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle pointDepth = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle spotDepth = BGFX_INVALID_HANDLE;
		LocalShadowState slots[kMaxLocalShadows];
		bgfx::ProgramHandle copy = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle shadowCamera = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle copyParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle copyTypes = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle atlasSampler = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle atlas = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle target = BGFX_INVALID_HANDLE;
		uint16_t resolution = 0;
	};

	struct LocalLightSystem
	{
		static void startup(World& _world, ResMutT<LocalLightResource> _local)
		{
			_local->lights.setAllocator(_world.getAllocator());
			_local->program = createProgram("vs_deferred_light", "fs_local_light");
			_local->shadows[0] = createProgram("vs_local_shadow", "fs_local_shadow");
			_local->shadows[1] = createProgram("vs_local_shadow", "fs_local_shadow_masked");
			_local->instanced[0] = createProgram("vs_local_shadow_instanced", "fs_local_shadow");
			_local->instanced[1] = createProgram("vs_local_shadow_instanced", "fs_local_shadow_masked");
			_local->parameters = bgfx::createUniform("u_localLight", bgfx::UniformType::Vec4, 8);
			_local->projection = bgfx::createUniform("u_localProjection", bgfx::UniformType::Mat4);
			_local->shadowParameters = bgfx::createUniform("u_localShadow", bgfx::UniformType::Vec4, 3);
			_local->pointSampler = bgfx::createUniform("s_pointShadows", bgfx::UniformType::Sampler);
			_local->spotSampler = bgfx::createUniform("s_spotShadows", bgfx::UniformType::Sampler);
			_local->copy = bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders,
				bgfx::getRendererType(), "cs_local_shadow_copy"), true);
			FROST_ASSERT(bgfx::isValid(_local->copy), "Failed to create local shadow copy program.");
			_local->shadowCamera = bgfx::createUniform("u_localShadowViewProjection", bgfx::UniformType::Mat4);
			_local->copyParameters = bgfx::createUniform("u_localShadowCopy", bgfx::UniformType::Vec4);
			_local->copyTypes = bgfx::createUniform("u_localShadowTypes", bgfx::UniformType::Vec4, 8);
			_local->atlasSampler = bgfx::createUniform("s_localShadowAtlas", bgfx::UniformType::Sampler);
			bgfx::setViewName(kRenderPassLocalShadow, "Local Shadow");
			bgfx::setViewName(kRenderPassLocalShadowCopy, "Local Shadow copy");
			bgfx::setViewMode(kRenderPassLocalShadow, bgfx::ViewMode::Sequential);
			bgfx::setViewName(kRenderPassLocalLight, "Local Light");
			bgfx::setViewMode(kRenderPassLocalLight, bgfx::ViewMode::Sequential);
		}

		static void destroyTargets(LocalLightResource& _local)
		{
			if (bgfx::isValid(_local.target)) { bgfx::destroy(_local.target); }
			if (bgfx::isValid(_local.atlas)) { bgfx::destroy(_local.atlas); }
			_local.target = BGFX_INVALID_HANDLE;
			_local.atlas = BGFX_INVALID_HANDLE;
			if (bgfx::isValid(_local.pointDepth)) { bgfx::destroy(_local.pointDepth); }
			if (bgfx::isValid(_local.spotDepth)) { bgfx::destroy(_local.spotDepth); }
			_local.pointDepth = _local.spotDepth = BGFX_INVALID_HANDLE;
			_local.resolution = 0;
			for (auto& slot : _local.slots) { slot.rendered = false; }
		}

		static void resize(LocalLightResource& _local, uint16_t _resolution)
		{
			destroyTargets(_local);
			const uint64_t flags = BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
				| BGFX_SAMPLER_W_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT;
			FROST_ASSERT(0 != (bgfx::getCaps()->supported & BGFX_CAPS_TEXTURE_CUBE_ARRAY)
				&& 0 != (bgfx::getCaps()->supported & BGFX_CAPS_COMPUTE), "Local shadows require compute and cubemap arrays.");
			_local.pointDepth = bgfx::createTextureCube(_resolution, false, kMaxLocalShadows, bgfx::TextureFormat::R32F, flags);
			_local.spotDepth = bgfx::createTexture2D(_resolution, _resolution, false, kMaxLocalShadows, bgfx::TextureFormat::R32F, flags);
			_local.atlas = bgfx::createTexture2D(_resolution * kLocalShadowColumns, _resolution * kLocalShadowRows,
				false, 1, bgfx::TextureFormat::D16, BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
			bgfx::Attachment attachment;
			attachment.init(_local.atlas, bgfx::Access::Write, 0, 1, 0, 0);
			_local.target = bgfx::createFrameBuffer(1, &attachment, false);
			FROST_ASSERT(bgfx::isValid(_local.target), "Failed to create local shadow atlas.");
			bgfx::setName(_local.atlas, "Local Shadow - depth atlas");
			bgfx::setName(_local.pointDepth, "Local Shadow - point/rect cube array (six slices per light)");
			bgfx::setName(_local.spotDepth, "Local Shadow - spot array (one slice per light)");
			_local.resolution = _resolution;
		}

		template<typename Ty>
		static LocalLight* append(World& _world, LocalLightResource& _local, const RenderFrame& _frame,
			Entity _entity, const Ty& _component, const TransformComponent* _transform, float _type)
		{
			if (!isFinite(_component.intensity) || _component.intensity <= 0.0f
				|| !isFinite(_component.range) || _component.range <= 0.01f)
			{
				return NULL;
			}
			float matrix[16];
			if (!getWorldTransform(matrix, _world, _entity, _transform)) { return NULL; }
			const Vec3 position = { matrix[12], matrix[13], matrix[14] };
			const Vec3 extent = { _component.range, _component.range, _component.range };
			const bool visible = visibleBounds({ sub(position, extent), add(position, extent) }, _frame.frustum);
			// Bake static lights even outside the camera view, on their first scene frame.
			if (!visible && !(_component.isStatic && _component.castShadows)) { return NULL; }
			if (_local.count == _local.lights.getCapacity())
			{
				_local.lights.setCapacity(max(32u, _local.count * 2), _local.count);
			}
			LocalLight& light = *_local.lights.getPtr(_local.count++);
			light = {};
			light.entity = _entity;
			light.castShadows = _component.castShadows;
			light.isStatic = _component.isStatic;
			light.visible = visible;
			store(light.data[0], position);
			light.data[0][3] = _component.range;
			store(light.data[1], mul(max(_component.color, { 0, 0, 0 }), _component.intensity));
			light.data[1][3] = _type;
			store(light.data[2], normalize({ matrix[8], matrix[9], matrix[10] }));
			store(light.data[3], normalize({ matrix[0], matrix[1], matrix[2] }));
			store(light.data[4], normalize({ matrix[4], matrix[5], matrix[6] }));
			light.data[5][1] = clamp(_component.sourceRadius, 0.01f, _component.range * 0.25f);
			return &light;
		}

		static void shadows(World& _world, ResT<RenderFrame> _frame, ResT<MeshRenderResource> _meshes,
			ResT<GBufferResource> _gbuffer, ResMutT<MeshDrawResource> _draws, ResT<GraphicsSettings> _graphics,
			ResT<LocalShadowResource> _settings, ResMutT<LocalLightResource> _local, ResMutT<GraphicsStatsResource> _stats,
			QueryT<Entity, const PointLightComponent&, OptionalT<const TransformComponent>> _points,
			QueryT<Entity, const SpotLightComponent&, OptionalT<const TransformComponent>> _spots,
			QueryT<Entity, const RectLightComponent&, OptionalT<const TransformComponent>> _rects)
		{
			_local->count = 0;
			_stats->localShadows = {};
			_stats->localLights = 0;
			if (!_frame->active) { return; }
			_points.each([&](Entity _entity, const PointLightComponent& _light, const TransformComponent* _transform)
			{
				append(_world, *_local, *_frame, _entity, _light, _transform, 0.0f);
			});
			_spots.each([&](Entity _entity, const SpotLightComponent& _light, const TransformComponent* _transform)
			{
				if (auto* light = append(_world, *_local, *_frame, _entity, _light, _transform, 1.0f))
				{
					light->data[2][3] = cos(toRad(clamp(_light.outerAngle, 0.1f, 89.0f)));
					light->data[5][0] = cos(toRad(clamp(_light.innerAngle, 0.0f, clamp(_light.outerAngle, 0.1f, 89.0f))));
				}
			});
			_rects.each([&](Entity _entity, const RectLightComponent& _light, const TransformComponent* _transform)
			{
				if (auto* light = append(_world, *_local, *_frame, _entity, _light, _transform, 2.0f))
				{
					light->data[3][3] = max(0.001f, _light.width) * 0.5f;
					light->data[4][3] = max(0.001f, _light.height) * 0.5f;
					if (_light.twoSided) { light->data[5][2] = 1.0f; }
				}
			});
			for (uint32_t ii = 0; ii < _local->count; ++ii)
			{
				if (_local->lights.getPtr(ii)->visible) { ++_stats->localLights; }
			}
			if (0 == _local->count) { return; }
			const uint16_t maxResolution = uint16_t(bgfx::getCaps()->limits.maxTextureSize / max(kLocalShadowColumns, kLocalShadowRows));
			const uint16_t resolution = min(clamp(_settings->resolution, uint16_t(64), uint16_t(2048)), maxResolution);
			if (_local->resolution != resolution) { resize(*_local, resolution); }

			const uint16_t limit = min(_settings->maxLights, uint16_t(kMaxLocalShadows));
			bool used[kMaxLocalShadows] = {};
			for (uint32_t ii = 0; ii < _local->count; ++ii)
			{
				LocalLight& light = *_local->lights.getPtr(ii);
				if (!light.castShadows) { continue; }
				for (uint16_t jj = 0; jj < limit; ++jj)
				{
					if (_local->slots[jj].entity == light.entity && _local->slots[jj].type == uint8_t(light.data[1][3]))
					{
						light.slot = int16_t(jj);
						used[jj] = true;
						break;
					}
				}
			}
			for (uint16_t ii = 0; ii < kMaxLocalShadows; ++ii)
			{
				if (!used[ii]) { _local->slots[ii] = {}; }
			}
			float types[32];
			for (float& type : types) { type = -1.0f; }
			bgfx::setViewRect(kRenderPassLocalShadow, 0, 0, resolution * kLocalShadowColumns, resolution * kLocalShadowRows);
			bgfx::setViewFrameBuffer(kRenderPassLocalShadow, _local->target);
			bgfx::setViewClear(kRenderPassLocalShadow, BGFX_CLEAR_DEPTH, 0, 1.0f);
			bgfx::setViewTransform(kRenderPassLocalShadow, NULL, NULL);
			uint16_t copies = 0;
			for (uint32_t ii = 0; ii < _local->count; ++ii)
			{
				LocalLight& light = *_local->lights.getPtr(ii);
				if (!light.castShadows) { continue; }
				if (light.slot < 0)
				{
					for (uint16_t jj = 0; jj < limit; ++jj)
					{
						if (!used[jj])
						{
							_local->slots[jj] = { light.entity, uint8_t(light.data[1][3]), false };
							light.slot = int16_t(jj);
							used[jj] = true;
							break;
						}
					}
				}
				if (light.slot < 0) { continue; }
				const Vec3 position = load<Vec3>(light.data[0]);
				const Vec3 direction = load<Vec3>(light.data[2]);
				const bool spot = 1.0f == light.data[1][3];

				float projection[16], view[6][16];
				float fov = 90.0f;
				if (spot) { fov = toDeg(acos(light.data[2][3])) * 2.0f; }
				mtxProj(projection, fov, 1.0f, light.data[5][1], light.data[0][3], bgfx::getCaps()->homogeneousDepth);
				if (spot)
				{
					mtxLookAt(view[0], position, add(position, direction), load<Vec3>(light.data[4]));
					mtxMul(light.projection, view[0], projection);
				}
				LocalShadowState& state = _local->slots[light.slot];
				if (light.isStatic && state.rendered) { continue; }
				types[light.slot] = 0.0f;
				if (spot) { types[light.slot] = 1.0f; }
				copies = max(copies, uint16_t(light.slot + 1));
				uint8_t faces = 6;
				if (spot) { faces = 1; }
				const Vec3 extent = { light.data[0][3], light.data[0][3], light.data[0][3] };
				const Aabb bounds = { sub(position, extent), add(position, extent) };
				MeshPass passes[6];
				for (uint8_t face = 0; face < faces; ++face)
				{
					if (!spot) { cubeView(view[face], position, face); }
					float viewProjection[16];
					mtxMul(viewProjection, view[face], projection);
					Plane* frustum = passes[face].planes;
					buildFrustumPlanes(frustum, viewProjection, bgfx::getCaps()->homogeneousDepth);
					MeshPass& pass = passes[face];
					pass.view = kRenderPassLocalShadow;
					pass.flags = MeshPass::DepthOnly | MeshPass::IndependentView;
					pass.state = BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
					pass.frustum = frustum;
					pass.sortView = view[face];
					pass.depthScale = 1.0f / light.data[0][3];
					pass.bounds = &bounds;
					pass.parameters = _local->shadowParameters;
					pass.numParameters = 3;
					pass.camera = _local->shadowCamera;
					memCopy(pass.cameraMatrix, viewProjection, sizeof(viewProjection));
					const uint16_t tile = uint16_t(light.slot * 6 + face);
					const uint16_t xx = tile % kLocalShadowColumns;
					const uint16_t yy = tile / kLocalShadowColumns;
					pass.values[8] = 1.0f / float(kLocalShadowColumns);
					pass.values[9] = 1.0f / float(kLocalShadowRows);
					pass.values[10] = (float(xx) * 2.0f + 1.0f) / float(kLocalShadowColumns) - 1.0f;
					pass.values[11] = 1.0f - (float(yy) * 2.0f + 1.0f) / float(kLocalShadowRows);
					pass.scissor[0] = xx * resolution;
					pass.scissor[1] = yy * resolution;
					pass.scissor[2] = pass.scissor[3] = resolution;
					memCopy(pass.values, light.data[0], 16);
					memCopy(pass.values + 4, light.data[2], 12);
					if (spot) { pass.values[7] = 1.0f; }
					memCopy(pass.programs, _local->shadows, sizeof(pass.programs));
					memCopy(pass.instancedPrograms, _local->instanced, sizeof(pass.instancedPrograms));
				}
				gatherMeshes(*_meshes, *_draws, passes, faces, *_graphics, _stats->localShadows);
				submitMeshes(*_draws, _gbuffer->material, passes, *_graphics, _stats->localShadows);
				state.rendered = light.isStatic;
			}
			if (copies > 0)
			{
				bgfx::touch(kRenderPassLocalShadow);
				float parameters[] = { float(resolution), float(kLocalShadowColumns), float(kLocalShadowRows), 0.0f };
				if (bgfx::getCaps()->originBottomLeft) { parameters[3] = 1.0f; }
				bgfx::setUniform(_local->copyParameters, parameters);
				bgfx::setUniform(_local->copyTypes, types, 8);
				bgfx::setTexture(2, _local->atlasSampler, _local->atlas);
				bgfx::setImage(0, _local->pointDepth, 0, kMaxLocalShadows * 6, 0, bgfx::Access::Write);
				bgfx::setImage(1, _local->spotDepth, 0, kMaxLocalShadows, 0, bgfx::Access::Write);
				bgfx::dispatch(kRenderPassLocalShadowCopy, _local->copy, (resolution + 7) / 8, (resolution + 7) / 8, copies * 6);
			}
		}

		static void update(ResT<RenderFrame> _frame, ResT<GBufferResource> _gbuffer, ResT<LightResource> _light,
			ResMutT<LocalLightResource> _local, ResT<LocalShadowResource> _settings)
		{
			if (!_frame->active || 0 == _local->count) { return; }
			bgfx::setViewRect(kRenderPassLocalLight, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassLocalLight, _light->frameBuffer);
			bgfx::setViewClear(kRenderPassLocalLight, BGFX_CLEAR_NONE);
			bgfx::setViewTransform(kRenderPassLocalLight, NULL, _frame->screenProjection);
			float viewProjection[16];
			mtxMul(viewProjection, _frame->view, _frame->projection);
			for (uint32_t ii = 0; ii < _local->count; ++ii)
			{
				LocalLight& light = *_local->lights.getPtr(ii);
				if (!light.visible) { continue; }
				light.data[6][0] = float(light.slot);
				light.data[6][1] = max(0.0f, _settings->depthBias);
				light.data[6][2] = max(0.0f, _settings->normalBias);
				light.data[6][3] = 1.0f / float(_local->resolution);
				// A projected range box bounds the fullscreen triangle; the shader clips the volume.
				float x0 = 1.0f, y0 = 1.0f, x1 = -1.0f, y1 = -1.0f;
				bool full = false;
				for (uint8_t corner = 0; corner < 8; ++corner)
				{
					float point[4] = { light.data[0][0], light.data[0][1], light.data[0][2], 1.0f };
					for (uint8_t axis = 0; axis < 3; ++axis)
					{
						float offset = light.data[0][3];
						if (0 == (corner & (1 << axis))) { offset = -offset; }
						point[axis] += offset;
					}
					float clip[4];
					vec4MulMtx(clip, point, viewProjection);
					if (clip[3] <= 0.0f) { full = true; break; }
					const float x = clip[0] / clip[3], y = -clip[1] / clip[3];
					x0 = min(x0, x); y0 = min(y0, y); x1 = max(x1, x); y1 = max(y1, y);
				}
				if (full) { x0 = y0 = -1.0f; x1 = y1 = 1.0f; }
				const uint16_t x = uint16_t(clamp((x0 * 0.5f + 0.5f) * _frame->width, 0.0f, float(_frame->width)));
				const uint16_t y = uint16_t(clamp((y0 * 0.5f + 0.5f) * _frame->height, 0.0f, float(_frame->height)));
				const uint16_t right = uint16_t(clamp(ceil((x1 * 0.5f + 0.5f) * _frame->width), 0.0f, float(_frame->width)));
				const uint16_t bottom = uint16_t(clamp(ceil((y1 * 0.5f + 0.5f) * _frame->height), 0.0f, float(_frame->height)));
				if (right <= x || bottom <= y) { continue; }
				bgfx::setScissor(x, y, right - x, bottom - y);
				LightSystem::bindScene(*_frame, *_gbuffer, *_light);
				bgfx::setTexture(6, _local->pointSampler, _local->pointDepth);
				bgfx::setTexture(7, _local->spotSampler, _local->spotDepth);
				bgfx::setUniform(_local->parameters, light.data, 8);
				bgfx::setUniform(_local->projection, light.projection);
				screenSpaceTriangle(_light->layout);
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ADD);
				bgfx::submit(kRenderPassLocalLight, _local->program);
			}
		}

		static void shutdown(ResMutT<LocalLightResource> _local)
		{
			destroyTargets(*_local);
			bgfx::destroy(_local->program);
			for (auto program : _local->shadows) { bgfx::destroy(program); }
			for (auto program : _local->instanced) { bgfx::destroy(program); }
			bgfx::destroy(_local->parameters);
			bgfx::destroy(_local->projection);
			bgfx::destroy(_local->shadowParameters);
			bgfx::destroy(_local->pointSampler);
			bgfx::destroy(_local->spotSampler);
			bgfx::destroy(_local->copy);
			bgfx::destroy(_local->shadowCamera);
			bgfx::destroy(_local->copyParameters);
			bgfx::destroy(_local->copyTypes);
			bgfx::destroy(_local->atlasSampler);
		}
	};
}

// Environment capture and image based lighting
namespace frost
{
	struct EnvironmentResource
	{
		bgfx::ProgramHandle capture = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle instanced = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle filter = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle sky = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle light = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle captureParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle captureLights = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle captureCamera = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle captureSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle filterParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle lightParameters = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle sourceSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle irradianceSampler = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle source = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle depth = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle irradiance = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle specular = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle target = BGFX_INVALID_HANDLE;
		float luminance[4] = {};
		uint16_t resolution = 0;
		uint8_t mips = 0;
		bool active = false;
		Entity entity;
		bool rendered = false;
	};

	struct EnvironmentSystem
	{
		static void startup(ResMutT<EnvironmentResource> _environment)
		{
			_environment->capture = createProgram("vs_capture", "fs_capture");
			_environment->instanced = createProgram("vs_capture_instanced", "fs_capture");
			_environment->filter = bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders,
				bgfx::getRendererType(), "cs_environment_filter"), true);
			FROST_ASSERT(bgfx::isValid(_environment->filter), "Failed to create environment filtering compute program.");
			_environment->sky = createProgram("vs_deferred_light", "fs_environment_sky");
			_environment->light = createProgram("vs_deferred_light", "fs_environment_light");
			_environment->captureParameters = bgfx::createUniform("u_capture", bgfx::UniformType::Vec4, 2);
			_environment->captureLights = bgfx::createUniform("u_captureLights", bgfx::UniformType::Vec4, 8);
			_environment->filterParameters = bgfx::createUniform("u_environmentFilter", bgfx::UniformType::Vec4);
			_environment->lightParameters = bgfx::createUniform("u_environmentLight", bgfx::UniformType::Vec4);
			_environment->sourceSampler = bgfx::createUniform("s_environment", bgfx::UniformType::Sampler);
			_environment->irradianceSampler = bgfx::createUniform("s_irradiance", bgfx::UniformType::Sampler);
			_environment->captureCamera = bgfx::createUniform("u_captureViewProjection", bgfx::UniformType::Mat4);
			_environment->captureSampler = bgfx::createUniform("s_environmentCapture", bgfx::UniformType::Sampler);
			bgfx::setViewName(kRenderPassEnvironment, "Environment");
			bgfx::setViewName(kRenderPassEnvironmentFilter, "Environment filter");
			bgfx::setViewMode(kRenderPassEnvironment, bgfx::ViewMode::Sequential);
		}

		static void destroyTargets(EnvironmentResource& _environment)
		{
			if (bgfx::isValid(_environment.target)) { bgfx::destroy(_environment.target); }
			_environment.target = BGFX_INVALID_HANDLE;
			const bgfx::TextureHandle textures[] = { _environment.source, _environment.depth, _environment.irradiance, _environment.specular };
			for (auto texture : textures)
			{
				if (bgfx::isValid(texture)) { bgfx::destroy(texture); }
			}
			_environment.source = _environment.depth = _environment.irradiance = _environment.specular = BGFX_INVALID_HANDLE;
			_environment.resolution = 0;
			_environment.rendered = false;
		}

		static void resize(EnvironmentResource& _environment, uint16_t _resolution)
		{
			destroyTargets(_environment);
			const uint64_t sampler = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP;
			_environment.source = bgfx::createTexture2D(_resolution * 3, _resolution * 2, false, 1,
				bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | sampler);
			_environment.depth = bgfx::createTexture2D(_resolution * 3, _resolution * 2, false, 1,
				bgfx::TextureFormat::D16, BGFX_TEXTURE_RT);
			_environment.irradiance = bgfx::createTextureCube(16, false, 1, bgfx::TextureFormat::RGBA16F,
				BGFX_TEXTURE_COMPUTE_WRITE | sampler);
			_environment.specular = bgfx::createTextureCube(_resolution, true, 1, bgfx::TextureFormat::RGBA16F,
				BGFX_TEXTURE_COMPUTE_WRITE | sampler);
			bgfx::setName(_environment.source, "Environment - capture atlas");
			bgfx::setName(_environment.irradiance, "Environment - diffuse irradiance");
			bgfx::setName(_environment.specular, "Environment - GGX roughness mip chain");
			bgfx::Attachment attachments[2];
			attachments[0].init(_environment.source);
			attachments[1].init(_environment.depth, bgfx::Access::Write, 0, 1, 0, 0);
			_environment.target = bgfx::createFrameBuffer(2, attachments, false);
			FROST_ASSERT(bgfx::isValid(_environment.target), "Failed to create environment capture target.");
			_environment.mips = 1;
			for (uint16_t size = _resolution; size > 1; size >>= 1) { ++_environment.mips; }
			_environment.resolution = _resolution;
		}

		static void update(World& _world, ResT<MeshRenderResource> _meshes,
			ResT<GBufferResource> _gbuffer, ResT<LightResource> _light, ResT<SkyResource> _sky,
			ResMutT<MeshDrawResource> _draws, ResT<GraphicsSettings> _graphics, ResMutT<GraphicsStatsResource> _stats,
			ResMutT<EnvironmentResource> _environment, QueryT<Entity, const SkyLightComponent&, OptionalT<const TransformComponent>> _skylights,
			QueryT<const SkyAtmosphereComponent&> _atmospheres, QueryT<const DirectionalLightComponent&> _directional)
		{
			_environment->active = false;
			_stats->environment = {};
			_skylights.single([&](Entity _entity, const SkyLightComponent& _component, const TransformComponent* _transform)
			{
				uint16_t resolution = 16;
				while (resolution < min(128u, _component.resolution)) { resolution *= 2; }
				if (_environment->resolution != resolution) { resize(*_environment, resolution); }
				store(_environment->luminance, mul(max(_component.color, { 0, 0, 0 }), max(0.0f, _component.intensity)));
				_environment->luminance[3] = float(_environment->mips - 1);
				if (_component.isStatic && _environment->rendered && _environment->entity == _entity)
				{
					_environment->active = true;
					return;
				}
				float transform[16];
				if (!getWorldTransform(transform, _world, _entity, _transform)) { return; }
				const Vec3 position = { transform[12], transform[13], transform[14] };
				const float threshold = max(0.0f, _component.skyDistanceThreshold);
				float farPlane = max(1000.0f, threshold * 2.0f);
				if (_meshes->count > 0)
				{
					const Aabb& bounds = _meshes->nodes.getPtr()->bounds;
					farPlane = max(farPlane, length(sub(getCenter(bounds), position)) + length(getExtents(bounds)) + 1.0f);
				}
				float projection[16];
				float screenProjection[16];
				mtxOrtho(screenProjection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
				mtxProj(projection, 90.0f, 1.0f, max(0.05f, threshold * 0.25f), farPlane, bgfx::getCaps()->homogeneousDepth);
				float lights[32] = {};
				uint8_t count = 0;
				_directional.each([&](const DirectionalLightComponent& _direction)
				{
					if (count < 4 && _direction.intensity > 0.0f && dot(_direction.direction, _direction.direction) > 0.0f)
					{
						store(lights + count * 4, normalize(_direction.direction));
						store(lights + 16 + count * 4, mul(max(_direction.color, { 0, 0, 0 }), _direction.intensity));
						++count;
					}
				});
				bgfx::setViewRect(kRenderPassEnvironment, 0, 0, resolution * 3, resolution * 2);
				bgfx::setViewFrameBuffer(kRenderPassEnvironment, _environment->target);
				bgfx::setViewClear(kRenderPassEnvironment, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0, 1.0f);
				bgfx::setViewTransform(kRenderPassEnvironment, NULL, screenProjection);
				bgfx::touch(kRenderPassEnvironment);
				float parameters[] = { 0.0f, 0.0f, float(resolution), 0.0f };
				if (bgfx::getCaps()->originBottomLeft) { parameters[3] = 1.0f; }
				_atmospheres.single([&](const SkyAtmosphereComponent& _atmosphere)
				{
					bindSky(*_sky, _atmosphere);
					bgfx::setUniform(_environment->filterParameters, parameters);
					screenSpaceTriangle(_light->layout);
					bgfx::setState(BGFX_STATE_WRITE_RGB);
					bgfx::submit(kRenderPassEnvironment, _environment->sky);
				});
				MeshPass passes[6];
				float views[6][16];
				for (uint8_t face = 0; face < 6; ++face)
				{
					float* view = views[face];
					float viewProjection[16];
					cubeView(view, position, face);
					mtxMul(viewProjection, view, projection);
					Plane* frustum = passes[face].planes;
					buildFrustumPlanes(frustum, viewProjection, bgfx::getCaps()->homogeneousDepth);
					MeshPass& pass = passes[face];
					pass.view = kRenderPassEnvironment;
					pass.programs[0] = _environment->capture;
					pass.instancedPrograms[0] = _environment->instanced;
					pass.state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
					pass.flags = MeshPass::IndependentView;
					pass.frustum = frustum;
					pass.sortView = view;
					pass.nearDepth = threshold;
					pass.depthScale = 1.0f / max(1.0f, farPlane - threshold);
					pass.parameters = _environment->captureParameters;
					pass.origin = position;
					pass.minimumDistance = threshold;
					store(pass.values, position);
					pass.values[3] = threshold;
					pass.values[4] = 1.0f / 3.0f;
					pass.values[5] = 0.5f;
					pass.values[6] = float(face % 3) * (2.0f / 3.0f) - 2.0f / 3.0f;
					pass.values[7] = 0.5f - float(face / 3);
					pass.camera = _environment->captureCamera;
					memCopy(pass.cameraMatrix, viewProjection, sizeof(viewProjection));
					pass.scissor[0] = (face % 3) * resolution;
					pass.scissor[1] = (face / 3) * resolution;
					pass.scissor[2] = pass.scissor[3] = resolution;
					pass.lights = _environment->captureLights;
					memCopy(pass.lightValues, lights, sizeof(lights));
				}
				gatherMeshes(*_meshes, *_draws, passes, 6, *_graphics, _stats->environment);
				submitMeshes(*_draws, _gbuffer->material, passes, *_graphics, _stats->environment);
				// Filter all six layers after this frame's capture.
				for (uint8_t filter = 0; filter <= _environment->mips; ++filter)
				{
					uint16_t size = 16;
					uint8_t mip = 0;
					bgfx::TextureHandle output = _environment->irradiance;
					parameters[1] = -1.0f;
					if (filter > 0)
					{
						mip = filter - 1;
						size = max(uint16_t(1), uint16_t(resolution >> mip));
						output = _environment->specular;
						parameters[1] = float(mip) / float(_environment->mips - 1);
					}
					parameters[0] = float(size);
					bgfx::setUniform(_environment->filterParameters, parameters);
					bgfx::setTexture(1, _environment->captureSampler, _environment->source);
					bgfx::setImage(0, output, 0, 6, mip, bgfx::Access::Write);
					bgfx::dispatch(kRenderPassEnvironmentFilter, _environment->filter, (size + 7) / 8, (size + 7) / 8, 6);
				}
				_environment->active = true;
				_environment->entity = _entity;
				_environment->rendered = _component.isStatic;
			});
			if (!_environment->active) { _environment->rendered = false; }
		}

		static void light(ResT<RenderFrame> _frame, ResT<GBufferResource> _gbuffer, ResT<LightResource> _light,
			ResT<EnvironmentResource> _environment)
		{
			if (!_frame->active || !_environment->active) { return; }
			LightSystem::bindScene(*_frame, *_gbuffer, *_light);
			bgfx::setTexture(6, _environment->sourceSampler, _environment->specular);
			bgfx::setTexture(7, _environment->irradianceSampler, _environment->irradiance);
			bgfx::setUniform(_environment->lightParameters, _environment->luminance);
			screenSpaceTriangle(_light->layout);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_BLEND_ADD);
			bgfx::submit(kRenderPassLight, _environment->light);
		}

		static void shutdown(ResMutT<EnvironmentResource> _environment)
		{
			destroyTargets(*_environment);
			const bgfx::ProgramHandle programs[] = { _environment->capture, _environment->instanced, _environment->filter, _environment->sky, _environment->light };
			for (auto program : programs)
			{
				bgfx::destroy(program);
			}
			const bgfx::UniformHandle uniforms[] = { _environment->captureParameters, _environment->captureLights, _environment->filterParameters,
				_environment->lightParameters, _environment->sourceSampler, _environment->irradianceSampler,
				_environment->captureCamera, _environment->captureSampler };
			for (auto uniform : uniforms)
			{
				bgfx::destroy(uniform);
			}
		}
	};
}


// Tone mapping
namespace frost
{
	struct ToneMapResource
	{
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_scene = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_outputParameters = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout layout;
	};

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
			_output->u_outputParameters = bgfx::createUniform("u_outputParameters", bgfx::UniformType::Vec4);
			_output->layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
		}

		static void update(ResT<RenderFrame> _frame, ResT<GraphicsDesc> _graphics,
			ResT<LightResource> _light, ResT<ToneMapResource> _output)
		{
			if (0 == _frame->width || 0 == _frame->height) { return; }
			// Presentation and UI still work without a selected camera.
			bgfx::setViewRect(kRenderPassToneMap, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassToneMap, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kRenderPassToneMap, BGFX_CLEAR_COLOR, UINT32_C(0x000000ff));
			bgfx::touch(kRenderPassToneMap);
			if (!_frame->active) { return; }

			const float parameters[] = { _frame->exposure, _graphics->preventBanding ? 1.0f : 0.0f, 0.0f, 0.0f };
			bgfx::setViewTransform(kRenderPassToneMap, NULL, _frame->screenProjection);
			bgfx::setUniform(_output->u_outputParameters, parameters);
			bgfx::setTexture(0, _output->s_scene, _light->texture);
			screenSpaceTriangle(_output->layout);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			bgfx::submit(kRenderPassToneMap, _output->program);
		}

		static void shutdown(ResMutT<ToneMapResource> _output)
		{
			bgfx::destroy(_output->program);
			bgfx::destroy(_output->s_scene);
			bgfx::destroy(_output->u_outputParameters);
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

namespace frost
{
	void GraphicsPlugin::build(App& _app)
	{
		// Resources
		_app.getWorld().initResource<ActiveCameraResource>();
		_app.insertResource<RenderFrame>();
		_app.insertResource<GBufferResource>();
		_app.insertResource<LightResource>();
		_app.getWorld().initResource<SunShadowResource>();
		_app.insertResource<SunShadowTargets>();
		_app.getWorld().initResource<GraphicsSettings>();
		_app.insertResource<GraphicsStatsResource>();
		_app.insertResource<MeshRenderResource>();
		_app.insertResource<MeshDrawResource>();
		_app.insertResource<SkyResource>();
		_app.insertResource<EnvironmentResource>();
		_app.insertResource<LocalLightResource>();
		_app.getWorld().initResource<LocalShadowResource>();
		_app.insertResource<ToneMapResource>();

		// Initialize the backend before creating GPU resources.
		SystemId startup = _app.addSystems(EcsStage::Startup, RendererSystem::startup).id();
		startup = _app.addSystems(EcsStage::Startup, TextureSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, MaterialSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, MeshSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, MeshRenderSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, GBufferSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, LightSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, SkySystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, SunShadowSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, EnvironmentSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, LocalLightSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, ToneMapSystem::startup).after(startup).id();
		_app.addSystems(EcsStage::Startup, UiSystem::startup).after(startup);

		_app.addSystems(EcsStage::PreUpdate, SkySystem::prepare);

		// Prepare shared frame data, then submit passes in dependency order.
		SystemId update = _app.addSystems(EcsStage::PostUpdate, RendererSystem::update).id();
		update = _app.addSystems(EcsStage::PostUpdate, RenderFrameSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, MeshRenderSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, EnvironmentSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, GBufferSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, SunShadowSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, LocalLightSystem::shadows).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, LightSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, EnvironmentSystem::light).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, LocalLightSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, SkySystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, ToneMapSystem::update).after(update).id();
		_app.addSystems(EcsStage::PostUpdate, UiSystem::update).after(update);
		_app.addSystems(EcsStage::Last, RendererSystem::endFrame);

		// Release GPU resources before stopping the backend.
		SystemId shutdown = _app.addSystems(EcsStage::Shutdown, UiSystem::shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, ToneMapSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, LocalLightSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, EnvironmentSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, SunShadowSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, SkySystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, LightSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, GBufferSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, MeshSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, MaterialSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, TextureSystem::shutdown).after(shutdown).id();
		_app.addSystems(EcsStage::Shutdown, RendererSystem::shutdown).after(shutdown);
	}

} // namespace frost
