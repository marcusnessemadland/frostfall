/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_MUTEX_H_HEADER_GUARD
#define FROST_MUTEX_H_HEADER_GUARD

#include "frost.h"

namespace frost
{
	///
	class Mutex
	{
		FROST_CLASS(Mutex
			, NO_COPY
			);

	public:
		///
		Mutex();

		///
		~Mutex();

		///
		void lock();

		///
		void unlock();

	private:
		FROST_ALIGN_DECL(16, uint8_t) m_internal[64];
	};

	///
	class MutexScope
	{
		FROST_CLASS(MutexScope
			, NO_DEFAULT_CTOR
			, NO_COPY
			);

	public:
		///
		MutexScope(Mutex& _mutex);

		///
		~MutexScope();

	private:
		Mutex& m_mutex;
	};

} // namespace frost

#include "inline/mutex.inl"

#endif // FROST_MUTEX_H_HEADER_GUARD
