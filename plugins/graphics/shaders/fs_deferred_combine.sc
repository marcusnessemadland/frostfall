$input v_texcoord0

/*
 * Adapted from bgfx/examples/21-deferred and 36-sky.
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * Copyright 2017 Stanislav Pidhorskyi. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"
SAMPLER2D(s_scene, 0);
uniform vec4 u_outputParameters; // x: exposure multiplier, y: dither, z: tone mapping.

#include "tonemap.sh"

void main()
{
	vec3 scene = max(texture2D(s_scene, v_texcoord0).rgb, vec3_splat(0.0));
	vec3 color = scene * u_outputParameters.x;
	if (u_outputParameters.z > 0.5) { color = toneMapUncharted2(color); }
	// Dither in display space, then let the sRGB backbuffer perform final encoding.
	float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	color = toGammaAccurate(color) + vec3_splat((noise - 0.5) * u_outputParameters.y / 255.0);
	gl_FragColor = vec4(toLinearAccurate(saturate(color)), 1.0);
}
