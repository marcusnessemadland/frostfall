$input v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"

SAMPLERCUBE(s_environmentSource, 0);

uniform vec4 u_environmentBake; // face, perceptual roughness, mode (specular/diffuse/BRDF), unused.

vec2 hammersley(int _index)
{
	float index = float(_index);
	float inverse = 0.0;
	float weight = 0.5;
	for (int bit = 0; bit < 8; ++bit)
	{
		inverse += mod(index, 2.0) * weight;
		index = floor(index * 0.5);
		weight *= 0.5;
	}
	return vec2(float(_index) / 256.0, inverse);
}

vec3 sampleGgx(vec2 _xi, float _roughness)
{
	float alpha = max(_roughness * _roughness, 0.0001);
	float phi = 6.28318530718 * _xi.x;
	float cosine = sqrt((1.0 - _xi.y) / (1.0 + (alpha * alpha - 1.0) * _xi.y));
	float sine = sqrt(max(0.0, 1.0 - cosine * cosine));
	return vec3(cos(phi) * sine, sin(phi) * sine, cosine);
}

vec3 cubeDirection(vec2 _uv, float _face)
{
	vec2 p = _uv * 2.0 - 1.0;
	if (_face < 0.5) { return normalize(vec3( 1.0, -p.y, -p.x)); }
	if (_face < 1.5) { return normalize(vec3(-1.0, -p.y,  p.x)); }
	if (_face < 2.5) { return normalize(vec3( p.x,  1.0,  p.y)); }
	if (_face < 3.5) { return normalize(vec3( p.x, -1.0, -p.y)); }
	if (_face < 4.5) { return normalize(vec3( p.x, -p.y,  1.0)); }
	return normalize(vec3(-p.x, -p.y, -1.0));
}

void main()
{
	float roughness = u_environmentBake.y;
	if (u_environmentBake.z > 1.5)
	{
		float NoV = max(v_texcoord0.x, 0.0001);
		roughness = max(v_texcoord0.y, 0.045);
		vec3 view = vec3(sqrt(1.0 - NoV * NoV), 0.0, NoV);
		vec2 integrated = vec2_splat(0.0);
		float alpha2 = pow(roughness, 4.0);
		for (int ii = 0; ii < 256; ++ii)
		{
			vec3 halfway = sampleGgx(hammersley(ii), roughness);
			float VoH = saturate(dot(view, halfway));
			vec3 light = 2.0 * VoH * halfway - view;
			float NoL = saturate(light.z);
			float gv = NoL * sqrt(NoV * NoV * (1.0 - alpha2) + alpha2);
			float gl = NoV * sqrt(NoL * NoL * (1.0 - alpha2) + alpha2);
			float weight = 2.0 * NoL * VoH / max((gv + gl) * halfway.z, 0.000001);
			float fresnel = pow(1.0 - VoH, 5.0);
			integrated += vec2(1.0 - fresnel, fresnel) * weight;
		}
		gl_FragColor = vec4(integrated / 256.0, 0.0, 1.0);
		return;
	}

	vec3 normal = cubeDirection(v_texcoord0, u_environmentBake.x);
	if (u_environmentBake.z < 0.5 && roughness < 0.001)
	{
		gl_FragColor = vec4(textureCubeLod(s_environmentSource, normal, 0.0).rgb, 1.0);
		return;
	}
	vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, normal));
	vec3 bitangent = cross(normal, tangent);
	vec3 sum = vec3_splat(0.0);
	float total = 0.0;
	for (int ii = 0; ii < 256; ++ii)
	{
		vec2 xi = hammersley(ii);
		vec3 local;
		if (u_environmentBake.z > 0.5)
		{
			float phi = 6.28318530718 * xi.x;
			local = vec3(cos(phi) * sqrt(xi.y), sin(phi) * sqrt(xi.y), sqrt(1.0 - xi.y));
		}
		else
		{
			vec3 halfway = sampleGgx(xi, roughness);
			local = 2.0 * halfway.z * halfway - vec3(0.0, 0.0, 1.0);
		}
		float weight = u_environmentBake.z > 0.5 ? 1.0 : saturate(local.z);
		vec3 direction = tangent * local.x + bitangent * local.y + normal * local.z;
		sum += textureCubeLod(s_environmentSource, direction, 0.0).rgb * weight;
		total += weight;
	}
	// Diffuse stores irradiance / PI, ready to multiply by diffuse reflectance.
	gl_FragColor = vec4(sum / max(total, 0.0001), 1.0);
}
