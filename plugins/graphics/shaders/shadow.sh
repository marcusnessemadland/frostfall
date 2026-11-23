/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_SHADOW_SAMPLER
#define FROST_SHADOW_SAMPLER 5
#endif
SAMPLER2DARRAY(s_shadowAtlas, FROST_SHADOW_SAMPLER);
uniform mat4 u_shadowMatrix[8];
uniform vec4 u_shadowSplits[2];
uniform vec4 u_shadowTexels[2]; // World units per texel, one per cascade.
uniform vec4 u_shadowParameters; // depth bias, normal bias, blend fraction, filter radius.
uniform vec4 u_shadowInfo[2]; // inverse resolution, shadow distance, enabled for this light, unused.
uniform vec4 u_shadowDirection[2];
uniform mat4 u_shadowView[2];
uniform vec4 u_shadowSelection;

float shadowDepth(vec2 _uv, int _cascade, int _light)
{
	// Clamp every PCF tap inside its tile to prevent leaking between cascades.
	vec2 uv = clamp(_uv, vec2_splat(u_shadowInfo[_light].x * 0.5),
		vec2_splat(1.0 - u_shadowInfo[_light].x * 0.5));
	uv.x = (uv.x + float(_cascade)) * 0.25;
	return texture2DArrayLod(s_shadowAtlas, vec3(uv, float(_light)), 0.0).r;
}

int shadowCascade(float _depth, int _light)
{
	if (_depth < u_shadowSplits[_light].x) { return 0; }
	if (_depth < u_shadowSplits[_light].y) { return 1; }
	if (_depth < u_shadowSplits[_light].z) { return 2; }
	return 3;
}

vec3 shadowProjected(vec3 _position, vec3 _normal, int _cascade, int _light)
{
	float texel = _cascade == 0 ? u_shadowTexels[_light].x : (_cascade == 1 ? u_shadowTexels[_light].y
		: (_cascade == 2 ? u_shadowTexels[_light].z : u_shadowTexels[_light].w));
	float NoL = saturate(dot(_normal, u_shadowDirection[_light].xyz));
	float slope = sqrt(max(0.0, 1.0 - NoL * NoL));
	vec4 shadowPosition = vec4(_position + _normal * (texel * u_shadowParameters.y * slope), 1.0);
	vec4 clip = mul(u_shadowMatrix[_light * 4 + _cascade], shadowPosition);
	vec3 projected = clip.xyz / clip.w;
	vec2 uv = vec2(projected.x, toClipSpaceY(projected.y)) * 0.5 + 0.5;
	projected.z = toDepthTextureZ(projected.z);
	return vec3(uv, projected.z);
}

float sampleCascade(vec3 _position, vec3 _normal, int _cascade, int _light)
{
	vec3 projected = shadowProjected(_position, _normal, _cascade, _light);
	vec2 uv = projected.xy;
	if (any(lessThan(uv, vec2_splat(0.0))) || any(greaterThan(uv, vec2_splat(1.0)))
		|| projected.z < 0.0 || projected.z > 1.0) { return 1.0; }
	float result = 0.0;
	for (int yy = -1; yy <= 1; ++yy)
	{
		for (int xx = -1; xx <= 1; ++xx)
		{
			vec2 offset = vec2(float(xx), float(yy)) * u_shadowInfo[_light].x * u_shadowParameters.w;
			result += step(projected.z - u_shadowParameters.x, shadowDepth(uv + offset, _cascade, _light));
		}
	}
	return result / 9.0;
}

float shadowVisibility(vec3 _position, vec3 _normal, int _light)
{
	if (_light < 0 || _light >= 2) { return 1.0; }
	if (u_shadowInfo[_light].z < 0.5) { return 1.0; }
	float depth = mul(u_shadowView[_light], vec4(_position, 1.0)).z;
	if (depth > u_shadowInfo[_light].y) { return 1.0; }
	int cascade = shadowCascade(depth, _light);
	float result = sampleCascade(_position, _normal, cascade, _light);
	float nearSplit = cascade == 0 ? 0.0 : (cascade == 1 ? u_shadowSplits[_light].x
		: (cascade == 2 ? u_shadowSplits[_light].y : u_shadowSplits[_light].z));
	float farSplit = cascade == 0 ? u_shadowSplits[_light].x : (cascade == 1 ? u_shadowSplits[_light].y
		: (cascade == 2 ? u_shadowSplits[_light].z : u_shadowSplits[_light].w));
	float transition = smoothstep(farSplit - max((farSplit - nearSplit) * u_shadowParameters.z, 0.0001),
		farSplit, depth);
	if (transition > 0.0)
	{
		float next = 1.0;
		if (cascade < 3) { next = sampleCascade(_position, _normal, cascade + 1, _light); }
		result = mix(result, next, transition);
	}
	return result;
}

bool shadowCoverage(vec3 _position, vec3 _normal, int _light)
{
	if (_light < 0 || _light >= 2) { return false; }
	if (u_shadowInfo[_light].z < 0.5) { return false; }
	float depth = mul(u_shadowView[_light], vec4(_position, 1.0)).z;
	if (depth < 0.0 || depth > u_shadowInfo[_light].y) { return false; }
	int cascade = shadowCascade(depth, _light);
	float nearSplit = cascade == 0 ? 0.0 : (cascade == 1 ? u_shadowSplits[_light].x
		: (cascade == 2 ? u_shadowSplits[_light].y : u_shadowSplits[_light].z));
	float farSplit = cascade == 0 ? u_shadowSplits[_light].x : (cascade == 1 ? u_shadowSplits[_light].y
		: (cascade == 2 ? u_shadowSplits[_light].z : u_shadowSplits[_light].w));
	bool blend = depth > farSplit - max((farSplit - nearSplit) * u_shadowParameters.z, 0.0001);
	if (blend && cascade == 3) { return false; }
	float border = u_shadowInfo[_light].x * (u_shadowParameters.w + 0.5);
	for (int ii = 0; ii < 2; ++ii)
	{
		if (ii == 1 && !blend) { break; }
		vec3 projected = shadowProjected(_position, _normal, cascade + ii, _light);
		if (any(lessThan(projected.xy, vec2_splat(border))) || any(greaterThan(projected.xy, vec2_splat(1.0 - border)))
			|| projected.z < 0.0 || projected.z > 1.0) { return false; }
	}
	return true;
}
