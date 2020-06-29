

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file lockfree_rb_q.cpp
 * @brief Implementation of Lock-free ring-buffer queue
 * Copyright (C) 2012-2013 Alexander Krizhanovsky (ak@natsys-lab.com).
 * Modified heavily by Anders Gjerdrum (andersgjerdrum@outlook.com) for C compatibility

 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 * See http://www.gnu.org/licenses/lgpl.html . * @version 0.1
 * @date 2020-02-04
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "lockfree_rb_q.h"



/**
 * @brief Create new lock free queue with a particular count of slots and a predicted ammount of producers and consumers.
 * Producers may be consumers aswell.
 * Only stores pointer to buffers managed elseweres
 * All access to queue must use functions to maintain concurrent ABA safety and lockfree properties.
 * NB! Q_SIZE must be power of 2
 * @param Q_SIZE Size of pointer queue.
 * @param prod number of producers onto the queue
 * @param cons numbers of consumers of the queue
 * @return lf_buffer_t* return pointer to buffer.
 */
lf_buffer_t* lf_new(size_t Q_SIZE, unsigned long prod, unsigned long cons)
{
    DIGGI_ASSERT((Q_SIZE & (Q_SIZE - 1)) == 0);
	auto lfbuffer = (lf_buffer_t* )memalign(PAGE_SIZE, sizeof(lf_buffer_t));
	lfbuffer->n_producers_ = prod;
	lfbuffer->n_consumers_ = cons;
	lfbuffer->head_ = 0;
	lfbuffer->tail_ = 0;
	lfbuffer->last_head_ = 0;
	lfbuffer->last_tail_ = 0;
	lfbuffer->Q_SIZE = Q_SIZE;
	lfbuffer->Q_MASK = Q_SIZE - 1;
	auto n = MAX(cons, prod);
	lfbuffer->thr_p_ = (ThrPos *)memalign(PAGE_SIZE, sizeof(ThrPos) * n);
	// Set per thread tail and head to ULONG_MAX.
	memset((void *)lfbuffer->thr_p_, 0xFF, sizeof(ThrPos) * n);
	lfbuffer->ptr_array_ = (void **)memalign(PAGE_SIZE,
			Q_SIZE * sizeof(void *));
	memset(lfbuffer->ptr_array_,0x0,Q_SIZE * sizeof(void *));
	for(unsigned i = 0; i < n; i++){
		lfbuffer->thr_p_[i].in_situ = 0;
	}
	return lfbuffer;
}
/**
 * @brief destroy lock free queue
 * @warning ensure no threads are using buffer.
 * @param lf 
 */
void lf_destroy(lf_buffer_t *lf){
	free(lf->thr_p_);
	free(lf->ptr_array_);
	free(lf);
}

/**
 * @brief send message pointer from thread.
 * May block on full queue.
 * Must ensure that calling thread is able to correctly identify itself, relative to others using the same queue.
 * @param lf lock free queue struct
 * @param msg message pointer to send
 * @param requesting_thread id of requesting thread
 */
void lf_send(lf_buffer_t *lf, void *msg, size_t requesting_thread)
{
	DIGGI_ASSERT(requesting_thread < lf->n_producers_);
	lf->thr_p_[requesting_thread].head = lf->head_;
	lf->thr_p_[requesting_thread].head = __sync_fetch_and_add(&lf->head_, 1);

	while (__builtin_expect(lf->thr_p_[requesting_thread].head >= lf->last_tail_ + lf->Q_SIZE, 0))
	{
		auto min = lf->tail_;

		for (size_t i = 0; i < lf->n_consumers_; ++i) {
			auto tmp_t = lf->thr_p_[i].tail;

			asm volatile("" ::: "memory");

			if (tmp_t < min)
				min = tmp_t;
		}
		lf->last_tail_ = min;

		if (lf->thr_p_[requesting_thread].head < lf->last_tail_ + lf->Q_SIZE)
			break;
		__asm volatile ("pause" ::: "memory");
	}

	lf->ptr_array_[lf->thr_p_[requesting_thread].head & lf->Q_MASK] = msg;

	lf->thr_p_[requesting_thread].head = ULONG_MAX;
}
/**
 * @brief try a recieve operation on queue witout blocking.
 * Must ensure that calling thread is able to correctly identify itself, relative to others using the same queue.
 * @param lf lock free queue struct
 * @param requesting_thread id of requesting thread
 * @return void* null or message struct pointer
 */
void * lf_try_recieve(lf_buffer_t *lf, size_t requesting_thread)
{
	DIGGI_ASSERT(requesting_thread < lf->n_consumers_);	
	if(!lf->thr_p_[requesting_thread].in_situ){
		lf->thr_p_[requesting_thread].tail = lf->tail_;
		lf->thr_p_[requesting_thread].tail = __sync_fetch_and_add(&lf->tail_, 1);
		lf->thr_p_[requesting_thread].in_situ = 1;
	}

	while (__builtin_expect(lf->thr_p_[requesting_thread].tail >= lf->last_head_, 0))
	{
		auto min = lf->head_;

		// Update the last_head_.
		for (size_t i = 0; i < lf->n_producers_; ++i) {
			auto tmp_h = lf->thr_p_[i].head;

			// Force compiler to use tmp_h exactly once.
			asm volatile("" ::: "memory");

			if (tmp_h < min)
				min = tmp_h;
		}
		lf->last_head_ = min;

		if (lf->thr_p_[requesting_thread].tail < lf->last_head_)
			break;
		return nullptr;
	}

	void *ret = lf->ptr_array_[lf->thr_p_[requesting_thread].tail & lf->Q_MASK];
	// Allow producers rewrite the slot.
	lf->thr_p_[requesting_thread].tail = ULONG_MAX;
	lf->thr_p_[requesting_thread].in_situ = 0;
	return ret;
}
/**
 * @brief consume message from queue
 * may block if queue is empty
 * Must ensure that calling thread is able to correctly identify itself, relative to others using the same queue.
 * @param lf lock free queue struct
 * @param requesting_thread 
 * @return void* message struct pointer
 */
void * lf_recieve(lf_buffer_t *lf, size_t requesting_thread)
{
	DIGGI_ASSERT(requesting_thread < lf->n_consumers_);	
	if(!lf->thr_p_[requesting_thread].in_situ){
		lf->thr_p_[requesting_thread].tail = lf->tail_;
		lf->thr_p_[requesting_thread].tail = __sync_fetch_and_add(&lf->tail_, 1);
		lf->thr_p_[requesting_thread].in_situ = 1;
	}

	while (__builtin_expect(lf->thr_p_[requesting_thread].tail >= lf->last_head_, 0))
	{
		auto min = lf->head_;

		// Update the last_head_.
		for (size_t i = 0; i < lf->n_producers_; ++i) {
			auto tmp_h = lf->thr_p_[i].head;

			// Force compiler to use tmp_h exactly once.
			asm volatile("" ::: "memory");

			if (tmp_h < min)
				min = tmp_h;
		}
		lf->last_head_ = min;

		if (lf->thr_p_[requesting_thread].tail < lf->last_head_)
			break;
		__asm volatile ("pause" ::: "memory");
	}

	void *ret = lf->ptr_array_[lf->thr_p_[requesting_thread].tail & lf->Q_MASK];
	// Allow producers rewrite the slot.
	lf->thr_p_[requesting_thread].tail = ULONG_MAX;
	lf->thr_p_[requesting_thread].in_situ = 0;
	return ret;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */