/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef BX_PLUGIN_GRAPHICS_H_HEADER_GUARD
#define BX_PLUGIN_GRAPHICS_H_HEADER_GUARD

#include <frost/ecs.h>
#include <frost/asset.h>
#include <frost/handle.h>
#include <frost/math.h>
#include <frost/string.h>

namespace frost
{
	FROST_HANDLE(MeshHandle);
	FROST_HANDLE(TextureHandle);
	FROST_HANDLE(MaterialHandle);

	FROST_ERROR_RESULT(kErrorMeshCreate, FROST_MAKEFOURCC('m', 's', 'h', 'c') );
	FROST_ERROR_RESULT(kErrorMeshFormat, FROST_MAKEFOURCC('m', 's', 'h', 'f') );
	FROST_ERROR_RESULT(kErrorTextureCreate, FROST_MAKEFOURCC('t', 'x', 'c', 'r') );
	FROST_ERROR_RESULT(kErrorTextureFormat, FROST_MAKEFOURCC('t', 'x', 'f', 'm') );
	FROST_ERROR_RESULT(kErrorMaterialCreate, FROST_MAKEFOURCC('m', 't', 'c', 'r') );
	FROST_ERROR_RESULT(kErrorMaterialFormat, FROST_MAKEFOURCC('m', 't', 'f', 'm') );

	constexpr uint32_t kAssetTypeMesh = FROST_MAKEFOURCC('M', 'E', 'S', 'H');
	constexpr uint32_t kAssetTypeTexture = FROST_MAKEFOURCC('T', 'E', 'X', 'R');
	constexpr uint32_t kAssetTypeMaterial = FROST_MAKEFOURCC('M', 'A', 'T', 'L');

	constexpr uint32_t kMeshVersion   = 3;
	constexpr uint32_t kTextureVersion = 1;
	constexpr uint32_t kMaterialVersion = 1;

	constexpr uint32_t kMaterialPayloadSize = 96; // TODO: Should not be here

	///
	struct RendererType
	{
		enum Enum
		{
			Noop,       //!< No rendering.
			Agc,        //!< AGC
			Direct3D11, //!< Direct3D 11.0
			Direct3D12, //!< Direct3D 12.0
			Gnm,        //!< GNM
			Metal,      //!< Metal
			Nvn,        //!< NVN
			OpenGLES,   //!< OpenGL ES 3.0+
			OpenGL,     //!< OpenGL 4.3+
			Vulkan,     //!< Vulkan
			WebGPU,     //!< WebGPU

			Count		//!< Auto detection.
		};
	};

	///
	struct CameraProjection
	{
		enum Enum
		{
			Perspective,
			Orthographic,

			Count
		};
	};

	///
	struct GraphicsDebugView
	{
		enum Enum
		{
			None,
			BaseColor,
			Normals,
			Depth,
			LinearDepth,
			WorldPosition,
			Luminance,
			Metallic,
			Roughness,
			Occlusion,
			Emissive,
			Cascades,
			ShadowVisibility,
			ShadowMap0,
			ShadowMap1,
			ShadowMap2,
			ShadowMap3,
			ShadowAtlas,
			LunarSky,
			MoonLight,
			MoonDisk,
			Count
		};
	};

	///
	struct TextureAssetFlags
	{
		enum : uint32_t
		{
			Srgb = 0x1
		};
	};

	///
	struct MaterialTexture
	{
		enum Enum
		{
			BaseColor,
			Normal,
			MetallicRoughness,
			Occlusion,
			Emissive,

			Count
		};
	};

	///
	struct MaterialFlags
	{
		enum : uint32_t
		{
			Masked = 0x1,
			DoubleSided = 0x2
		};
	};

	///
	struct TextureSampler
	{
		enum : uint32_t
		{
			ClampU = 0x1,
			ClampV = 0x2,
			MirrorU = 0x4,
			MirrorV = 0x8,
			Point = 0x10,
			Anisotropic = 0x20,
		};
	};

