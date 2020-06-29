/*
Zero copy string implementation supporting the most common string operations
*/

#ifndef MBUF_H
#define MBUF_H

#include <stdlib.h>
#include <string.h>
#include <string>
#include "DiggiAssert.h"

/// Should we check memory alignment, used since mbuf may offset per single byte, causing terrible performance.
#ifdef MBUF_ALIGNMENT_CHECK

///check for allignment of given count
#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)

#else
///auto success
#define is_aligned(POINTER, BYTE_COUNT) \
	true

#endif
///check const alignment aswell
#define ALIGNED_CONST_CHAR(x) alignas(x) static const char
#define ALIGNED_CHAR(x) alignas(x) const char

///mbuf node struct declaration, each mbuf_t consists of multiple mbuf_node_t in a chain
typedef struct mbuf_node_t {
    ///size of data buffer in mbuf_node
	size_t size;
    ///pointer ot next mbuf_node in chain
	mbuf_node_t *next;
    /// current reference counter of current mbuf_node_t
	unsigned ref;
    /// is data object managed by mbuf, if so, mbuf_destroy will free buffer when reference count reaches zero
	bool alloc;
    /// data  buffer, unless alloc is set, managed externally.
	char *data;
}mbuf_node_t;
///mbuf reference
typedef struct mbuf_t {
    ///reference to head node in mbuf chain
	mbuf_node_t *head;
}mbuf_t;

size_t mbuf_append_tail(mbuf_t *mb, const char * data, size_t mbuf_size, size_t expected_size);

size_t mbuf_append_tail(mbuf_t *mb, const char * data, size_t expected_size);

void mbuf_incref(mbuf_t *mb);
void mbuf_incref(mbuf_t *mb, size_t count);

// Does not clean up b, others might be referencing it, free operation guarded by reference counting
void mbuf_concat(mbuf_t *a, mbuf_t *b);

size_t mbuf_append_tail(mbuf_t *mb, char * data, size_t mbuf_size, bool alloc, size_t expected_size);

size_t mbuf_append_head(mbuf_t *mb, const char * data, size_t mbuf_size);

size_t mbuf_append_head(mbuf_t *mb, const char * data);

size_t mbuf_append_head(mbuf_t *mb, char * data, size_t mbuf_size, bool alloc);

void *mbuf_remove_head(mbuf_t *mb, size_t size_mb);

mbuf_node_t *mbuf_node_at_pos(mbuf_t *mb, unsigned index, unsigned *node_base);

void *mbuf_remove_tail(mbuf_t *mb, size_t size_mb);

mbuf_node_t* mbuf_duplicate_offset(mbuf_t *mbuf, size_t offset);

void mbuf_to_printable(mbuf_t *mbuf, size_t length, size_t offset, std::string *str);
void mbuf_populate_buffer(mbuf_t *mb, size_t length, size_t offset, char* buf);
void mbuf_replace(mbuf_t *mb, size_t length, size_t offset, char* buf);

mbuf_t * mbuf_new(void);

size_t mbuf_node_destroy(mbuf_node_t * nd, bool freedata = false);

size_t mbuf_destroy(mbuf_t *mb, size_t offset, size_t length);


#endif