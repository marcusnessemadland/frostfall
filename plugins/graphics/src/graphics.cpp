/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#include <graphics.h>
#include <device.h>
#include <frost/os.h>

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
		static void startup(ResT<GraphicsResource> _graphics, ResT<DisplayResource> _display)
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
			if (!bgfx::init(init))
			{
				debugPrintf("Failed to initialize bgfx.\n");
				frost::exit(kExitFailure, false);
			}
			bgfx::setDebug(_graphics->gpuProfiling ? BGFX_DEBUG_PROFILER : BGFX_DEBUG_NONE);
		}

		static void update(ResT<DisplayResource> _display, ResT<GraphicsResource> _graphics)
		{
			bgfx::setDebug(_graphics->gpuProfiling ? BGFX_DEBUG_PROFILER : BGFX_DEBUG_NONE);
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
				swapChain.flags = BGFX_SWAP_CHAIN_SRGB_BACKBUFFER;
				swapChain.formatDepthStencil = bgfx::TextureFormat::Count;
				bgfx::reset(BGFX_RESET_NONE, &swapChain);
			}
		}

		static void endFrame()
		{
			bgfx::frame();
		}

		static void shutdown()
		{
			bgfx::shutdown();
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
		_app.insertResource<SkyResource>();
		_app.insertResource<ToneMapResource>();
		_app.getWorld().initResource<GraphicsStatsResource>();

		// Initialize the backend before creating GPU resources.
		SystemId startup = _app.addSystems(EcsStage::Startup, RendererSystem::startup).id();
		startup = _app.addSystems(EcsStage::Startup, TextureSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, MaterialSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, MeshSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, GBufferSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, LightSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, SkySystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, ToneMapSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, DebugViewSystem::startup).after(startup).id();
		_app.addSystems(EcsStage::Startup, UiSystem::startup).after(startup);

		_app.addSystems(EcsStage::PreUpdate, GraphicsStatsSystem::update);
		_app.addSystems(EcsStage::PreUpdate, SkySystem::prepare);

		// Prepare shared frame data, then submit passes in dependency order.
		SystemId update = _app.addSystems(EcsStage::PostUpdate, RendererSystem::update).id();
		update = _app.addSystems(EcsStage::PostUpdate, RenderFrameSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, GBufferSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, LightSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, SkySystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, ToneMapSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, DebugViewSystem::update).after(update).id();
		_app.addSystems(EcsStage::PostUpdate, UiSystem::update).after(update);
		_app.addSystems(EcsStage::Last, RendererSystem::endFrame);

		// Release GPU resources before stopping the backend.
		SystemId shutdown = _app.addSystems(EcsStage::Shutdown, UiSystem::shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, ToneMapSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, SkySystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, LightSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, GBufferSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, MeshSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, MaterialSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, TextureSystem::shutdown).after(shutdown).id();
		_app.addSystems(EcsStage::Shutdown, RendererSystem::shutdown).after(shutdown);
	}

} // namespace frost
