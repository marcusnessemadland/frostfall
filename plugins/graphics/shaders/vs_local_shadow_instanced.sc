/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

$input a_position, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0, v_worldPosition

#include "../common/common.sh"

uniform mat4 u_localShadowViewProjection;
uniform vec4 u_localShadow[3]; // position/range, direction/type, atlas tile.

void main()
{
	mat4 model = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
	v_worldPosition = mul(model, vec4(a_position, 1.0)).xyz;
	gl_Position = mul(u_localShadowViewProjection, vec4(v_worldPosition, 1.0));
	gl_Position.xy = gl_Position.xy * u_localShadow[2].xy + gl_Position.ww * u_localShadow[2].zw;
	v_texcoord0 = a_texcoord0;
}
