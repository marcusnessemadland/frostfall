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

namespace
{
	struct SceneResource
	{
		frost::MeshHandle mesh;
		frost::MeshHandle orb;
		frost::MeshHandle shadowOrbs[3];
		frost::TextureHandle textures[5];
		frost::MaterialHandle materials[17];
		frost::MaterialHandle gridBindings[12][2];

		enum Enum
		{
			Player, Floor, Box1, Box2, Box3, Sky, Moon,
			GridBegin,
			Count = GridBegin + 12
		};
		frost::Entity instances[Count];
	};

	struct SceneSystem
	{
		static const frost::Memory* readAsset(frost::World& _world, const char* _path, frost::Error* _err)
		{
			frost::FileReader reader;
			if (!frost::open(&reader, _path, _err)) { return NULL; }
			const int64_t size = frost::getSize(&reader);
			const frost::Memory* mem = 0 < size && size <= INT32_MAX
				? frost::allocMemory(_world.getAllocator(), uint32_t(size)) : NULL;
			if (NULL != mem) { frost::read(&reader, mem->data, int32_t(size), _err); }
			frost::close(&reader);
			return mem;
		}

		static void startup(frost::World& _world, frost::ResMutT<frost::ActiveCameraResource> _activeCamera,
			frost::ResMutT<SceneResource> _scene)
		{
			frost::Error err;
			const char* textures[] = { "stone-color.bin", "stone-normal.bin", "parameters.bin", "mask.bin", "moon.bin" };
			for (uint32_t ii = 0; ii < FROST_COUNTOF(textures); ++ii)
			{
				_scene->textures[ii] = frost::createTexture(readAsset(_world, textures[ii], &err), &err);
			}
			const char* materials[] = { "stone.bin", "metal.bin", "masked.bin", "emissive.bin", "floor.bin" };
			// Explicitly resolved in each material's dependency-table order.
			_scene->materials[0] = frost::createMaterial(readAsset(_world, materials[0], &err), _scene->textures, 3, &err);
			_scene->materials[1] = frost::createMaterial(readAsset(_world, materials[1], &err), NULL, 0, &err);
			_scene->materials[2] = frost::createMaterial(readAsset(_world, materials[2], &err), &_scene->textures[3], 1, &err);
			_scene->materials[3] = frost::createMaterial(readAsset(_world, materials[3], &err), &_scene->textures[0], 1, &err);
			_scene->materials[4] = frost::createMaterial(readAsset(_world, materials[4], &err), NULL, 0, &err);
			_scene->mesh = frost::createMesh(readAsset(_world, "cube.bin", &err), &err);
			_scene->orb = frost::createMesh(readAsset(_world, "orb.bin", &err), _scene->materials, 2, &err);
			const char* shadowMeshes[] = { "orb-shadow1.bin", "orb-shadow2.bin", "orb-shadow3.bin" };
			for (uint8_t ii = 0; ii < 3; ++ii)
			{
				_scene->shadowOrbs[ii] = frost::createMesh(readAsset(_world, shadowMeshes[ii], &err), _scene->materials, 2, &err);
			}
			if (!err.isOk())
			{
				frost::debugPrintf("Unable to load scene assets: %s\n", err.getMessage().getCPtr());
				frost::exit(frost::kExitFailure, false);
			}

			frost::TransformComponent transform;
			transform.position = { 0.0f, -1.1f, 0.0f };
			transform.scale = { 100.0f, 1.0f, 100.0f };
			_scene->instances[SceneResource::Floor] = _world.spawn(
				frost::MeshComponent{ _scene->mesh, &_scene->materials[4], 1 }, transform).id();

			// The imported orb keeps two distinct default material bindings.
			transform = {};
			transform.position = { -3.5f, 0.0f, -2.0f };
			transform.scale = { 1.5f, 1.5f, 1.5f };
			_scene->instances[SceneResource::Box1] = _world.spawn(frost::MeshComponent{ _scene->orb }, transform).id();
			transform.position = { 0.0f, 1.0f, -2.0f };
			transform.scale = { 1.5f, 1.0f, 0.1f };
			_scene->instances[SceneResource::Box2] = _world.spawn(
				frost::MeshComponent{ _scene->mesh, &_scene->materials[2], 1 }, transform).id();
			transform.position = { 3.5f, 0.75f, -2.0f };
			transform.scale = { 1.0f, 1.0f, 1.0f };
			_scene->instances[SceneResource::Box3] = _world.spawn(
				frost::MeshComponent{ _scene->mesh, &_scene->materials[3], 1 }, transform).id();

			// Lower row: dielectric. Upper row: metal. Roughness increases left to right.
			for (uint32_t row = 0; row < 2; ++row)
			{
				for (uint32_t column = 0; column < 6; ++column)
				{
					const uint32_t index = row * 6 + column;
					frost::MaterialDesc material;
					material.baseColor[0] = 0.8f;
					material.baseColor[1] = 0.3f;
					material.baseColor[2] = 0.08f;
					material.metallic = float(row);
					material.roughness = 0.05f + float(column) * 0.18f;
					_scene->materials[5 + index] = frost::createMaterial(material, &err);
					_scene->gridBindings[index][0] = _scene->materials[5 + index];
					_scene->gridBindings[index][1] = _scene->materials[5 + index];
					transform.position = { -5.0f + float(column) * 2.0f, 2.4f + float(row) * 2.0f, 2.0f };
					transform.scale = { 1.0f, 1.0f, 1.0f };
					_scene->instances[SceneResource::GridBegin + index] = _world.spawn(
						frost::MeshComponent{ _scene->orb, _scene->gridBindings[index], 2 }, transform).id();
				}
			}
			if (!err.isOk()) { frost::debugPrintf("Unable to create material grid.\n"); frost::exit(frost::kExitFailure, false); }

			// Shadow LODs borrow the separately owned simplified orb meshes.
			_world.query<frost::MeshComponent&>().each([&](frost::MeshComponent& _mesh)
			{
				if (_mesh.handle.idx == _scene->orb.idx)
				{
					for (uint8_t ii = 0; ii < 3; ++ii) { _mesh.shadowLods[ii] = _scene->shadowOrbs[ii]; }
				}
			});

			frost::SkyComponent sky;
			sky.time = 10.0f;
			sky.moonTexture = _scene->textures[4];
			sky.moonAlbedo = 0.37746f;
			_scene->instances[SceneResource::Sky] = _world.spawn(sky, frost::DirectionalLightComponent{}).id();
			_scene->instances[SceneResource::Moon] = _world.spawn(frost::DirectionalLightComponent{}).id();
			_world.getResourceMut<frost::SunShadowResource>()->light = _scene->instances[SceneResource::Sky];
			transform = {};
			transform.position = { 0.0f, 2.8f, 0.0f };
			frost::CameraComponent camera;
			camera.fov = 50.0f;
			CameraComponent controller;
			controller.distance = 12.0f;
			CameraSystem::reset(camera, controller, &transform);
			const frost::Entity player = _world.spawn(transform, camera, controller).id();
			_world.getMut<CameraComponent>(player)->target = player;
			_scene->instances[SceneResource::Player] = player;
			_activeCamera->entity = player;
		}

		static void update(frost::World& _world, frost::ResT<SceneResource> _scene,
			frost::ResMutT<frost::SunShadowResource> _shadows,
			frost::QueryT<const frost::SkyComponent&, frost::DirectionalLightComponent&> _suns)
		{
			// Update directional light direction and color based on sun and moon.
			_suns.each([&](const frost::SkyComponent& _sky, frost::DirectionalLightComponent& _light)
			{
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
			for (frost::Entity entity : _scene->instances) { _world.despawn(entity); }
			frost::release(_scene->orb);
			for (frost::MeshHandle mesh : _scene->shadowOrbs) { frost::release(mesh); }
			frost::release(_scene->mesh);
			for (frost::MaterialHandle material : _scene->materials) { frost::release(material); }
			for (frost::TextureHandle texture : _scene->textures) { frost::release(texture); }
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
