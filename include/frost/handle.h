/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_HANDLE_H_HEADER_GUARD
#define FROST_HANDLE_H_HEADER_GUARD

#include "frost.h"

/// Declare a distinct handle type. Copies do not change resource ownership.
#define FROST_HANDLE(_name)                                                               \
	struct _name { uint16_t idx = frost::kInvalidHandle; };                                 \
	inline constexpr bool isValid(_name _handle) { return frost::kInvalidHandle != _handle.idx; }

/// Initializer for any handle declared with FROST_HANDLE.
#define FROST_INVALID_HANDLE { frost::kInvalidHandle }

namespace frost
{
	constexpr uint16_t kInvalidHandle = UINT16_MAX;

} // namespace frost

#endif // FROST_HANDLE_H_HEADER_GUARD
