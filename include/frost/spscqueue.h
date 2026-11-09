/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_SPSCQUEUE_H_HEADER_GUARD
#define FROST_SPSCQUEUE_H_HEADER_GUARD

#include "allocator.h"
#include "cpu.h"
#include "semaphore.h"

namespace frost
{
	///
	class SpScUnboundedQueue
	{
		FROST_CLASS(SpScUnboundedQueue
			, NO_DEFAULT_CTOR
			, NO_COPY
			);

	public:
		///
		SpScUnboundedQueue(AllocatorI* _allocator);

		///
		~SpScUnboundedQueue();

		///
		void push(void* _ptr);

		///
		void* peek();

		///
		void* pop();

	private:
		struct Node
		{
			///
			Node(void* _ptr);

			void* m_ptr;
			Node* m_next;
		};

		AllocatorI* m_allocator;
		Node* m_first;
		Node* m_divider;
		Node* m_last;
	};

	///
	template<typename Ty>
	class SpScUnboundedQueueT
	{
		FROST_CLASS(SpScUnboundedQueueT
			, NO_DEFAULT_CTOR
			, NO_COPY
			);

	public:
		///
		SpScUnboundedQueueT(AllocatorI* _allocator);

		///
		~SpScUnboundedQueueT();

		///
		void push(Ty* _ptr);

		///
		Ty* peek();

		///
		Ty* pop();

	private:
		SpScUnboundedQueue m_queue;
	};

#if FROST_CONFIG_SUPPORTS_THREADING
	///
	class SpScBlockingUnboundedQueue
	{
		FROST_CLASS(SpScBlockingUnboundedQueue
			, NO_DEFAULT_CTOR
			, NO_COPY
			);

	public:
		///
		SpScBlockingUnboundedQueue(AllocatorI* _allocator);

		///
		~SpScBlockingUnboundedQueue();

		///
		void push(void* _ptr); // producer only

		///
		void* peek(); // consumer only

		///
		void* pop(int32_t _msecs = -1); // consumer only

	private:
		Semaphore m_count;
		SpScUnboundedQueue m_queue;
	};

	///
	template<typename Ty>
	class SpScBlockingUnboundedQueueT
	{
		FROST_CLASS(SpScBlockingUnboundedQueueT
			, NO_DEFAULT_CTOR
			, NO_COPY
			);

	public:
		///
		SpScBlockingUnboundedQueueT(AllocatorI* _allocator);

		///
		~SpScBlockingUnboundedQueueT();

		///
		void push(Ty* _ptr); // producer only

		///
		Ty* peek(); // consumer only

		///
		Ty* pop(int32_t _msecs = -1); // consumer only

	private:
		SpScBlockingUnboundedQueue m_queue;
	};
#endif // FROST_CONFIG_SUPPORTS_THREADING

} // namespace frost

#include "inline/spscqueue.inl"

#endif // FROST_SPSCQUEUE_H_HEADER_GUARD
