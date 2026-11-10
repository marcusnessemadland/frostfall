/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef BX_PLUGIN_GRAPHICS_H_HEADER_GUARD
#define BX_PLUGIN_GRAPHICS_H_HEADER_GUARD

#include <frost/ecs.h>
#include <frost/math.h>

namespace frost
{
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
	struct Camera
	{
		/// World position and unit orientation. Identity looks along +Z, with +Y up.
		///
		Vec3 position = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };

		CameraProjection::Enum projection = CameraProjection::Perspective;

		/// Vertical field of view in radians, used by perspective projection.
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
	struct ActiveCamera
	{
		Entity entity;
	};

} // namespace frost

namespace frost
{
	struct GraphicsResource
	{
		/// Initial render width in pixels.
		///
		uint32_t width = 1280;

		/// Initial render height in pixels.
		///
		uint32_t height = 720;
	};

	struct GraphicsPlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // BX_PLUGIN_GRAPHICS_H_HEADER_GUARD
