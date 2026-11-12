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
			Box1,
			Box2,
			Box3,
			Sky,

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
			{
				frost::TransformComponent transform;
					transform.scale = { 100.0f, 1.0f, 100.0f };
				_scene->instances[SceneResource::Floor] = _world.spawn(frost::MeshComponent{ _scene->mesh }, transform).id();
			}

			// Create boxes
			{
				frost::TransformComponent transform;
				transform.position = { -3.0f, 2.0f, 0.0f };
				transform.rotation = frost::rotateY(frost::toRad(20.0f));
				transform.scale = { 1.0f, 1.0f, 1.0f };
				_scene->instances[SceneResource::Box1] = _world.spawn(frost::MeshComponent{ _scene->mesh }, transform).id();
			}
			{
				frost::TransformComponent transform;
				transform.position = { 0.0f, 3.0f, 2.5f };
				transform.rotation = frost::rotateY(frost::toRad(-25.0f));
				transform.scale = { 0.75f, 2.0f, 0.75f };
				_scene->instances[SceneResource::Box2] = _world.spawn(frost::MeshComponent{ _scene->mesh }, transform).id();
			}
			{
				frost::TransformComponent transform;
				transform.scale = { 1.5f, 0.5f, 0.75f };
				transform.rotation = frost::mul(frost::rotateY(frost::toRad(35.0f)), frost::rotateZ(frost::toRad(25.0f)));
				transform.position = { 3.0f, 1.0f + transform.scale.x * frost::sin(frost::toRad(25.0f)) + transform.scale.y * frost::cos(frost::toRad(25.0f)), 0.0f };
				_scene->instances[SceneResource::Box3] = _world.spawn(frost::MeshComponent{ _scene->mesh }, transform).id();
			}

			// Create sky
			{
				frost::SkyComponent sky;
				sky.hps = 0.5f;
				_scene->instances[SceneResource::Sky] = _world.spawn(sky, frost::DirectionalLightComponent{}).id();
			}

			// Create flying camera entity.
			{
				frost::CameraComponent camera;
				camera.position = { 0.0f, 3.0f, -5.0f };
				_scene->instances[SceneResource::Player] = _world.spawn(camera, ExampleCameraComponent{}).id();
			}

			// Active camera on player entity TODO: Figure out a better way for this?
			_activeCamera->entity = _scene->instances[SceneResource::Player];
		}

		// This scene chooses to use the sky's sun as a directional light.
		static void updateSun(frost::QueryT<const frost::SkyComponent&, frost::DirectionalLightComponent&> _suns)
		{
			_suns.each([](const frost::SkyComponent& _sky, frost::DirectionalLightComponent& _light)
				{
					_light.direction = _sky.sunDirection;
					_light.color = _sky.sunColor;
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
}

#endif // FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
