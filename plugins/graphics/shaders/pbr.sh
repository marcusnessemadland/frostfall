/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

vec3 evaluatePbr(vec3 _normal, vec3 _view, vec3 _light, vec3 _baseColor, float _metallic, float _roughness)
{
	float NoL = saturate(dot(_normal, _light));
	float NoV = max(dot(_normal, _view), 0.0001);
	vec3 halfway = _light + _view;
	halfway *= inversesqrt(max(dot(halfway, halfway), 0.000001));
	float NoH = saturate(dot(_normal, halfway));
	float VoH = saturate(dot(_view, halfway));
	float alpha = max(_roughness, 0.045);
	alpha *= alpha;
	float alpha2 = alpha * alpha;
	float denominator = (1.0 - NoH * NoH) + NoH * NoH * alpha2;
	float distribution = alpha2 / (3.14159265359 * denominator * denominator);
	float gv = NoL * sqrt(NoV * NoV * (1.0 - alpha2) + alpha2);
	float gl = NoV * sqrt(NoL * NoL * (1.0 - alpha2) + alpha2);
	float visibility = 0.5 / max(gv + gl, 0.000001);
	vec3 f0 = mix(vec3_splat(0.04), _baseColor, _metallic);
	vec3 fresnel = f0 + (vec3_splat(1.0) - f0) * pow(1.0 - VoH, 5.0);
	vec3 diffuse = (vec3_splat(1.0) - fresnel) * (1.0 - _metallic) * _baseColor / 3.14159265359;
	return (diffuse + distribution * visibility * fresnel) * NoL;
}

// Input is irradiance / pi, including visibility, before the material response.
vec3 evaluateIndirectDiffuse(vec3 _irradiance, vec3 _baseColor, float _metallic)
{
	vec3 f0 = mix(vec3_splat(0.04), _baseColor, _metallic);
	return _irradiance * _baseColor * (1.0 - _metallic) * (vec3_splat(1.0) - f0);
}

// Input is reflected radiance filtered for roughness, including visibility.
vec3 evaluateIndirectSpecular(vec3 _radiance, float _NoV, vec3 _f0, float _roughness)
{
	// Analytic approximation of the integrated split-sum GGX BRDF (Lazarov, SIGGRAPH 2013).
	vec4 r = _roughness * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
	float a004 = min(r.x * r.x, exp2(-9.28 * _NoV)) * r.x + r.y;
	vec2 brdf = vec2(-1.04, 1.04) * a004 + r.zw;
	return _radiance * (_f0 * brdf.x + brdf.y);
}
