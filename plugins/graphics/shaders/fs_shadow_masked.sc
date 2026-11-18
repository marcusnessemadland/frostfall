$input v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"

SAMPLER2D(s_baseColorMap, 0);
uniform vec4 u_materialBaseColor;
uniform vec4 u_materialEmissive;

void main()
{
	if (texture2D(s_baseColorMap, v_texcoord0).a * u_materialBaseColor.a < u_materialEmissive.w)
	{
		discard;
	}
	gl_FragColor = vec4_splat(0.0);
}
