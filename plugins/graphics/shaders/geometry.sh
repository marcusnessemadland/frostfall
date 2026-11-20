/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/frostfall/blob/main/LICENSE
 */

#if FROST_CAPTURE
uniform mat4 u_captureViewProjection;
uniform vec4 u_capture[2];
#endif

void main()
{
	mat4 _model = FROST_GEOMETRY_MODEL;
	vec3 wpos = mul(_model, vec4(a_position, 1.0) ).xyz;
	gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );
#if FROST_CAPTURE
	v_worldPosition = wpos;
	gl_Position = mul(u_captureViewProjection, vec4(wpos, 1.0));
	gl_Position.xy = gl_Position.xy * u_capture[1].xy + gl_Position.ww * u_capture[1].zw;
#endif

	vec4 normal = a_normal * 2.0 - 1.0;
	// Cofactors handle the shear produced by composing rotated, nonuniform parents.
	vec3 axisX = mul(_model, vec4(1.0, 0.0, 0.0, 0.0) ).xyz;
	vec3 axisY = mul(_model, vec4(0.0, 1.0, 0.0, 0.0) ).xyz;
	vec3 axisZ = mul(_model, vec4(0.0, 0.0, 1.0, 0.0) ).xyz;
	float determinant = dot(cross(axisX, axisY), axisZ);
	vec3 wnormal = (cross(axisY, axisZ) * normal.x + cross(axisZ, axisX) * normal.y
		+ cross(axisX, axisY) * normal.z) / determinant;

	vec4 tangent = a_tangent * 2.0 - 1.0;
	vec3 wtangent = mul(_model, vec4(tangent.xyz, 0.0) ).xyz;

	vec3 viewNormal = normalize(mul(u_view, vec4(wnormal, 0.0) ).xyz);
	vec3 viewTangent = mul(u_view, vec4(wtangent, 0.0) ).xyz;
	viewTangent = normalize(viewTangent - viewNormal * dot(viewNormal, viewTangent));
	float mirrorSign = dot(cross(axisX, axisY), axisZ) < 0.0 ? -1.0 : 1.0;
	vec3 viewBitangent = cross(viewNormal, viewTangent) * tangent.w * mirrorSign;
	v_normal    = viewNormal;
	v_tangent   = viewTangent;
	v_bitangent = viewBitangent;

	v_texcoord0 = a_texcoord0;
}
