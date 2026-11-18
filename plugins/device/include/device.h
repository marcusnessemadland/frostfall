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

			Count
		};
	};

	/// Window handles and drawable dimensions maintained by DevicePlugin.
	///
	/// @remark Handles are borrowed from and remain valid until the device shuts down.
	///
	struct DisplayResource
	{
		void* window = NULL;
		void* nativeWindow = NULL;
		void* nativeDisplay = NULL;
		NativeWindowHandleType::Enum nativeWindowHandleType = NativeWindowHandleType::Default;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	/// Physical keyboard keys, independent of keyboard layout and input backend.
	///
	/// @remark Letter names identify positions on a US keyboard; these codes are not text input.
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

			Count
		};
	};

	/// Flags for mouse keys.
	///
	struct MouseButton
	{
		enum Enum
		{
			None   = 0,
			Left   = 1 << 0,
			Middle = 1 << 1,
			Right  = 1 << 2,
			Extra1 = 1 << 3,
			Extra2 = 1 << 4,
		};
	};

	/// Standard gamepad buttons, independent of the input backend.
	///
	struct GamepadButton
	{
		enum Enum
		{
			South,
			East,
			West,
			North,
			Back,
			Guide,
			Start,
			LeftStick,
			RightStick,
			LeftShoulder,
			RightShoulder,
			DpadUp,
			DpadDown,
			DpadLeft,
			DpadRight,

			Count
		};
	};

	/// One active gamepad.
	///
	/// @remark Sticks have a radial deadzone and range [-1, 1].
	/// @remark Positive X is right and positive Y is down. Triggers range from 0 to 1.
	///
	struct GamepadState
	{
		bool connected = false;

		float leftX = 0.0f;
		float leftY = 0.0f;
		float rightX = 0.0f;
		float rightY = 0.0f;

		float leftTrigger = 0.0f;
		float rightTrigger = 0.0f;

		bool buttons[GamepadButton::Count] = {};
		bool pressed[GamepadButton::Count] = {};
		bool released[GamepadButton::Count] = {};
	};

	/// Keyboard, mouse and gamepad state sampled during PreUpdate.
	///
	/// @remark Pressed, released, relative motion and wheel values are reset before event pumping.
	///
	struct InputResource
	{
		GamepadState gamepad;

		bool keys[Key::Count] = {};
		bool pressed[Key::Count] = {};
		bool released[Key::Count] = {};

		uint32_t mouseButtons = 0;

		bool focused = true;

		int32_t mouseX = 0;
		int32_t mouseY = 0;

		int32_t mouseDeltaX = 0;
		int32_t mouseDeltaY = 0;

		int32_t wheel = 0;
	};

} // namespace frost

namespace frost
{
	/// Const device configuration read on startup.
	///
	struct DeviceDesc
	{
		const char* title = "frost"; //!< Window title, read during Startup.
		uint32_t width = 1280; 		 //!< Initial window width in screen coordinates.
		uint32_t height = 720; 		 //!< Initial window height in screen coordinates.
	};

	/// Runtime device configuration read every frame.
	///
	/// @remark Available after plugin is built.
	///
	struct DeviceSettings
	{
		// TODO:
	};

	/// Creates the window and registers device/input resources and systems.
	///
	/// @remark Insert `DeviceDesc` before plugin is added to application.
	///
	struct DevicePlugin : PluginI
	{
		virtual void build(App& _app) override;
	};

} // namespace frost

#endif // FROST_PLUGIN_DEVICE_H_HEADER_GUARD
