/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <device.h>
#include <frost/timer.h>
#include <frost/os.h>

#include <SDL3/SDL.h>

namespace frost
{
	// Translate backend codes at the device boundary; public indices belong to Frost.
	static Key::Enum toKey(SDL_Scancode _scancode)
	{
		switch (_scancode)
		{
			case SDL_SCANCODE_A: return Key::A;
			case SDL_SCANCODE_B: return Key::B;
			case SDL_SCANCODE_C: return Key::C;
			case SDL_SCANCODE_D: return Key::D;
			case SDL_SCANCODE_E: return Key::E;
			case SDL_SCANCODE_F: return Key::F;
			case SDL_SCANCODE_G: return Key::G;
			case SDL_SCANCODE_H: return Key::H;
			case SDL_SCANCODE_I: return Key::I;
			case SDL_SCANCODE_J: return Key::J;
			case SDL_SCANCODE_K: return Key::K;
			case SDL_SCANCODE_L: return Key::L;
			case SDL_SCANCODE_M: return Key::M;
			case SDL_SCANCODE_N: return Key::N;
			case SDL_SCANCODE_O: return Key::O;
			case SDL_SCANCODE_P: return Key::P;
			case SDL_SCANCODE_Q: return Key::Q;
			case SDL_SCANCODE_R: return Key::R;
			case SDL_SCANCODE_S: return Key::S;
			case SDL_SCANCODE_T: return Key::T;
			case SDL_SCANCODE_U: return Key::U;
			case SDL_SCANCODE_V: return Key::V;
			case SDL_SCANCODE_W: return Key::W;
			case SDL_SCANCODE_X: return Key::X;
			case SDL_SCANCODE_Y: return Key::Y;
			case SDL_SCANCODE_Z: return Key::Z;
			case SDL_SCANCODE_1: return Key::Num1;
			case SDL_SCANCODE_2: return Key::Num2;
			case SDL_SCANCODE_3: return Key::Num3;
			case SDL_SCANCODE_4: return Key::Num4;
			case SDL_SCANCODE_5: return Key::Num5;
			case SDL_SCANCODE_6: return Key::Num6;
			case SDL_SCANCODE_7: return Key::Num7;
			case SDL_SCANCODE_8: return Key::Num8;
			case SDL_SCANCODE_9: return Key::Num9;
			case SDL_SCANCODE_0: return Key::Num0;
			case SDL_SCANCODE_RETURN: return Key::Enter;
			case SDL_SCANCODE_ESCAPE: return Key::Escape;
			case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
			case SDL_SCANCODE_TAB: return Key::Tab;
			case SDL_SCANCODE_SPACE: return Key::Space;
			case SDL_SCANCODE_MINUS: return Key::Minus;
			case SDL_SCANCODE_EQUALS: return Key::Equals;
			case SDL_SCANCODE_LEFTBRACKET: return Key::LeftBracket;
			case SDL_SCANCODE_RIGHTBRACKET: return Key::RightBracket;
			case SDL_SCANCODE_BACKSLASH: return Key::Backslash;
			case SDL_SCANCODE_NONUSHASH: return Key::NonUsHash;
			case SDL_SCANCODE_SEMICOLON: return Key::Semicolon;
			case SDL_SCANCODE_APOSTROPHE: return Key::Apostrophe;
			case SDL_SCANCODE_GRAVE: return Key::Grave;
			case SDL_SCANCODE_COMMA: return Key::Comma;
			case SDL_SCANCODE_PERIOD: return Key::Period;
			case SDL_SCANCODE_SLASH: return Key::Slash;
			case SDL_SCANCODE_CAPSLOCK: return Key::CapsLock;
			case SDL_SCANCODE_F1: return Key::F1;
			case SDL_SCANCODE_F2: return Key::F2;
			case SDL_SCANCODE_F3: return Key::F3;
			case SDL_SCANCODE_F4: return Key::F4;
			case SDL_SCANCODE_F5: return Key::F5;
			case SDL_SCANCODE_F6: return Key::F6;
			case SDL_SCANCODE_F7: return Key::F7;
			case SDL_SCANCODE_F8: return Key::F8;
			case SDL_SCANCODE_F9: return Key::F9;
			case SDL_SCANCODE_F10: return Key::F10;
			case SDL_SCANCODE_F11: return Key::F11;
			case SDL_SCANCODE_F12: return Key::F12;
			case SDL_SCANCODE_PRINTSCREEN: return Key::PrintScreen;
			case SDL_SCANCODE_SCROLLLOCK: return Key::ScrollLock;
			case SDL_SCANCODE_PAUSE: return Key::Pause;
			case SDL_SCANCODE_INSERT: return Key::Insert;
			case SDL_SCANCODE_HOME: return Key::Home;
			case SDL_SCANCODE_PAGEUP: return Key::PageUp;
			case SDL_SCANCODE_DELETE: return Key::Delete;
			case SDL_SCANCODE_END: return Key::End;
			case SDL_SCANCODE_PAGEDOWN: return Key::PageDown;
			case SDL_SCANCODE_RIGHT: return Key::Right;
			case SDL_SCANCODE_LEFT: return Key::Left;
			case SDL_SCANCODE_DOWN: return Key::Down;
			case SDL_SCANCODE_UP: return Key::Up;
			case SDL_SCANCODE_NUMLOCKCLEAR: return Key::NumLock;
			case SDL_SCANCODE_KP_DIVIDE: return Key::KeypadDivide;
			case SDL_SCANCODE_KP_MULTIPLY: return Key::KeypadMultiply;
			case SDL_SCANCODE_KP_MINUS: return Key::KeypadMinus;
			case SDL_SCANCODE_KP_PLUS: return Key::KeypadPlus;
			case SDL_SCANCODE_KP_ENTER: return Key::KeypadEnter;
			case SDL_SCANCODE_KP_1: return Key::Keypad1;
			case SDL_SCANCODE_KP_2: return Key::Keypad2;
			case SDL_SCANCODE_KP_3: return Key::Keypad3;
			case SDL_SCANCODE_KP_4: return Key::Keypad4;
			case SDL_SCANCODE_KP_5: return Key::Keypad5;
			case SDL_SCANCODE_KP_6: return Key::Keypad6;
			case SDL_SCANCODE_KP_7: return Key::Keypad7;
			case SDL_SCANCODE_KP_8: return Key::Keypad8;
			case SDL_SCANCODE_KP_9: return Key::Keypad9;
			case SDL_SCANCODE_KP_0: return Key::Keypad0;
			case SDL_SCANCODE_KP_PERIOD: return Key::KeypadPeriod;
			case SDL_SCANCODE_NONUSBACKSLASH: return Key::NonUsBackslash;
			case SDL_SCANCODE_APPLICATION: return Key::Application;
			case SDL_SCANCODE_POWER: return Key::Power;
			case SDL_SCANCODE_KP_EQUALS: return Key::KeypadEquals;
			case SDL_SCANCODE_F13: return Key::F13;
			case SDL_SCANCODE_F14: return Key::F14;
			case SDL_SCANCODE_F15: return Key::F15;
			case SDL_SCANCODE_F16: return Key::F16;
			case SDL_SCANCODE_F17: return Key::F17;
			case SDL_SCANCODE_F18: return Key::F18;
			case SDL_SCANCODE_F19: return Key::F19;
			case SDL_SCANCODE_F20: return Key::F20;
			case SDL_SCANCODE_F21: return Key::F21;
			case SDL_SCANCODE_F22: return Key::F22;
			case SDL_SCANCODE_F23: return Key::F23;
			case SDL_SCANCODE_F24: return Key::F24;
			case SDL_SCANCODE_EXECUTE: return Key::Execute;
			case SDL_SCANCODE_HELP: return Key::Help;
			case SDL_SCANCODE_MENU: return Key::Menu;
			case SDL_SCANCODE_SELECT: return Key::Select;
			case SDL_SCANCODE_STOP: return Key::Stop;
			case SDL_SCANCODE_AGAIN: return Key::Again;
			case SDL_SCANCODE_UNDO: return Key::Undo;
			case SDL_SCANCODE_CUT: return Key::Cut;
			case SDL_SCANCODE_COPY: return Key::Copy;
			case SDL_SCANCODE_PASTE: return Key::Paste;
			case SDL_SCANCODE_FIND: return Key::Find;
			case SDL_SCANCODE_MUTE: return Key::Mute;
			case SDL_SCANCODE_VOLUMEUP: return Key::VolumeUp;
			case SDL_SCANCODE_VOLUMEDOWN: return Key::VolumeDown;
			case SDL_SCANCODE_KP_COMMA: return Key::KeypadComma;
			case SDL_SCANCODE_KP_EQUALSAS400: return Key::KeypadEqualsAs400;
			case SDL_SCANCODE_INTERNATIONAL1: return Key::International1;
			case SDL_SCANCODE_INTERNATIONAL2: return Key::International2;
			case SDL_SCANCODE_INTERNATIONAL3: return Key::International3;
			case SDL_SCANCODE_INTERNATIONAL4: return Key::International4;
			case SDL_SCANCODE_INTERNATIONAL5: return Key::International5;
			case SDL_SCANCODE_INTERNATIONAL6: return Key::International6;
			case SDL_SCANCODE_INTERNATIONAL7: return Key::International7;
			case SDL_SCANCODE_INTERNATIONAL8: return Key::International8;
			case SDL_SCANCODE_INTERNATIONAL9: return Key::International9;
			case SDL_SCANCODE_LANG1: return Key::Language1;
			case SDL_SCANCODE_LANG2: return Key::Language2;
			case SDL_SCANCODE_LANG3: return Key::Language3;
			case SDL_SCANCODE_LANG4: return Key::Language4;
			case SDL_SCANCODE_LANG5: return Key::Language5;
			case SDL_SCANCODE_LANG6: return Key::Language6;
			case SDL_SCANCODE_LANG7: return Key::Language7;
			case SDL_SCANCODE_LANG8: return Key::Language8;
			case SDL_SCANCODE_LANG9: return Key::Language9;
			case SDL_SCANCODE_ALTERASE: return Key::AltErase;
			case SDL_SCANCODE_SYSREQ: return Key::SysReq;
			case SDL_SCANCODE_CANCEL: return Key::Cancel;
			case SDL_SCANCODE_CLEAR: return Key::Clear;
			case SDL_SCANCODE_PRIOR: return Key::Prior;
			case SDL_SCANCODE_RETURN2: return Key::Enter2;
			case SDL_SCANCODE_SEPARATOR: return Key::Separator;
			case SDL_SCANCODE_OUT: return Key::Out;
			case SDL_SCANCODE_OPER: return Key::Oper;
			case SDL_SCANCODE_CLEARAGAIN: return Key::ClearAgain;
			case SDL_SCANCODE_CRSEL: return Key::CrSel;
			case SDL_SCANCODE_EXSEL: return Key::ExSel;
			case SDL_SCANCODE_FRONT: return Key::Front;
			case SDL_SCANCODE_KP_00: return Key::Keypad00;
			case SDL_SCANCODE_KP_000: return Key::Keypad000;
			case SDL_SCANCODE_THOUSANDSSEPARATOR: return Key::ThousandsSeparator;
			case SDL_SCANCODE_DECIMALSEPARATOR: return Key::DecimalSeparator;
			case SDL_SCANCODE_CURRENCYUNIT: return Key::CurrencyUnit;
			case SDL_SCANCODE_CURRENCYSUBUNIT: return Key::CurrencySubunit;
			case SDL_SCANCODE_KP_LEFTPAREN: return Key::KeypadLeftParen;
			case SDL_SCANCODE_KP_RIGHTPAREN: return Key::KeypadRightParen;
			case SDL_SCANCODE_KP_LEFTBRACE: return Key::KeypadLeftBrace;
			case SDL_SCANCODE_KP_RIGHTBRACE: return Key::KeypadRightBrace;
			case SDL_SCANCODE_KP_TAB: return Key::KeypadTab;
			case SDL_SCANCODE_KP_BACKSPACE: return Key::KeypadBackspace;
			case SDL_SCANCODE_KP_A: return Key::KeypadA;
			case SDL_SCANCODE_KP_B: return Key::KeypadB;
			case SDL_SCANCODE_KP_C: return Key::KeypadC;
			case SDL_SCANCODE_KP_D: return Key::KeypadD;
			case SDL_SCANCODE_KP_E: return Key::KeypadE;
			case SDL_SCANCODE_KP_F: return Key::KeypadF;
			case SDL_SCANCODE_KP_XOR: return Key::KeypadXor;
			case SDL_SCANCODE_KP_POWER: return Key::KeypadPower;
			case SDL_SCANCODE_KP_PERCENT: return Key::KeypadPercent;
			case SDL_SCANCODE_KP_LESS: return Key::KeypadLess;
			case SDL_SCANCODE_KP_GREATER: return Key::KeypadGreater;
			case SDL_SCANCODE_KP_AMPERSAND: return Key::KeypadAmpersand;
			case SDL_SCANCODE_KP_DBLAMPERSAND: return Key::KeypadDoubleAmpersand;
			case SDL_SCANCODE_KP_VERTICALBAR: return Key::KeypadVerticalBar;
			case SDL_SCANCODE_KP_DBLVERTICALBAR: return Key::KeypadDoubleVerticalBar;
			case SDL_SCANCODE_KP_COLON: return Key::KeypadColon;
			case SDL_SCANCODE_KP_HASH: return Key::KeypadHash;
			case SDL_SCANCODE_KP_SPACE: return Key::KeypadSpace;
			case SDL_SCANCODE_KP_AT: return Key::KeypadAt;
			case SDL_SCANCODE_KP_EXCLAM: return Key::KeypadExclamation;
			case SDL_SCANCODE_KP_MEMSTORE: return Key::KeypadMemoryStore;
			case SDL_SCANCODE_KP_MEMRECALL: return Key::KeypadMemoryRecall;
			case SDL_SCANCODE_KP_MEMCLEAR: return Key::KeypadMemoryClear;
			case SDL_SCANCODE_KP_MEMADD: return Key::KeypadMemoryAdd;
			case SDL_SCANCODE_KP_MEMSUBTRACT: return Key::KeypadMemorySubtract;
			case SDL_SCANCODE_KP_MEMMULTIPLY: return Key::KeypadMemoryMultiply;
			case SDL_SCANCODE_KP_MEMDIVIDE: return Key::KeypadMemoryDivide;
			case SDL_SCANCODE_KP_PLUSMINUS: return Key::KeypadPlusMinus;
			case SDL_SCANCODE_KP_CLEAR: return Key::KeypadClear;
			case SDL_SCANCODE_KP_CLEARENTRY: return Key::KeypadClearEntry;
			case SDL_SCANCODE_KP_BINARY: return Key::KeypadBinary;
			case SDL_SCANCODE_KP_OCTAL: return Key::KeypadOctal;
			case SDL_SCANCODE_KP_DECIMAL: return Key::KeypadDecimal;
			case SDL_SCANCODE_KP_HEXADECIMAL: return Key::KeypadHexadecimal;
			case SDL_SCANCODE_LCTRL: return Key::LeftControl;
			case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
			case SDL_SCANCODE_LALT: return Key::LeftAlt;
			case SDL_SCANCODE_LGUI: return Key::LeftSuper;
			case SDL_SCANCODE_RCTRL: return Key::RightControl;
			case SDL_SCANCODE_RSHIFT: return Key::RightShift;
			case SDL_SCANCODE_RALT: return Key::RightAlt;
			case SDL_SCANCODE_RGUI: return Key::RightSuper;
			case SDL_SCANCODE_MODE: return Key::Mode;
			case SDL_SCANCODE_SLEEP: return Key::Sleep;
			case SDL_SCANCODE_WAKE: return Key::Wake;
			case SDL_SCANCODE_CHANNEL_INCREMENT: return Key::ChannelIncrement;
			case SDL_SCANCODE_CHANNEL_DECREMENT: return Key::ChannelDecrement;
			case SDL_SCANCODE_MEDIA_PLAY: return Key::MediaPlay;
			case SDL_SCANCODE_MEDIA_PAUSE: return Key::MediaPause;
			case SDL_SCANCODE_MEDIA_RECORD: return Key::MediaRecord;
			case SDL_SCANCODE_MEDIA_FAST_FORWARD: return Key::MediaFastForward;
			case SDL_SCANCODE_MEDIA_REWIND: return Key::MediaRewind;
			case SDL_SCANCODE_MEDIA_NEXT_TRACK: return Key::MediaNextTrack;
			case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK: return Key::MediaPreviousTrack;
			case SDL_SCANCODE_MEDIA_STOP: return Key::MediaStop;
			case SDL_SCANCODE_MEDIA_EJECT: return Key::MediaEject;
			case SDL_SCANCODE_MEDIA_PLAY_PAUSE: return Key::MediaPlayPause;
			case SDL_SCANCODE_MEDIA_SELECT: return Key::MediaSelect;
			case SDL_SCANCODE_AC_NEW: return Key::AppNew;
			case SDL_SCANCODE_AC_OPEN: return Key::AppOpen;
			case SDL_SCANCODE_AC_CLOSE: return Key::AppClose;
			case SDL_SCANCODE_AC_EXIT: return Key::AppExit;
			case SDL_SCANCODE_AC_SAVE: return Key::AppSave;
			case SDL_SCANCODE_AC_PRINT: return Key::AppPrint;
			case SDL_SCANCODE_AC_PROPERTIES: return Key::AppProperties;
			case SDL_SCANCODE_AC_SEARCH: return Key::BrowserSearch;
			case SDL_SCANCODE_AC_HOME: return Key::BrowserHome;
			case SDL_SCANCODE_AC_BACK: return Key::BrowserBack;
			case SDL_SCANCODE_AC_FORWARD: return Key::BrowserForward;
			case SDL_SCANCODE_AC_STOP: return Key::BrowserStop;
			case SDL_SCANCODE_AC_REFRESH: return Key::BrowserRefresh;
			case SDL_SCANCODE_AC_BOOKMARKS: return Key::BrowserBookmarks;
			case SDL_SCANCODE_SOFTLEFT: return Key::SoftLeft;
			case SDL_SCANCODE_SOFTRIGHT: return Key::SoftRight;
			case SDL_SCANCODE_CALL: return Key::Call;
			case SDL_SCANCODE_ENDCALL: return Key::EndCall;
			default: return Key::Unknown;
		}
	}

