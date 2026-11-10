/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <device.h>

#include <SDL3/SDL.h>

namespace frost
{
	static_assert(Input::kNumScancodes == SDL_SCANCODE_COUNT,
		"Input must have storage for every SDL scancode.");

	static void initializeNativeWindow(ResMutT<Display>& _display, SDL_Window* _window)
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

	struct SdlControl
	{
		App* app = NULL;
	};

	struct SdlSystem
	{
		static void startup(ResT<SdlControl> _control, ResT<DeviceResource> _device, ResMutT<Display> _display, ResMutT<Input> _input)
		{
			if (!SDL_Init(SDL_INIT_VIDEO))
			{
				debugPrintf("Failed to initialize SDL: %s\n", SDL_GetError());
				_control->app->exit();

				return;
			}

			SDL_Window* window = SDL_CreateWindow(_device->title,
				((int32_t)(_device->width)),
				((int32_t)(_device->height)),
				SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

			if (NULL == window)
			{
				debugPrintf("Failed to create SDL window: %s\n", SDL_GetError());
				SDL_Quit();
				_control->app->exit();

				return;
			}

			_display->window = window;

			// Platform initialization
			initializeNativeWindow(_display, window);

			if (NULL == _display->nativeWindow)
			{
				debugPrintf("SDL did not provide a native window handle: %s\n", SDL_GetError());
				_control->app->exit();
			}
		}

		static void update(ResT<SdlControl> _control, ResMutT<Display> _display, ResMutT<Input> _input)
		{
			SDL_Window* window = ((SDL_Window*)(_display->window));
			const SDL_WindowID windowId = NULL == window ? 0 : SDL_GetWindowID(window);

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
						const uint32_t key = ((uint32_t)(event.key.scancode));

						if (key < Input::kNumScancodes)
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
						const uint32_t key = ((uint32_t)(event.key.scancode));

						if (key < Input::kNumScancodes)
						{
							_input->keys[key] = false;
							_input->released[key] = true;
						}

						break;
					}

					case SDL_EVENT_MOUSE_BUTTON_DOWN:
						_input->mouseButtons |= SDL_BUTTON_MASK(event.button.button);
						break;
					case SDL_EVENT_MOUSE_BUTTON_UP:
						_input->mouseButtons &= ~SDL_BUTTON_MASK(event.button.button);
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

		static void shutdown(ResMutT<Display> _display)
		{
			SDL_Window* window = ((SDL_Window*)(_display->window));

			if (NULL != window)
			{
				SDL_DestroyWindow(window);
			}

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
		_app.insertResource<SdlControl>(SdlControl{ &_app }); // For exiting application

		_app.insertResource<Display>();
		_app.insertResource<Input>();

		// Systems
		_app.addSystems(EcsStage::Startup, SdlSystem::startup);
		_app.addSystems(EcsStage::PreUpdate, SdlSystem::update);
		_app.addSystems(EcsStage::Shutdown, SdlSystem::shutdown);

		// Runner
		_app.setRunner([](App& _app, void*)
		{
			while (!_app.shouldExit())
			{
				if (!_app.update())
				{
					break;
				}
			}
		});
	}

} // namespace frost
