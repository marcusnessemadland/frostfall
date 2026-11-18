/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/bx/blob/main/LICENSE
 */

#ifndef FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD

// Engine
#include <frost/ecs.h>
#include <frost/file.h>
#include <frost/os.h>

// Engine Plugins
#include <graphics.h>

#include "camera.h" //CameraControllerComponent

namespace
{
	struct SceneResource
	{
		// Assets
		frost::MeshHandle floor;
		frost::MeshHandle dragon;
		frost::TextureHandle moonTexture;
		frost::MaterialHandle materials[2];

		// Entities
		enum Enum
		{
			Player,
			Floor,
			Dragon,
			Sky,
			Moon,

			Count
		};
		frost::Entity instances[Count];
	};

	struct SceneSystem
	{
		static const frost::Memory* readAsset(frost::World& _world, const char* _path, frost::Error* _err)
		{
			frost::FileReader reader;

			if (!frost::open(&reader, _path, _err))
			{
				return NULL;
			}

			const int64_t size = frost::getSize(&reader);
			const frost::Memory* mem = frost::allocMemory(_world.getAllocator(), uint32_t(size));
			frost::read(&reader, mem->data, int32_t(size), _err);

			frost::close(&reader);
			return mem;
		}

		static void startup(frost::World& _world, frost::ResMutT<frost::ActiveCameraResource> _activeCamera,
			frost::ResMutT<SceneResource> _scene)
		{
			// Read assets
			frost::Error err;
			_scene->moonTexture = frost::createTexture(readAsset(_world, "tex_moon.bin", &err), &err);
			_scene->materials[0] = frost::createMaterial(readAsset(_world, "mat_floor.bin", &err), NULL, 0, &err);
			_scene->materials[1] = frost::createMaterial(readAsset(_world, "mat_dragon.bin", &err), NULL, 0, &err);
			_scene->floor = frost::createMesh(readAsset(_world, "mesh_cube.bin", &err), &err);
			_scene->dragon = frost::createMesh(readAsset(_world, "mesh_dragon.bin", &err), &err);
			if (!err.isOk())
			{
				frost::debugPrintf("Unable to load scene assets: %s\n", err.getMessage().getCPtr());
				frost::exit(frost::kExitFailure, false);
			}

			// Spawn floor
			frost::TransformComponent transform;
			transform.position = { 0.0f, -1.1f, 0.0f };
			transform.scale = { 100.0f, 1.0f, 100.0f };
			_scene->instances[SceneResource::Floor] = _world.spawn(
				frost::MeshComponent{ _scene->floor, &_scene->materials[0], 1 }, transform).id();

			// Spawn dragon
			transform = {};
			transform.position = { 0.0f, -0.1f, 0.0f };
			transform.rotation = frost::rotateY(frost::kPi);
			transform.scale = { 3.0f, 3.0f, 3.0f };
			_scene->instances[SceneResource::Dragon] = _world.spawn(
				frost::MeshComponent{ _scene->dragon, &_scene->materials[1], 1 }, transform).id();

			// Spawn sky TODO: This is a mess, figure out how to deal with moon and sun in the same sky system...
			frost::SkyComponent sky;
			sky.time = 10.0f;
			sky.moonTexture = _scene->moonTexture;
			sky.moonAlbedo = 0.37746f;
			_scene->instances[SceneResource::Sky] = _world.spawn(sky, frost::DirectionalLightComponent{}).id();
			_scene->instances[SceneResource::Moon] = _world.spawn(frost::DirectionalLightComponent{}).id();
			_world.getResourceMut<frost::SunShadowResource>()->light = _scene->instances[SceneResource::Sky];

			// Spawn player TODO: This is also overly complex, lets reset on spawn? and spawn cameracomponent in the camerasystem?
			transform = {};
			transform.position = { 0.0f, 2.2f, 0.0f };
			frost::CameraComponent camera;
			camera.fov = 50.0f;
			CameraControllerComponent controller;
			controller.distance = 10.0f;
			CameraSystem::reset(camera, controller, &transform);
			_scene->instances[SceneResource::Player] = _world.spawn(transform, camera, controller).id();

			// Set camera orbit target to player, set active camera to player TODO: There has to be a betteer way to do this
			const frost::Entity player = _scene->instances[SceneResource::Player];
			_world.getMut<CameraControllerComponent>(player)->target = player;
			_activeCamera->entity = player;
		}

		static void update(frost::World& _world, frost::ResT<SceneResource> _scene,
			frost::ResT<frost::TimeResource> _time,
			frost::ResMutT<frost::SunShadowResource> _shadows,
			frost::QueryT<const frost::SkyComponent&, frost::DirectionalLightComponent&> _suns)
		{
			// Animate the dragon around its base with a uniform scale.
			frost::TransformComponent* transform = _world.getMut<frost::TransformComponent>(_scene->instances[SceneResource::Dragon]);
			const float time = float(_time->elapsed);
			const float scale = 3.0f + 0.25f * frost::sin(time * 1.5f);
			transform->scale = { scale, scale, scale };
			transform->rotation = frost::rotateY(frost::kPi + time * 0.35f);

			// Update directional light direction and color based on sun and moon.
			_suns.each([&](const frost::SkyComponent& _sky, frost::DirectionalLightComponent& _light)
			{
				// TODO: Overly complex
				_light.direction = _sky.sunDirection;
				_light.color = _sky.sunColor;
				frost::DirectionalLightComponent* moon = _world.getMut<frost::DirectionalLightComponent>(
					_scene->instances[SceneResource::Moon]);
				moon->direction = _sky.moonLightDirection;
				moon->color = _sky.moonColor;
				const frost::Vec3 weights = { 0.2126f, 0.7152f, 0.0722f };
				const float sunlight = frost::dot(_light.color, weights) * _light.intensity;
				const float moonlight = frost::dot(moon->color, weights) * moon->intensity;
				// Reuse one atlas. Hysteresis avoids flipping ownership near equal brightness.
				if (_shadows->light == _scene->instances[SceneResource::Sky] && moonlight > sunlight * 1.25f)
				{
					_shadows->light = _scene->instances[SceneResource::Moon];
				}
				else if (_shadows->light == _scene->instances[SceneResource::Moon] && sunlight > moonlight * 1.25f)
				{
					_shadows->light = _scene->instances[SceneResource::Sky];
				}
			});
		}

		static void shutdown(frost::World& _world, frost::ResMutT<SceneResource> _scene)
		{
			// Destroy entities
			for (frost::Entity entity : _scene->instances)
			{
				_world.despawn(entity);
			}

			// Destroy assets
			frost::release(_scene->dragon);
			frost::release(_scene->floor);
			for (frost::MaterialHandle material : _scene->materials)
			{
				frost::release(material);
			}
			frost::release(_scene->moonTexture);
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
