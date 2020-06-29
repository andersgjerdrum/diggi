#ifndef LOCKFREE_RB_H
#define LOCKFREE_RB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#ifndef DIGGI_ENCLAVE
#include <malloc.h>
#endif

#include "DiggiAssert.h"
#define DCACHE1_LINESIZE 64
#define ____cacheline_aligned	__attribute__((aligned(DCACHE1_LINESIZE)))

#define PAGE_SIZE 4096u
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
typedef struct ThrPos {
	    volatile unsigned long head;
		volatile unsigned long tail;
		volatile unsigned long in_situ;
}ThrPos;

typedef struct lf_buffer_t {
    volatile size_t n_producers_; 
	volatile size_t n_consumers_;
	// currently free position (next to insert)
	volatile unsigned long	head_ ____cacheline_aligned;
	// current tail, next to pop
	volatile unsigned long	tail_ ____cacheline_aligned;
	// last not-processed producer's pointer
	volatile unsigned long	last_head_ ____cacheline_aligned;
	// last not-processed consumer's pointer
	volatile unsigned long	last_tail_ ____cacheline_aligned;
	ThrPos		* thr_p_;
	void  ** ptr_array_;
    volatile unsigned long Q_SIZE;
    volatile unsigned long Q_MASK;
}lf_buffer_t;


lf_buffer_t * lf_new(size_t size, size_t prod, size_t cons);

void lf_destroy(lf_buffer_t *lf);

void lf_send(lf_buffer_t *lf, void *msg, size_t requesting_thread);

void* lf_recieve(lf_buffer_t *lf, size_t requesting_thread);
void* lf_try_recieve(lf_buffer_t *lf, size_t requesting_thread);

/*
LOCKFREE_RB_H
*/


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif