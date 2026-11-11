$input v_texcoord0

/*
 * Adapted from bgfx/examples/21-deferred.
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"

SAMPLER2D(s_normal, 0);
uniform vec4 u_sunDirection;
uniform vec4 u_sunLuminance;

void main()
{
	vec4 encodedNormal = texture2D(s_normal, v_texcoord0);
	if (encodedNormal.w < 0.5)
	{
		discard;
	}
	vec3 normal = normalize(decodeNormalUint(encodedNormal.xyz));
	float diffuse = max(0.0, dot(normal, u_sunDirection.xyz));
	gl_FragColor = vec4(u_sunLuminance.rgb * diffuse, 1.0);
}
