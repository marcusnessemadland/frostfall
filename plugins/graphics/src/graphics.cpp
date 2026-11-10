/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#include <graphics.h>
#include <device.h>

#include <frost/math.h>
#include <frost/timer.h>

#include "graphics_p.h"

#include <bgfx/embedded_shader.h>
#include "vs_deferred_geom.bin.h"
#include "fs_deferred_geom.bin.h"

// TEMP asset stuff, so we can work on rendering without asset system
namespace frost
{
	struct PosNormalTangentTexcoordVertex
	{
		float m_x;
		float m_y;
		float m_z;
		uint32_t m_normal;
		uint32_t m_tangent;
		int16_t m_u;
		int16_t m_v;

		static void init()
		{
			ms_layout
				.begin()
				.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Normal,    4, bgfx::AttribType::Uint8, true, true)
				.add(bgfx::Attrib::Tangent,   4, bgfx::AttribType::Uint8, true, true)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true)
				.end();
		}

		static bgfx::VertexLayout ms_layout;
	};

	bgfx::VertexLayout PosNormalTangentTexcoordVertex::ms_layout;

	static PosNormalTangentTexcoordVertex s_cubeVertices[24] =
	{
		{-1.0f,  1.0f,  1.0f, encodeNormalRgba8( 0.0f,  0.0f,  1.0f), 0,      0,      0 },
		{ 1.0f,  1.0f,  1.0f, encodeNormalRgba8( 0.0f,  0.0f,  1.0f), 0, 0x7fff,      0 },
		{-1.0f, -1.0f,  1.0f, encodeNormalRgba8( 0.0f,  0.0f,  1.0f), 0,      0, 0x7fff },
		{ 1.0f, -1.0f,  1.0f, encodeNormalRgba8( 0.0f,  0.0f,  1.0f), 0, 0x7fff, 0x7fff },
		{-1.0f,  1.0f, -1.0f, encodeNormalRgba8( 0.0f,  0.0f, -1.0f), 0,      0,      0 },
		{ 1.0f,  1.0f, -1.0f, encodeNormalRgba8( 0.0f,  0.0f, -1.0f), 0, 0x7fff,      0 },
		{-1.0f, -1.0f, -1.0f, encodeNormalRgba8( 0.0f,  0.0f, -1.0f), 0,      0, 0x7fff },
		{ 1.0f, -1.0f, -1.0f, encodeNormalRgba8( 0.0f,  0.0f, -1.0f), 0, 0x7fff, 0x7fff },
		{-1.0f,  1.0f,  1.0f, encodeNormalRgba8( 0.0f,  1.0f,  0.0f), 0,      0,      0 },
		{ 1.0f,  1.0f,  1.0f, encodeNormalRgba8( 0.0f,  1.0f,  0.0f), 0, 0x7fff,      0 },
		{-1.0f,  1.0f, -1.0f, encodeNormalRgba8( 0.0f,  1.0f,  0.0f), 0,      0, 0x7fff },
		{ 1.0f,  1.0f, -1.0f, encodeNormalRgba8( 0.0f,  1.0f,  0.0f), 0, 0x7fff, 0x7fff },
		{-1.0f, -1.0f,  1.0f, encodeNormalRgba8( 0.0f, -1.0f,  0.0f), 0,      0,      0 },
		{ 1.0f, -1.0f,  1.0f, encodeNormalRgba8( 0.0f, -1.0f,  0.0f), 0, 0x7fff,      0 },
		{-1.0f, -1.0f, -1.0f, encodeNormalRgba8( 0.0f, -1.0f,  0.0f), 0,      0, 0x7fff },
		{ 1.0f, -1.0f, -1.0f, encodeNormalRgba8( 0.0f, -1.0f,  0.0f), 0, 0x7fff, 0x7fff },
		{ 1.0f, -1.0f,  1.0f, encodeNormalRgba8( 1.0f,  0.0f,  0.0f), 0,      0,      0 },
		{ 1.0f,  1.0f,  1.0f, encodeNormalRgba8( 1.0f,  0.0f,  0.0f), 0, 0x7fff,      0 },
		{ 1.0f, -1.0f, -1.0f, encodeNormalRgba8( 1.0f,  0.0f,  0.0f), 0,      0, 0x7fff },
		{ 1.0f,  1.0f, -1.0f, encodeNormalRgba8( 1.0f,  0.0f,  0.0f), 0, 0x7fff, 0x7fff },
		{-1.0f, -1.0f,  1.0f, encodeNormalRgba8(-1.0f,  0.0f,  0.0f), 0,      0,      0 },
		{-1.0f,  1.0f,  1.0f, encodeNormalRgba8(-1.0f,  0.0f,  0.0f), 0, 0x7fff,      0 },
		{-1.0f, -1.0f, -1.0f, encodeNormalRgba8(-1.0f,  0.0f,  0.0f), 0,      0, 0x7fff },
		{-1.0f,  1.0f, -1.0f, encodeNormalRgba8(-1.0f,  0.0f,  0.0f), 0, 0x7fff, 0x7fff },
	};

	static const uint16_t s_cubeIndices[36] =
	{
		 0,  2,  1,
		 1,  2,  3,
		 4,  5,  6,
		 5,  7,  6,

		 8, 10,  9,
		 9, 10, 11,
		12, 13, 14,
		13, 15, 14,

		16, 18, 17,
		17, 18, 19,
		20, 21, 22,
		21, 23, 22,
	};

	struct TempAssetsResource
	{
		bgfx::VertexBufferHandle cubeVertexBuffer;
		bgfx::IndexBufferHandle cubeIndexBuffer;
	};

	struct TempAssetsSystem
	{
		static void startup(ResMutT<TempAssetsResource> _assets)
		{
			PosNormalTangentTexcoordVertex::init();

			calcTangents(s_cubeVertices
				, BX_COUNTOF(s_cubeVertices)
				, PosNormalTangentTexcoordVertex::ms_layout
				, s_cubeIndices
				, BX_COUNTOF(s_cubeIndices)
				);

			_assets->cubeVertexBuffer = bgfx::createVertexBuffer(
					bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices) )
					, PosNormalTangentTexcoordVertex::ms_layout
					);
			_assets->cubeIndexBuffer = bgfx::createIndexBuffer(
				bgfx::makeRef(s_cubeIndices, sizeof(s_cubeIndices) ) );
		}

		static void shutdown(ResT<TempAssetsResource> _assets)
		{
			bgfx::destroy(_assets->cubeVertexBuffer);
			bgfx::destroy(_assets->cubeIndexBuffer);
		}
	};

} // namespace frost

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
		bgfx::ProgramHandle program;
		bgfx::UniformHandle s_texColor;
		bgfx::UniformHandle s_texNormal;

		bgfx::FrameBufferHandle frameBuffer;
		bgfx::TextureHandle textures[3];
		uint32_t width, height;
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

			_gbuffer->s_texColor  = bgfx::createUniform("s_texColor",  bgfx::UniformType::Sampler);
			_gbuffer->s_texNormal = bgfx::createUniform("s_texNormal", bgfx::UniformType::Sampler);

			// Invalidate framebuffer so it gets created
			_gbuffer->frameBuffer = BGFX_INVALID_HANDLE;
		}

		static void update(ResT<Display> _display, ResT<TempAssetsResource> _assets, ResMutT<GBufferResource> _gbuffer)
		{
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
				_gbuffer->frameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(gbufferAt), gbufferAt, true);
			}

			// Setup views
			float vp[16];
			float invMvp[16];
			{
				bgfx::setViewClear(kRenderPassGeometry
				, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
				, 1.0f
				, 0
				, 1
				, 0
				);
				bgfx::setViewRect(kRenderPassGeometry, 0, 0, uint16_t(_gbuffer->width), uint16_t(_gbuffer->height));
				//bgfx::setViewFrameBuffer(kRenderPassGeometry, _gbuffer->frameBuffer);
				bgfx::setViewFrameBuffer(kRenderPassGeometry, BGFX_INVALID_HANDLE); // test presenting

				const bgfx::Caps* caps = bgfx::getCaps();

				float view[16];
				bx::mtxLookAt(view, {0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});

				float proj[16];
				bx::mtxProj(proj, 60.0f, float(_gbuffer->width)/float(_gbuffer->height), 0.1f, 100.0f, caps->homogeneousDepth);

				bgfx::setViewTransform(kRenderPassGeometry, view, proj);
			}

			// Submit geometry
			const float time = frost::toSeconds<float>(frost::getTicksSinceStartup());
			float mtx[16];
			frost::mtxRotateXY(mtx, time * 0.5f, time);

			bgfx::setTransform(mtx);

			bgfx::setVertexBuffer(0, _assets->cubeVertexBuffer);
			bgfx::setIndexBuffer(_assets->cubeIndexBuffer);

			// TODO: Fix once we have asset system
			//bgfx::setTexture(0, _gbuffer->s_texColor,  _assets->textureColor);
			//bgfx::setTexture(1, _gbuffer->s_texNormal, _assets->textureNormal);

			bgfx::setState(0
				| BGFX_STATE_WRITE_RGB
				| BGFX_STATE_WRITE_A
				| BGFX_STATE_WRITE_Z
				| BGFX_STATE_DEPTH_TEST_LESS
				| BGFX_STATE_MSAA
			);

			bgfx::submit(kRenderPassGeometry, _gbuffer->program);
		}

		static void shutdown(ResMutT<GBufferResource> _gbuffer)
		{
			bgfx::destroy(_gbuffer->s_texColor);
			bgfx::destroy(_gbuffer->s_texNormal);

			// Shaders are destroyed when program is destroyed
			bgfx::destroy(_gbuffer->program);

			// Textures are destroyed when framebuffer is destroyed
			bgfx::destroy(_gbuffer->frameBuffer);
		}
	};

} // namespace frost

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
		// Resources
		_app.insertResource<GBufferResource>();
		_app.insertResource<TempAssetsResource>();

		// Systems
		_app.addSystems(EcsStage::Startup, BgfxSystem::startup);

		_app.addSystems(EcsStage::Startup, TempAssetsSystem::startup);
		_app.addSystems(EcsStage::Startup, GBufferSystem::startup);

		_app.addSystems(EcsStage::PreUpdate, BgfxSystem::preUpdate);

		_app.addSystems(EcsStage::Update, GBufferSystem::update);

		_app.addSystems(EcsStage::PostUpdate, BgfxSystem::postUpdate); // Swaps buffers

		_app.addSystems(EcsStage::Shutdown, GBufferSystem::shutdown);
		_app.addSystems(EcsStage::Shutdown, TempAssetsSystem::shutdown);

		_app.addSystems(EcsStage::Shutdown, BgfxSystem::shutdown);
	}

} // namespace frost
