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
	constexpr uint32_t kMeshVersion   = 2;
	constexpr uint32_t kAssetTypeTexture = FROST_MAKEFOURCC('T', 'E', 'X', 'R');
	constexpr uint32_t kAssetTypeMaterial = FROST_MAKEFOURCC('M', 'A', 'T', 'L');
	constexpr uint32_t kTextureVersion = 1; // Little-endian TextureAssetFlags followed by DDS.
	constexpr uint32_t kMaterialVersion = 1;
	constexpr uint32_t kMaterialPayloadSize = 96;

	struct TextureAssetFlags
	{
		enum : uint32_t { Srgb = 0x1 };
	};

	struct MaterialTexture
	{
		enum Enum { BaseColor, Normal, MetallicRoughness, Occlusion, Emissive, Count };
	};

	struct MaterialFlags
	{
		enum : uint32_t { Masked = 0x1, DoubleSided = 0x2 };
	};

	struct TextureSampler
	{
		enum : uint32_t
		{
			ClampU = 0x1, ClampV = 0x2, MirrorU = 0x4, MirrorV = 0x8,
			Point = 0x10, Anisotropic = 0x20,
		};
	};

	/// Linear factors. Color textures are sRGB; normal and parameter textures are linear.
	/// Metallic-roughness uses G for roughness and B for metallic; occlusion uses R.
	/// Tangent-space normal maps use +Y, with reconstructed positive Z.
	/// Material v1 stores 12 float factors, u32 flags, float cutoff, then five
	/// (u32 dependency index or UINT32_MAX, u32 sampler flags) pairs. Do not write this struct.
	struct MaterialDesc
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

	/// Creation consumes _mem even on failure, and returns one owned reference.
	/// Call on the application thread after graphics startup, like createMesh.
	TextureHandle createTexture(const Memory* _mem, Error* _err = ErrorIgnore{});
	MaterialHandle createMaterial(const MaterialDesc& _desc, Error* _err = ErrorIgnore{});

	/// Resolved textures are in asset dependency-table order. The material retains them.
	MaterialHandle createMaterial(const Memory* _mem, const TextureHandle* _textures, uint16_t _numTextures,
		Error* _err = ErrorIgnore{});
	void addRef(TextureHandle _handle);
	void addRef(MaterialHandle _handle);
	void release(TextureHandle _handle);
	void release(MaterialHandle _handle);

	/// Resolved default materials are in dependency-table order. The mesh retains them.
	MeshHandle createMesh(const Memory* _mem, const MaterialHandle* _materials, uint16_t _numMaterials,
		Error* _err = ErrorIgnore{});
	uint16_t getMeshMaterialCount(MeshHandle _handle);
	/// The returned name borrows storage from the live mesh.
	StringView getMeshMaterialName(MeshHandle _handle, uint16_t _slot);

	/// Mesh v2: group count (u32), material-slot count (u16), then slots
	/// (u16 name length, name bytes, u32 dependency index or UINT32_MAX), then groups.
	/// Each group contains bounds, vertex layout, counts/flags, material slot (u16),
	/// primitive count (u16), buffers and primitive ranges. Scalars are little endian.
	struct MeshFormat
	{
		enum : uint32_t
		{
			VertexCompressed = 0x1,
			IndexCompressed  = 0x2,
			Index32          = 0x4,
		};
	};

	/// Mesh instance. Borrows the handle; copies and removal do not change ownership.
	struct MeshComponent
	{
		MeshHandle handle;
		const MaterialHandle* materials = NULL; //!< Borrowed overrides indexed by mesh material slot.
		uint16_t numMaterials = 0; //!< Keep the array and referenced materials alive while rendering.
	};

	/// World-space mesh transform. An absent component means identity.
	struct TransformComponent
	{
		Vec3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		Vec3 scale = { 1.0f, 1.0f, 1.0f };
	};

	/// Directional illumination, independent of any sky.
	struct DirectionalLightComponent
	{
		Vec3 direction = { 0.0f, 1.0f, 0.0f }; //!< World direction toward the light; must be finite and nonzero.
		Vec3 color = { 1.0f, 1.0f, 1.0f };      //!< Linear RGB illumination, clamped nonnegative.
		float intensity = 1.0f;                //!< Nonnegative multiplier; zero disables the light.
	};

	/// Procedural Perez atmosphere. Use one SkyComponent for the active view.
	/// PreUpdate advances time and calculates sunlight; this component does not create a light.
	/// Application Update systems can copy sunDirection and sunColor into a DirectionalLightComponent.
	struct SkyComponent
	{
		enum Month
		{
			January, February, March, April, May, June,
			July, August, September, October, November, December,
		};

		float time = 12.0f;         //!< Hours, wrapped into [0, 24).
		float hps = 0.0f;			//!< Sky hours per second;
		Month month = June;
		float latitude = 50.0f;     //!< Degrees, north positive.
		float turbidity = 2.15f;    //!< Atmospheric haziness, [1.9, 10].
		float sunSize = 0.02f;
		float sunBloom = 3.0f;
		Vec3 sunTint = { 1.0f, 1.0f, 1.0f }; //!< Linear RGB multiplier.
		float sunIntensity = 1.0f;  //!< Multiplier for direct sunlight and sun disk.

		/// Calculated during PreUpdate. Unit world direction toward the sun.
		Vec3 sunDirection = { 0.0f, 1.0f, 0.0f };

		/// Calculated linear RGB sunlight, including sunTint and sunIntensity.
		Vec3 sunColor = { 0.0f, 0.0f, 0.0f };
	};

	/// Create a mesh from a cooked asset after graphics startup. Always consumes
	/// _mem, including on failure. Returns one owned reference, or an invalid handle.
	/// MeshComponent instances borrow this reference; component copies and removal
	/// do not change its count. Call on the application thread.
	MeshHandle createMesh(const Memory* _mem, Error* _err = ErrorIgnore{});

	/// Retain one reference to a live mesh. Handle copies do not retain it.
	void addRef(MeshHandle _handle);

	/// Release one reference. Zero releases the mesh and its GPU buffers.
	/// An invalid handle is ignored. Release owned references before graphics stops.
	void release(MeshHandle _handle);

	struct CameraProjection
	{
		enum Enum
		{
			Perspective,
			Orthographic,

			Count
		};
	};

	/// Camera pose and projection. Aspect ratio follows the drawable dimensions.
	///
	struct CameraComponent
	{
		/// World position and unit orientation. Identity looks along +Z, with +Y up.
		///
		Vec3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };

		CameraProjection::Enum projection = CameraProjection::Perspective;

		/// Vertical field of view in degrees, used by perspective projection.
		///
		float fov = 90.0f;

		/// Visible vertical span in world units, used by orthographic projection.
		///
		float orthographicHeight = 10.0f;

		/// Clipping distances. Require 0 < nearPlane < farPlane.
		///
		float nearPlane = 0.1f;
		float farPlane = 1000.0f;

		/// Manual exposure in stops. Increasing by one doubles image brightness.
		float exposure = -3.0f;
	};

	/// Renderer camera selection. Assign another camera entity to switch views.
	/// An unset, stale, or non-camera entity disables scene rendering.
	///
	struct ActiveCameraResource
	{
		Entity entity;
	};

} // namespace frost

