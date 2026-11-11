$input v_texcoord0

/*
 * Adapted from bgfx/examples/21-deferred and 36-sky.
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * Copyright 2017 Stanislav Pidhorskyi. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"

SAMPLER2D(s_albedo, 0);
SAMPLER2D(s_light, 1);
uniform vec4 u_outputParameters; // x: prevent banding, y: time.

// Interleaved gradient noise from example 36, applied after display encoding.
float interleavedGradientNoise(vec2 screenPos, float temporalFactor)
{
	vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	vec2 coord = screenPos + fract(temporalFactor) * vec2_splat(13.0);
	return fract(magic.z * fract(dot(coord, magic.xy)));
}

void main()
{
	vec3 albedo = toLinear(texture2D(s_albedo, v_texcoord0).rgb);
	vec3 light = texture2D(s_light, v_texcoord0).rgb;
	vec3 color = toGamma(max(albedo * light, vec3_splat(0.0)));
	float dither = (interleavedGradientNoise(gl_FragCoord.xy, u_outputParameters.y) - 0.5) / 255.0;
	gl_FragColor = vec4(color + vec3_splat(dither * u_outputParameters.x), 1.0);
}
