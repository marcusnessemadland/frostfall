/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_ENDIAN_H_HEADER_GUARD
#define FROST_ENDIAN_H_HEADER_GUARD

#include "frost.h"

namespace frost
{
	///
	int16_t endianSwap(int16_t _in);

	///
	uint16_t endianSwap(uint16_t _in);

	///
	int32_t endianSwap(int32_t _in);

	///
	uint32_t endianSwap(uint32_t _in);

	///
	int64_t endianSwap(int64_t _in);

	///
	uint64_t endianSwap(uint64_t _in);

	/// Input argument is encoded as little endian, convert it if necessary
	/// depending on host CPU endianness.
	template <typename Ty>
	Ty toLittleEndian(const Ty _in);

	/// Input argument is encoded as big endian, convert it if necessary
	/// depending on host CPU endianness.
	template <typename Ty>
	Ty toBigEndian(const Ty _in);

	/// If _littleEndian is true, converts input argument to from little endian
	/// to host CPU endiness.
	template <typename Ty>
	Ty toHostEndian(const Ty _in, bool _fromLittleEndian);

} // namespace frost

#include "inline/endian.inl"

#endif // FROST_ENDIAN_H_HEADER_GUARD
