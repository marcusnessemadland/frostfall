/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "raytrace.sh"
#include "raylighting.sh"

IMAGE2D_WO(s_reflectionOutput, rgba16f, 0);
IMAGE2D_ARRAY_RO(s_reflectionHit, rgba32f, 15);
uniform vec4 u_reflectionTrace;
uniform vec4 u_sceneCamera;

NUM_THREADS(8, 8, 1)
void main()
{
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(vec2(pixel), u_ddgiTrace.xy))) { return; }
	vec4 record = imageLoad(s_reflectionHit, ivec3(pixel, 0));
	if (record.z < 0.0) { return; }
	vec4 ray = imageLoad(s_reflectionHit, ivec3(pixel, 1));
	RayHit hit;
	hit.distance = record.x;
	hit.primitive = uint(record.y);
	hit.instanceIndex = uint(record.z);
	hit.barycentric = vec2(mod(record.w, 4096.0), floor(record.w / 4096.0)) / 4095.0;
	bool probeRay = float(pixel.y) >= u_ddgiTrace.z;
	vec3 position = rayHitPosition(hit);
	vec3 primary = position - ray.xyz * hit.distance;
	float radius = probeRay ? hit.distance * 2.0 / sqrt(u_ddgiGrid[1].w)
		: (mix(length(primary - u_sceneCamera.xyz), 1.0, u_sceneCamera.w) + hit.distance) / max(1.0, u_rayScene.w);
	radius += probeRay ? 0.0 : hit.distance * ray.w * ray.w * 0.5;
	vec3 color = shadeRayHit(position, ray.xyz, hit, uint(u_reflectionTrace.w), ray.w, radius, probeRay);
	float distance = hit.distance;
	if (probeRay) { distance = min(distance / u_ddgiGrid[0].w, 1.0); }
	imageStore(s_reflectionOutput, pixel, vec4(min(color, vec3_splat(60000.0)), min(distance, 60000.0)));
}