namespace frost
{
	/// Screen-space UI root. Coordinates are drawable pixels divided by scale.
	/// Children use the normal ECS hierarchy; higher layers draw last.
	struct UiCanvasComponent
	{
		float scale = 1.0f;
		int32_t layer = 0;
		bool visible = true;
	};

	/// Rectangle relative to its parent. Zero dimensions measure text/layout content.
	/// Anchors and pivots are fractions of the parent and own dimensions respectively.
	/// A canvas without a node fills the drawable area. Hidden nodes hide their subtree.
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
		int32_t order = 0; //!< Sibling order; entity ID breaks ties.
	};

	struct UiLayoutComponent
	{
		enum Direction { Column, Row };
		Direction direction = Column;
		float padding = 0.0f;
		float spacing = 0.0f;
	};

	/// UI colors are packed 0xRRGGBBAA, with straight alpha.
	struct UiRectComponent
	{
		uint32_t color = 0xffffffff;
		uint32_t outlineColor = 0x000000ff;
		float outlineWidth = 0.0f;
	};

	/// Owned text using the built-in 8x8 bitmap font. Supports ASCII and newlines.
	/// Height also sets the monospaced glyph width. Font assets can extend this later.
	struct UiTextComponent
	{
		FixedString1024 text;
		uint32_t color = 0xffffffff;
		float height = 16.0f;
	};

	struct UiProgressBarComponent
	{
		float value = 0.0f;
		float minimum = 0.0f;
		float maximum = 1.0f;
		uint32_t backgroundColor = 0x303030ff;
		uint32_t fillColor = 0x80c0ffff;
	};

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

	struct GraphicsDebugView
	{
		enum Enum
		{
			None, BaseColor, Normals, Depth, LinearDepth, WorldPosition, Luminance,
			Metallic, Roughness, Occlusion, Emissive,
			Count
		};
	};

	/// Latest available bgfx measurements. Negative GPU times mean unavailable.
	struct GraphicsStatsResource
	{
		double gpuMs = -1.0;
		double geometryMs = -1.0, lightMs = -1.0, skyMs = -1.0;
		double toneMapMs = -1.0, debugViewMs = -1.0, uiMs = -1.0;
		uint32_t draws = 0;
		uint16_t textures = 0;
		int64_t renderTargetBytes = 0;
	};

	struct GraphicsResource
	{
		/// Initial render width in pixels.
		///
		uint32_t width = 1280;

		/// Initial render height in pixels.
		///
		uint32_t height = 720;

		/// Renderer backend graphics API .
		///
		RendererType::Enum type = RendererType::Count;

		/// Maximum number of simultaneously loaded meshes.
		///
		/// TODO: Define macros for static arrays and make them synced with BGFX_CONFIG_ macros
		///
		uint16_t maxMeshes = 1024;
		uint16_t maxTextures = 1024;
		uint16_t maxMaterials = 1024;

		GraphicsDebugView::Enum debugView = GraphicsDebugView::None;
		float debugDepth = 100.0f; //!< View-space distance mapped to white in the linear depth view.
		bool preventBanding = true;
		bool gpuProfiling = true;
	};

	struct GraphicsPlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_H_HEADER_GUARD
