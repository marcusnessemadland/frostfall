/*
 * Copyright 2026-2027 Marcus Nesse Madland. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/memory.h>

namespace frost
{
	struct MemoryRef
	{
		Memory          memory;
		AllocatorI*     allocator;
		MemoryReleaseFn releaseFn;
		void*           userData;
	};

	const Memory* allocMemory(AllocatorI* _allocator, uint32_t _size, const Location& _location)
	{
		FROST_ASSERT(NULL != _allocator, "Memory requires an allocator.");
		if (NULL == _allocator || size_t(_size) > SIZE_MAX - sizeof(MemoryRef) - 15)
		{
			return NULL;
		}

		MemoryRef* ref = (MemoryRef*)alloc(_allocator, sizeof(MemoryRef) + 15 + size_t(_size), 16, _location);
		if (NULL == ref)
		{
			return NULL;
		}

		ref->memory.data = alignPtr(ref + 1, 0, 16);
		ref->memory.size = _size;
		ref->allocator  = _allocator;
		ref->releaseFn  = NULL;
		ref->userData   = NULL;
		return &ref->memory;
	}

	const Memory* copyMemory(AllocatorI* _allocator, const void* _data, uint32_t _size, const Location& _location)
	{
		if (NULL == _data && 0 != _size)
		{
			return NULL;
		}

		const Memory* mem = allocMemory(_allocator, _size, _location);
		if (NULL != mem && 0 != _size)
		{
			memCopy(mem->data, _data, _size);
		}
		return mem;
	}

	const Memory* makeRef(AllocatorI* _allocator, const void* _data, uint32_t _size, MemoryReleaseFn _releaseFn, void* _userData, const Location& _location)
	{
		FROST_ASSERT(NULL != _allocator, "Memory requires an allocator.");
		if (NULL == _allocator || (NULL == _data && 0 != _size) )
		{
			return NULL;
		}

		MemoryRef* ref = (MemoryRef*)alloc(_allocator, sizeof(MemoryRef), 16, _location);
		if (NULL == ref)
		{
			return NULL;
		}

		ref->memory.data = const_cast<void*>(_data);
		ref->memory.size = _size;
		ref->allocator  = _allocator;
		ref->releaseFn  = _releaseFn;
		ref->userData   = _userData;
		return &ref->memory;
	}

	void release(const Memory* _mem)
	{
		if (NULL != _mem)
		{
			MemoryRef* ref = (MemoryRef*)const_cast<Memory*>(_mem);
			AllocatorI* allocator = ref->allocator;
			if (NULL != ref->releaseFn)
			{
				ref->releaseFn(ref->memory.data, ref->userData);
			}
			free(allocator, ref, 16);
		}
	}

} // namespace frost
