$input a_position, a_texcoord0
$output v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"

uniform mat4 u_shadowViewProjection; // Model to light clip space.
uniform vec4 u_shadowTile; // Clip-space X scale and offset for this atlas tile.

void main()
{
	vec4 clip = mul(u_shadowViewProjection, vec4(a_position, 1.0));
	clip.x = clip.x * u_shadowTile.x + clip.w * u_shadowTile.y;
	gl_Position = clip;
	v_texcoord0 = a_texcoord0;
}
