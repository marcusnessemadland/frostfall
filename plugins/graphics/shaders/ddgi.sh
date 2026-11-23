/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#ifndef FROST_DDGI_SH_HEADER_GUARD
#define FROST_DDGI_SH_HEADER_GUARD

#include "reflection.sh"

// A single world-space volume. The octahedral tiles include a one-texel border.
uniform vec4 u_ddgiGrid[3]; // Origin / maximum distance, spacing / rays per probe, dimensions / unused.
uniform vec4 u_ddgiParameters; // Normal bias, view bias, distance sharpness, valid history.
uniform vec4 u_ddgiTrace; // Ray buffer width/height, first probe row, trace glossy.
uniform mat4 u_ddgiRotation;

uvec3 ddgiCell(uint _probe)
{
	uvec3 size = uvec3(u_ddgiGrid[2].xyz);
	return uvec3(_probe % size.x, _probe / (size.x * size.z), (_probe / size.x) % size.z);
}

ivec2 ddgiTile(uvec3 _cell)
{
	return ivec2(_cell.x + _cell.y * uint(u_ddgiGrid[2].x), _cell.z);
}

vec3 ddgiPosition(uint _probe)
{
	return u_ddgiGrid[0].xyz + vec3(ddgiCell(_probe)) * u_ddgiGrid[1].xyz;
}

vec3 ddgiDirection(uint _ray)
{
	float z = 1.0 - 2.0 * (float(_ray) + 0.5) / u_ddgiGrid[1].w;
	float radius = sqrt(max(0.0, 1.0 - z * z));
	float phi = float(_ray) * 2.39996322973;
	return mul(u_ddgiRotation, vec4(cos(phi) * radius, sin(phi) * radius, z, 0.0)).xyz;
}

ivec2 ddgiRayPixel(uint _probe, uint _ray)
{
	uint index = _probe * uint(u_ddgiGrid[1].w) + _ray;
	return ivec2(index % uint(u_ddgiTrace.x), uint(u_ddgiTrace.z) + index / uint(u_ddgiTrace.x));
}

#if defined(FROST_DDGI_SAMPLE)
#ifndef FROST_DDGI_IRRADIANCE_SLOT
#define FROST_DDGI_IRRADIANCE_SLOT 13
#define FROST_DDGI_DISTANCE_SLOT 14
#endif
SAMPLER2D(s_ddgiIrradiance, FROST_DDGI_IRRADIANCE_SLOT);
SAMPLER2D(s_ddgiDistance, FROST_DDGI_DISTANCE_SLOT);

vec2 ddgiUv(ivec2 _tile, vec3 _direction, float _size)
{
	vec2 oct = reflectionEncodeNormal(_direction) * 0.5 + 0.5;
	vec2 dimensions = vec2(u_ddgiGrid[2].x * u_ddgiGrid[2].y, u_ddgiGrid[2].z);
	return (vec2(_tile) * _size + 1.0 + oct * (_size - 2.0)) / (dimensions * _size);
}

// Returns incident irradiance / pi, before the material response or occlusion map.
vec3 sampleDiffuseGi(vec3 _position, vec3 _normal, vec3 _geometric, vec3 _view)
{
	if (u_ddgiParameters.w < 0.5) { return vec3_splat(0.0); }
	vec3 biased = _position + _geometric * u_ddgiParameters.x + _view * u_ddgiParameters.y;
	vec3 grid = (biased - u_ddgiGrid[0].xyz) / u_ddgiGrid[1].xyz;
	if (any(lessThan(grid, vec3_splat(0.0))) || any(greaterThan(grid, u_ddgiGrid[2].xyz - 1.0)))
	{
		return vec3_splat(0.0);
	}
	vec3 base = min(floor(grid), u_ddgiGrid[2].xyz - 2.0);
	vec3 fraction = grid - base;
	vec3 sum = vec3_splat(0.0);
	float weight = 0.0, support = 0.0;
	for (int zz = 0; zz < 2; ++zz)
	{
		for (int yy = 0; yy < 2; ++yy)
		{
			for (int xx = 0; xx < 2; ++xx)
			{
				vec3 offset = vec3(float(xx), float(yy), float(zz));
				vec3 cell = base + offset;
				ivec2 tile = ddgiTile(uvec3(cell));
				vec3 center = u_ddgiGrid[0].xyz + cell * u_ddgiGrid[1].xyz;
				vec3 delta = biased - center;
				float distance = max(length(delta), 0.00001);
				vec3 toward = center - _position;
				toward *= inversesqrt(max(dot(toward, toward), 0.00000001));
				float facing = dot(toward, _normal) * 0.5 + 0.5;
				vec3 spatial = mix(1.0 - fraction, fraction, offset);
				float adjacency = spatial.x * spatial.y * spatial.z + 0.001;
				float w = (facing * facing + 0.2) * adjacency;
				float available = w < 0.2 ? w * w * w / 0.04 : w;
				support += available;
				vec2 moments = texture2DLod(s_ddgiDistance, ddgiUv(tile, delta / distance, 16.0), 0.0).rg;
				float variance = max(moments.y - moments.x * moments.x, 0.00000001);
				float difference = max(0.0, distance / u_ddgiGrid[0].w - moments.x);
				float visibility = variance / (variance + difference * difference);
				w *= visibility * visibility * visibility;
				// Crush weak visibility before normalization, retaining its confidence below.
				if (w < 0.2) { w *= w * w / 0.04; }
				sum += texture2DLod(s_ddgiIrradiance, ddgiUv(tile, _normal, 6.0), 0.0).rgb * w;
				weight += w;
			}
		}
	}
	vec3 irradiance = sum / max(weight, 0.00000001);
	float confidence = weight / max(support, 0.00000001);
	return irradiance * irradiance * smoothstep(0.01, 0.1, confidence);
}
#endif
#endif // FROST_DDGI_SH_HEADER_GUARD
