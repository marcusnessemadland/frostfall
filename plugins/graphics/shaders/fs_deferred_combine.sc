$input v_texcoord0

/*
 * Adapted from bgfx/examples/21-deferred and 36-sky.
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * Copyright 2017 Stanislav Pidhorskyi. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"
SAMPLER2D(s_scene, 0);
uniform vec4 u_outputParameters; // x: exposure multiplier, y: dither.

// John Hable's Uncharted 2 filmic curve, evaluated in linear space.
// https://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 uncharted2Curve(vec3 _color)
{
	const float shoulder = 0.15;
	const float linearStrength = 0.50;
	const float linearAngle = 0.10;
	const float toeStrength = 0.20;
	const float toeNumerator = 0.02;
	const float toeDenominator = 0.30;
	vec3 numerator = _color * (shoulder * _color + linearAngle * linearStrength)
		+ toeStrength * toeNumerator;
	vec3 denominator = _color * (shoulder * _color + linearStrength)
		+ toeStrength * toeDenominator;
	return numerator / denominator - vec3_splat(toeNumerator / toeDenominator);
}

vec3 toneMapUncharted2(vec3 _color)
{
	const float exposureBias = 2.0;
	const float whitePoint = 11.2;
	return uncharted2Curve(_color * exposureBias) / uncharted2Curve(vec3_splat(whitePoint));
}

void main()
{
	vec3 scene = max(texture2D(s_scene, v_texcoord0).rgb, vec3_splat(0.0));
	vec3 color = toneMapUncharted2(scene * u_outputParameters.x);
	// Dither in display space, then let the sRGB backbuffer perform final encoding.
	float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	color = toGammaAccurate(color) + vec3_splat((noise - 0.5) * u_outputParameters.y / 255.0);
	gl_FragColor = vec4(toLinearAccurate(saturate(color)), 1.0);
}
