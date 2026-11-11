/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_GUID_H_HEADER_GUARD
#define FROST_GUID_H_HEADER_GUARD

#include "error.h"

namespace frost
{
	FROST_ERROR_RESULT(kErrorGuidGenerate, FROST_MAKEFOURCC('g', 'u', 'i', 'd') );

	/// Persistent 128-bit identifier in canonical byte order. Zero is invalid.
	struct Guid
	{
		bool operator==(const Guid& _rhs) const;
		bool operator!=(const Guid& _rhs) const;

		uint8_t data[16] = {};
	};

	constexpr Guid kInvalidGuid = {};

	/// Return true when at least one byte is nonzero.
	bool isValid(const Guid& _guid);

	/// Generate a random version 4 identifier. Returns an invalid GUID on failure.
	Guid generateGuid(Error* _err = ErrorIgnore{});

	/// Hash all 16 bytes for use in lookup tables.
	uint32_t hash(const Guid& _guid);

	/// Write the canonical 36-character representation and a terminating zero.
	/// Returns 36 on success, or 0 when _max is less than 37.
	int32_t toString(char* _out, int32_t _max, const Guid& _guid);

	/// Parse exactly 36 characters, including the four separating hyphens.
	/// Leaves _out unchanged on failure. Accepts uppercase and lowercase hex.
	bool fromString(Guid* _out, const StringView& _str);

} // namespace frost

#include "inline/guid.inl"

#endif // FROST_GUID_H_HEADER_GUARD
