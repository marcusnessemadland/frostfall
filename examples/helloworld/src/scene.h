#ifndef FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD

// Engine
#include <frost/ecs.h>
#include <frost/file.h>

// Engine Plugins
#include <graphics.h>

namespace
{
	struct SceneResource
	{
		// Assets
		frost::MeshHandle mesh;

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
		static void startup(frost::World& _world, frost::ResMutT<frost::ActiveCameraResource> _activeCamera, frost::ResMutT<SceneResource> _scene)
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
			frost::TransformComponent transform;
			{
				transform.scale = { 100.0f, 1.0f, 100.0f };
			}
			_scene->instances[SceneResource::Floor] = _world.spawn(frost::MeshComponent{ _scene->mesh }, transform).id();

			// One component controls the visible atmosphere and its sunlight.
			frost::SkyComponent sky;
			{
				sky.hps = 0.5f;
			}
			_scene->instances[SceneResource::Sun] = _world.spawn(sky).id();

			// Create flying camera entity.
			frost::CameraComponent camera;
			{
				camera.position = { 0.0f, 3.0f, -5.0f };
			}
			_scene->instances[SceneResource::Player] = _world.spawn(camera, ExampleCameraComponent{}).id();

			// Active camera entity TODO: Figure out a better way for this?
			_activeCamera->entity = _scene->instances[SceneResource::Player];
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
}

#endif // FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
