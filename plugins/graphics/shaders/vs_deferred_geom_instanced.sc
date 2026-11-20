$input a_position, a_normal, a_tangent, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_normal, v_tangent, v_bitangent, v_texcoord0

/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: https://github.com/marcusnessemadland/frostfall/blob/main/LICENSE
 */

#include "../common/common.sh"

#define FROST_GEOMETRY_MODEL mtxFromCols(i_data0, i_data1, i_data2, i_data3)
#include "geometry.sh"
