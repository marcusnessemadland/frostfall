/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "reflection.sh"

IMAGE2D_WO(s_reflectionOutput, rgba16f, 0);
IMAGE2D_RO(s_reflectionInput, rgba16f, 1);
IMAGE2D_WO(s_reflectionGuideOutput, rgba16f, 2);
IMAGE2D_RO(s_reflectionGuideInput, rgba16f, 3);
SAMPLER2D(s_depth, 4);
SAMPLER2D(s_reflectionSurface, 5);
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_reflectionFilter; // Output width/height, input width/height.
uniform vec4 u_reflectionProjection; // Projection Z scale/offset, orthographic, reconstructing mip zero.
uniform vec4 u_reflectionScreen; // Screen width/height, pixel projection scale, unused.

vec3 reflectionPosition(vec2 _uv)
{
	vec3 clip = vec3(_uv * 2.0 - 1.0, toClipSpaceDepth(texture2DLod(s_depth, _uv, 0.0).r));
	clip.y = toClipSpaceY(clip.y);
	return clipToWorld(u_invSceneViewProj, clip) - u_sceneCamera.xyz;
}

NUM_THREADS(8, 8, 1)
void main()
{
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(vec2(pixel), u_reflectionFilter.xy))) { return; }
	vec2 uv = (vec2(pixel) + 0.5) / u_reflectionFilter.xy;
	vec4 surface = texture2DLod(s_reflectionSurface, uv, 0.0);
	if (surface.x > 1.5)
	{
		imageStore(s_reflectionOutput, pixel, vec4(0.0, 0.0, 0.0, -1.0));
		imageStore(s_reflectionGuideOutput, pixel, vec4_splat(0.0));
		return;
	}
	vec3 position = reflectionPosition(uv);
	vec3 normal = reflectionDecodeNormal(surface.zw);
	float depth = dot(position, u_sceneDirection.xyz);
	float tolerance = reflectionTolerance(depth, u_reflectionScreen.z, u_reflectionProjection.z);
	vec4 sum = vec4_splat(0.0);
	float weight = 0.0, available = 0.0;
	bool reconstruct = u_reflectionProjection.w > 0.5;
	vec2 source = reconstruct ? vec2(float(pixel.x), float(pixel.y) * 0.5) : uv * u_reflectionFilter.zw - 0.5;
	ivec2 center = ivec2(floor(source));
	for (int yy = -1; yy <= 1; ++yy)
	{
		for (int xx = -1; xx <= 1; ++xx)
		{
			if (reconstruct && (xx != 0 || yy < 0)) { continue; }
			ivec2 samplePixel = clamp(center + ivec2(xx, yy), ivec2(0, 0), ivec2(u_reflectionFilter.zw) - 1);
			vec2 delta = vec2(samplePixel) - source;
			float spatial = reconstruct ? max(0.0, 1.0 - abs(delta.y)) : exp(-dot(delta, delta) * 1.5);
			if (spatial <= 0.0) { continue; }
			available += spatial;
			vec4 value = imageLoad(s_reflectionInput, samplePixel);
			if (value.a < 0.0) { continue; }
			vec4 guide;
			BRANCH if (reconstruct)
			{
				vec2 sampleUv = (vec2(float(samplePixel.x), float(samplePixel.y * 2)) + 0.5) / u_reflectionScreen.xy;
				vec4 sampleSurface = texture2DLod(s_reflectionSurface, sampleUv, 0.0);
				if (sampleSurface.x > 1.5) { continue; }
				vec3 sampleNormal = reflectionDecodeNormal(sampleSurface.zw);
				guide = vec4(sampleNormal, dot(sampleNormal, reflectionPosition(sampleUv)));
			}
			else { guide = imageLoad(s_reflectionGuideInput, samplePixel); }
			float w = spatial * reflectionWeight(guide, position, normal, tolerance);
			sum += value * w;
			weight += w;
		}
	}
	vec4 result = vec4(0.0, 0.0, 0.0, -1.0);
	vec4 guide = vec4_splat(0.0);
	if (weight > 0.0001)
	{
		result = sum / weight;
		guide = vec4(normal * min(1.0, weight / max(available, 0.0001)), dot(normal, position));
	}
	imageStore(s_reflectionOutput, pixel, result);
	imageStore(s_reflectionGuideOutput, pixel, guide);
}
