$input v_viewDir

/*
 * Sky adapted from bgfx/examples/36-sky.
 * Copyright 2017 Stanislav Pidhorskyi. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "../common/common.sh"
#include "atmosphere.sh"

void main()
{
	gl_FragColor = vec4(evaluateAtmosphere(v_viewDir), 1.0);
}
