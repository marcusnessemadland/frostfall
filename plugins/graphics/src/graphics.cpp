/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#include <graphics.h>
#include <device.h>

#include <bgfx/bgfx.h>

namespace frost
{
	static bgfx::NativeWindowHandleType::Enum getBgfxWindowHandleType(NativeWindowHandleType::Enum _type)
	{
		return _type == NativeWindowHandleType::Wayland
			? bgfx::NativeWindowHandleType::Wayland : bgfx::NativeWindowHandleType::Default;
	}

	struct BgfxSystem
	{
		static void startup(ResT<GraphicsResource> _graphics, ResT<Display> _display)
		{
			bgfx::Init init;
			init.type = bgfx::RendererType::Count; // auto
			init.vendorId = BGFX_PCI_ID_NONE; // auto
			init.swapChain.nwh = _display->nativeWindow;
			init.swapChain.ndt = _display->nativeDisplay;
			init.platformData.type = getBgfxWindowHandleType(_display->nativeWindowHandleType);
			init.swapChain.width  = _graphics->width;
			init.swapChain.height = _graphics->height;
			init.reset  = BGFX_RESET_NONE;
			if (!bgfx::init(init))
			{
				debugPrintf("Failed to initialize bgfx context: %s\n");
				return;
			}
		}

		static void preUpdate(ResT<Display> _display)
		{
			// Reset if needed
			const bgfx::Stats* stats = bgfx::getStats();
			if (stats->width != _display->width || stats->height != _display->height)
			{
				bgfx::SwapChain swapChain;
				swapChain.width  = _display->width;
				swapChain.height = _display->height;
				bgfx::reset(BGFX_RESET_NONE, &swapChain);
			}

			// Set view
			bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0x303030ff
			, 1.0f
			, 0
			);
			bgfx::setViewRect(0, 0, 0, uint16_t(_display->width), uint16_t(_display->height) );

			// This dummy draw call is here to make sure that view 0 is cleared
			// if no other draw calls are submitted to view 0.
			bgfx::touch(0);
		}

		static void postUpdate()
		{
			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
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
		_app.addSystems(EcsStage::Startup, BgfxSystem::startup);
		_app.addSystems(EcsStage::PreUpdate, BgfxSystem::preUpdate);
		_app.addSystems(EcsStage::PostUpdate, BgfxSystem::postUpdate); // Swaps buffers
		_app.addSystems(EcsStage::Shutdown, BgfxSystem::shutdown);
	}

} // namespace frost
