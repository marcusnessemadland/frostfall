$input v_texcoord0, v_color0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_ui, 0);

void main()
{
	gl_FragColor = vec4(v_color0.rgb, v_color0.a * texture2D(s_ui, v_texcoord0).r);
}
