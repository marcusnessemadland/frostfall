/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD

#include <frost/os.h>
#include "assets.h"
#include "camera.h"
#include "sky.h"

namespace
{
	struct SceneSystem
	{
		static void startup(frost::World& _world)
		{
			frost::Error err;
			const frost::FilePath executable(frost::Dir::Executable);
			AssetLoader assets(_world.getAllocator(), executable.getPath(), &err);
			const frost::SceneHandle scene = assets.loadScene("courtyard.bin", &err);
			const frost::Entity root = frost::spawnScene(_world, scene, &err);
			frost::release(scene);
			if (!err.isOk())
			{
				frost::debugPrintf("Unable to load courtyard: %s\n", err.getMessage().getCPtr());
				frost::exit(frost::kExitFailure, false);
			}

			// The export supplies placements and assets; the example supplies its viewer and daylight.
			frost::CameraComponent camera;
			camera.position = { 0, 2, 5 };
			const auto start = frost::findSceneEntity(_world, root, "Player_Start");
			if (const auto* transform = _world.get<frost::TransformComponent>(start))
			{
				camera.position = frost::add(transform->position, { 0, 0.7f, 0 });
				camera.rotation = transform->rotation;
			}
			CameraSystem::setup(_world, _world.spawn(camera).id());

			// Concentrate the default cascades around the courtyard.
			_world.getResourceMut<frost::SunShadowResource>()->distanceScale = 1.0f / 3.0f;
			frost::SkyAtmosphereComponent sky;
			sky.time = 10;
			SkyLightingSystem::setup(_world, _world.spawn(sky).id(),
				_world.spawn(frost::DirectionalLightComponent{}).id());
			_world.spawn(frost::SkyLightComponent{});

			// Glass emitter centers from courtyard.gltf, including every parent transform.
			const frost::Vec3 fixtures[] =
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
			frost::PointLightComponent light;
			light.color = { 1.0f, 0.55f, 0.22f };
			light.intensity = 14.0f;
			light.range = 4.0f;
			// Clip the current opaque glass shell; translucent glass will be rendered separately.
			light.sourceRadius = 0.25f;
			for (const frost::Vec3& position : fixtures)
			{
				frost::TransformComponent transform;
				transform.position = position;
				_world.spawn(transform, light);
			}
		}
	};
}

#endif // FROST_EXAMPLE_HELLOWORLD_SCENE_H_HEADER_GUARD
