/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_MUTEX_H_HEADER_GUARD
#	error "Must be included from frost/mutex.h!"
#endif // FROST_MUTEX_H_HEADER_GUARD

namespace frost
{
	inline MutexScope::MutexScope(Mutex& _mutex)
		: m_mutex(_mutex)
	{
		m_mutex.lock();
	}

	inline MutexScope::~MutexScope()
	{
		m_mutex.unlock();
	}

} // namespace frost
