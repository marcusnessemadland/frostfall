/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_GUID_H_HEADER_GUARD
#	error "Must be included from frost/guid.h!"
#endif // FROST_GUID_H_HEADER_GUARD

namespace frost
{
	inline bool Guid::operator==(const Guid& _rhs) const
	{
		return 0 == memCmp(data, _rhs.data, sizeof(data) );
	}

	inline bool Guid::operator!=(const Guid& _rhs) const
	{
		return !(*this == _rhs);
	}

	inline bool isValid(const Guid& _guid)
	{
		return _guid != kInvalidGuid;
	}

} // namespace frost
