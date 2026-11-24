/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "ddgi.sh"

IMAGE2D_WO(s_ddgiIrradianceOutput, rgba16f, 0);
IMAGE2D_WO(s_ddgiDistanceOutput, rgba32f, 1);
SAMPLER2D(s_ddgiPreviousIrradiance, 2);
SAMPLER2D(s_ddgiPreviousDistance, 3);
IMAGE2D_RO(s_ddgiRays, rgba16f, 4);
uniform vec4 u_ddgiBlend; // Radiance and distance history weights.

SHARED vec4 s_rayRadiance[256];
SHARED vec3 s_rayDirection[256];
SHARED vec3 s_irradiance[36];
SHARED vec2 s_distance[256];
SHARED float s_active;

ivec2 ddgiBorder(ivec2 _pixel, int _size)
{
	if (_pixel.y == 0) { _pixel.y = 1; _pixel.x = _size - 1 - _pixel.x; }
	else if (_pixel.y == _size - 1) { _pixel.y = _size - 2; _pixel.x = _size - 1 - _pixel.x; }
	if (_pixel.x == 0) { _pixel.x = 1; _pixel.y = _size - 1 - _pixel.y; }
	else if (_pixel.x == _size - 1) { _pixel.x = _size - 2; _pixel.y = _size - 1 - _pixel.y; }
	return _pixel;
}

NUM_THREADS(16, 16, 1)
void main()
{
	uint probe = gl_WorkGroupID.x;
	ivec2 pixel = ivec2(gl_LocalInvocationID.xy);
	uint lane = gl_LocalInvocationIndex;
	uint rays = uint(u_ddgiGrid[1].w);
	if (lane < rays)
	{
		s_rayRadiance[lane] = imageLoad(s_ddgiRays, ddgiRayPixel(probe, lane));
		s_rayDirection[lane] = ddgiDirection(lane);
	}
	barrier();
	if (lane == 0u)
	{
		uint backfaces = 0u;
		for (uint ii = 0u; ii < ddgiFixedRays(); ++ii) { backfaces += s_rayRadiance[ii].a < 0.0 ? 1u : 0u; }
		s_active = float(backfaces) / float(ddgiFixedRays()) > 0.25 ? 0.0 : 1.0;
	}
	barrier();
	ivec2 tile = ddgiTile(ddgiCell(probe));
	if (all(greaterThan(pixel, ivec2(0, 0))) && all(lessThan(pixel, ivec2(15, 15))))
	{
		vec3 direction = reflectionDecodeNormal((vec2(pixel) - 0.5) / 14.0 * 2.0 - 1.0);
		vec2 moments = vec2_splat(0.0);
		float weight = 0.0;
		for (uint ii = ddgiFixedRays(); ii < rays; ++ii)
		{
			float w = pow(max(0.0, dot(direction, s_rayDirection[ii])), u_ddgiParameters.z);
			if (w < 0.001) { continue; }
			float distance = abs(s_rayRadiance[ii].a);
			moments += vec2(distance, distance * distance) * w;
			weight += w;
		}
		vec2 size = vec2(u_ddgiGrid[2].x * u_ddgiGrid[2].y, u_ddgiGrid[2].z) * 16.0;
		vec2 uv = (vec2(tile * 16 + pixel) + 0.5) / size;
		vec2 previous = texture2DLod(s_ddgiPreviousDistance, uv, 0.0).rg;
		vec2 value = weight > 0.000001 ? moments / weight : previous;
		// Randomly rotated rays change this estimate even in a static scene.
		// Keep the configured history instead of interpreting sampling noise as motion.
		s_distance[lane] = mix(value, previous, u_ddgiBlend.y);
	}
	if (all(greaterThan(pixel, ivec2(0, 0))) && all(lessThan(pixel, ivec2(5, 5))))
	{
		vec3 direction = reflectionDecodeNormal((vec2(pixel) - 0.5) / 4.0 * 2.0 - 1.0);
		vec3 irradiance = vec3_splat(0.0);
		float weight = 0.0;
		uint backfaces = 0u;
		for (uint ii = ddgiFixedRays(); ii < rays; ++ii)
		{
			if (s_rayRadiance[ii].a < 0.0) { ++backfaces; continue; }
			float w = max(0.0, dot(direction, s_rayDirection[ii]));
			irradiance += s_rayRadiance[ii].rgb * w;
			weight += w;
		}
		// Uniform spherical rays: normalization gives E/pi and preserves a constant sky.
		vec3 value = pow(max(irradiance / max(weight, 0.000001), vec3_splat(0.0)), vec3_splat(1.0 / FROST_DDGI_IRRADIANCE_GAMMA));
		vec2 size = vec2(u_ddgiGrid[2].x * u_ddgiGrid[2].y, u_ddgiGrid[2].z) * 6.0;
		vec4 previousProbe = texture2DLod(s_ddgiPreviousIrradiance, (vec2(tile * 6 + pixel) + 0.5) / size, 0.0);
		vec3 previous = previousProbe.rgb;
		float history = previousProbe.a > 0.5 && dot(previous, previous) > 0.0 ? u_ddgiBlend.x : 0.0;
		vec3 delta = value - previous;
		if (max(max(-delta.r, -delta.g), -delta.b) > 0.25) { history = max(0.0, history - 0.75); }
		// Keep rare bright ray hits from replacing a stable probe estimate.
		if (dot(delta, vec3(0.2126, 0.7152, 0.0722)) > 0.1 && history > 0.0) { delta *= 0.25; }
		vec3 change = delta * (1.0 - history);
		// A minimum darkening step prevents quantized history from retaining bright outliers.
		if (max(value.r, max(value.g, value.b)) < max(previous.r, max(previous.g, previous.b)))
		{
			change = sign(change) * min(abs(delta), max(abs(change), vec3_splat(1.0 / 1024.0)));
		}
		bool valid = s_active > 0.5 && float(backfaces) < float(rays - ddgiFixedRays()) * 0.1;
		s_irradiance[pixel.y * 6 + pixel.x] = valid ? previous + change : previous;
	}
	barrier();
	ivec2 source = ddgiBorder(pixel, 16);
	imageStore(s_ddgiDistanceOutput, tile * 16 + pixel, vec4(s_distance[source.y * 16 + source.x], 0.0, 0.0));
	if (all(lessThan(pixel, ivec2(6, 6))))
	{
		source = ddgiBorder(pixel, 6);
		imageStore(s_ddgiIrradianceOutput, tile * 6 + pixel, vec4(s_irradiance[source.y * 6 + source.x], s_active));
	}
}
