/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "raytrace.sh"
#include "reflection.sh"
#include "raylighting.sh"
IMAGE2D_WO(s_reflectionOutput, rgba16f, 0);
IMAGE2D_RO(s_reflectionHit, rgba32f, 15);
SAMPLER2D(s_reflectionSurface, 13);
SAMPLER2D(s_depth, 14);
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_reflectionTrace;
NUM_THREADS(8, 8, 1)
void main()
{
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	if (pixel.x >= int(u_reflectionTrace.x) || pixel.y >= int(u_reflectionTrace.z)) { return; }
	vec4 record = imageLoad(s_reflectionHit, pixel);
	if (record.z < 0.0) { return; }
	// Pick an actual G-buffer row, avoiding interpolation of unrelated surfaces.
	ivec2 scenePixel = ivec2(pixel.x, min(pixel.y * 2, int(u_reflectionTrace.y) - 1));
	vec2 uv = (vec2(scenePixel) + 0.5) / u_reflectionTrace.xy;
	vec4 surface = texture2DLod(s_reflectionSurface, uv, 0.0);
	if (surface.x > 1.5)
	{
		imageStore(s_reflectionOutput, pixel, vec4(0.0, 0.0, 0.0, -1.0));
		return;
	}
	vec3 clip = vec3(uv * 2.0 - 1.0, toClipSpaceDepth(texture2DLod(s_depth, uv, 0.0).r));
	clip.y = toClipSpaceY(clip.y);
	vec3 position = clipToWorld(u_invSceneViewProj, clip);
	vec3 normal = reflectionDecodeNormal(surface.xy);
	vec3 geometric = reflectionDecodeNormal(surface.zw);
	vec3 view = normalize(mix(u_sceneCamera.xyz - position, -u_sceneDirection.xyz, u_sceneCamera.w));
	vec3 direction = reflect(-view, normal);
	vec3 origin = reflectionOrigin(position, geometric, u_rayScene.z);
	RayHit hit;
	hit.distance = record.x;
	hit.primitive = uint(record.y);
	hit.instanceIndex = uint(max(0.0, record.z));
	hit.barycentric = vec2(mod(record.w, 4096.0), floor(record.w / 4096.0)) / 4095.0;
	vec3 color = shadeRayHit(origin, direction, hit, uint(u_reflectionTrace.w));
	imageStore(s_reflectionOutput, pixel, vec4(min(color, vec3_splat(60000.0)), min(hit.distance, 60000.0)));
}
