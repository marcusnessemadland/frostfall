/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_MPSCQUEUE_H_HEADER_GUARD
#	error "Must be included from frost/mpscqueue.h!"
#endif // FROST_MPSCQUEUE_H_HEADER_GUARD

namespace frost
{
	template <typename Ty>
	inline MpScUnboundedQueueT<Ty>::MpScUnboundedQueueT(AllocatorI* _allocator)
		: m_queue(_allocator)
	{
	}

	template <typename Ty>
	inline MpScUnboundedQueueT<Ty>::~MpScUnboundedQueueT()
	{
	}

	template <typename Ty>
	inline void MpScUnboundedQueueT<Ty>::push(Ty* _ptr)
	{
		MutexScope lock(m_write);
		m_queue.push(_ptr);
	}

	template <typename Ty>
	inline Ty* MpScUnboundedQueueT<Ty>::peek()
	{
		return m_queue.peek();
	}

	template <typename Ty>
	inline Ty* MpScUnboundedQueueT<Ty>::pop()
	{
		return m_queue.pop();
	}

	template <typename Ty>
	inline MpScUnboundedBlockingQueue<Ty>::MpScUnboundedBlockingQueue(AllocatorI* _allocator)
		: m_queue(_allocator)
	{
	}

	template <typename Ty>
	inline MpScUnboundedBlockingQueue<Ty>::~MpScUnboundedBlockingQueue()
	{
	}

	template <typename Ty>
	inline void MpScUnboundedBlockingQueue<Ty>::push(Ty* _ptr)
	{
		m_queue.push(_ptr);
		m_sem.post();
	}

	template <typename Ty>
	inline Ty* MpScUnboundedBlockingQueue<Ty>::pop()
	{
		m_sem.wait();
		return m_queue.pop();
	}

} // namespace frost
