/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#include <graphics.h>
#include <device.h>
#include <frost/os.h>

#include "graphics_p.h"

namespace frost
{
	static bgfx::NativeWindowHandleType::Enum getBgfxWindowHandleType(NativeWindowHandleType::Enum _type)
	{
		return _type == NativeWindowHandleType::Wayland
			? bgfx::NativeWindowHandleType::Wayland : bgfx::NativeWindowHandleType::Default;
	}

	static bgfx::RendererType::Enum getBgfxRendererType(RendererType::Enum _type)
	{
		return static_cast<bgfx::RendererType::Enum>(_type);
	}

	struct RendererSystem
	{
		static void startup(World& _world, ResT<GraphicsResource> _graphics, ResT<DisplayResource> _display,
			ResMutT<GBufferResource> _gbuffer, ResMutT<LightResource> _light,
			ResMutT<SkyResource> _sky, ResMutT<CombineResource> _combine)
		{
			bgfx::Init init;
			init.type = getBgfxRendererType(_graphics->type);
			init.vendorId = BGFX_PCI_ID_NONE; // Autoselect adapter.
			init.swapChain.nwh = _display->nativeWindow;
			init.swapChain.ndt = _display->nativeDisplay;
			init.platformData.type = getBgfxWindowHandleType(_display->nativeWindowHandleType);
			init.swapChain.width = _display->width;
			init.swapChain.height = _display->height;
			init.reset = BGFX_RESET_NONE; // TODO: Expose vsync in GraphicsResource.
			if (!bgfx::init(init))
			{
				debugPrintf("Failed to initialize bgfx.\n");
				frost::exit(kExitFailure, false);
			}
			MeshSystem::startup(_world, _graphics);
			GBufferSystem::startup(*_gbuffer);
			LightSystem::startup(*_light);
			SkySystem::startup(*_sky);
			CombineSystem::startup(*_combine);
		}

		static void update(ResT<DisplayResource> _display, ResT<ActiveCameraResource> _activeCamera,
			QueryT<const CameraComponent&> _cameras,
			QueryT<const MeshComponent&, OptionalT<const TransformComponent>> _meshes,
			QueryT<const DirectionalLightComponent&> _lights, QueryT<const SkyComponent&> _skies,
			ResMutT<GBufferResource> _gbuffer, ResMutT<LightResource> _light,
			ResT<SkyResource> _sky, ResT<CombineResource> _combine)
		{
			if (0 == _display->width || 0 == _display->height)
			{
				return;
			}
			const bgfx::Stats* stats = bgfx::getStats();
			if (stats->width != _display->width || stats->height != _display->height)
			{
				bgfx::SwapChain swapChain;
				swapChain.width = _display->width;
				swapChain.height = _display->height;
				bgfx::reset(BGFX_RESET_NONE, &swapChain);
			}

			// Clear presentation even when there is no selected camera.
			bgfx::setViewRect(kRenderPassCombine, 0, 0, uint16_t(_display->width), uint16_t(_display->height));
			bgfx::setViewFrameBuffer(kRenderPassCombine, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kRenderPassCombine, BGFX_CLEAR_COLOR, UINT32_C(0x000000ff));
			bgfx::touch(kRenderPassCombine);

			RenderFrame frame;
			frame.width = uint16_t(_display->width);
			frame.height = uint16_t(_display->height);
			if (!getCameraMatrices(*_activeCamera, _cameras, frame.width, frame.height, frame.view, frame.projection))
			{
				return;
			}
			mtxOrtho(frame.screenProjection, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f,
				0.0f, bgfx::getCaps()->homogeneousDepth);
			if (_gbuffer->width != frame.width || _gbuffer->height != frame.height)
			{
				GBufferSystem::resize(*_gbuffer, frame.width, frame.height);
				LightSystem::resize(*_light, *_gbuffer);
			}

			GBufferSystem::update(frame, _meshes, *_gbuffer);
			LightSystem::update(frame, *_gbuffer, _lights, *_light);
			SkySystem::update(frame, *_light, _skies, *_sky);
			CombineSystem::update(frame, *_gbuffer, *_light, _skies, *_combine);
		}

		static void endFrame()
		{
			bgfx::frame();
		}

		static void shutdown(ResMutT<GBufferResource> _gbuffer, ResMutT<LightResource> _light,
			ResMutT<SkyResource> _sky, ResMutT<CombineResource> _combine)
		{
			CombineSystem::shutdown(*_combine);
			SkySystem::shutdown(*_sky);
			LightSystem::shutdown(*_light);
			GBufferSystem::shutdown(*_gbuffer);
			MeshSystem::shutdown();
			bgfx::shutdown();
		}
	};

} // namespace frost

// TODO: should this be here? cant we just have this in graphics_p.h?
namespace frost
{
	MeshHandle createMesh(const Memory* _mem, Error* _err)
	{
		FROST_ERROR_SCOPE(_err);
		const MeshHandle handle = meshCreate(_mem, _err);
		release(_mem);
		return handle;
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

} // namespace frost

namespace frost
{
	void GraphicsPlugin::build(App& _app)
	{
		// Resources
		_app.getWorld().initResource<ActiveCameraResource>();
		_app.insertResource<GBufferResource>();
		_app.insertResource<LightResource>();
		_app.insertResource<SkyResource>();
		_app.insertResource<CombineResource>();

		// Graphics owns its pass order; scene systems run before this renderer entry point.
		_app.addSystems(EcsStage::Startup, RendererSystem::startup);
		_app.addSystems(EcsStage::PreUpdate, SkySystem::prepare);
		_app.addSystems(EcsStage::PostUpdate, RendererSystem::update);
		_app.addSystems(EcsStage::Last, RendererSystem::endFrame);
		_app.addSystems(EcsStage::Shutdown, RendererSystem::shutdown);
	}

} // namespace frost
