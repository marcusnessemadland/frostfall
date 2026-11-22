/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_baseColorMap, 0);
uniform vec4 u_materialBaseColor;
uniform vec4 u_materialEmissive;

void main()
{
	float alpha = texture2D(s_baseColorMap, v_texcoord0).a * u_materialBaseColor.a;
	if (alpha < u_materialEmissive.w) { discard; }
	gl_FragColor = vec4_splat(0.0);
}
