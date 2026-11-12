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

namespace frost
{
	FROST_HANDLE(MeshHandle);

	FROST_ERROR_RESULT(kErrorMeshCreate, FROST_MAKEFOURCC('m', 's', 'h', 'c') );
	FROST_ERROR_RESULT(kErrorMeshFormat, FROST_MAKEFOURCC('m', 's', 'h', 'f') );

	constexpr uint32_t kAssetTypeMesh = FROST_MAKEFOURCC('M', 'E', 'S', 'H');
	constexpr uint32_t kMeshVersion   = 1;

	/// Mesh payload buffer encodings. All scalar fields are little endian.
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
	};

	/// World-space mesh transform. An absent component means identity.
	struct TransformComponent
	{
		Vec3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		Vec3 scale = { 1.0f, 1.0f, 1.0f };
	};

	/// Procedural Perez atmosphere and its directional sunlight. Use one SkyComponent for
	/// the active view. SkySystem advances time at the configured hps.
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
		float exposure = 0.1f;      //!< Linear exposure for sky and surface sunlight.
		bool preventBanding = true;
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
		/// TODO: Define macros for static arrays
		///
		uint16_t maxMeshes = 1024;
	};

	struct GraphicsPlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_H_HEADER_GUARD
