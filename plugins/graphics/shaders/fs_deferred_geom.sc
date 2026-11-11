$input v_wpos, v_view, v_normal, v_tangent, v_bitangent, v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/frostfall/blob/main/LICENSE
 */

#include "../common/common.sh"

void main()
{
	vec4 albedoMapPlaceholder = vec4(0.5, 0.5, 0.5, 1.0);
	vec3 normalMapPlaceholder = vec3(0.5, 0.5, 1.0);

	vec3 normal;
	normal.xy = normalMapPlaceholder.xy * 2.0 - 1.0;
	normal.z  = sqrt(1.0 - dot(normal.xy, normal.xy) );

	mat3 tbn = mtxFromCols(
				normalize(v_tangent),
				normalize(v_bitangent),
				normalize(v_normal)
				);

	normal = normalize(mul(tbn, normal) );

	vec3 wnormal = normalize(mul(u_invView, vec4(normal, 0.0) ).xyz);

	gl_FragData[0] = albedoMapPlaceholder;
	gl_FragData[1] = vec4(encodeNormalUint(wnormal), 1.0);
}
