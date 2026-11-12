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

	/// Physical keyboard keys, independent of keyboard layout and input backend.
	/// Letter names identify positions on a US keyboard; these codes are not text input.
	///
	struct Key
	{
		enum Enum
		{
			Unknown,
			A,
			B,
			C,
			D,
			E,
			F,
			G,
			H,
			I,
			J,
			K,
			L,
			M,
			N,
			O,
			P,
			Q,
			R,
			S,
			T,
			U,
			V,
			W,
			X,
			Y,
			Z,
			Num1,
			Num2,
			Num3,
			Num4,
			Num5,
			Num6,
			Num7,
			Num8,
			Num9,
			Num0,
			Enter,
			Escape,
			Backspace,
			Tab,
			Space,
			Minus,
			Equals,
			LeftBracket,
			RightBracket,
			Backslash,
			NonUsHash,
			Semicolon,
			Apostrophe,
			Grave,
			Comma,
			Period,
			Slash,
			CapsLock,
			F1,
			F2,
			F3,
			F4,
			F5,
			F6,
			F7,
			F8,
			F9,
			F10,
			F11,
			F12,
			PrintScreen,
			ScrollLock,
			Pause,
			Insert,
			Home,
			PageUp,
			Delete,
			End,
			PageDown,
			Right,
			Left,
			Down,
			Up,
			NumLock,
			KeypadDivide,
			KeypadMultiply,
			KeypadMinus,
			KeypadPlus,
			KeypadEnter,
			Keypad1,
			Keypad2,
			Keypad3,
			Keypad4,
			Keypad5,
			Keypad6,
			Keypad7,
			Keypad8,
			Keypad9,
			Keypad0,
			KeypadPeriod,
			NonUsBackslash,
			Application,
			Power,
			KeypadEquals,
			F13,
			F14,
			F15,
			F16,
			F17,
			F18,
			F19,
			F20,
			F21,
			F22,
			F23,
			F24,
			Execute,
			Help,
			Menu,
			Select,
			Stop,
			Again,
			Undo,
			Cut,
			Copy,
			Paste,
			Find,
			Mute,
			VolumeUp,
			VolumeDown,
			KeypadComma,
			KeypadEqualsAs400,
			International1,
			International2,
			International3,
			International4,
			International5,
			International6,
			International7,
			International8,
			International9,
			Language1,
			Language2,
			Language3,
			Language4,
			Language5,
			Language6,
			Language7,
			Language8,
			Language9,
			AltErase,
			SysReq,
			Cancel,
			Clear,
			Prior,
			Enter2,
			Separator,
			Out,
			Oper,
			ClearAgain,
			CrSel,
			ExSel,
			Front,
			Keypad00,
			Keypad000,
			ThousandsSeparator,
			DecimalSeparator,
			CurrencyUnit,
			CurrencySubunit,
			KeypadLeftParen,
			KeypadRightParen,
			KeypadLeftBrace,
			KeypadRightBrace,
			KeypadTab,
			KeypadBackspace,
			KeypadA,
			KeypadB,
			KeypadC,
			KeypadD,
			KeypadE,
			KeypadF,
			KeypadXor,
			KeypadPower,
			KeypadPercent,
			KeypadLess,
			KeypadGreater,
			KeypadAmpersand,
			KeypadDoubleAmpersand,
			KeypadVerticalBar,
			KeypadDoubleVerticalBar,
			KeypadColon,
			KeypadHash,
			KeypadSpace,
			KeypadAt,
			KeypadExclamation,
			KeypadMemoryStore,
			KeypadMemoryRecall,
			KeypadMemoryClear,
			KeypadMemoryAdd,
			KeypadMemorySubtract,
			KeypadMemoryMultiply,
			KeypadMemoryDivide,
			KeypadPlusMinus,
			KeypadClear,
			KeypadClearEntry,
			KeypadBinary,
			KeypadOctal,
			KeypadDecimal,
			KeypadHexadecimal,
			LeftControl,
			LeftShift,
			LeftAlt,
			LeftSuper,
			RightControl,
			RightShift,
			RightAlt,
			RightSuper,
			Mode,
			Sleep,
			Wake,
			ChannelIncrement,
			ChannelDecrement,
			MediaPlay,
			MediaPause,
			MediaRecord,
			MediaFastForward,
			MediaRewind,
			MediaNextTrack,
			MediaPreviousTrack,
			MediaStop,
			MediaEject,
			MediaPlayPause,
			MediaSelect,
			AppNew,
			AppOpen,
			AppClose,
			AppExit,
			AppSave,
			AppPrint,
			AppProperties,
			BrowserSearch,
			BrowserHome,
			BrowserBack,
			BrowserForward,
			BrowserStop,
			BrowserRefresh,
			BrowserBookmarks,
			SoftLeft,
			SoftRight,
			Call,
			EndCall,

			Count //!< Number of keyboard state slots.
		};
	};

	/// Flags used by InputResource::mouseButtons.
	///
	struct MouseButton
	{
		enum Enum : uint32_t
		{
			None   = 0,
			Left   = 1 << 0,
			Middle = 1 << 1,
			Right  = 1 << 2,
			Extra1 = 1 << 3,
			Extra2 = 1 << 4,
		};
	};

	/// Keyboard and mouse state sampled during PreUpdate.
	///
	/// pressed, released, relative motion and wheel values are reset before event pumping.
	///
	struct InputResource
	{
		/// Number of physical keyboard keys.
		///
		static constexpr uint32_t kNumKeys = Key::Count;

		/// Held keyboard state indexed by Key::Enum. Unknown keys are ignored.
		///
		bool keys[kNumKeys] = {};

		/// Keys newly pressed during the current event pump, indexed by Key::Enum.
		///
		bool pressed[kNumKeys] = {};

		/// Keys released during the current event pump, indexed by Key::Enum.
		///
		bool released[kNumKeys] = {};

		/// Held mouse buttons as a bitmask of MouseButton flags.
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
