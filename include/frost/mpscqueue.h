/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_MPSCQUEUE_H_HEADER_GUARD
#define FROST_MPSCQUEUE_H_HEADER_GUARD

#include "allocator.h"
#include "mutex.h"
#include "spscqueue.h"

namespace frost
{
	///
	template <typename Ty>
	class MpScUnboundedQueueT
	{
		FROST_CLASS(MpScUnboundedQueueT
			, NO_DEFAULT_CTOR
			, NO_COPY
			);

	public:
		///
		MpScUnboundedQueueT(AllocatorI* _allocator);

		///
		~MpScUnboundedQueueT();

		///
		void push(Ty* _ptr); // producer only

		///
		Ty* peek(); // consumer only

		///
		Ty* pop(); // consumer only

	private:
		Mutex m_write;
		SpScUnboundedQueueT<Ty> m_queue;
	};

	///
	template <typename Ty>
	class MpScUnboundedBlockingQueue
	{
		FROST_CLASS(MpScUnboundedBlockingQueue
			, NO_DEFAULT_CTOR
			, NO_COPY
			);

	public:
		///
		MpScUnboundedBlockingQueue(AllocatorI* _allocator);

		///
		~MpScUnboundedBlockingQueue();

		///
		void push(Ty* _ptr); // producer only

		///
		Ty* pop(); // consumer only

	private:
		MpScUnboundedQueueT<Ty> m_queue;
		Semaphore m_sem;
	};

} // namespace frost

#include "inline/mpscqueue.inl"

#endif // FROST_MPSCQUEUE_H_HEADER_GUARD
