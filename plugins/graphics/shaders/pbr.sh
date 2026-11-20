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