	///
	struct MeshFormat
	{
		enum : uint32_t
		{
			VertexCompressed = 0x1,
			IndexCompressed  = 0x2,
			Index32          = 0x4,
		};
	};

	///
	struct MaterialCreate
	{
		float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		Vec3 emissive = { 0.0f, 0.0f, 0.0f };
		float emissiveStrength = 1.0f;
		float metallic = 0.0f;
		float roughness = 1.0f;
		float normalScale = 1.0f;
		float occlusionStrength = 1.0f;
		uint32_t flags = 0;
		float alphaCutoff = 0.5f;
		TextureHandle textures[MaterialTexture::Count];
		uint32_t samplers[MaterialTexture::Count] = {};
	};

	///
	struct MeshComponent
	{
		MeshHandle handle;
		const MaterialHandle* materials = NULL;
		uint16_t numMaterials = 0;
		bool castShadows = true;
	};

	///
	struct TransformComponent
	{
		Vec3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		Vec3 scale = { 1.0f, 1.0f, 1.0f };
	};

	///
	struct DirectionalLightComponent
	{
		Vec3 direction = { 0.0f, 1.0f, 0.0f }; //!< World direction toward the light; must be finite and nonzero.
		Vec3 color = { 1.0f, 1.0f, 1.0f };     //!< Linear RGB illumination, clamped nonnegative.
		float intensity = 1.0f;						 //!< Nonnegative multiplier; zero disables the light.
	};

	/// TODO: Bloat and also maybe call it direct shadows and in the future local shadows for points lights etc.
	struct SunShadowResource
	{
		Entity light;
		uint16_t resolution = 1024;
		float distance = 150.0f;
		float splitLambda = 0.7f;
		float casterDistance = 200.0f;
		float depthBias = 0.002f;
		float normalBias = 1.0f;
		float blend = 0.1f;
		float filterRadius = 1.0f;
		bool lods = true;
		bool instancing = true;
	};

	/// TODO: This is bloated with data
	struct SkyComponent
	{
		enum Month
		{
			January,
			February,
			March,
			April,
			May,
			June,
			July,
			August,
			September,
			October,
			November,
			December,
		};
		float time = 12.0f;
		float hps = 0.0f;
		Month month = June;
		float latitude = 50.0f;
		float turbidity = 2.15f;
		float sunSize = 0.03f;
		float sunBloom = 3.0f;
		Vec3 sunTint = { 1.0f, 1.0f, 1.0f };
		float sunIntensity = 1.0f;
		float moonCycle = 0.5f;
		bool pauseMoonCycle = false;
		float moonSize = 0.52f;
		float moonIntensity = 1.0f;
		Vec3 moonTint = { 1.0f, 1.0f, 1.0f };
		TextureHandle moonTexture;
		float moonAlbedo = 1.0f;
		Vec3 sunDirection = { 0.0f, 1.0f, 0.0f };
		Vec3 sunColor = { 0.0f, 0.0f, 0.0f };
		Vec3 moonDirection = { 0.0f, -1.0f, 0.0f };
		Vec3 moonLightDirection = { 0.0f, 1.0f, 0.0f };
		Vec3 moonUp = { 0.0f, 1.0f, 0.0f };
		Vec3 moonColor = { 0.0f, 0.0f, 0.0f };
		float moonPhase = 1.0f;
		float moonIlluminance = 0.0f;
		float moonElevation = -90.0f;
		float moonPhaseBrightness = 1.0f;
	};

	///
	struct CameraComponent
	{
		Vec3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		CameraProjection::Enum projection = CameraProjection::Perspective;
		float fov = 90.0f;
		float orthographicHeight = 10.0f;
		float nearPlane = 0.1f;
		float farPlane = 1000.0f;
		float exposure = -3.0f;
	};

	/// TODO: Lets see if this can be improved?
	struct ActiveCameraResource
	{
		Entity entity;
	};

	///
	struct UiCanvasComponent
	{
		float scale = 1.0f;
		int32_t layer = 0;
		bool visible = true;
	};

