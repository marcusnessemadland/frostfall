$input v_normal

/*
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"

void main()
{
	// Default surface for mesh instances without material assets.
	gl_FragData[0] = vec4(1.0, 0.0, 1.0, 1.0);
	gl_FragData[1] = vec4(encodeNormalUint(normalize(v_normal) ), 1.0);
}
