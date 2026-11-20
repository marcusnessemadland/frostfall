/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_EXAMPLE_HELLOWORLD_SKY_H_HEADER_GUARD
#define FROST_EXAMPLE_HELLOWORLD_SKY_H_HEADER_GUARD

#include <graphics.h>

namespace
{
	struct SkyLightsComponent { frost::Entity directional; };

	/// The example chooses how celestial simulation drives its single direct light.
	struct SkyLightingSystem
	{
		static void setup(frost::World& _world, frost::Entity _sky, frost::Entity _directional)
		{
			_world.insert(_sky, SkyLightsComponent{ _directional });
			_world.getResourceMut<frost::SunShadowResource>()->light = _directional;
		}

		static void update(frost::World& _world,
			frost::QueryT<const frost::SkyAtmosphereComponent&, const SkyLightsComponent&> _skies)
		{
			_skies.each([&](const frost::SkyAtmosphereComponent& _sky, const SkyLightsComponent& _lights)
			{
				auto* light = _world.getMut<frost::DirectionalLightComponent>(_lights.directional);
				const frost::Vec3 weights = { 0.2126f, 0.7152f, 0.0722f };
				light->direction = _sky.sunDirection;
				light->color = _sky.sunColor;
				if (_sky.moonDirection.y > 0.0f && frost::dot(_sky.moonColor, weights) > frost::dot(_sky.sunColor, weights))
				{
					light->direction = _sky.moonDirection;
					light->color = _sky.moonColor;
				}
			});
		}
	};
}

#endif // FROST_EXAMPLE_HELLOWORLD_SKY_H_HEADER_GUARD
