$input v_normal, v_tangent, v_bitangent, v_texcoord0, v_worldPosition

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"
#include "pbr.sh"
uniform vec4 u_capture[2];
uniform vec4 u_captureLights[8];

SAMPLER2D(s_baseColorMap, 0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_metallicRoughnessMap, 2);
SAMPLER2D(s_occlusionMap, 3);
SAMPLER2D(s_emissiveMap, 4);
uniform vec4 u_materialBaseColor;
uniform vec4 u_materialParameters; // metallic, perceptual roughness, normal scale, occlusion strength.
uniform vec4 u_materialEmissive; // linear emission, alpha cutoff.
uniform vec4 u_materialFlags; // masked, double sided.

void main()
{
	if (length(v_worldPosition - u_capture[0].xyz) < u_capture[0].w) { discard; }
	vec4 baseColor = texture2D(s_baseColorMap, v_texcoord0) * u_materialBaseColor;
	if (u_materialFlags.x > 0.5 && baseColor.a < u_materialEmissive.w)
	{
		discard;
	}
	vec3 n = normalize(v_normal);
	vec3 t = normalize(v_tangent - n * dot(n, v_tangent));
	float handedness = dot(cross(n, t), v_bitangent) < 0.0 ? -1.0 : 1.0;
	vec3 b = cross(n, t) * handedness;
	vec2 xy = texture2D(s_normalMap, v_texcoord0).rg * 2.0 - 1.0;
	vec3 mapped = normalize(vec3(xy * u_materialParameters.z, sqrt(max(0.0, 1.0 - dot(xy, xy)))));
	vec3 normal = normalize(mul(mtxFromCols(t, b, n), mapped));
	if (u_materialFlags.y > 0.5 && !gl_FrontFacing) { normal = -normal; }
	vec3 wnormal = normalize(mul(u_invView, vec4(normal, 0.0)).xyz);
	vec4 parameters = texture2D(s_metallicRoughnessMap, v_texcoord0);
	float occlusion = mix(1.0, texture2D(s_occlusionMap, v_texcoord0).r, u_materialParameters.w);

	vec3 view = normalize(u_capture[0].xyz - v_worldPosition);
	vec3 color = texture2D(s_emissiveMap, v_texcoord0).rgb * u_materialEmissive.rgb;
	for (int ii = 0; ii < 4; ++ii)
	{
		color += evaluatePbr(wnormal, view, u_captureLights[ii].xyz, baseColor.rgb,
			parameters.b * u_materialParameters.x, parameters.g * u_materialParameters.y) * u_captureLights[ii + 4].rgb;
	}
	gl_FragColor = vec4(color, 1.0);
}
