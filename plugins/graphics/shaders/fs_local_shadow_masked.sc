/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

$input v_texcoord0, v_worldPosition

#include "../common/common.sh"

uniform vec4 u_localShadow[2]; // position/range, emission direction/spot flag.
SAMPLER2D(s_baseColorMap, 0);
uniform vec4 u_materialBaseColor;
uniform vec4 u_materialEmissive;

void main()
{
	float alpha = texture2D(s_baseColorMap, v_texcoord0).a * u_materialBaseColor.a;
	if (alpha < u_materialEmissive.w) { discard; }
	vec3 delta = v_worldPosition - u_localShadow[0].xyz;
	float depth = length(delta);
	if (u_localShadow[1].w > 0.5) { depth = dot(delta, u_localShadow[1].xyz); }
	gl_FragDepth = saturate(depth / u_localShadow[0].w);
	gl_FragColor = vec4_splat(0.0);
}
