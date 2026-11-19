$input v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"
#include "shadow.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_albedo, 1);
SAMPLER2D(s_material, 2);
SAMPLER2D(s_emissive, 3);
SAMPLER2D(s_depth, 4);
uniform vec4 u_sunDirection;
uniform vec4 u_sunLuminance; // w == 0 seeds emission; w == 1 adds one light.
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera; // position, orthographic flag.
uniform vec4 u_sceneDirection;

void main()
{
	if (u_sunLuminance.w < -0.5) { gl_FragColor = vec4_splat(0.0); return; }
	vec4 encodedNormal = texture2D(s_normal, v_texcoord0);
	if (encodedNormal.w < 0.5) { discard; }
	if (u_sunLuminance.w < 0.5)
	{
		gl_FragColor = vec4(texture2D(s_emissive, v_texcoord0).rgb, 1.0);
		return;
	}
	float depth = texture2D(s_depth, v_texcoord0).x;
	vec3 clip = vec3(v_texcoord0 * 2.0 - 1.0, toClipSpaceDepth(depth));
	clip.y = toClipSpaceY(clip.y);
	vec3 position = clipToWorld(u_invSceneViewProj, clip);
	vec3 view = normalize(mix(u_sceneCamera.xyz - position, -u_sceneDirection.xyz, u_sceneCamera.w));
	vec3 normal = normalize(decodeNormalUint(encodedNormal.xyz));
	vec3 light = u_sunDirection.xyz;
	float NoL = saturate(dot(normal, light));
	float NoV = max(dot(normal, view), 0.0001);
	vec3 halfway = light + view;
	halfway *= inversesqrt(max(dot(halfway, halfway), 0.000001));
	float NoH = saturate(dot(normal, halfway));
	float VoH = saturate(dot(view, halfway));
	vec3 baseColor = texture2D(s_albedo, v_texcoord0).rgb;
	vec3 parameters = texture2D(s_material, v_texcoord0).rgb;
	float metallic = parameters.r;
	float roughness = max(parameters.g, 0.045);
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denominator = (1.0 - NoH * NoH) + NoH * NoH * alpha2;
	float distribution = alpha2 / (3.14159265359 * denominator * denominator);
	float gv = NoL * sqrt(NoV * NoV * (1.0 - alpha2) + alpha2);
	float gl = NoV * sqrt(NoL * NoL * (1.0 - alpha2) + alpha2);
	float visibility = 0.5 / max(gv + gl, 0.000001);
	vec3 f0 = mix(vec3_splat(0.04), baseColor, metallic);
	vec3 fresnel = f0 + (vec3_splat(1.0) - f0) * pow(1.0 - VoH, 5.0);
	vec3 diffuse = (vec3_splat(1.0) - fresnel) * (1.0 - metallic) * baseColor / 3.14159265359;
	vec3 specular = distribution * visibility * fresnel;
	// Material occlusion is reserved for indirect illumination.
	gl_FragColor = vec4((diffuse + specular) * u_sunLuminance.rgb * NoL
		* shadowVisibility(position, normal), 0.0);
}
