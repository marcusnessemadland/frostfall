$input v_texcoord0, v_color0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"

SAMPLER2D(s_ui, 0);

void main()
{
	// UI colors are authored in sRGB. The sRGB backbuffer encodes after blending.
	gl_FragColor = vec4(toLinearAccurate(v_color0.rgb), v_color0.a * texture2D(s_ui, v_texcoord0).r);
}
