$input a_position, a_normal
$output v_normal

/*
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"

uniform mat4 u_normalMatrix;
uniform vec4 u_normalParams;

void main()
{
	vec3 wpos = mul(u_model[0], vec4(a_position, 1.0) ).xyz;
	gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );
	vec3 normal = a_normal.xyz * u_normalParams.x + u_normalParams.y;
	v_normal = mul(u_normalMatrix, vec4(normal, 0.0) ).xyz;
}
