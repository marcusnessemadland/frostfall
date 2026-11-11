/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_MEMORY_H_HEADER_GUARD
#define FROST_MEMORY_H_HEADER_GUARD

#include "allocator.h"

namespace frost
{
	/// Contiguous bytes. Ownership belongs to the accompanying memory API.
	struct Memory
	{
		void*    data;
		uint32_t size;
	};

	/// Called once when referenced bytes are no longer needed.
	/// The consumer determines which thread invokes the callback.
	typedef void (*MemoryReleaseFn)(void* _data, void* _userData);

	/// Allocate a memory block and 16-byte-aligned storage through _allocator.
	/// The allocator must outlive the block. Returns NULL on allocation failure.
	const Memory* allocMemory(
		  AllocatorI* _allocator
		, uint32_t _size
		, const Location& _location = Location::current()
		);

	/// Allocate a block and copy _size bytes. Returns NULL on allocation failure.
	const Memory* copyMemory(
		  AllocatorI* _allocator
		, const void* _data
		, uint32_t _size
		, const Location& _location = Location::current()
		);

	/// Allocate a wrapper around external bytes without copying them.
	/// Keep the bytes valid and unchanged until release. Without a callback, the
	/// caller owns the external storage. Failure leaves that storage with the caller.
	const Memory* makeRef(
		  AllocatorI* _allocator
		, const void* _data
		, uint32_t _size
		, MemoryReleaseFn _releaseFn = NULL
		, void* _userData = NULL
		, const Location& _location = Location::current()
		);

	/// Release a block created by allocMemory, copyMemory, or makeRef. Accepts NULL.
	/// Consumers taking ownership must release it on success as well as failure.
	void release(const Memory* _mem);

} // namespace frost

#endif // FROST_MEMORY_H_HEADER_GUARD
