$input a_position, a_normal, a_tangent, a_texcoord0
$output v_wpos, v_view, v_normal, v_tangent, v_bitangent, v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/frostfall/blob/main/LICENSE
 */

#include "../common/common.sh"

void main()
{
	vec3 wpos = mul(u_model[0], vec4(a_position, 1.0) ).xyz;
	gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );

	vec4 normal = a_normal * 2.0 - 1.0;
	// For an SRT model, dividing by squared axis lengths before applying the
	// model is equivalent to the inverse-transpose normal transform.
	vec3 axisX = mul(u_model[0], vec4(1.0, 0.0, 0.0, 0.0) ).xyz;
	vec3 axisY = mul(u_model[0], vec4(0.0, 1.0, 0.0, 0.0) ).xyz;
	vec3 axisZ = mul(u_model[0], vec4(0.0, 0.0, 1.0, 0.0) ).xyz;
	vec3 scaleSquared = vec3(dot(axisX, axisX), dot(axisY, axisY), dot(axisZ, axisZ));
	vec3 wnormal = mul(u_model[0], vec4(normal.xyz / scaleSquared, 0.0) ).xyz;

	vec4 tangent = a_tangent * 2.0 - 1.0;
	vec3 wtangent = mul(u_model[0], vec4(tangent.xyz, 0.0) ).xyz;

	vec3 viewNormal = normalize(mul(u_view, vec4(wnormal, 0.0) ).xyz);
	vec3 viewTangent = mul(u_view, vec4(wtangent, 0.0) ).xyz;
	viewTangent = normalize(viewTangent - viewNormal * dot(viewNormal, viewTangent));
	float mirrorSign = dot(cross(axisX, axisY), axisZ) < 0.0 ? -1.0 : 1.0;
	vec3 viewBitangent = cross(viewNormal, viewTangent) * tangent.w * mirrorSign;
	mat3 tbn = mtxFromCols(viewTangent, viewBitangent, viewNormal);

	v_wpos = wpos;

	vec3 view = mul(u_view, vec4(wpos, 0.0) ).xyz;
	v_view = mul(view, tbn);

	v_normal    = viewNormal;
	v_tangent   = viewTangent;
	v_bitangent = viewBitangent;

	v_texcoord0 = a_texcoord0;
}
