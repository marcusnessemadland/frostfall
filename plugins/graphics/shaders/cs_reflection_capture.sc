/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "environment.sh"
#include "raytrace.sh"
#define FROST_RAY_CAPTURE 1
#define FROST_DDGI_IRRADIANCE_SLOT 12
#define FROST_DDGI_DISTANCE_SLOT 14
#include "raylighting.sh"

IMAGE2D_ARRAY_WO(s_reflectionCaptureOutput, rgba16f, 0);
IMAGE2D_ARRAY_WO(s_reflectionVisibilityOutput, rgba32f, 15);
SAMPLERCUBE(s_rayEnvironment, 13);
uniform vec4 u_rayEnvironment;
uniform vec4 u_reflectionProbes[3]; // Grid origin / capture resolution, spacing / maximum mip, dimensions / light count.

NUM_THREADS(8, 8, 1)
void main()
{
	ivec3 pixel = ivec3(gl_GlobalInvocationID);
	if (any(greaterThanEqual(vec2(pixel.xy), vec2_splat(u_reflectionProbes[0].w)))) { return; }
	uint probe = uint(pixel.z) / 6u;
	uvec3 dimensions = uvec3(u_reflectionProbes[2].xyz);
	uvec3 cell = uvec3(probe % dimensions.x, (probe / dimensions.x) % dimensions.y, probe / (dimensions.x * dimensions.y));
	vec3 origin = u_reflectionProbes[0].xyz + vec3(cell) * u_reflectionProbes[1].xyz;
	vec2 uv = (vec2(pixel.xy) + 0.5) / u_reflectionProbes[0].w;
	vec3 direction = environmentDirection(uv, float(pixel.z % 6));
	RayHit hit;
	hit.distance = 1e30;
	bool found = traceRay(origin, direction, false, hit);
	vec3 radiance;
	BRANCH if (found) { radiance = shadeRayHit(origin + direction * hit.distance, direction, hit, uint(u_reflectionProbes[2].w),
		sqrt(2.0 / u_reflectionProbes[0].w), hit.distance * 2.0 / u_reflectionProbes[0].w, false); }
	else { radiance = textureCubeLod(s_rayEnvironment, direction, 0.0).rgb * u_rayEnvironment.rgb; }
	// Bound moments to the capture volume; a miss remains visible throughout that volume.
	float extent = length(u_reflectionProbes[1].xyz * max(u_reflectionProbes[2].xyz, vec3_splat(2.0)));
	float distance = min(hit.distance, extent);
	imageStore(s_reflectionCaptureOutput, pixel, vec4(min(radiance, vec3_splat(60000.0)), 1.0));
	imageStore(s_reflectionVisibilityOutput, pixel, vec4(distance, distance * distance, 0.0, 0.0));
}
