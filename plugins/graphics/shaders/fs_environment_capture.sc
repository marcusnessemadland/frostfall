$input v_viewDir

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include "../common/common.sh"
#include "sky.sh"

void main()
{
	// Direct sunlight is evaluated separately by the directional light pass.
	gl_FragColor = vec4(evaluateSky(normalize(v_viewDir)), 1.0);
}
