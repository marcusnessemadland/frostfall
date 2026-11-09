/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_SEM_H_HEADER_GUARD
#define FROST_SEM_H_HEADER_GUARD

#include "frost.h"

namespace frost
{
	///
	class Semaphore
	{
		FROST_CLASS(Semaphore
			, NO_COPY
			);

	public:
		///
		Semaphore();

		///
		~Semaphore();

		///
		void post(uint32_t _count = 1);

		///
		bool wait(int32_t _msecs = -1);

	private:
		FROST_ALIGN_DECL(16, uint8_t) m_internal[128];
	};

} // namespace frost

#endif // FROST_SEM_H_HEADER_GUARD
