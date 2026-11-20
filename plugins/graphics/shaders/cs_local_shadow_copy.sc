/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"
#include <bgfx_compute.sh>

IMAGE2D_ARRAY_WO(s_pointShadowOutput, r32f, 0);
IMAGE2D_ARRAY_WO(s_spotShadowOutput, r32f, 1);
SAMPLER2D(s_localShadowAtlas, 2);
uniform vec4 u_localShadowCopy; // face size, atlas columns/rows, bottom-left origin.
uniform vec4 u_localShadowTypes[8]; // One packed float per light: unchanged = -1, point/rect = 0, spot = 1.

NUM_THREADS(8, 8, 1)
void main()
{
	ivec3 pixel = ivec3(gl_GlobalInvocationID);
	int size = int(u_localShadowCopy.x);
	if (pixel.x >= size || pixel.y >= size) { return; }
	int light = pixel.z / 6;
	float type = u_localShadowTypes[light / 4][light % 4];
	if (type < 0.0) { return; }
	bool spot = type > 0.5;
	if (spot && pixel.z % 6 != 0) { return; }
	int columns = int(u_localShadowCopy.y);
	ivec2 source = pixel.xy + ivec2(pixel.z % columns, pixel.z / columns) * size;
	if (u_localShadowCopy.w > 0.5) { source.y = int(u_localShadowCopy.z) * size - 1 - source.y; }
	float depth = texelFetch(s_localShadowAtlas, source, 0).r;
	if (spot)
	{
		imageStore(s_spotShadowOutput, ivec3(pixel.xy, light), vec4_splat(depth));
	}
	else
	{
		imageStore(s_pointShadowOutput, pixel, vec4_splat(depth));
	}
}
