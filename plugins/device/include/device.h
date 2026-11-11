/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_PLUGIN_DEVICE_H_HEADER_GUARD
#define FROST_PLUGIN_DEVICE_H_HEADER_GUARD

#include <frost/error.h>
#include <frost/ecs.h>

namespace frost
{
	/// Platform convention used by the native window handle.
	///
	struct NativeWindowHandleType
	{
		enum Enum
		{
			Default, //!< Default platform window handle convention.
			Wayland, //!< Wayland surface and display handles.

			Count //!< Number of handle conventions.
		};
	};

	/// Window handles and drawable dimensions maintained by DevicePlugin.
	///
	/// Handles are borrowed from and remain valid until the device shuts down.
	///
	struct DisplayResource
	{
		/// Borrowed internal window pointer.
		///
		void* window = NULL;

		/// Platform window/surface handle supplied to the renderer.
		///
		void* nativeWindow = NULL;

		/// Borrowed native display connection when required by the platform.
		///
		void* nativeDisplay = NULL;

		/// Interpretation of nativeWindow and nativeDisplay.
		///
		NativeWindowHandleType::Enum nativeWindowHandleType = NativeWindowHandleType::Default;

		/// Drawable framebuffer width in pixels.
		///
		uint32_t width = 0;

		/// Drawable framebuffer height in pixels.
		///
		uint32_t height = 0;
	};

	/// Keyboard and mouse state sampled during PreUpdate.
	///
	/// pressed, released, relative motion and wheel values are reset before event pumping.
	///
	struct InputResource
	{
		/// SDL scancode storage capacity.
		///
		static constexpr uint32_t kNumScancodes = 512;

		/// Held keyboard state indexed by SDL scancode.
		///
		bool keys[kNumScancodes] = {};

		/// Keys newly pressed during the current event pump.
		///
		bool pressed[kNumScancodes] = {};

		/// Keys released during the current event pump.
		///
		bool released[kNumScancodes] = {};

		/// Held mouse buttons as an SDL button bitmask.
		///
		uint32_t mouseButtons = 0;

		/// Whether the window currently has input focus.
		///
		bool focused = true;

		/// Window-relative cursor X position.
		///
		int32_t mouseX = 0;

		/// Window-relative cursor Y position.
		///
		int32_t mouseY = 0;

		/// Accumulated horizontal motion this frame.
		///
		int32_t mouseDeltaX = 0;

		/// Accumulated vertical motion this frame.
		///
		int32_t mouseDeltaY = 0;

		/// Accumulated vertical wheel steps this frame.
		///
		int32_t wheel = 0;
	};

} // namespace frost

namespace frost
{
	/// Initial device configuration, read during Startup.
	///
	struct DeviceResource
	{
		/// Window title, read during Startup.
		///
		const char* title = "frost";

		/// Initial window width in screen coordinates.
		///
		uint32_t width = 1280;

		/// Initial window height in screen coordinates.
		///
		uint32_t height = 720;
	};

	/// Creates the window and registers device/input resources and systems.
	///
	/// Adds DisplayResource and InputResource. Startup creates the window, PreUpdate pumps
	/// events, and Shutdown destroys the window and releases video state.
	///
	struct DevicePlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // FROST_PLUGIN_DEVICE_H_HEADER_GUARD
