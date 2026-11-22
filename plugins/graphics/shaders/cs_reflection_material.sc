/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */
#include "../common/common.sh"
#include <bgfx_compute.sh>
IMAGE2D_ARRAY_WO(s_baseOutput, rgba8, 0);
IMAGE2D_ARRAY_WO(s_parameterOutput, rgba8, 1);
IMAGE2D_ARRAY_WO(s_emissiveOutput, rgba16f, 2);
SAMPLER2D(s_baseColorMap, 8);
SAMPLER2D(s_metallicRoughnessMap, 9);
SAMPLER2D(s_emissiveMap, 10);
uniform vec4 u_materialBaseColor;
uniform vec4 u_materialParameters;
uniform vec4 u_materialEmissive;
uniform vec4 u_rayBake; // Layer, size, unused, unused.
uniform vec4 u_rayMaterialLod;
NUM_THREADS(8, 8, 1)
void main()
{
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(vec2(pixel), vec2_splat(u_rayBake.y)))) { return; }
	vec2 uv = (vec2(pixel) + 0.5) / u_rayBake.y;
	vec4 base = texture2DLod(s_baseColorMap, uv, u_rayMaterialLod.x) * u_materialBaseColor;
	vec4 parameters = texture2DLod(s_metallicRoughnessMap, uv, u_rayMaterialLod.y);
	vec3 emissive = texture2DLod(s_emissiveMap, uv, u_rayMaterialLod.z).rgb * u_materialEmissive.rgb;
	ivec3 address = ivec3(pixel, int(u_rayBake.x));
	imageStore(s_baseOutput, address, base);
	imageStore(s_parameterOutput, address, vec4(parameters.b * u_materialParameters.x, parameters.g * u_materialParameters.y, 0.0, 0.0));
	imageStore(s_emissiveOutput, address, vec4(emissive, 0.0));
}
