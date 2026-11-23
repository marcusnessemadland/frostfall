#ifndef FROST_TONEMAP_SH_HEADER_GUARD
#define FROST_TONEMAP_SH_HEADER_GUARD

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

#endif // FROST_TONEMAP_SH_HEADER_GUARD
