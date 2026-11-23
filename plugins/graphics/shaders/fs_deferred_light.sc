$input v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"
#include "shadow.sh"
#include "pbr.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_albedo, 1);
SAMPLER2D(s_material, 2);
SAMPLER2D(s_roughness, 11);
SAMPLER2D(s_emissive, 3);
SAMPLER2D(s_depth, 4);
uniform vec4 u_sunDirection;
uniform vec4 u_sunLuminance; // w == 0 seeds emission; w == 1 adds one light.
uniform mat4 u_invSceneViewProj;
uniform vec4 u_sceneCamera; // position, orthographic flag.
uniform vec4 u_sceneDirection;

void main()
{
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
	vec3 baseColor = texture2D(s_albedo, v_texcoord0).rgb;
	vec3 parameters = texture2D(s_material, v_texcoord0).rgb;
	float roughness = texture2D(s_roughness, v_texcoord0).r;
	// Material occlusion is reserved for indirect illumination.
	gl_FragColor = vec4(evaluatePbr(normal, view, light, baseColor, parameters.r, roughness)
		* u_sunLuminance.rgb * shadowVisibility(position, normal, int(u_shadowSelection.x)), 0.0);
}
