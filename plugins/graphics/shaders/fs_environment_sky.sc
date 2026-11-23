/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

$input v_texcoord0

#include "../common/common.sh"
#include "atmosphere.sh"
#include "environment.sh"

uniform vec4 u_environmentFilter;

void main()
{
	vec2 uv = v_texcoord0;
	if (u_environmentFilter.w > 0.5) { uv.y = 1.0 - uv.y; }
	vec2 tile = min(floor(uv * vec2(3.0, 2.0)), vec2(2.0, 1.0));
	vec3 direction = environmentDirection(uv * vec2(3.0, 2.0) - tile, tile.x + tile.y * 3.0);
	// Analytic directional lights own the sun/moon disks. GI samples their scattered sky.
	gl_FragColor = vec4(evaluateSky(direction), 1.0);
}
