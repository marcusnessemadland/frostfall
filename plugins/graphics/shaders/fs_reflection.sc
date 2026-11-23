/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
$input v_texcoord0
#include "../common/common.sh"
#include "pbr.sh"
#include "reflection.sh"
#define FROST_DDGI_SAMPLE 1
#include "ddgi.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_albedo, 1);
SAMPLER2D(s_material, 2);
SAMPLER2D(s_roughness, 11);
SAMPLER2D(s_depth, 4);
SAMPLER2D(s_reflection, 6);
SAMPLER2D(s_reflectionGuide, 7);
SAMPLER2D(s_reflectionSurface, 8);
SAMPLERCUBEARRAY(s_reflectionProbes, 9);
SAMPLERCUBEARRAY(s_reflectionVisibility, 10);
uniform vec4 u_reflectionProbes[3];
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_reflectionResolve; // Pixel projection scale, maximum mip, diffuse enabled, glossy enabled.
uniform vec4 u_reflectionScreen;

vec4 gatherReflection(vec2 _uv, float _mip, vec3 _position, vec3 _normal, float _tolerance)
{
	vec2 size = max(vec2_splat(1.0), floor(u_reflectionScreen.xy * exp2(-_mip)));
	vec2 coordinate = _uv * size - 0.5;
	vec2 base = floor(coordinate), fraction = fract(coordinate);
	vec4 sum = vec4_splat(0.0);
	for (int yy = 0; yy < 2; ++yy)
	{
		for (int xx = 0; xx < 2; ++xx)
		{
			vec2 pixel = clamp(base + vec2(float(xx), float(yy)), vec2_splat(0.0), size - 1.0);
			vec2 uv = (pixel + 0.5) / size;
			vec4 value = texture2DLod(s_reflection, uv, _mip);
			vec4 guide = texture2DLod(s_reflectionGuide, uv, _mip);
			float spatial = (xx == 0 ? 1.0 - fraction.x : fraction.x) * (yy == 0 ? 1.0 - fraction.y : fraction.y);
			float weight = value.a < 0.0 ? 0.0 : spatial * reflectionWeight(guide, _position, _normal, _tolerance);
			sum += vec4(value.rgb * weight, weight);
		}
	}
	// Missing coverage fades out instead of promoting a sharp or unrelated sample.
	return sum;
}

vec3 sampleReflectionProbes(vec3 _position, vec3 _normal, vec3 _direction, float _roughness)
{
	vec3 grid = clamp((_position - u_reflectionProbes[0].xyz) / u_reflectionProbes[1].xyz,
		vec3_splat(0.0), u_reflectionProbes[2].xyz - 1.0);
	vec3 base = min(floor(grid), u_reflectionProbes[2].xyz - 2.0), fraction = grid - base;
	vec3 sum = vec3_splat(0.0);
	float weight = 0.0, support = 0.0;
	for (int zz = 0; zz < 2; ++zz)
	{
		for (int yy = 0; yy < 2; ++yy)
		{
			for (int xx = 0; xx < 2; ++xx)
			{
				vec3 offset = vec3(float(xx), float(yy), float(zz)), cell = base + offset;
				float probe = cell.x + u_reflectionProbes[2].x * (cell.y + u_reflectionProbes[2].y * cell.z);
				vec3 center = u_reflectionProbes[0].xyz + cell * u_reflectionProbes[1].xyz;
				vec3 delta = _position + _normal * 0.02 - center;
				float distance = max(length(delta), 0.0001);
				vec2 moments = textureCubeArrayLod(s_reflectionVisibility, vec4(delta / distance, probe), 0.0).rg;
				float variance = max(moments.y - moments.x * moments.x, 0.0001);
				float difference = max(0.0, distance - moments.x - 0.02);
				float visibility = variance / (variance + difference * difference);
				visibility = smoothstep(0.2, 1.0, visibility * visibility * visibility);
				vec3 spatial = mix(1.0 - fraction, fraction, offset);
				float facing = saturate(dot(_normal, -delta / distance));
				float w = spatial.x * spatial.y * spatial.z * facing * facing;
				support += w;
				w *= visibility;
				sum += textureCubeArrayLod(s_reflectionProbes, vec4(_direction, probe), _roughness * u_reflectionProbes[1].w).rgb * w;
				weight += w;
			}
		}
	}
	// Visibility confidence must survive normalization of the available probes.
	float confidence = weight / max(support, 0.0001);
	return sum / max(weight, 0.01) * smoothstep(0.05, 0.25, confidence);
}