	static uint32_t toMouseButton(uint8_t _button)
	{
		switch (_button)
		{
			case SDL_BUTTON_LEFT:   return MouseButton::Left;
			case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
			case SDL_BUTTON_RIGHT:  return MouseButton::Right;
			case SDL_BUTTON_X1:     return MouseButton::Extra1;
			case SDL_BUTTON_X2:     return MouseButton::Extra2;
			default:               return MouseButton::None;
		}
	}

	static void initializeNativeWindow(ResMutT<DisplayResource>& _display, SDL_Window* _window)
	{
		const SDL_PropertiesID properties = SDL_GetWindowProperties(_window);

#if FROST_PLATFORM_WINDOWS
		_display->nativeWindow =
			SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#elif FROST_PLATFORM_LINUX
		_display->nativeWindow =
			SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);

		if (NULL != _display->nativeWindow)
		{
			_display->nativeDisplay =
				SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
			_display->nativeWindowHandleType = NativeWindowHandleType::Wayland;
		}
		else
		{
			_display->nativeWindow = ((void*)(((uintptr_t)(SDL_GetNumberProperty(
				properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)))));
			_display->nativeDisplay =
				SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
		}
#elif FROST_PLATFORM_OSX
		_display->nativeWindow =
			SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
#elif FROST_PLATFORM_IOS
		_display->nativeWindow =
			SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
#elif FROST_PLATFORM_ANDROID
		_display->nativeWindow =
			SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);
