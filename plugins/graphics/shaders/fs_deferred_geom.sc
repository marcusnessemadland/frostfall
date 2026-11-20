$input v_normal, v_tangent, v_bitangent, v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"

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

	gl_FragData[0] = vec4(baseColor.rgb, 1.0);
	gl_FragData[1] = vec4(encodeNormalUint(wnormal), 1.0);
	gl_FragData[2] = vec4(parameters.b * u_materialParameters.x, parameters.g * u_materialParameters.y, occlusion, 0.0);
	gl_FragData[3] = vec4(texture2D(s_emissiveMap, v_texcoord0).rgb * u_materialEmissive.rgb, 0.0);
}
