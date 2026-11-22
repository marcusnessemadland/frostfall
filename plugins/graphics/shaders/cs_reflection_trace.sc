/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "raytrace.sh"
#include "reflection.sh"
IMAGE2D_WO(s_reflectionOutput, rgba32f, 0);
IMAGE2D_WO(s_reflectionMirror, rgba16f, 1);
SAMPLERCUBE(s_rayEnvironment, 12);
uniform vec4 u_rayEnvironment;
SAMPLER2D(s_reflectionSurface, 13);
SAMPLER2D(s_depth, 14);
SAMPLER2D(s_material, 15);
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_reflectionTrace;
NUM_THREADS(8, 8, 1)
void main()
{
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	if (pixel.x >= int(u_reflectionTrace.x) || pixel.y >= int(u_reflectionTrace.z)) { return; }
	// Pick an actual G-buffer row, avoiding interpolation of unrelated surfaces.
	ivec2 scenePixel = ivec2(pixel.x, min(pixel.y * 2, int(u_reflectionTrace.y) - 1));
	vec2 uv = (vec2(scenePixel) + 0.5) / u_reflectionTrace.xy;
	vec4 surface = texture2DLod(s_reflectionSurface, uv, 0.0);
	if (surface.x > 1.5 || reflectionRayWeight(texture2DLod(s_material, uv, 0.0).g) <= 0.0)
	{
		imageStore(s_reflectionOutput, pixel, vec4(-1.0, 0.0, -1.0, 0.0));
		imageStore(s_reflectionMirror, pixel, vec4(0.0, 0.0, 0.0, -1.0));
		return;
	}
	vec3 clip = vec3(uv * 2.0 - 1.0, toClipSpaceDepth(texture2DLod(s_depth, uv, 0.0).r));
	clip.y = toClipSpaceY(clip.y);
	vec3 position = clipToWorld(u_invSceneViewProj, clip);
	vec3 normal = reflectionDecodeNormal(surface.xy);
	vec3 geometric = reflectionDecodeNormal(surface.zw);
	vec3 view = normalize(mix(u_sceneCamera.xyz - position, -u_sceneDirection.xyz, u_sceneCamera.w));
	vec3 direction = reflect(-view, normal);
	if (dot(view, normal) <= 0.0 || dot(direction, geometric) <= 0.0)
	{
		imageStore(s_reflectionOutput, pixel, vec4(-1.0, 0.0, -1.0, 0.0));
		imageStore(s_reflectionMirror, pixel, vec4(0.0, 0.0, 0.0, -1.0));
		return;
	}
	vec3 origin = reflectionOrigin(position, geometric, u_rayScene.z);
	RayHit hit;
	hit.distance = 1e30;
	bool found = traceRay(origin, direction, false, hit);
	vec4 record = found ? vec4(hit.distance, float(hit.primitive), float(hit.instanceIndex), floor(hit.barycentric.x * 4095.0 + 0.5) + floor(hit.barycentric.y * 4095.0 + 0.5) * 4096.0)
		: vec4(1e30, 0.0, -1.0, 0.0);
	imageStore(s_reflectionOutput, pixel, record);
	vec3 environment = found ? vec3_splat(0.0) : textureCubeLod(s_rayEnvironment, direction, 0.0).rgb * u_rayEnvironment.rgb;
	imageStore(s_reflectionMirror, pixel, vec4(min(environment, vec3_splat(60000.0)), min(hit.distance, 60000.0)));
}