#elif FROST_PLATFORM_EMSCRIPTEN
		_display->nativeWindow = ((char*)(SDL_GetStringProperty(
			properties, SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING, "#canvas")));
#else
#error Unsupported native window platform.
#endif

		int32_t width = 0;
		int32_t height = 0;
		SDL_GetWindowSizeInPixels(_window, &width, &height);
		_display->width = ((uint32_t)(width));
		_display->height = ((uint32_t)(height));
	}

	struct SdlControlResource
	{
		App* app = NULL;
	};

	struct SdlSystem
	{
		static void startup(ResT<DeviceResource> _device, ResMutT<DisplayResource> _display)
		{
			if (!SDL_Init(SDL_INIT_VIDEO))
			{
				debugPrintf("Failed to initialize SDL: %s\n", SDL_GetError());
				frost::exit(kExitFailure, false);
			}

			SDL_Window* window = SDL_CreateWindow(_device->title,
				((int32_t)(_device->width)),
				((int32_t)(_device->height)),
				SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

			if (NULL == window)
			{
				debugPrintf("Failed to create SDL window: %s\n", SDL_GetError());
				frost::exit(kExitFailure, false);
			}

			_display->window = window;

			// Platform initialization
			initializeNativeWindow(_display, window);

			if (NULL == _display->nativeWindow)
			{
				debugPrintf("SDL did not provide a native window handle: %s\n", SDL_GetError());
				frost::exit(kExitFailure, false);
			}
		}

		static void update(ResT<SdlControlResource> _control, ResMutT<DisplayResource> _display, ResMutT<InputResource> _input)
		{
			SDL_Window* window = ((SDL_Window*)(_display->window));
			const SDL_WindowID windowId = SDL_GetWindowID(window);

			frost::memSet(_input->pressed, 0, sizeof(_input->pressed));
			frost::memSet(_input->released, 0, sizeof(_input->released));

			_input->mouseDeltaX = 0;
			_input->mouseDeltaY = 0;
			_input->wheel = 0;

			SDL_Event event;

			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
					case SDL_EVENT_QUIT:
					{
						_control->app->exit();
						break;
					}

					case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
					{
						if (event.window.windowID == windowId)
						{
							_control->app->exit();
						}

						break;
					}

					case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
					{
						if (event.window.windowID == windowId)
						{
							_display->width = ((uint32_t)(event.window.data1));
							_display->height = ((uint32_t)(event.window.data2));
						}

						break;
					}

					case SDL_EVENT_KEY_DOWN:
					{
						const Key::Enum key = toKey(event.key.scancode);

						if (Key::Unknown != key)
						{
							if (!_input->keys[key])
							{
								_input->pressed[key] = true;
							}

							_input->keys[key] = true;
						}

						break;
					}

					case SDL_EVENT_KEY_UP:
					{
						const Key::Enum key = toKey(event.key.scancode);

						if (Key::Unknown != key)
						{
							_input->keys[key] = false;
							_input->released[key] = true;
						}

						break;
					}

					case SDL_EVENT_MOUSE_BUTTON_DOWN:
						_input->mouseButtons |= toMouseButton(event.button.button);
						break;
					case SDL_EVENT_MOUSE_BUTTON_UP:
						_input->mouseButtons &= ~toMouseButton(event.button.button);
						break;
					case SDL_EVENT_WINDOW_FOCUS_LOST:
						_input->focused = false;
						_input->mouseButtons = 0;
						frost::memSet(_input->keys, 0, sizeof(_input->keys));
						break;
					case SDL_EVENT_WINDOW_FOCUS_GAINED:
						_input->focused = true;
						break;
					case SDL_EVENT_MOUSE_MOTION:
					{
						_input->mouseDeltaX += ((int32_t)(event.motion.xrel));
						_input->mouseDeltaY += ((int32_t)(event.motion.yrel));
						_input->mouseX = ((int32_t)(event.motion.x));
						_input->mouseY = ((int32_t)(event.motion.y));

						break;
					}

					case SDL_EVENT_MOUSE_WHEEL:
					{
						_input->wheel += event.wheel.integer_y;
						break;
					}
				}
			}
		}

		static void shutdown(ResMutT<DisplayResource> _display)
		{
			SDL_Window* window = ((SDL_Window*)(_display->window));

			SDL_DestroyWindow(window);

			_display->window = NULL;
			_display->nativeWindow = NULL;
			_display->nativeDisplay = NULL;
			_display->width = 0;
			_display->height = 0;

			SDL_Quit();
		}
	};

} // namespace frost

namespace frost
{
	void DevicePlugin::build(App& _app)
	{
		// Resources
		_app.insertResource<SdlControlResource>(SdlControlResource{ &_app }); // For exiting application

		_app.insertResource<DisplayResource>();
		_app.insertResource<InputResource>();

		// Systems
		_app.addSystems(EcsStage::Startup, SdlSystem::startup);
		_app.addSystems(EcsStage::PreUpdate, SdlSystem::update);
		_app.addSystems(EcsStage::PostShutdown, SdlSystem::shutdown);

		// Runner
		_app.setRunner([](App& _app, void*)
		{
			// Run startup with zero delta before starting the frame clock.
			if (!_app.update())
			{
				return;
			}

			Ticks previous = getNow();
			while (!_app.shouldExit())
			{
				const Ticks now = getNow();
				const double delta = toSeconds<double>(now - previous);
				previous = now;

				if (!_app.update(delta))
				{
					break;
				}
			}
		});
	}

} // namespace frost
