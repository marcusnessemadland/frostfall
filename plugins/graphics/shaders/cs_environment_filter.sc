/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "environment.sh"

IMAGE2D_ARRAY_WO(s_output, rgba16f, 0);
SAMPLER2D(s_environmentCapture, 1);
uniform vec4 u_environmentFilter; // output size, roughness (-1 for diffuse), face resolution, bottom-left origin.

vec3 sampleEnvironment(vec3 _direction)
{
	vec3 axis = abs(_direction);
	vec2 uv;
	float face;
	if (axis.x >= axis.y && axis.x >= axis.z)
	{
		face = 0.0;
		uv = vec2(-_direction.z, -_direction.y) / axis.x;
		if (_direction.x < 0.0) { face = 1.0; uv.x = -uv.x; }
	}
	else if (axis.y >= axis.z)
	{
		face = 2.0;
		uv = vec2(_direction.x, _direction.z) / axis.y;
		if (_direction.y < 0.0) { face = 3.0; uv.y = -uv.y; }
	}
	else
	{
		face = 4.0;
		uv = vec2(_direction.x, -_direction.y) / axis.z;
		if (_direction.z < 0.0) { face = 5.0; uv.x = -uv.x; }
	}
	float border = 0.5 / u_environmentFilter.z;
	uv = clamp(uv * 0.5 + 0.5, vec2_splat(border), vec2_splat(1.0 - border));
	uv = (uv + vec2(mod(face, 3.0), floor(face / 3.0))) / vec2(3.0, 2.0);
	if (u_environmentFilter.w > 0.5) { uv.y = 1.0 - uv.y; }
	return texture2DLod(s_environmentCapture, uv, 0.0).rgb;
}

NUM_THREADS(8, 8, 1)
void main()
{
	if (any(greaterThanEqual(vec2(gl_GlobalInvocationID.xy), vec2_splat(u_environmentFilter.x)))) { return; }
	vec2 uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / u_environmentFilter.x;
	vec3 normal = environmentDirection(uv, float(gl_GlobalInvocationID.z));
	float roughness = u_environmentFilter.y;
	if (roughness == 0.0)
	{
		imageStore(s_output, ivec3(gl_GlobalInvocationID), vec4(sampleEnvironment(normal), 1.0));
		return;
	}
	vec3 up = vec3(0.0, 1.0, 0.0);
	if (abs(normal.y) > 0.99) { up = vec3(1.0, 0.0, 0.0); }
	vec3 right = normalize(cross(up, normal));
	up = cross(normal, right);
	vec3 color = vec3_splat(0.0);
	float weight = 0.0;
	for (int ii = 0; ii < 64; ++ii)
	{
		// Deterministic, low discrepancy samples; no temporal history or frame noise.
		float x = (float(ii) + 0.5) / 64.0;
		float phi = 6.28318530718 * fract(float(ii) * 0.61803398875);
		float cosTheta = sqrt(1.0 - x);
		if (roughness >= 0.0)
		{
			float alpha = roughness * roughness;
			cosTheta = sqrt((1.0 - x) / (1.0 + (alpha * alpha - 1.0) * x));
		}
		float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
		vec3 direction = right * (cos(phi) * sinTheta) + up * (sin(phi) * sinTheta) + normal * cosTheta;
		if (roughness >= 0.0) { direction = 2.0 * dot(normal, direction) * direction - normal; }
		float NoL = 1.0;
		if (roughness >= 0.0) { NoL = saturate(dot(normal, direction)); }
		color += sampleEnvironment(direction) * NoL;
		weight += NoL;
	}
	imageStore(s_output, ivec3(gl_GlobalInvocationID), vec4(color / max(weight, 0.0001), 1.0));
}
