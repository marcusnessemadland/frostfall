$input a_position, a_texcoord0
$output v_probe

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include "ddgi.sh"

uniform vec4 u_ddgiDebug; // Radius, scene depth test, valid irradiance, unused.
uniform vec4 u_sceneCamera;

void main()
{
	vec3 center = ddgiPosition(uint(a_position.x));
	v_probe = vec4(center, a_position.x);
	vec3 view = mul(u_view, vec4(center, 1.0)).xyz;
	float radius = u_ddgiDebug.x;
	vec2 scale = vec2(u_proj[0][0], u_proj[1][1]);
	vec2 position;
	if (u_sceneCamera.w > 0.5)
	{
		vec4 clip = mul(u_viewProj, vec4(center, 1.0));
		position = clip.xy / clip.w + a_texcoord0 * abs(scale) * radius;
	}
	else if (view.z > radius)
	{
		// Exact tangent bounds of a perspective sphere, including off-axis probes.
		float denominator = view.z * view.z - radius * radius;
		vec2 projected = scale * view.xy * view.z / denominator;
		vec2 extent = abs(scale) * radius * sqrt(view.xy * view.xy + denominator) / denominator;
		position = projected + a_texcoord0 * extent;
	}
	else
	{
		// A sphere crossing the eye plane can cover the entire viewport.
		position = view.z >= -radius ? a_texcoord0 : vec2_splat(2.0);
	}
	gl_Position = vec4(position, 0.0, 1.0);
}
