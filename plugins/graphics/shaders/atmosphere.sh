/*
 * Sky adapted from bgfx/examples/36-sky.
 * Copyright 2017 Stanislav Pidhorskyi. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "sky.sh"

SAMPLER2D(s_moon, 0);
uniform vec4 u_sunTint;

vec3 evaluateMoon(vec3 _direction)
{
	vec3 center = u_moonDirection.xyz;
	vec3 up = u_moonUp.xyz;
	vec3 right = normalize(cross(center, up));
	vec2 p = vec2(dot(_direction, right), dot(_direction, up)) / u_moonParameters.x;
	float radius = length(p);
	float edge = max(fwidth(radius), 0.0001);
	float coverage = saturate((1.0 - radius) / edge + 0.5);
	if (dot(_direction, center) <= 0.0 || _direction.y < 0.0 || coverage <= 0.0)
	{
		return vec3_splat(0.0);
	}
	vec3 normal = vec3(p, sqrt(max(0.0, 1.0 - dot(p, p))));
	normal = normalize(normal);
	vec3 worldNormal = right * normal.x + up * normal.y - center * normal.z;
	float NoL = max(0.0, dot(worldNormal, u_sunDirection.xyz));
	// Lommel-Seeliger gives the full moon its characteristic lack of limb darkening.
	float reflectance = 2.0 * NoL / max(NoL + normal.z, 0.0001);
	vec2 uv = vec2(atan2(normal.x, normal.z) / 6.28318530718 + 0.5,
		acos(clamp(normal.y, -1.0, 1.0)) / 3.14159265359);
	vec3 albedo = texture2D(s_moon, uv).rgb * u_moonParameters.z;
	return albedo * u_moonColor.rgb * u_moonParameters.y * reflectance * coverage;
}

vec3 evaluateAtmosphere(vec3 _direction)
{
	vec3 direction = normalize(_direction);
	float size2 = u_parameters.x * u_parameters.x;
	float dist = 2.0 * (1.0 - dot(direction, normalize(u_sunDirection.xyz)));
	float sun = exp(-dist / u_parameters.y / size2) + step(dist, size2);
	vec3 color = evaluateSky(direction) + evaluateMoon(direction)
		+ min(sun * sun, 1.0) * u_sunTint.rgb * step(0.0, direction.y);
	// Linear HDR color; the output pass handles exposure and tone mapping.
	return color;
}