	///
	struct UiNodeComponent
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float anchorX = 0.0f;
		float anchorY = 0.0f;
		float pivotX = 0.0f;
		float pivotY = 0.0f;
		bool visible = true;
		bool clip = false;
		int32_t order = 0;
	};

	///
	struct UiLayoutComponent
	{
		enum Direction
		{
			Column,
			Row
		};
		Direction direction = Column;
		float padding = 0.0f;
		float spacing = 0.0f;
	};

	///
	struct UiRectComponent
	{
		uint32_t color = 0xffffffff;
		uint32_t outlineColor = 0x000000ff;
		float outlineWidth = 0.0f;
	};

	///
	struct UiTextComponent
	{
		FixedString1024 text;
		uint32_t color = 0xffffffff;
		float height = 16.0f;
	};

	///
	struct UiProgressBarComponent
	{
		float value = 0.0f;
		float minimum = 0.0f;
		float maximum = 1.0f;
		uint32_t backgroundColor = 0x303030ff;
		uint32_t fillColor = 0x80c0ffff;
	};

	/// Latest available bgfx measurements. Negative GPU times mean unavailable.
	/// TODO: Bloat, bloat, bloat. Lets look into how to improve this
	struct GraphicsStatsResource
	{
		double gpuMs = -1.0;
		double geometryMs = -1.0, lightMs = -1.0, skyMs = -1.0;
		double toneMapMs = -1.0, debugViewMs = -1.0, uiMs = -1.0;
		double shadowMs = -1.0;
		uint32_t shadowCasters[4] = {};
		uint32_t shadowDraws = 0;
		uint64_t shadowTriangles = 0;
		uint32_t draws = 0;
		uint16_t textures = 0;
		int64_t renderTargetBytes = 0;
	};

	///
	MeshHandle createMesh(const Memory* _mem, const MaterialHandle* _materials, uint16_t _numMaterials, Error* _err = ErrorIgnore{});

	///
	MeshHandle createMesh(const Memory* _mem, Error* _err = ErrorIgnore{});

	///
	void addRef(MeshHandle _handle);

	///
	void release(MeshHandle _handle);

	///
	uint16_t getMeshMaterialCount(MeshHandle _handle);

	///
	StringView getMeshMaterialName(MeshHandle _handle, uint16_t _slot);

	///
	TextureHandle createTexture(const Memory* _mem, Error* _err = ErrorIgnore{});

	///
	void addRef(TextureHandle _handle);

	///
	void release(TextureHandle _handle);

	///
	MaterialHandle createMaterial(const MaterialCreate& _desc, Error* _err = ErrorIgnore{});

	///
	MaterialHandle createMaterial(const Memory* _mem, const TextureHandle* _textures, uint16_t _numTextures, Error* _err = ErrorIgnore{});

	///
	void addRef(MaterialHandle _handle);

	///
	void release(MaterialHandle _handle);

} // namespace frost

namespace frost
{
	/// Const graphics configuration read on startup.
	///
	struct GraphicsDesc
	{
		uint32_t width = 1280; //!< Initial render width in pixels.
		uint32_t height = 720; //!< Initial render height in pixels.
		RendererType::Enum type = RendererType::Count; //!< Renderer backend graphics API .

		/// TODO: Define macros for static arrays and make them synced with BGFX_CONFIG_ macros
		uint16_t maxMeshes = 1024;
		uint16_t maxTextures = 1024;
		uint16_t maxMaterials = 1024;

		// TODO: Should these be runtime settings?
		GraphicsDebugView::Enum debugView = GraphicsDebugView::None;
		float debugDepth = 100.0f;
		bool preventBanding = true;
		bool gpuProfiling = true;
		Entity debugLight; //!< Explicit light entity shown by GraphicsDebugView::MoonLight.
	};

	/// Runtime graphics configuration read every frame.
	///
	/// @remark Available after plugin is built.
	///
	struct GraphicsSettings
	{
	};

	/// Creates the graphics api backend and handles rendering.
	///
	/// @remark Requires `DevicePlugin`.
	/// @remark Insert `GraphicsDesc` before plugin is added to application.
	///
	struct GraphicsPlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_H_HEADER_GUARD
