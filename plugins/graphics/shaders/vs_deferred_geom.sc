$input a_position, a_normal, a_tangent, a_texcoord0
$output v_normal, v_tangent, v_bitangent, v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/frostfall/blob/main/LICENSE
 */

#include "../common/common.sh"

#define FROST_GEOMETRY_MODEL u_model[0]
#include "geometry.sh"
