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
		}

		static void update(ResT<DisplayResource> _display, ResT<GraphicsDesc> _graphics)
		{
			bgfx::setDebug(_graphics->gpuProfiling ? BGFX_DEBUG_PROFILER : BGFX_DEBUG_NONE);

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
#include "fs_deferred_geom.bin.h"
#include "vs_deferred_light.bin.h"
#include "fs_deferred_light.bin.h"
#include "vs_sky.bin.h"
#include "fs_sky.bin.h"
#include "vs_deferred_combine.bin.h"
#include "fs_deferred_combine.bin.h"
#include "vs_ui.bin.h"
#include "fs_ui.bin.h"
#include "fs_environment.bin.h"
#include "fs_environment_capture.bin.h"
#include "vs_shadow.bin.h"
#include "vs_shadow_instanced.bin.h"
#include "fs_shadow.bin.h"
#include "fs_shadow_masked.bin.h"

#include <bgfx/embedded_shader.h>

#include <charset.h> // bgfx's built-in bitmap font, retained with its upstream license.

namespace frost
{
	// TODO: Unsure about these three constexprs
	constexpr uint16_t kEnvironmentSize = 128;
	constexpr uint8_t kEnvironmentMips = 8;
	constexpr uint16_t kEnvironmentViews = 6 + 6 * kEnvironmentMips + 6 + 1;

	constexpr bgfx::ViewId kRenderPassEnvironment = 0;
	constexpr bgfx::ViewId kRenderPassGeometry = kRenderPassEnvironment + kEnvironmentViews;
	constexpr bgfx::ViewId kRenderPassShadow = kRenderPassGeometry + 1;
	constexpr bgfx::ViewId kRenderPassLight = kRenderPassShadow + 1;
	constexpr bgfx::ViewId kRenderPassSky = kRenderPassLight + 1;
	constexpr bgfx::ViewId kRenderPassToneMap = kRenderPassSky + 1;
	constexpr bgfx::ViewId kRenderPassDebug = kRenderPassToneMap + 1;
	constexpr bgfx::ViewId kRenderPassUi = kRenderPassDebug + 1;

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
		BGFX_EMBEDDED_SHADER(fs_environment),
		BGFX_EMBEDDED_SHADER(fs_environment_capture),
		BGFX_EMBEDDED_SHADER(vs_shadow),
		BGFX_EMBEDDED_SHADER(vs_shadow_instanced),
		BGFX_EMBEDDED_SHADER(fs_shadow),
		BGFX_EMBEDDED_SHADER(fs_shadow_masked),
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

		static void bindMaterial(const MaterialCreate& _material, const GBufferResource& _gbuffer, bool _shadow = false)
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
				bgfx::setTexture(ii, _gbuffer.samplers[ii], NULL != texture ? texture->texture : fallback, sampler);
			}
			const float parameters[] = { _material.metallic, _material.roughness, _material.normalScale, _material.occlusionStrength };
			const float emissive[] = { _material.emissive.x * _material.emissiveStrength,
				_material.emissive.y * _material.emissiveStrength, _material.emissive.z * _material.emissiveStrength, _material.alphaCutoff };
			const float flags[] = { 0 != (_material.flags & MaterialFlags::Masked) ? 1.0f : 0.0f,
				0 != (_material.flags & MaterialFlags::DoubleSided) ? 1.0f : 0.0f, 0.0f, 0.0f };
			bgfx::setUniform(_gbuffer.u_materialBaseColor, _material.baseColor);
			bgfx::setUniform(_gbuffer.u_materialEmissive, emissive);
			if (!_shadow)
			{
				bgfx::setUniform(_gbuffer.u_materialParameters, parameters);
				bgfx::setUniform(_gbuffer.u_materialFlags, flags);
			}
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
				if (!getMeshTransform(mtx, _transform)) { return; }

				for (uint32_t ii = 0; ii < mesh->numGroups; ++ii)
				{
					const MeshGroup& group = mesh->groups[ii];
					const MaterialCreate& desc = getMeshMaterial(_mesh, *mesh, group.materialSlot);
					const uint64_t state = meshState(desc, _transform) | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
						| BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
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

// Environment and sun shadow resources
namespace frost
{
	struct EnvironmentLightingResource
	{
		bgfx::ProgramHandle captureProgram = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle source = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_source = BGFX_INVALID_HANDLE;
		float capturePosition[4] = {};
		bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle specular = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle diffuse = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle brdf = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle targets[kEnvironmentViews];
		bgfx::UniformHandle samplers[3];
		bgfx::UniformHandle u_environment = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle u_environmentBake = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout layout;
		float skyState[23] = {};
		double elapsed = 0.0;
		bool initialized = false;
		bool active = false;
	};

	static void bindEnvironment(const EnvironmentLightingResource& _environment,
		const EnvironmentResource& _settings)
	{
		bgfx::setTexture(5, _environment.samplers[0], _environment.specular);
		bgfx::setTexture(6, _environment.samplers[1], _environment.diffuse);
		bgfx::setTexture(7, _environment.samplers[2], _environment.brdf);
		const float parameters[] = { _environment.active
			? max(0.0f, _settings.intensity) : 0.0f,
			float(kEnvironmentMips - 1), 0.0f, 0.0f };
		bgfx::setUniform(_environment.u_environment, parameters);
	}

	struct ShadowDraw
	{
		const MeshGroup* group;
		const MaterialCreate* material; // Only masked casters need material bindings.
		uint64_t state;
		uint8_t cascade;
		float modelProjection[16];
	};

	struct ShadowDrawResource
	{
		ShadowDrawResource() = default;
		ShadowDrawResource(ShadowDrawResource&& _other) : count(_other.count)
		{
			draws = frost::move(_other.draws);
		}
		DynamicStorageT<ShadowDraw> draws;
		uint32_t count = 0;
	};

	static int32_t compareShadowDraws(const void* _lhs, const void* _rhs)
	{
		const ShadowDraw& a = *(const ShadowDraw*)_lhs;
		const ShadowDraw& b = *(const ShadowDraw*)_rhs;
		if (a.cascade != b.cascade) { return a.cascade < b.cascade ? -1 : 1; }
		if (a.group != b.group) { return uintptr_t(a.group) < uintptr_t(b.group) ? -1 : 1; }
		if (a.state != b.state) { return a.state < b.state ? -1 : 1; }
		if (a.material != b.material) { return uintptr_t(a.material) < uintptr_t(b.material) ? -1 : 1; }
		return 0;
	}

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
	};

	static void bindShadows(const SunShadowTargets& _shadows, const SunShadowResource& _settings,
		const RenderFrame& _frame, bool _enabled)
	{
		bgfx::setTexture(8, _shadows.sampler, _shadows.atlas);
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
		bgfx::setUniform(_shadows.u_sceneView, _frame.view);
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
			bgfx::setName(_shadows.atlas, "Sun shadow atlas");
			_shadows.resolution = _resolution;
		}

		static void startup(World& _world, ResMutT<SunShadowTargets> _shadows, ResMutT<ShadowDrawResource> _draws)
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
			bgfx::setViewName(kRenderPassShadow, "Sun shadow atlas");
			_shadows->u_shadowMatrix = bgfx::createUniform("u_shadowMatrix", bgfx::UniformType::Mat4, 4);
			_shadows->u_shadowSplits = bgfx::createUniform("u_shadowSplits", bgfx::UniformType::Vec4);
			_shadows->u_shadowTexels = bgfx::createUniform("u_shadowTexels", bgfx::UniformType::Vec4);
			_shadows->u_shadowParameters = bgfx::createUniform("u_shadowParameters", bgfx::UniformType::Vec4);
			_shadows->u_shadowInfo = bgfx::createUniform("u_shadowInfo", bgfx::UniformType::Vec4);
			_shadows->u_shadowDirection = bgfx::createUniform("u_shadowDirection", bgfx::UniformType::Vec4);
			_shadows->u_sceneView = bgfx::createUniform("u_sceneView", bgfx::UniformType::Mat4);
			resize(*_shadows, 1); // Valid bindings even without an active shadow light.
		}

		static void submit(const ShadowDrawResource& _draws, const SunShadowResource& _settings,
			const GBufferResource& _gbuffer, const SunShadowTargets& _shadows, GraphicsStatsResource& _stats)
		{
			const bool instancing = _settings.instancing;
			for (uint32_t begin = 0; begin < _draws.count;)
			{
				const ShadowDraw& draw = *_draws.draws.getPtr(begin);
				uint32_t end = begin + 1;
				while (end < _draws.count && 0 == compareShadowDraws(&draw, _draws.draws.getPtr(end))) { ++end; }
				const MeshGroup& group = *draw.group;
				const float tile[] = { 0.25f, -0.75f + 0.5f * float(draw.cascade), 0.0f, 0.0f };
				const bool masked = NULL != draw.material;
				while (begin < end)
				{
					const uint32_t available = instancing
						? bgfx::getAvailInstanceDataBuffer(end - begin, 64) : 0;
					const uint32_t count = available > 1 ? available : 1;
					bgfx::InstanceDataBuffer instances;
					if (count > 1)
					{
						bgfx::allocInstanceDataBuffer(&instances, count, 64);
						for (uint32_t ii = 0; ii < count; ++ii)
						{
							memCopy(instances.data + ii * 64, _draws.draws.getPtr(begin + ii)->modelProjection, 64);
						}
					}
					for (uint16_t ii = 0; ii < group.numPrimitives; ++ii)
					{
						const MeshPrimitive& primitive = group.primitives[ii];
						bgfx::setVertexBuffer(0, group.vertexBuffer);
						bgfx::setIndexBuffer(group.indexBuffer, primitive.startIndex, primitive.numIndices);
						bgfx::setScissor(uint16_t(_shadows.resolution * draw.cascade), 0,
							_shadows.resolution, _shadows.resolution);
						bgfx::setUniform(_shadows.u_shadowTile, tile);
						if (masked) { GBufferSystem::bindMaterial(*draw.material, _gbuffer, true); }
						if (count > 1) { bgfx::setInstanceDataBuffer(&instances); }
						else { bgfx::setUniform(_shadows.u_shadowViewProjection, _draws.draws.getPtr(begin)->modelProjection); }
						bgfx::setState(draw.state);
						bgfx::submit(kRenderPassShadow, count > 1
							? _shadows.instancedPrograms[masked ? 1 : 0] : _shadows.programs[masked ? 1 : 0]);
						++_stats.shadowDraws;
					}
					begin += count;
				}
			}
		}

		static void update(ResT<RenderFrame> _frame, ResT<SunShadowResource> _settings,
			ResT<ActiveCameraResource> _activeCamera, QueryT<const CameraComponent&> _cameras,
			QueryT<const DirectionalLightComponent&> _lights,
			QueryT<const MeshComponent&, OptionalT<const TransformComponent>> _meshes,
			ResT<GBufferResource> _gbuffer, ResMutT<SunShadowTargets> _shadows,
			ResMutT<GraphicsStatsResource> _stats, ResMutT<ShadowDrawResource> _draws)
		{
			_draws->count = 0;
			_stats->shadowDraws = 0;
			_stats->shadowTriangles = 0;
			_shadows->active = false;
			memSet(_stats->shadowCasters, 0, sizeof(_stats->shadowCasters));
			if (!_frame->active || _settings->distance <= 0.0f) { return; }
			DirectionalLightComponent light;
			if (!_lights.get(_settings->light, [&](const DirectionalLightComponent& _light) { light = _light; })
				|| light.intensity <= 0.0f || max(light.color.x, max(light.color.y, light.color.z)) <= 0.0f) { return; }
			const float lengthSq = dot(light.direction, light.direction);
			FROST_ASSERT(isFinite(lengthSq) && lengthSq > 0.0f, "Shadow light direction must be finite and nonzero.");
			const Vec3 direction = mul(light.direction, rsqrt(lengthSq));
			store(_shadows->direction, direction);
			CameraComponent camera;
			_cameras.get(_activeCamera->entity, [&](const CameraComponent& _camera) { camera = _camera; });
			const float nearPlane = camera.nearPlane;
			const float farPlane = min(camera.farPlane, _settings->distance);
			if (farPlane <= nearPlane) { return; }
			const uint16_t resolution = uint16_t(clamp(uint32_t(_settings->resolution), 16u,
				min(65535u, bgfx::getCaps()->limits.maxTextureSize) / 4));
			if (_shadows->resolution != resolution) { resize(*_shadows, resolution); }
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
			}
			// One atlas pass and clear. Transform/cull each group once for all cascades.
			bgfx::setViewRect(kRenderPassShadow, 0, 0, uint16_t(resolution * 4), resolution);
			bgfx::setViewFrameBuffer(kRenderPassShadow, _shadows->target);
			bgfx::setViewClear(kRenderPassShadow, BGFX_CLEAR_DEPTH, 0, 1.0f);
			bgfx::setViewTransform(kRenderPassShadow, NULL, NULL);
			bgfx::touch(kRenderPassShadow);
			_meshes.each([&](const MeshComponent& _mesh, const TransformComponent* _transform)
			{
				if (!_mesh.castShadows) { return; }
				float model[16];
				if (!getMeshTransform(model, _transform)) { return; }
				float modelLight[16];
				mtxMul(modelLight, model, lightView);
				MeshHandle handle = _mesh.handle;
				for (uint8_t cascade = 0; cascade < 4; ++cascade)
				{
					if (_settings->lods && cascade > 0 && isValid(_mesh.shadowLods[cascade - 1]))
					{
						handle = _mesh.shadowLods[cascade - 1];
					}
					const MeshRef* mesh = getMeshRef(handle);
					FROST_ASSERT(NULL != mesh, "Shadow casters must reference live meshes and LODs.");
					for (uint32_t ii = 0; ii < mesh->numGroups; ++ii)
					{
						const MeshGroup& group = mesh->groups[ii];
						Aabb bounds;
						bounds.min = { kFloatLargest, kFloatLargest, kFloatLargest };
						bounds.max = { -kFloatLargest, -kFloatLargest, -kFloatLargest };
						for (uint8_t jj = 0; jj < 8; ++jj)
						{
							const Aabb& local = group.bounds.aabb;
							const Vec3 corner = mul({ jj & 1 ? local.max.x : local.min.x,
								jj & 2 ? local.max.y : local.min.y,
								jj & 4 ? local.max.z : local.min.z }, modelLight);
							bounds.min = min(bounds.min, corner);
							bounds.max = max(bounds.max, corner);
						}
						if (!overlap(bounds, cascadeBounds[cascade])) { continue; }
						if (_draws->count == _draws->draws.getCapacity())
						{
							_draws->draws.setCapacity(max(128u, _draws->count * 2), _draws->count);
						}
						const MaterialCreate& material = getMeshMaterial(_mesh, *mesh, group.materialSlot);
						ShadowDraw& draw = *_draws->draws.getPtr(_draws->count++);
						draw.group = &group;
						draw.material = material.flags & MaterialFlags::Masked ? &material : NULL;
						draw.state = meshState(material, _transform)
							| BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
						draw.cascade = cascade;
						mtxMul(draw.modelProjection, model, _shadows->matrices[cascade]);
						_stats->shadowCasters[cascade] += group.numPrimitives;
						for (uint16_t jj = 0; jj < group.numPrimitives; ++jj)
						{
							_stats->shadowTriangles += group.primitives[jj].numIndices / 3;
						}
					}
				}
			});
			quickSort(_draws->draws.getPtr(), _draws->count, compareShadowDraws);
			submit(*_draws, *_settings, *_gbuffer, *_shadows, *_stats);

			_shadows->active = true;
		}

		static void shutdown(ResMutT<SunShadowTargets> _shadows)
		{
			destroyTargets(*_shadows);
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
			QueryT<Entity, const DirectionalLightComponent&> _lights, ResMutT<LightResource> _light,
			ResT<EnvironmentLightingResource> _environment, ResT<EnvironmentResource> _environmentSettings,
			ResT<SunShadowTargets> _shadows, ResT<SunShadowResource> _shadowSettings,
			ResT<GraphicsDesc> _graphics, QueryT<const SkyComponent&> _skies)
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

			// Seed emission and environment once, including frames without a directional light.
			bindEnvironment(*_environment, *_environmentSettings);
			bindShadows(*_shadows, *_shadowSettings, *_frame, false);
			const bool lunarDebug = _graphics->debugView >= GraphicsDebugView::LunarSky;
			const float emission[4] = { 0.0f, 0.0f, 0.0f, lunarDebug ? -1.0f : 0.0f };
			bgfx::setUniform(_light->u_sunLuminance, emission);
			bgfx::setUniform(_light->u_sunDirection, emission);
			bindScene(*_frame, *_gbuffer, *_light);
			screenSpaceTriangle(_light->layout);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			bgfx::submit(kRenderPassLight, _light->program);

			SkyComponent sky;
			_skies.single([&](const SkyComponent& _component) { sky = _component; });
			_lights.each([&](Entity _entity, const DirectionalLightComponent& _component)
			{
				if (0.0f == _component.intensity || (lunarDebug
					&& (_graphics->debugView != GraphicsDebugView::MoonLight
						|| dot(normalize(_component.direction), sky.moonLightDirection) < 0.99999f)))
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
				bindEnvironment(*_environment, *_environmentSettings);
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

	static float skyDaylight(const SkyComponent& _sky)
	{
		return smoothStep(clamp((_sky.sunDirection.y + 0.207912f) / 0.242812f, 0.0f, 1.0f));
	}

	static Vec3 solarSkyLuminance(const SkyComponent& _sky)
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

	static Vec3 lunarSkyLuminance(const SkyComponent& _sky)
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

	static void bindSky(const SkyResource& _sky, const SkyComponent& _component,
		GraphicsDebugView::Enum _debug, bool _active = true)
	{
		float direction[4] = {};
		store(direction, _component.sunDirection);
		float luminance[4] = {};
		store(luminance, mul(solarSkyLuminance(_component), _active ? 1.0f : 0.0f));
		const float sunTint[] = { max(0.0f, _component.sunTint.x) * max(0.0f, _component.sunIntensity),
			max(0.0f, _component.sunTint.y) * max(0.0f, _component.sunIntensity),
			max(0.0f, _component.sunTint.z) * max(0.0f, _component.sunIntensity), 0.0f };
		const float parameters[] = { max(0.0001f, _component.sunSize), max(0.0001f, _component.sunBloom),
			float(_debug), _active ? 1.0f : 0.0f };
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
		store(moonColor, mul(mul(transmission, tint), _active
			? sampleSkyLuminance(s_moonLuminance, hour).y * max(0.0f, _component.moonIntensity) : 0.0f));
		float moonSkyLuminance[4] = {};
		store(moonSkyLuminance, mul(lunarSkyLuminance(_component), _active ? 1.0f : 0.0f));
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
		static void prepare(ResT<TimeResource> _time, QueryT<SkyComponent&> _skies)
		{
			const float deltaTime = float(min(_time->delta, 0.1));
			_skies.each([&](SkyComponent& _sky)
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
				_sky.moonPhase = (1.0f + phaseCosine) * 0.5f;
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
			QueryT<const SkyComponent&> _skies, ResT<SkyResource> _sky, ResT<GraphicsDesc> _graphics)
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
				bindSky(*_sky, _component, _graphics->debugView);

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

// Procedural sky environment
namespace frost
{
	struct EnvironmentLightingSystem
	{
		static void startup(ResMutT<EnvironmentLightingResource> _environment)
		{
			const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP;
			FROST_ASSERT(bgfx::isTextureValid(0, true, 1, bgfx::TextureFormat::RGBA16F, flags),
				"HDR environment cubemaps are unsupported.");
			_environment->source = bgfx::createTextureCube(kEnvironmentSize, false, 1, bgfx::TextureFormat::RGBA16F, flags);
			_environment->specular = bgfx::createTextureCube(kEnvironmentSize, true, 1, bgfx::TextureFormat::RGBA16F, flags);
			_environment->diffuse = bgfx::createTextureCube(16, false, 1, bgfx::TextureFormat::RGBA16F, flags);
			_environment->brdf = bgfx::createTexture2D(128, 128, false, 1, bgfx::TextureFormat::RGBA16F, flags);
			bgfx::setName(_environment->source, "Sky environment capture");
			bgfx::setName(_environment->specular, "Environment GGX specular");
			bgfx::setName(_environment->diffuse, "Environment diffuse");
			bgfx::setName(_environment->brdf, "Environment BRDF lookup");
			const bgfx::RendererType::Enum type = bgfx::getRendererType();
			_environment->captureProgram = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_sky"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_environment_capture"), true);
			FROST_ASSERT(bgfx::isValid(_environment->captureProgram), "Failed to create environment capture program.");
			_environment->s_source = bgfx::createUniform("s_environmentSource", bgfx::UniformType::Sampler);
			_environment->program = bgfx::createProgram(
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_deferred_light"),
				bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_environment"), true);
			FROST_ASSERT(bgfx::isValid(_environment->program), "Failed to create environment filtering program.");
			const char* samplers[] = { "s_environmentSpecular", "s_environmentDiffuse", "s_environmentBrdf" };
			for (uint8_t ii = 0; ii < 3; ++ii)
			{
				_environment->samplers[ii] = bgfx::createUniform(samplers[ii], bgfx::UniformType::Sampler);
			}
			_environment->u_environment = bgfx::createUniform("u_environment", bgfx::UniformType::Vec4);
			_environment->u_environmentBake = bgfx::createUniform("u_environmentBake", bgfx::UniformType::Vec4);
			_environment->layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
			for (uint16_t ii = 0; ii < kEnvironmentViews; ++ii)
			{
				const bool capture = ii < 6;
				const bool specular = ii >= 6 && ii < 6 + 6 * kEnvironmentMips;
				const bool brdf = ii == kEnvironmentViews - 1;
				const bgfx::TextureHandle texture = capture ? _environment->source
					: (brdf ? _environment->brdf : (specular ? _environment->specular : _environment->diffuse));
				bgfx::Attachment attachment;
				attachment.init(texture, bgfx::Access::Write, brdf ? 0 : ii % 6, 1,
					specular ? uint16_t((ii - 6) / 6) : 0, 0);
				_environment->targets[ii] = bgfx::createFrameBuffer(1, &attachment, false);
				FROST_ASSERT(bgfx::isValid(_environment->targets[ii]), "Failed to create an environment target.");
				bgfx::setViewName(kRenderPassEnvironment + ii, capture ? "Sky cubemap capture"
					: (brdf ? "Environment BRDF" : (specular ? "Environment specular" : "Environment diffuse")));
			}

		}

		static void update(ResT<RenderFrame> _frame, ResT<TimeResource> _time,
			ResT<EnvironmentResource> _settings, QueryT<const SkyComponent&> _skies,
			ResT<SkyResource> _sky, ResMutT<EnvironmentLightingResource> _environment)
		{
			if (!_frame->active) { return; }
			_environment->elapsed += max(0.0, _time->delta);
			SkyComponent sky;
			bool active = false;
			_skies.single([&](const SkyComponent& _component) { sky = _component; active = true; });
			const float hour = isFinite(sky.time) ? wrap(sky.time, 24.0f) : 12.0f;
			const float state[] = { active ? 1.0f : 0.0f, hour, sky.turbidity,
				sky.sunDirection.x, sky.sunDirection.y, sky.sunDirection.z,
				_frame->cameraPosition[0], _frame->cameraPosition[1], _frame->cameraPosition[2],
				sky.moonDirection.x, sky.moonDirection.y, sky.moonDirection.z,
				sky.moonColor.x, sky.moonColor.y, sky.moonColor.z,
				sky.sunColor.x, sky.sunColor.y, sky.sunColor.z, sky.moonIlluminance,
				sky.moonIntensity, sky.moonTint.x, sky.moonTint.y, sky.moonTint.z };
			const bool changed = 0 != memCmp(state, _environment->skyState, sizeof(state));
			if (_environment->initialized && (!changed || (_environment->active == active
				&& _environment->elapsed < max(0.0f, _settings->updateInterval)))) { return; }

			const Vec3 eye = { _frame->cameraPosition[0], _frame->cameraPosition[1], _frame->cameraPosition[2] };
			const Vec3 directions[] = {
				{ 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
				{ 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
			};
			const Vec3 ups[] = {
				{ 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
				{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f },
				{ 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
			};
			float captureProjection[16];
			mtxProj(captureProjection, 90.0f, 1.0f, 0.1f, 1000.0f, bgfx::getCaps()->homogeneousDepth);
			if (bgfx::getCaps()->originBottomLeft) { captureProjection[5] = -captureProjection[5]; }
			for (uint16_t ii = 0; ii < kEnvironmentViews; ++ii)
			{
				const bool capture = ii < 6;
				const bool specular = ii >= 6 && ii < 6 + 6 * kEnvironmentMips;
				const bool brdf = ii == kEnvironmentViews - 1;
				if (brdf && _environment->initialized) { continue; }
				const uint8_t mip = specular ? uint8_t((ii - 6) / 6) : 0;
				const uint16_t size = brdf ? 128 : (capture ? kEnvironmentSize
					: (specular ? kEnvironmentSize >> mip : 16));
				const bgfx::ViewId view = kRenderPassEnvironment + ii;
				bgfx::setViewRect(view, 0, 0, size, size);
				bgfx::setViewFrameBuffer(view, _environment->targets[ii]);
				bgfx::setViewClear(view, BGFX_CLEAR_NONE);
				if (capture)
				{
					bindSky(*_sky, sky, GraphicsDebugView::None, active);
					float captureView[16];
					mtxLookAt(captureView, eye, add(eye, directions[ii]), ups[ii]);
					bgfx::setViewTransform(view, captureView, captureProjection);
					bgfx::setVertexBuffer(0, _sky->vertices);
					bgfx::setIndexBuffer(_sky->indices);
				}
				else
				{
					bgfx::setViewTransform(view, NULL, _frame->screenProjection);
					const float bake[] = { float(ii % 6), float(mip) / float(kEnvironmentMips - 1),
						brdf ? 2.0f : (specular ? 0.0f : 1.0f), 0.0f };
					bgfx::setUniform(_environment->u_environmentBake, bake);
					bgfx::setTexture(0, _environment->s_source, _environment->source);
					screenSpaceTriangle(_environment->layout);
				}
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
				bgfx::submit(view, capture ? _environment->captureProgram : _environment->program);
			}
			memCopy(_environment->capturePosition, _frame->cameraPosition, sizeof(_environment->capturePosition));

			memCopy(_environment->skyState, state, sizeof(state));
			_environment->elapsed = 0.0;
			_environment->initialized = true;
			_environment->active = active;
		}

		static void shutdown(ResMutT<EnvironmentLightingResource> _environment)
		{
			for (bgfx::FrameBufferHandle target : _environment->targets) { bgfx::destroy(target); }
			bgfx::destroy(_environment->source);
			bgfx::destroy(_environment->captureProgram);
			bgfx::destroy(_environment->s_source);
			bgfx::destroy(_environment->specular);
			bgfx::destroy(_environment->diffuse);
			bgfx::destroy(_environment->brdf);
			bgfx::destroy(_environment->program);
			for (bgfx::UniformHandle sampler : _environment->samplers) { bgfx::destroy(sampler); }
			bgfx::destroy(_environment->u_environment);
			bgfx::destroy(_environment->u_environmentBake);
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
		bgfx::UniformHandle u_invSceneViewProj = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout layout;
		bgfx::UniformHandle s_material = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_emissive = BGFX_INVALID_HANDLE;
	};

	static void submitOutput(bgfx::ViewId _view, uint32_t _mode, const RenderFrame& _frame,
		const GraphicsDesc& _graphics, const GBufferResource& _gbuffer,
		const LightResource& _light, const ToneMapResource& _output,
		const EnvironmentLightingResource& _environment, const EnvironmentResource& _environmentSettings,
		const SunShadowTargets& _shadows, const SunShadowResource& _shadowSettings)
	{
		const float parameters[4] = { _frame.exposure, _graphics.preventBanding ? 1.0f : 0.0f,
			float(_mode), 1.0f / max(0.001f, _graphics.debugDepth) };
		bgfx::setViewTransform(_view, NULL, _frame.screenProjection);
		bgfx::setUniform(_output.u_outputParameters, parameters);
		bgfx::setUniform(_output.u_invSceneViewProj, _frame.inverseViewProjection);
		bgfx::setTexture(0, _output.s_scene, _light.texture);
		bgfx::setTexture(1, _output.s_albedo, _gbuffer.textures[0]);
		bgfx::setTexture(2, _output.s_normal, _gbuffer.textures[1]);
		bgfx::setTexture(3, _output.s_depth, _gbuffer.textures[2]);
		bgfx::setTexture(4, _output.s_material, _gbuffer.textures[3]);
		bgfx::setTexture(12, _output.s_emissive, _gbuffer.textures[4]);
		bgfx::setUniform(_light.u_sceneCamera, _frame.cameraPosition);
		bgfx::setUniform(_light.u_sceneDirection, _frame.cameraDirection);
		bindEnvironment(_environment, _environmentSettings);
		bindShadows(_shadows, _shadowSettings, _frame, true);
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
			_output->u_invSceneViewProj = bgfx::createUniform("u_invSceneViewProj", bgfx::UniformType::Mat4);
			_output->layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
		}

		static void update(ResT<RenderFrame> _frame, ResT<GraphicsDesc> _graphics,
			ResT<GBufferResource> _gbuffer, ResT<LightResource> _light, ResT<ToneMapResource> _output,
			ResT<EnvironmentLightingResource> _environment, ResT<EnvironmentResource> _environmentSettings,
			ResT<SunShadowTargets> _shadows, ResT<SunShadowResource> _shadowSettings)
		{
			if (0 == _frame->width || 0 == _frame->height) { return; }
			// Presentation and UI still work without a selected camera.
			bgfx::setViewRect(kRenderPassToneMap, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassToneMap, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kRenderPassToneMap, BGFX_CLEAR_COLOR, UINT32_C(0x000000ff));
			bgfx::touch(kRenderPassToneMap);
			if (_frame->active)
			{
				submitOutput(kRenderPassToneMap, GraphicsDebugView::None, *_frame, *_graphics, *_gbuffer, *_light, *_output,
				*_environment, *_environmentSettings, *_shadows, *_shadowSettings);
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
			bgfx::destroy(_output->u_invSceneViewProj);
		}
	};

	struct DebugViewSystem
	{
		static void startup()
		{
			bgfx::setViewName(kRenderPassDebug, "Debug View");
		}

		static void update(ResT<RenderFrame> _frame, ResT<GraphicsDesc> _graphics,
			ResT<GBufferResource> _gbuffer, ResT<LightResource> _light, ResT<ToneMapResource> _output,
			ResT<EnvironmentLightingResource> _environment, ResT<EnvironmentResource> _environmentSettings,
			ResT<SunShadowTargets> _shadows, ResT<SunShadowResource> _shadowSettings)
		{
			if (!_frame->active || _graphics->debugView <= GraphicsDebugView::None
				|| _graphics->debugView >= GraphicsDebugView::Count) { return; }
			bgfx::setViewRect(kRenderPassDebug, 0, 0, _frame->width, _frame->height);
			bgfx::setViewFrameBuffer(kRenderPassDebug, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kRenderPassDebug, BGFX_CLEAR_NONE);
			submitOutput(kRenderPassDebug, _graphics->debugView, *_frame, *_graphics, *_gbuffer, *_light, *_output,
				*_environment, *_environmentSettings, *_shadows, *_shadowSettings);
		}
	};

	struct GraphicsStatsSystem
	{
		static void update(ResT<GraphicsDesc> _graphics, ResMutT<GraphicsStatsResource> _stats)
		{
			const bgfx::Stats* stats = bgfx::getStats();
			double* times[] = { &_stats->geometryMs, &_stats->lightMs, &_stats->skyMs,
				&_stats->toneMapMs, &_stats->debugViewMs, &_stats->uiMs };
			const bgfx::ViewId views[] = { kRenderPassGeometry, kRenderPassLight, kRenderPassSky,
				kRenderPassToneMap, kRenderPassDebug, kRenderPassUi };
			for (double* time : times) { *time = -1.0; }
			_stats->shadowMs = -1.0;
			_stats->environmentMs = -1.0;
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
				if (view.gpuTimeEnd <= view.gpuTimeBegin) { continue; }
				const double duration = double(view.gpuTimeEnd - view.gpuTimeBegin) * milliseconds;
				for (uint32_t jj = 0; jj < FROST_COUNTOF(views); ++jj)
				{
					if (view.view == views[jj]) { *times[jj] = duration; }
				}
				if (view.view == kRenderPassShadow) { _stats->shadowMs = duration; }
				else if (view.view < kRenderPassEnvironment + kEnvironmentViews)
				{
					_stats->environmentMs = max(0.0, _stats->environmentMs) + duration;
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

namespace frost
{
	void GraphicsPlugin::build(App& _app)
	{
		// Resources
		_app.getWorld().initResource<ActiveCameraResource>();
		_app.insertResource<RenderFrame>();
		_app.insertResource<GBufferResource>();
		_app.insertResource<LightResource>();
		_app.getWorld().initResource<EnvironmentResource>();
		_app.getWorld().initResource<SunShadowResource>();
		_app.insertResource<EnvironmentLightingResource>();
		_app.insertResource<SunShadowTargets>();
		_app.insertResource<ShadowDrawResource>();
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
		startup = _app.addSystems(EcsStage::Startup, EnvironmentLightingSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, SunShadowSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, ToneMapSystem::startup).after(startup).id();
		startup = _app.addSystems(EcsStage::Startup, DebugViewSystem::startup).after(startup).id();
		_app.addSystems(EcsStage::Startup, UiSystem::startup).after(startup);

		_app.addSystems(EcsStage::PreUpdate, GraphicsStatsSystem::update);
		_app.addSystems(EcsStage::PreUpdate, SkySystem::prepare);

		// Prepare shared frame data, then submit passes in dependency order.
		SystemId update = _app.addSystems(EcsStage::PostUpdate, RendererSystem::update).id();
		update = _app.addSystems(EcsStage::PostUpdate, RenderFrameSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, EnvironmentLightingSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, GBufferSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, SunShadowSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, LightSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, SkySystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, ToneMapSystem::update).after(update).id();
		update = _app.addSystems(EcsStage::PostUpdate, DebugViewSystem::update).after(update).id();
		_app.addSystems(EcsStage::PostUpdate, UiSystem::update).after(update);
		_app.addSystems(EcsStage::Last, RendererSystem::endFrame);

		// Release GPU resources before stopping the backend.
		SystemId shutdown = _app.addSystems(EcsStage::Shutdown, UiSystem::shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, ToneMapSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, SunShadowSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, EnvironmentLightingSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, SkySystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, LightSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, GBufferSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, MeshSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, MaterialSystem::shutdown).after(shutdown).id();
		shutdown = _app.addSystems(EcsStage::Shutdown, TextureSystem::shutdown).after(shutdown).id();
		_app.addSystems(EcsStage::Shutdown, RendererSystem::shutdown).after(shutdown);
	}

} // namespace frost
