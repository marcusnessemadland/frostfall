/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
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

} // namespace frost

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

} // namespace

namespace
{
	struct SceneResource
	{
		// Assets
		frost::MeshHandle mesh;
		frost::Ticks previous = frost::getNow();
		float hoursPerSecond = 0.5f;

		// Instances
		enum Enum
		{
			Player,
			Floor,
			Sun,

			Count
		};
		frost::Entity instances[SceneResource::Count];
	};

	struct SceneSystem
	{
		static void startup(frost::World& _world, frost::ResMutT<frost::ActiveCamera> _activeCamera, frost::ResMutT<SceneResource> _scene)
		{
			// Load and create assets
			frost::Error err;
			frost::FileReader reader;
			const frost::Memory* mem = NULL;
			if (frost::open(&reader, "cube.mesh", &err) )
			{
				const int64_t size = frost::getSize(&reader);
				if (0 < size && INT32_MAX >= size)
				{
					mem = frost::allocMemory(_world.getAllocator(), uint32_t(size) );
					if (NULL != mem)
					{
						frost::read(&reader, mem->data, int32_t(size), &err);
					}
				}
				frost::close(&reader);
			}

			_scene->mesh = frost::createMesh(mem, &err);
			if (!frost::isValid(_scene->mesh) )
			{
				const frost::StringLiteral message = err.getMessage();
				return;
			}

			// Create floor
			frost::Transform transform;
			transform.scale = { 100.0f, 1.0f, 100.0f };
			_scene->instances[SceneResource::Floor] = _world.spawn(_scene->mesh, transform).id();

			// One component controls the visible atmosphere and its sunlight.
			_scene->instances[SceneResource::Sun] = _world.spawn(frost::Sky{}).id();
			_scene->previous = frost::getNow();

			// Create flying camera entity.
			frost::Camera camera;
			camera.position = { 0.0f, 3.0f, -5.0f };
			_scene->instances[SceneResource::Player] = _world.spawn(camera, FlyCameraController{}).id();
			_activeCamera->entity = _scene->instances[SceneResource::Player]; // Active camera entity TODO: Figure out a better way for this?
		}

		static void update(frost::ResMutT<SceneResource> _scene, frost::QueryT<frost::Sky&> _skies)
		{
			const frost::Ticks now = frost::getNow();
			const float delta = frost::min(frost::toSeconds<float>(now - _scene->previous), 0.1f);
			_scene->previous = now;
			_skies.get(_scene->instances[SceneResource::Sun], [&](frost::Sky& _sky)
				{
					_sky.time = frost::wrap(_sky.time + delta * _scene->hoursPerSecond, 24.0f);
				});
		}

		static void shutdown(frost::World& _world, frost::ResMutT<SceneResource> _scene)
		{
			for (uint32_t ii = 0; ii < SceneResource::Count; ++ii)
			{
				_world.despawn(_scene->instances[ii]);
			}

			frost::release(_scene->mesh);
		}
	};

} // namespace

namespace
{
	struct ExamplePlugin : frost::PluginI
	{
		virtual void build(frost::App& _app) override
		{
			_app.insertResource<SceneResource>(SceneResource{});

			_app.addSystems(frost::EcsStage::PostStartup, SceneSystem::startup);
			_app.addSystems(frost::EcsStage::Update, FlyCameraSystem::update);
			_app.addSystems(frost::EcsStage::Update, SceneSystem::update);
			_app.addSystems(frost::EcsStage::Shutdown, SceneSystem::shutdown);
		}
	};

} // namespace

static const frost::CommandLineOption s_options[] =
{
	{ 'h',   "help",   0, NULL,       "Display this help and exit." },
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
		app.addPlugins<ExamplePlugin>();

		app.insertResource<frost::DeviceResource>(
			frost::DeviceResource{ "Frostfall Engine | Helloworld Example", width, height });
		app.addPlugins<frost::DevicePlugin>();

		app.insertResource<frost::GraphicsResource>(
			frost::GraphicsResource{ width, height });
		app.addPlugins<frost::GraphicsPlugin>();

	}
	app.run();

	return 0;
}
