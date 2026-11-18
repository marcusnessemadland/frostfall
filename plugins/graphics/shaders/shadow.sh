/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

SAMPLER2D(s_shadowAtlas, 8);
uniform mat4 u_shadowMatrix[4];
uniform vec4 u_shadowSplits;
uniform vec4 u_shadowTexels; // World units per texel, one per cascade.
uniform vec4 u_shadowParameters; // depth bias, normal bias, blend fraction, filter radius.
uniform vec4 u_shadowInfo; // inverse resolution, shadow distance, enabled for this light, unused.
uniform vec4 u_shadowDirection;
uniform mat4 u_sceneView;

float shadowDepth(vec2 _uv, int _cascade)
{
	// Clamp every PCF tap inside its tile to prevent leaking between cascades.
	vec2 uv = clamp(_uv, vec2_splat(u_shadowInfo.x * 0.5),
		vec2_splat(1.0 - u_shadowInfo.x * 0.5));
	uv.x = (uv.x + float(_cascade)) * 0.25;
	return texture2D(s_shadowAtlas, uv).r;
}

int shadowCascade(float _depth)
{
	if (_depth < u_shadowSplits.x) { return 0; }
	if (_depth < u_shadowSplits.y) { return 1; }
	if (_depth < u_shadowSplits.z) { return 2; }
	return 3;
}

float sampleCascade(vec3 _position, vec3 _normal, int _cascade)
{
	float texel = _cascade == 0 ? u_shadowTexels.x : (_cascade == 1 ? u_shadowTexels.y
		: (_cascade == 2 ? u_shadowTexels.z : u_shadowTexels.w));
	float NoL = saturate(dot(_normal, u_shadowDirection.xyz));
	float slope = sqrt(max(0.0, 1.0 - NoL * NoL));
	vec4 shadowPosition = vec4(_position + _normal * (texel * u_shadowParameters.y * slope), 1.0);
	vec4 clip;
	if (_cascade == 0) { clip = mul(u_shadowMatrix[0], shadowPosition); }
	else if (_cascade == 1) { clip = mul(u_shadowMatrix[1], shadowPosition); }
	else if (_cascade == 2) { clip = mul(u_shadowMatrix[2], shadowPosition); }
	else { clip = mul(u_shadowMatrix[3], shadowPosition); }
	vec3 projected = clip.xyz / clip.w;
	vec2 uv = vec2(projected.x, toClipSpaceY(projected.y)) * 0.5 + 0.5;
	projected.z = toDepthTextureZ(projected.z);
	if (any(lessThan(uv, vec2_splat(0.0))) || any(greaterThan(uv, vec2_splat(1.0)))
		|| projected.z < 0.0 || projected.z > 1.0) { return 1.0; }
	float result = 0.0;
	for (int yy = -1; yy <= 1; ++yy)
	{
		for (int xx = -1; xx <= 1; ++xx)
		{
			vec2 offset = vec2(float(xx), float(yy)) * u_shadowInfo.x * u_shadowParameters.w;
			result += step(projected.z - u_shadowParameters.x, shadowDepth(uv + offset, _cascade));
		}
	}
	return result / 9.0;
}

float shadowVisibility(vec3 _position, vec3 _normal)
{
	if (u_shadowInfo.z < 0.5) { return 1.0; }
	float depth = mul(u_sceneView, vec4(_position, 1.0)).z;
	if (depth > u_shadowInfo.y) { return 1.0; }
	int cascade = shadowCascade(depth);
	float result = sampleCascade(_position, _normal, cascade);
	float nearSplit = cascade == 0 ? 0.0 : (cascade == 1 ? u_shadowSplits.x
		: (cascade == 2 ? u_shadowSplits.y : u_shadowSplits.z));
	float farSplit = cascade == 0 ? u_shadowSplits.x : (cascade == 1 ? u_shadowSplits.y
		: (cascade == 2 ? u_shadowSplits.z : u_shadowSplits.w));
	float transition = smoothstep(farSplit - max((farSplit - nearSplit) * u_shadowParameters.z, 0.0001),
		farSplit, depth);
	if (transition > 0.0)
	{
		float next = 1.0;
		if (cascade < 3) { next = sampleCascade(_position, _normal, cascade + 1); }
		result = mix(result, next, transition);
	}
	return result;
}
