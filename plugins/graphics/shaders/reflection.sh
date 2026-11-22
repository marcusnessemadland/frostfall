/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

vec2 reflectionEncodeNormal(vec3 _normal)
{
	_normal /= abs(_normal.x) + abs(_normal.y) + abs(_normal.z);
	vec2 encoded = _normal.xy;
	if (_normal.z < 0.0)
	{
		encoded = (1.0 - abs(encoded.yx)) * vec2(encoded.x >= 0.0 ? 1.0 : -1.0, encoded.y >= 0.0 ? 1.0 : -1.0);
	}
	return encoded;
}

vec3 reflectionDecodeNormal(vec2 _encoded)
{
	vec3 normal = vec3(_encoded, 1.0 - abs(_encoded.x) - abs(_encoded.y));
	float fold = max(-normal.z, 0.0);
	normal.xy += vec2(normal.x >= 0.0 ? -fold : fold, normal.y >= 0.0 ? -fold : fold);
	return normalize(normal);
}

vec3 reflectionOrigin(vec3 _position, vec3 _geometricNormal, float _bias)
{
	float magnitude = max(abs(_position.x), max(abs(_position.y), abs(_position.z)));
	return _position + _geometricNormal * max(_bias, magnitude * 0.0000002);
}

float reflectionTolerance(float _depth, float _projectionScale, float _orthographic)
{
	return max(0.005, (_orthographic > 0.5 ? 1.0 : abs(_depth)) * 1.5 / max(1.0, _projectionScale));
}

// Guides carry a camera-relative surface plane; normal length carries valid coverage.
float reflectionWeight(vec4 _guide, vec3 _position, vec3 _normal, float _tolerance)
{
	float coverage = length(_guide.xyz);
	if (coverage < 0.0001) { return 0.0; }
	vec3 normal = _guide.xyz / coverage;
	float alignment = dot(normal, _normal);
	float plane = 1.0 - smoothstep(_tolerance * 0.5, _tolerance, abs(dot(normal, _position) - _guide.w));
	return min(1.0, coverage) * smoothstep(0.85, 0.95, alignment) * plane * pow(saturate(alignment), 8.0);
}

// Broad specular lobes use fixed world-space captures instead of a single mirror ray.
float reflectionRayWeight(float _roughness)
{
	return 1.0 - smoothstep(0.2, 0.5, _roughness);
}
