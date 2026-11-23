/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "raytrace.sh"
#include "ddgi.sh"

IMAGE2D_ARRAY_WO(s_reflectionOutput, rgba32f, 0);
IMAGE2D_WO(s_reflectionMirror, rgba16f, 1);
SAMPLERCUBE(s_rayEnvironment, 12);
SAMPLER2D(s_reflectionSurface, 13);
SAMPLER2D(s_depth, 14);
SAMPLER2D(s_roughness, 15);
uniform vec4 u_rayEnvironment;
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_reflectionTrace; // Screen width/height, glossy rows, light count.

NUM_THREADS(8, 8, 1)
void main()
{
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(vec2(pixel), u_ddgiTrace.xy))) { return; }
	bool probeRay = float(pixel.y) >= u_ddgiTrace.z;
	vec3 origin = vec3_splat(0.0), direction = vec3(0.0, 1.0, 0.0);
	float roughness = 1.0;
	bool valid = true;
	if (probeRay)
	{
		uint index = uint(pixel.y - int(u_ddgiTrace.z)) * uint(u_ddgiTrace.x) + uint(pixel.x);
		uint rays = uint(u_ddgiGrid[1].w);
		uint probe = index / rays;
		valid = probe < uint(u_ddgiGrid[2].x * u_ddgiGrid[2].y * u_ddgiGrid[2].z);
		if (valid)
		{
			origin = ddgiPosition(probe);
			direction = ddgiDirection(index % rays);
		}
	}
	else
	{
		valid = float(pixel.x) < u_reflectionTrace.x && float(pixel.y) < u_reflectionTrace.z && u_ddgiTrace.w > 0.5;
		if (valid)
		{
			ivec2 scenePixel = ivec2(pixel.x, min(pixel.y * 2, int(u_reflectionTrace.y) - 1));
			vec2 uv = (vec2(scenePixel) + 0.5) / u_reflectionTrace.xy;
			vec4 surface = texture2DLod(s_reflectionSurface, uv, 0.0);
			roughness = texture2DLod(s_roughness, uv, 0.0).r;
			valid = surface.x <= 1.5 && reflectionRayWeight(roughness) > 0.0;
			if (valid)
			{
				vec3 clip = vec3(uv * 2.0 - 1.0, toClipSpaceDepth(texture2DLod(s_depth, uv, 0.0).r));
				clip.y = toClipSpaceY(clip.y);
				vec3 position = clipToWorld(u_invSceneViewProj, clip);
				vec3 normal = reflectionDecodeNormal(surface.xy), geometric = reflectionDecodeNormal(surface.zw);
				vec3 view = normalize(mix(u_sceneCamera.xyz - position, -u_sceneDirection.xyz, u_sceneCamera.w));
				direction = reflect(-view, normal);
				origin = reflectionOrigin(position, geometric, u_rayScene.z);
				valid = dot(view, normal) > 0.0 && dot(direction, geometric) > 0.0;
			}
		}
	}
	vec4 record = vec4(-1.0, 0.0, -1.0, 0.0);
	vec4 result = vec4(0.0, 0.0, 0.0, -1.0);
	if (valid)
	{
		RayHit hit;
		hit.distance = 1e30;
		hit.primitive = 0u; hit.instanceIndex = 0u; hit.barycentric = vec2_splat(0.0);
		bool found = traceRay(origin, direction, false, hit);
		bool backface = false;
		if (found) { backface = rayHitBackface(hit, direction); }
		record = found ? vec4(hit.distance, float(hit.primitive), backface ? -2.0 : float(hit.instanceIndex),
			floor(hit.barycentric.x * 4095.0 + 0.5) + floor(hit.barycentric.y * 4095.0 + 0.5) * 4096.0)
			: vec4(1e30, 0.0, -1.0, 0.0);
		vec3 radiance = found ? vec3_splat(0.0) : textureCubeLod(s_rayEnvironment, direction, 0.0).rgb * u_rayEnvironment.rgb;
		float distance = hit.distance;
		// Probe distances are normalized before storing in the half-float work buffer.
		if (probeRay) { distance = min(distance * (backface ? 0.2 : 1.0) / u_ddgiGrid[0].w, 1.0); }
		result = vec4(min(radiance, vec3_splat(60000.0)), min(distance, 60000.0));
	}
	imageStore(s_reflectionOutput, ivec3(pixel, 0), record);
	imageStore(s_reflectionOutput, ivec3(pixel, 1), vec4(direction, roughness));
	imageStore(s_reflectionMirror, pixel, result);
}
