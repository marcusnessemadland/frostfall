/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_THREAD_H_HEADER_GUARD
#	error "Must be included from frost/thread.h!"
#endif // FROST_THREAD_H_HEADER_GUARD

#define FROST_TLS_DATA_TEB (FROST_PLATFORM_WINDOWS && FROST_CPU_X86 && !FROST_CRT_NONE)

#if FROST_TLS_DATA_TEB
#	if FROST_COMPILER_MSVC
#		if FROST_ARCH_64BIT
extern "C" unsigned __int64 __readgsqword(unsigned long _offset);
#			pragma intrinsic(__readgsqword)
#		else
extern "C" unsigned long __readfsdword(unsigned long _offset);
#			pragma intrinsic(__readfsdword)
#		endif // FROST_ARCH_64BIT
#	endif // FROST_COMPILER_MSVC

namespace frost
{
	constexpr uint32_t kTebTlsSlotsOffset          = FROST_ARCH_64BIT ? 0x1480 : 0xe10;
	constexpr uint32_t kTebTlsExpansionSlotsOffset = FROST_ARCH_64BIT ? 0x1780 : 0xf94;
	constexpr uint32_t kTebTlsSlotsNum             = 64;

#	if FROST_COMPILER_MSVC && FROST_ARCH_64BIT
	inline void* readTeb(uint32_t _offset)
	{
		return (void*)__readgsqword(_offset);
	}
#	elif FROST_COMPILER_MSVC
	inline void* readTeb(uint32_t _offset)
	{
		return (void*)__readfsdword(_offset);
	}
#	elif FROST_ARCH_64BIT
	inline void* readTeb(uint32_t _offset)
	{
		void* ptr;
		__asm__ volatile ("movq %%gs:(%1), %0" : "=r" (ptr) : "r" (uintptr_t(_offset) ) );
		return ptr;
	}
#	else
	inline void* readTeb(uint32_t _offset)
	{
		void* ptr;
		__asm__ volatile ("movl %%fs:(%1), %0" : "=r" (ptr) : "r" (uintptr_t(_offset) ) );
		return ptr;
	}
#	endif // FROST_COMPILER_*

	inline void* TlsData::get() const
	{
		const uint32_t slot = *(const uint32_t*)m_internal;

		if (slot < kTebTlsSlotsNum) [[likely]]
		{
			return readTeb(kTebTlsSlotsOffset + slot*sizeof(void*) );
		}

		void** expansionSlots = (void**)readTeb(kTebTlsExpansionSlotsOffset);

		return NULL == expansionSlots
			? NULL
			: expansionSlots[slot - kTebTlsSlotsNum]
			;
	}

} // namespace frost
#endif // FROST_TLS_DATA_TEB
