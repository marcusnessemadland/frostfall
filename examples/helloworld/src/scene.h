/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD



// Engine
#include <frost/file.h>

// Engine Plugins
#include <scene.h>

// Example
#include "asset.h"
#include "camera.h"

namespace
{
	/// Hardcoded light positions for the scene. TODO: Export lights as part of gltf.
	static const frost::Vec3 s_lightPositions[] =
	{
		{ 0.00211f, 3.28571f, -5.60000f }, // SM_HangingLamp_3
		{ -2.60000f, 3.28520f, -5.60000f }, // SM_HangingLamp_4
		{ 2.40193f, 3.28649f, -5.60000f }, // SM_HangingLamp_5
		{ -2.60000f, 3.28701f, 5.70000f }, // SM_HangingLamp_6
		{ 0.00000f, 3.28547f, 5.70000f }, // SM_HangingLamp_7
		{ 2.59922f, 3.28722f, 5.70000f }, // SM_HangingLamp_8
		{ 7.10000f, 2.57270f, 0.00261f }, // SM_HangingLamp_12
		{ -2.60000f, 7.48761f, -5.60000f }, // SM_HangingLamp_14
		{ 2.40000f, 7.48424f, -5.60000f }, // SM_HangingLamp_15
		{ -2.60000f, 7.48540f, 5.70000f }, // SM_HangingLamp_16
		{ 2.59945f, 7.48670f, 5.70000f }, // SM_HangingLamp_17
		{ -5.20000f, 7.48662f, -0.00224f }, // SM_HangingLamp_18
		{ -3.34028f, 2.74798f, 1.60000f }, // SM_WallLamp_01
		{ -3.34028f, 2.74798f, -1.60000f }, // SM_WallLamp_2
		{ 3.34028f, 2.74798f, 1.60000f }, // SM_WallLamp_3
		{ 3.34028f, 2.74798f, -1.60000f }, // SM_WallLamp_4
		{ -10.66758f, 4.23318f, -4.74842f }, // SM_WallLamp_5
		{ 6.89426f, 1.53140f, 2.49371f }, // SM_FloorLantern_Glass_A
		{ 1.02925f, 0.16465f, 1.81340f }, // SM_FloorLantern_Glass_A
		{ 6.90709f, 1.52240f, -2.49240f }, // SM_FloorLantern_Glass_A
		{ -1.01685f, 0.16460f, 1.84444f }, // SM_FloorLantern_Glass_A
		{ 1.19855f, 4.65484f, -6.44041f }, // SM_FloorLantern_Glass_A
		{ -1.17531f, 4.65484f, -6.43550f }, // SM_FloorLantern_Glass_A
		{ 5.62475f, 4.65484f, 1.20287f }, // SM_FloorLantern_Glass_A
		{ 5.98097f, 0.35473f, 1.86531f }, // SM_FloorLantern_Glass_A
		{ 6.03467f, 0.35473f, -1.82071f }, // SM_FloorLantern_Glass_A
		{ 7.60018f, 1.52240f, -2.49240f }, // SM_FloorLantern_Glass_A
		{ 7.60735f, 1.53140f, 2.49371f }, // SM_FloorLantern_Glass_A
		{ 1.52861f, 0.16456f, -2.04079f }, // SM_FloorLantern_Glass_A
		{ -1.51551f, 0.16456f, -2.04105f }, // SM_FloorLantern_Glass_A
	};

	/// Loading a scene.
	///
	struct SceneSystem
	{
		static void startup(frost::World& _world)
		{
			AssetLoader assets(_world.getAllocator());

			// Load and spawn scene
			const frost::SceneHandle scene = assets.loadScene("courtyard.bin");
			const frost::Entity sceneRoot = frost::spawnScene(_world, scene, frost::ErrorAssert{});
			frost::release(scene);

			// Spawn player
			frost::Entity player = _world.spawn().id();
			{
				const frost::Entity playerStart = frost::findSceneEntity(_world, sceneRoot, "Player_Start");
				const frost::TransformComponent* playerStartTransform = _world.get<frost::TransformComponent>(playerStart);

				// Add transform
				if (NULL != playerStartTransform)
				{
					frost::TransformComponent transform;
					transform.position = playerStartTransform->position;
					transform.rotation = playerStartTransform->rotation;
					_world.insert(player, transform);
				}
			}
			{
				// Add camera
				CameraControllerComponent* controller = _world.emplace<CameraControllerComponent>(player);
				controller->distance = 10.0f;
				controller->mode = CameraMode::Flying;
				controller->target = player;

				// Set active graphics camera
				_world.getResourceMut<frost::ActiveCameraResource>()->entity = player;
			}

			// Spawn sky
			const frost::Entity sky = _world.spawn(frost::SkyAtmosphereComponent{},
				frost::DirectionalLightComponent{}, frost::SkyLightComponent{}).id();

			// Spawn local lights
			for (uint32_t ii = 0; ii < FROST_COUNTOF(s_lightPositions); ++ii)
			{
				frost::TransformComponent transform;
				transform.position = s_lightPositions[ii];

				frost::PointLightComponent light;
				light.color = { 1.0f, 0.55f, 0.22f };
				light.intensity = 14.0f;
				light.range = 4.0f;
				light.sourceRadius = 0.25f; // Clip the current opaque glass shell; translucent glass will be rendered separately.

				_world.spawn(transform, light);
			}

			// Modify shadow params TODO: I dont like this
			frost::SunShadowResource* shadows = _world.getResourceMut<frost::SunShadowResource>();
			shadows->light = sky;
			shadows->distanceScale = 1.0f / 3.0f;
		}

		static void update(frost::QueryT<const frost::SkyAtmosphereComponent&, frost::DirectionalLightComponent&> _skies)
		{
			_skies.each([](const frost::SkyAtmosphereComponent& _sky, frost::DirectionalLightComponent& _light)
			{
				// Check if sun is up or moon
				const bool sun = frost::dot(_sky.sunDirection, { 0.0f, 1.0f, 0.0f })
					>= frost::dot(_sky.moonDirection, { 0.0f, 1.0f, 0.0f });

				// Update directional light based on desired sun/moon direction
				_light.direction = sun ? _sky.sunDirection : _sky.moonDirection;
				_light.color = sun ? _sky.sunColor : _sky.moonColor;
			});
		}
	};

} // namespace

#endif // FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
