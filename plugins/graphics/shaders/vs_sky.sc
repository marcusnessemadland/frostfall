$input a_position
$output v_viewDir

/*
* Copyright 2017 Stanislav Pidhorskyi. All rights reserved.
* License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
*/


#include "../common/common.sh"

void main()
{
	vec4 rayStart = mul(u_invViewProj, vec4(a_position.xy, toClipSpaceDepth(0.0), 1.0));
	vec4 rayEnd = mul(u_invViewProj, vec4(a_position.xy, 1.0, 1.0));
	v_viewDir = rayEnd.xyz / rayEnd.w - rayStart.xyz / rayStart.w;
	gl_Position = vec4(a_position.xy, 1.0, 1.0);
}
