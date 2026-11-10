/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

// Engine
#include <frost/ecs.h>
#include <frost/file.h>
#include <frost/commandline.h>
#include <frost/timer.h>

// Plugins
#include <device.h>
#include <graphics.h>

// TODO: Create SDL scancodes in device.h
namespace frost
{
	// SDL scancodes and button mask, as exposed by the device Input resource.
	enum : uint32_t
	{
		KeyA = 4,
		KeyD = 7,
		KeyE = 8,
		KeyQ = 20,
		KeyS = 22,
		KeyW = 26,
		KeyLeftShift = 225,
		KeyRightShift = 229,
		MouseRight = 1 << 2,
	};
}

namespace
{
	struct FlyCameraController
	{
		float speed = 10.0f;
		float boost = 3.0f;
		float sensitivity = 0.002f;
	};

	struct FlyCameraResource
	{
		frost::Ticks previous = frost::getNow();
	};

	struct FlyCameraSystem
	{
		static void update(
			  frost::ResT<frost::Input> _input
			, frost::ResT<frost::ActiveCamera> _activeCamera
			, frost::QueryT<frost::Camera&, const FlyCameraController&> _cameras
			, frost::LocalT<FlyCameraResource> _flycamera
			)
		{
			// The device runner does not supply Time::delta yet.
			const frost::Ticks now = frost::getNow();
			const float delta = frost::min(frost::toSeconds<float>(now - _flycamera->previous), 0.1f);
			_flycamera->previous = now;

			// Dont process input if input is not in focus
			if (!_input->focused)
			{
				return;
			}

			// For each entity with Camera and FlyCameraController components...
			_cameras.get(_activeCamera->entity,
				[&](frost::Camera& _camera, const FlyCameraController& _controller)
				{
					// Rotate camera
					if (0 != (_input->mouseButtons & frost::MouseRight)
					&& (0 != _input->mouseDeltaX || 0 != _input->mouseDeltaY) )
					{
						const frost::Vec3 forward = frost::mul({ 0.0f, 0.0f, 1.0f }, _camera.rotation);

						const float yaw = frost::wrap(frost::atan2(forward.x, forward.z)
							+ float(_input->mouseDeltaX) * _controller.sensitivity, frost::kPi2);
						const float pitch = frost::clamp(frost::asin(frost::clamp(-forward.y, -1.0f, 1.0f))
							+ float(_input->mouseDeltaY) * _controller.sensitivity,
							-frost::toRad(89.0f), frost::toRad(89.0f));

						_camera.rotation = frost::normalize(frost::mul(frost::rotateY(yaw), frost::rotateX(pitch)));
					}

					// Move camera
					const frost::Vec3 forward = frost::mul({ 0.0f, 0.0f, 1.0f }, _camera.rotation);
					const frost::Vec3 right = frost::mul({ 1.0f, 0.0f, 0.0f }, _camera.rotation);

					const float forwardInput = float(_input->keys[frost::KeyW]) - float(_input->keys[frost::KeyS]);
					const float rightInput = float(_input->keys[frost::KeyD]) - float(_input->keys[frost::KeyA]);
					const float upInput = float(_input->keys[frost::KeyE]) - float(_input->keys[frost::KeyQ]);

					const frost::Vec3 movement = frost::add(
						frost::add(frost::mul(forward, forwardInput), frost::mul(right, rightInput)),
						{ 0.0f, upInput, 0.0f });

					if (0.0f < frost::dot(movement, movement))
					{
						const bool boosted = _input->keys[frost::KeyLeftShift] || _input->keys[frost::KeyRightShift];
						const float speed = _controller.speed * (boosted ? _controller.boost : 1.0f);

						_camera.position = frost::add(_camera.position, frost::mul(frost::normalize(movement), speed * delta));
					}
				});
		}
	};

	struct ExamplePlugin : frost::PluginI
	{
		static void startup(frost::World& _world, frost::ResMutT<frost::ActiveCamera> _activeCamera)
		{
			// Create flying camera entity
			const frost::Entity entity = _world.spawn(frost::Camera{}, FlyCameraController{}).id();
			_activeCamera->entity = entity; // Make sure this camera is used in the renderer
		}

		virtual void build(frost::App& _app) override
		{
			_app.addSystems(frost::EcsStage::Startup, startup);
			_app.addSystems(frost::EcsStage::Update, FlyCameraSystem::update);
		}
	};
}

static const frost::CommandLineOption s_options[] =
{
	{ 'h',  "help",   0, NULL,       "Display this help and exit." },
	{ NULL,  "width",  1, "<width>",  "Startup window width.\n"     },
	{ NULL,  "height", 1, "<height>", "Startup window height.\n"    }
};

static void help(const char* _error = NULL)
{
	if (NULL != _error)
	{
		frost::printf("Error:\n%s\n\n", _error);
	}

	frost::printf("helloworld, frost example.\n");
	frost::printf("Camera: WASD move, Q/E descend/ascend, Shift boost, hold right mouse to look.\n");

	frost::Error err;
	frost::write(frost::getStdOut(), s_options, FROST_COUNTOF(s_options), &err);
}

int main(int _argc, const char* _argv[])
{
	frost::DefaultAllocator allocator;

	// Parse commands
	frost::CommandLineArgs args(&allocator, _argc, _argv);
	frost::CommandLine cmdLine(args.getArgc(), args.getArgv(), s_options, FROST_COUNTOF(s_options));

	if (cmdLine.hasArg('h', "help") )
	{
		help();
		return frost::kExitFailure;
	}

	uint32_t width = 1280;
	const char* widthArg = cmdLine.findOption("width");
	if (NULL != widthArg)
	{
		frost::fromString(&width, widthArg);
	}

	uint32_t height = 720;
	const char* heightArg = cmdLine.findOption("height");
	if (NULL != heightArg)
	{
		frost::fromString(&height, heightArg);
	}

	// Run app...
	frost::App app(&allocator);
	{
		app.insertResource<frost::DeviceResource>(
			frost::DeviceResource{ "frost | helloworld example", width, height });
		app.addPlugins<frost::DevicePlugin>();

		app.insertResource<frost::GraphicsResource>(
			frost::GraphicsResource{ width, height });
		app.addPlugins<frost::GraphicsPlugin>();

		app.addPlugins<ExamplePlugin>();
	}
	app.run();

	return 0;
}
