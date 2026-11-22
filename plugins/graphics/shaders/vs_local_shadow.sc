/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

$input a_position, a_texcoord0
$output v_texcoord0

#include "../common/common.sh"

uniform mat4 u_localShadowViewProjection;
uniform vec4 u_localShadow[3]; // position/range, direction/type, atlas tile.

void main()
{
	mat4 model = u_model[0];
	vec4 worldPosition = mul(model, vec4(a_position, 1.0));
	gl_Position = mul(u_localShadowViewProjection, worldPosition);
	gl_Position.xy = gl_Position.xy * u_localShadow[2].xy + gl_Position.ww * u_localShadow[2].zw;
	v_texcoord0 = a_texcoord0;
}
