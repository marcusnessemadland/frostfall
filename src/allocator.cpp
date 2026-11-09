/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#include <frost/allocator.h>

#include <malloc.h>

#ifndef FROST_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT
#	define FROST_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT 8
#endif // FROST_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT

namespace frost
{
	DefaultAllocator::DefaultAllocator()
	{
	}

	DefaultAllocator::~DefaultAllocator()
	{
	}

	void* DefaultAllocator::realloc(void* _ptr, size_t _size, size_t _align, const char* _filePath, uint32_t _line)
	{
		if (0 == _size)
		{
			if (NULL != _ptr)
			{
				if (FROST_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT >= _align)
				{
					::free(_ptr);
					return NULL;
				}

#	if FROST_COMPILER_MSVC
				FROST_UNUSED(_filePath, _line);
				_aligned_free(_ptr);
#	else
				alignedFree(this, _ptr, _align, Location(_filePath, _line) );
#	endif // FROST_
			}

			return NULL;
		}
		else if (NULL == _ptr)
		{
			if (FROST_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT >= _align)
			{
				return ::malloc(_size);
			}

#	if FROST_COMPILER_MSVC
			FROST_UNUSED(_filePath, _line);
			return _aligned_malloc(_size, _align);
#	else
			return alignedAlloc(this, _size, _align, Location(_filePath, _line) );
#	endif // FROST_
		}

		if (FROST_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT >= _align)
		{
			return ::realloc(_ptr, _size);
		}

#	if FROST_COMPILER_MSVC
		FROST_UNUSED(_filePath, _line);
		return _aligned_realloc(_ptr, _size, _align);
#	else
		return alignedRealloc(this, _ptr, _size, _align, Location(_filePath, _line) );
#	endif // FROST_
	}

} // namespace frost
