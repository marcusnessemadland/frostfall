/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_H_HEADER_GUARD
#	error "Must be included from frost/os.h!"
#endif // FROST_H_HEADER_GUARD

namespace frost
{
	template<typename ProtoT>
	inline ProtoT dlsym(void* _handle, const StringView& _symbol)
	{
		return reinterpret_cast<ProtoT>(dlsym(_handle, _symbol) );
	}

} // namespace frost
