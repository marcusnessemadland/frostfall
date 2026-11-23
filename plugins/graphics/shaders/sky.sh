/*
 * Perez sky adapted from bgfx/examples/36-sky.
 * Copyright 2017 Stanislav Pidhorskyi. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

uniform vec4 u_sunDirection;
uniform vec4 u_sunTint; // Source tint RGB and intensity; sky luminance already includes intensity.
uniform vec4 u_skyLuminanceXYZ;
uniform vec4 u_perezCoeff[5];
uniform vec4 u_parameters; // sun size, sun bloom, debug view, unused.
uniform vec4 u_moonDirection;
uniform vec4 u_moonUp;
uniform vec4 u_moonColor; // full-phase disk RGB, unused.
uniform vec4 u_moonSkyLuminanceXYZ;
uniform vec4 u_moonParameters; // sine angular radius, disk brightness, albedo normalization, unused.

vec3 perez(float _cosTheta, float _cosGamma)
{
	float gamma = acos(clamp(_cosGamma, -0.9999, 0.9999));
	return (vec3_splat(1.0) + u_perezCoeff[0].xyz * exp(u_perezCoeff[1].xyz / max(_cosTheta, 0.001)))
		* (vec3_splat(1.0) + u_perezCoeff[2].xyz * exp(u_perezCoeff[3].xyz * gamma)
		+ u_perezCoeff[4].xyz * _cosGamma * _cosGamma);
}

vec3 evaluateSkySource(vec3 _direction, vec3 _source, vec3 _luminanceXYZ, float _chromaticity)
{
	// The planet blocks the sky below the horizon. Mirroring it into the lower
	// hemisphere injects unoccluded light beneath terrain and through open bounds.
	if (_luminanceXYZ.y <= 0.0 || _direction.y < 0.0) { return vec3_splat(0.0); }
	_source = normalize(vec3(_source.x, max(_source.y, 0.001), _source.z));
	float sum = max(_luminanceXYZ.x + _luminanceXYZ.y + _luminanceXYZ.z, 0.000001);
	vec3 xyY = vec3(_luminanceXYZ.xy / sum, _luminanceXYZ.y);
	vec3 value = xyY * perez(max(_direction.y, 0.001), dot(_direction, _source))
		/ max(perez(1.0, _source.y), vec3_splat(0.0001));
	// Keep the authored night color; the daytime fit supplies its angular luminance.
	value.xy = mix(xyY.xy, value.xy, _chromaticity);
	float y = max(value.y, 0.000001);
	return max(convertXYZ2RGB(vec3(value.x * value.z / y, value.z,
		(1.0 - value.x - value.y) * value.z / y)), vec3_splat(0.0));
}

vec3 evaluateMoonSky(vec3 _direction)
{
	return evaluateSkySource(_direction, u_moonDirection.xyz, u_moonSkyLuminanceXYZ.xyz, 0.0);
}

vec3 evaluateSky(vec3 _direction)
{
	return evaluateSkySource(_direction, u_sunDirection.xyz, u_skyLuminanceXYZ.xyz, 1.0)
		* u_sunTint.rgb + evaluateMoonSky(_direction);
}
