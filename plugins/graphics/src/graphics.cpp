/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#include <graphics.h>
#include <device.h>

#include "graphics_p.h"

namespace frost
{
	static bgfx::NativeWindowHandleType::Enum getBgfxWindowHandleType(NativeWindowHandleType::Enum _type)
	{
		return _type == NativeWindowHandleType::Wayland
			? bgfx::NativeWindowHandleType::Wayland : bgfx::NativeWindowHandleType::Default;
	}

	struct BgfxSystem
	{
		static void startup(World& _world, ResT<GraphicsResource> _graphics, ResT<DisplayResource> _display)
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
				debugPrintf("Failed to initialize bgfx context.\n");
				return;
			}
			MeshSystem::startup(_world, _graphics);
		}

		static void preUpdate(ResT<DisplayResource> _display)
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
		}

		static void postUpdate()
		{
			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();
		}

		static void shutdown()
		{
			MeshSystem::shutdown();
			bgfx::shutdown();
		}
	};

} // namespace frost

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
		if (NULL != mesh && UINT32_MAX != mesh->refCount)
		{
			++mesh->refCount;
		}
	}

	void release(MeshHandle _handle)
	{
		if (!isValid(_handle) )
		{
			return;
		}
		MeshRef* mesh = getMeshRef(_handle);
		FROST_ASSERT(NULL != mesh && 0 != mesh->refCount, "Cannot release an invalid mesh reference.");
		if (NULL != mesh && 0 != mesh->refCount && 0 == --mesh->refCount)
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

		// Systems
		_app.addSystems(EcsStage::Startup, BgfxSystem::startup);

		_app.addSystems(EcsStage::Startup, GBufferSystem::startup);
		_app.addSystems(EcsStage::Startup, LightSystem::startup);
		_app.addSystems(EcsStage::Startup, SkySystem::startup);
		_app.addSystems(EcsStage::Startup, CombineSystem::startup);

		_app.addSystems(EcsStage::PreUpdate, BgfxSystem::preUpdate);

		_app.addSystems(EcsStage::PreUpdate, SkySystem::prepare);

		_app.addSystems(EcsStage::PostUpdate, GBufferSystem::update);
		_app.addSystems(EcsStage::PostUpdate, LightSystem::update);
		_app.addSystems(EcsStage::PostUpdate, SkySystem::update);
		_app.addSystems(EcsStage::PostUpdate, CombineSystem::update);

		_app.addSystems(EcsStage::Last, BgfxSystem::postUpdate); // Swaps buffers

		_app.addSystems(EcsStage::Shutdown, CombineSystem::shutdown);
		_app.addSystems(EcsStage::Shutdown, SkySystem::shutdown);
		_app.addSystems(EcsStage::Shutdown, LightSystem::shutdown);
		_app.addSystems(EcsStage::Shutdown, GBufferSystem::shutdown);

		_app.addSystems(EcsStage::Shutdown, BgfxSystem::shutdown);
	}

} // namespace frost