void main()
{
	vec4 surface = texture2D(s_reflectionSurface, v_texcoord0);
	if (surface.x > 1.5) { discard; }
	vec3 normal = reflectionDecodeNormal(surface.xy), geometric = reflectionDecodeNormal(surface.zw);
	vec3 clip = vec3(v_texcoord0 * 2.0 - 1.0, toClipSpaceDepth(texture2D(s_depth, v_texcoord0).r));
	clip.y = toClipSpaceY(clip.y);
	vec3 position = clipToWorld(u_invSceneViewProj, clip);
	vec3 relative = position - u_sceneCamera.xyz;
	vec3 view = normalize(mix(-relative, -u_sceneDirection.xyz, u_sceneCamera.w));
	vec3 baseColor = texture2D(s_albedo, v_texcoord0).rgb;
	vec3 parameters = texture2D(s_material, v_texcoord0).rgb;
	float roughness = max(0.045, texture2D(s_roughness, v_texcoord0).r);
	vec3 diffuse = vec3_splat(0.0);
	if (u_reflectionResolve.z > 0.5)
	{
		// Diffuse uses the material normal, independently of the filtered glossy
		// normal or its view-facing test. A grazing glossy ray must not erase GI.
		vec3 diffuseNormal = normalize(decodeNormalUint(texture2D(s_normal, v_texcoord0).xyz));
		vec3 irradiance = sampleDiffuseGi(position, diffuseNormal, geometric, view);
		diffuse = u_reflectionScreen.w == 12.0 ? irradiance
			: evaluateIndirectDiffuse(irradiance, baseColor, parameters.r) * parameters.b;
	}
	if (u_reflectionResolve.w < 0.5 || dot(normal, view) <= 0.0)
	{
		gl_FragColor = vec4(diffuse, 0.0);
		return;
	}
	vec3 direction = reflect(-view, normal);
	vec4 reflection = vec4_splat(0.0);
	float rayWeight = reflectionRayWeight(roughness) * smoothstep(0.0, 0.1, dot(direction, geometric));
	BRANCH if (rayWeight > 0.0)
	{
		vec4 mirror = texture2DLod(s_reflection, v_texcoord0, 0.0);
		float depth = dot(relative, u_sceneDirection.xyz);
		float distance = mirror.a >= 0.0 ? mirror.a : max(0.001, depth);
		float footprint = roughness * roughness * distance * u_reflectionResolve.x;
		if (u_sceneCamera.w < 0.5) { footprint /= max(length(relative) + distance, 0.001); }
		footprint = max(footprint, roughness * roughness * 4.0);
		float mip = clamp(log2(max(1.0, footprint)), 0.0, u_reflectionResolve.y);
		float tolerance = reflectionTolerance(depth, u_reflectionResolve.x, u_sceneCamera.w);
		float level = floor(mip), blend = fract(mip);
		reflection = gatherReflection(v_texcoord0, level, relative, geometric, tolerance);
		BRANCH if (blend > 0.001)
		{
			reflection = mix(reflection, gatherReflection(v_texcoord0, min(level + 1.0, u_reflectionResolve.y), relative, geometric, tolerance), blend);
		}
	}
	float coverage = rayWeight * saturate(reflection.a);
	vec3 radiance = reflection.rgb * rayWeight;
	BRANCH if (coverage < 0.999)
	{
		radiance += sampleReflectionProbes(position, geometric, direction, roughness) * (1.0 - coverage);
	}
	vec3 f0 = mix(vec3_splat(0.04), baseColor, parameters.r);
	gl_FragColor = vec4(diffuse + evaluateIndirectSpecular(radiance, saturate(dot(normal, view)), f0, roughness) * parameters.b, 0.0);
}
