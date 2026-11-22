/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"
#include <bgfx_compute.sh>
#include "environment.sh"

IMAGE2D_ARRAY_WO(s_reflectionProbeOutput, rgba16f, 0);
SAMPLERCUBEARRAY(s_reflectionCapture, 1);
uniform vec4 u_reflectionFilter; // Output resolution, perceptual roughness, unused, unused.

NUM_THREADS(8, 8, 1)
void main()
{
	ivec3 pixel = ivec3(gl_GlobalInvocationID);
	if (any(greaterThanEqual(vec2(pixel.xy), vec2_splat(u_reflectionFilter.x)))) { return; }
	vec2 uv = (vec2(pixel.xy) + 0.5) / u_reflectionFilter.x;
	vec3 normal = environmentDirection(uv, float(pixel.z % 6));
	float probe = floor(float(pixel.z) / 6.0);
	float roughness = u_reflectionFilter.y;
	if (roughness == 0.0)
	{
		imageStore(s_reflectionProbeOutput, pixel, textureCubeArrayLod(s_reflectionCapture, vec4(normal, probe), 0.0));
		return;
	}
	vec3 up = abs(normal.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, normal));
	up = cross(normal, right);
	vec3 sum = vec3_splat(0.0);
	float weight = 0.0;
	for (int ii = 0; ii < 64; ++ii)
	{
		float x = (float(ii) + 0.5) / 64.0;
		float phi = 6.28318530718 * fract(float(ii) * 0.61803398875);
		float alpha = roughness * roughness;
		float cosTheta = sqrt((1.0 - x) / (1.0 + (alpha * alpha - 1.0) * x));
		float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
		vec3 halfway = right * (cos(phi) * sinTheta) + up * (sin(phi) * sinTheta) + normal * cosTheta;
		vec3 direction = 2.0 * dot(normal, halfway) * halfway - normal;
		float NoL = saturate(dot(normal, direction));
		sum += textureCubeArrayLod(s_reflectionCapture, vec4(direction, probe), 0.0).rgb * NoL;
		weight += NoL;
	}
	imageStore(s_reflectionProbeOutput, pixel, vec4(sum / max(weight, 0.0001), 1.0));
}
