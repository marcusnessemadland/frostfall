/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "reflection.sh"
#include "pbr.sh"

IMAGE2D_WO(s_reflectionSurfaceOutput, rgba16f, 0);
IMAGE2D_WO(s_roughnessOutput, r32f, 4);
SAMPLER2D(s_normal, 1);
SAMPLER2D(s_depth, 2);
SAMPLER2D(s_material, 3);
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_reflectionScreen;

vec3 reflectionPosition(vec2 _uv)
{
	vec3 clip = vec3(_uv * 2.0 - 1.0, toClipSpaceDepth(texture2DLod(s_depth, _uv, 0.0).r));
	clip.y = toClipSpaceY(clip.y);
	return clipToWorld(u_invSceneViewProj, clip);
}

NUM_THREADS(8, 8, 1)
void main()
{
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(vec2(pixel), u_reflectionScreen.xy))) { return; }
	vec2 uv = (vec2(pixel) + 0.5) / u_reflectionScreen.xy;
	vec4 encoded = texture2DLod(s_normal, uv, 0.0);
	if (encoded.w < 0.5)
	{
		imageStore(s_reflectionSurfaceOutput, pixel, vec4_splat(2.0));
		imageStore(s_roughnessOutput, pixel, vec4_splat(0.0));
		return;
	}
	vec3 position = reflectionPosition(uv);
	vec3 normal = normalize(decodeNormalUint(encoded.xyz));
	vec3 view = normalize(mix(u_sceneCamera.xyz - position, -u_sceneDirection.xyz, u_sceneCamera.w));
	vec2 offsets[4];
	offsets[0] = vec2(-1.0, 0.0); offsets[1] = vec2(1.0, 0.0);
	offsets[2] = vec2(0.0, -1.0); offsets[3] = vec2(0.0, 1.0);
	vec3 delta[4];
	vec4 neighbors[4];
	for (int ii = 0; ii < 4; ++ii)
	{
		vec2 sampleUv = clamp(uv + offsets[ii] / u_reflectionScreen.xy,
			0.5 / u_reflectionScreen.xy, 1.0 - 0.5 / u_reflectionScreen.xy);
		delta[ii] = reflectionPosition(sampleUv) - position;
		neighbors[ii] = texture2DLod(s_normal, sampleUv, 0.0);
	}
	float left = dot(delta[0], delta[0]);
	if (neighbors[0].w < 0.5 || left < 1e-16) { left = 1e30; }
	float right = dot(delta[1], delta[1]);
	if (neighbors[1].w < 0.5 || right < 1e-16) { right = 1e30; }
	float top = dot(delta[2], delta[2]);
	if (neighbors[2].w < 0.5 || top < 1e-16) { top = 1e30; }
	float bottom = dot(delta[3], delta[3]);
	if (neighbors[3].w < 0.5 || bottom < 1e-16) { bottom = 1e30; }
	vec3 dx = left < right ? -delta[0] : delta[1];
	vec3 dy = top < bottom ? -delta[2] : delta[3];
	vec3 geometric = cross(dx, dy);
	if (dot(geometric, geometric) < 1e-16 || min(left, right) >= 1e30 || min(top, bottom) >= 1e30) { geometric = normal; }
	geometric = normalize(geometric);
	if (dot(geometric, view) < 0.0) { geometric = -geometric; }

	// Rough reflectors filter normal detail spatially; smooth reflectors retain their mapped normal.
	float roughness = texture2DLod(s_material, uv, 0.0).g;
	// Match fullscreen quad derivatives, including boundaries between rasterized primitives.
	vec3 normalDx = normalize(decodeNormalUint(neighbors[(pixel.x & 1) == 0 ? 1 : 0].xyz)) - normal;
	vec3 normalDy = normalize(decodeNormalUint(neighbors[(pixel.y & 1) == 0 ? 3 : 2].xyz)) - normal;
	float filteredRoughness = u_reflectionSettings.x > 0.5 ? filterSpecularRoughness(normalDx, normalDy, roughness) : roughness;
	imageStore(s_roughnessOutput, pixel, vec4(filteredRoughness, 0.0, 0.0, 0.0));
	float blend = smoothstep(0.2, 0.75, roughness);
	BRANCH if (blend > 0.0)
	{
		vec3 sum = normal;
		float count = 1.0;
		float tolerance = max(0.002, min(length(dx), length(dy)) * 0.25);
		for (int ii = 0; ii < 4; ++ii)
		{
			if (neighbors[ii].w < 0.5 || abs(dot(geometric, delta[ii])) > tolerance) { continue; }
			sum += normalize(decodeNormalUint(neighbors[ii].xyz));
			count += 1.0;
		}
		if (dot(sum, sum) > 1e-8) { normal = normalize(mix(normal, sum / count, blend)); }
	}
	imageStore(s_reflectionSurfaceOutput, pixel, vec4(reflectionEncodeNormal(normal), reflectionEncodeNormal(geometric)));
}
