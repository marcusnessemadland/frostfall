#ifndef BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
#define BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>

#include <frost/math.h>

namespace frost
{
	constexpr bgfx::ViewId kRenderPassGeometry = 0;

} // namespace frost

#include <bx/pixelformat.h>

namespace frost
{
	uint32_t encodeNormalRgba8(float _x, float _y = 0.0f, float _z = 0.0f, float _w = 0.0f)
	{
		const float src[] =
		{
			_x * 0.5f + 0.5f,
			_y * 0.5f + 0.5f,
			_z * 0.5f + 0.5f,
			_w * 0.5f + 0.5f,
		};
		uint32_t dst;
		bx::packRgba8(&dst, src);
		return dst;
	}

	void calcTangents(void* _vertices, uint16_t _numVertices, bgfx::VertexLayout _layout, const uint16_t* _indices, uint32_t _numIndices)
	{
		struct PosTexcoord
		{
			float m_x;
			float m_y;
			float m_z;
			float m_pad0;
			float m_u;
			float m_v;
			float m_pad1;
			float m_pad2;
		};

		float* tangents = new float[6*_numVertices];
		bx::memSet(tangents, 0, 6*_numVertices*sizeof(float) );

		PosTexcoord v0;
		PosTexcoord v1;
		PosTexcoord v2;

		for (uint32_t ii = 0, num = _numIndices/3; ii < num; ++ii)
		{
			const uint16_t* indices = &_indices[ii*3];
			uint32_t i0 = indices[0];
			uint32_t i1 = indices[1];
			uint32_t i2 = indices[2];

			bgfx::vertexUnpack(&v0.m_x, bgfx::Attrib::Position, _layout, _vertices, i0);
			bgfx::vertexUnpack(&v0.m_u, bgfx::Attrib::TexCoord0, _layout, _vertices, i0);

			bgfx::vertexUnpack(&v1.m_x, bgfx::Attrib::Position, _layout, _vertices, i1);
			bgfx::vertexUnpack(&v1.m_u, bgfx::Attrib::TexCoord0, _layout, _vertices, i1);

			bgfx::vertexUnpack(&v2.m_x, bgfx::Attrib::Position, _layout, _vertices, i2);
			bgfx::vertexUnpack(&v2.m_u, bgfx::Attrib::TexCoord0, _layout, _vertices, i2);

			const float bax = v1.m_x - v0.m_x;
			const float bay = v1.m_y - v0.m_y;
			const float baz = v1.m_z - v0.m_z;
			const float bau = v1.m_u - v0.m_u;
			const float bav = v1.m_v - v0.m_v;

			const float cax = v2.m_x - v0.m_x;
			const float cay = v2.m_y - v0.m_y;
			const float caz = v2.m_z - v0.m_z;
			const float cau = v2.m_u - v0.m_u;
			const float cav = v2.m_v - v0.m_v;

			const float det = (bau * cav - bav * cau);
			const float invDet = 1.0f / det;

			const float tx = (bax * cav - cax * bav) * invDet;
			const float ty = (bay * cav - cay * bav) * invDet;
			const float tz = (baz * cav - caz * bav) * invDet;

			const float bx = (cax * bau - bax * cau) * invDet;
			const float by = (cay * bau - bay * cau) * invDet;
			const float bz = (caz * bau - baz * cau) * invDet;

			for (uint32_t jj = 0; jj < 3; ++jj)
			{
				float* tanu = &tangents[indices[jj]*6];
				float* tanv = &tanu[3];
				tanu[0] += tx;
				tanu[1] += ty;
				tanu[2] += tz;

				tanv[0] += bx;
				tanv[1] += by;
				tanv[2] += bz;
			}
		}

		for (uint32_t ii = 0; ii < _numVertices; ++ii)
		{
			const bx::Vec3 tanu = bx::load<bx::Vec3>(&tangents[ii*6]);
			const bx::Vec3 tanv = bx::load<bx::Vec3>(&tangents[ii*6 + 3]);

			float nxyzw[4];
			bgfx::vertexUnpack(nxyzw, bgfx::Attrib::Normal, _layout, _vertices, ii);

			const bx::Vec3 normal  = bx::load<bx::Vec3>(nxyzw);
			const float    ndt     = bx::dot(normal, tanu);
			const bx::Vec3 nxt     = bx::cross(normal, tanu);
			const bx::Vec3 tmp     = bx::sub(tanu, bx::mul(normal, ndt) );

			float tangent[4];
			bx::store(tangent, bx::normalize(tmp) );
			tangent[3] = bx::dot(nxt, tanv) < 0.0f ? -1.0f : 1.0f;

			bgfx::vertexPack(tangent, true, bgfx::Attrib::Tangent, _layout, _vertices, ii);
		}

		delete [] tangents;
	}

} // namespace frost

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

		static void update(
			  ResT<Display> _display
			, ResT<TempAssetsResource> _assets
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
				_gbuffer->frameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(gbufferAt), gbufferAt, true);
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
				// Calculate view projection matrix from camera
				const bgfx::Caps* caps = bgfx::getCaps();

				mtxFromQuaternion(view, camera.rotation, camera.position);

				const float aspect = float(_gbuffer->width) / float(_gbuffer->height);
				if (CameraProjection::Orthographic == camera.projection)
				{
					const float halfHeight = camera.orthographicHeight * 0.5f;
					const float halfWidth = halfHeight * aspect;
					mtxOrtho(proj, -halfWidth, halfWidth, -halfHeight, halfHeight,
						camera.nearPlane, camera.farPlane, 0.0f, caps->homogeneousDepth);
				}
				else
				{
					mtxProj(proj, camera.fov, aspect, camera.nearPlane, camera.farPlane, caps->homogeneousDepth);
				}
			}
			bgfx::setViewTransform(kRenderPassGeometry, view, proj);

			// Submit geometry
			float mtx[16];
			frost::mtxIdentity(mtx);

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

#endif // BX_PLUGIN_GRAPHICS_P_H_HEADER_GUARD
