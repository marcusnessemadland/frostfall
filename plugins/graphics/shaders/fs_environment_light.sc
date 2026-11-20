/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_albedo, 1);
SAMPLER2D(s_material, 2);
SAMPLER2D(s_depth, 4);
SAMPLERCUBE(s_environment, 6);
SAMPLERCUBE(s_irradiance, 7);
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera;
uniform vec4 u_sceneDirection;
uniform vec4 u_environmentLight; // RGB intensity, maximum roughness mip.

void main()
{
	vec4 encoded = texture2D(s_normal, v_texcoord0);
	if (encoded.w < 0.5) { discard; }
	vec3 clip = vec3(v_texcoord0 * 2.0 - 1.0, toClipSpaceDepth(texture2D(s_depth, v_texcoord0).r));
	clip.y = toClipSpaceY(clip.y);
	vec3 position = clipToWorld(u_invSceneViewProj, clip);
	vec3 normal = normalize(decodeNormalUint(encoded.xyz));
	vec3 view = normalize(mix(u_sceneCamera.xyz - position, -u_sceneDirection.xyz, u_sceneCamera.w));
	vec3 baseColor = texture2D(s_albedo, v_texcoord0).rgb;
	vec3 parameters = texture2D(s_material, v_texcoord0).rgb;
	float metallic = parameters.r;
	float roughness = max(parameters.g, 0.045);
	float NoV = saturate(dot(normal, view));
	vec3 f0 = mix(vec3_splat(0.04), baseColor, metallic);
	// Analytic approximation of the integrated split-sum GGX BRDF (Lazarov, SIGGRAPH 2013).
	vec4 r = roughness * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
	float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
	vec2 brdf = vec2(-1.04, 1.04) * a004 + r.zw;
	vec3 specular = textureCubeLod(s_environment, reflect(-view, normal), roughness * u_environmentLight.w).rgb;
	vec3 diffuse = textureCube(s_irradiance, normal).rgb * baseColor * (1.0 - metallic) * (vec3_splat(1.0) - f0);
	gl_FragColor = vec4((diffuse + specular * (f0 * brdf.x + brdf.y)) * parameters.b * u_environmentLight.rgb, 0.0);
}
