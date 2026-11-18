$input a_position, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"

uniform vec4 u_shadowTile;

void main()
{
	mat4 modelProjection = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
	vec4 clip = mul(modelProjection, vec4(a_position, 1.0));
	clip.x = clip.x * u_shadowTile.x + clip.w * u_shadowTile.y;
	gl_Position = clip;
	v_texcoord0 = a_texcoord0;
}
