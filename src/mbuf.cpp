/**
 * @file mbuf.cpp
 * @author your name (you@domain.com)
 * @brief implementation of virtual mutable contiguous buffer.
 * buffers which allow append, replace, concat operations without creating internal copies of the data.@
 * Supports pattern searching and traversing.
 * does not copy data, only stores a reference to it.
 * @version 0.1
 * @date 2020-02-05
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "mbuf.h"
/**
 * @brief append a char buffer to an existing mbuf
 * If mbuf is longer than expected, because of deletes from tail. Deletes from tail only reduces length counter(for performance), and does not perform pruning
 * If the tail is longer than expected_size, the tail mbuf_node_t is replaced 
 * @param mb mbuf datastruct reference
 * @param data data to add
 * @param mbuf_size size of data
 * @param expected_size expected total size of mbuf
 * @return size_t actual size of mbuf after operation
 */
size_t mbuf_append_tail(mbuf_t *mb, const char * data, size_t mbuf_size, size_t expected_size)
{
	return mbuf_append_tail(mb, (char*)data, mbuf_size, false, expected_size);
}
/**
 * @brief append zero-terminated constant char buffer to end of an existing mbuf
 * If mbuf is longer than expected, because of deletes from tail. Deletes from tail only reduces length counter(for performance), and does not perform pruning
 * If the tail is longer than expected_size, the tail mbuf_node_t is replaced 
 * @warning forces buffer traversal through strlen
 * @param mb mbuf datastruct reference
 * @param data data to append
 * @param expected_size expected total size of mbuf
 * @return size_t return actual size of mbuf after operation
 */
size_t mbuf_append_tail(mbuf_t *mb, const char * data, size_t expected_size)
{
	return mbuf_append_tail(mb, data, strlen(data), expected_size);
}
/**
 * @brief increase the external reference count to mbuf. 
 * Traverses buffer and increases reference count of all objects.
 * mbuf nodes may participate in multiple mbufs, as a result of concatenations and split operations.
 * For deletion of mbufs with multiple references, only the last remaining reference to a given mbuf node should delete it.
 * @param mb mbuf to increase reference count
 */
void mbuf_incref(mbuf_t *mb) 
{
	mbuf_incref(mb, 1);
}
/**
 * @brief traverse mbuf and increase reference count by a given amount. 
 * 
 * @param mb mbuf to increase
 * @param count increase by given amount
 */
void mbuf_incref(mbuf_t *mb, size_t count)
{
	DIGGI_ASSERT(mb != nullptr);
	auto node = mb->head;
	DIGGI_ASSERT(node != nullptr);
	DIGGI_ASSERT(node->size != 0);
	node->ref+= count;
	while (node->next != nullptr) {
		node = node->next;
		DIGGI_ASSERT(node != nullptr);
		DIGGI_ASSERT(node->size != 0);
		DIGGI_ASSERT(is_aligned(node->data, 8));
		node->ref+= count;
	}
}



/**
 * @brief Concat two existing mbufs, adds b to the end of a
 * @warning known bug,  non end concatenation will fork mbuf, and possibly loose mbuf reference at fork. Hard to fix. but only involve that 16 byte struct.
 * Increases reference on b buffer as it may be used both as independant and concatenated to end of a.
 * @param a right hand buffer
 * @param b left hand buffer(added to end)
 */
void mbuf_concat(mbuf_t *a, mbuf_t *b)
{
	DIGGI_ASSERT(a != nullptr);
	DIGGI_ASSERT(b != nullptr);
	DIGGI_ASSERT(b->head != nullptr);

	auto node = a->head;
	if (node != nullptr) {

		DIGGI_ASSERT(node->size != 0);
		DIGGI_ASSERT(is_aligned(node->data, 8));
		DIGGI_ASSERT(b->head != nullptr);
		while (node->next != nullptr) {
			node = node->next;

			DIGGI_ASSERT(node != nullptr);
			DIGGI_ASSERT(node->size != 0);
			DIGGI_ASSERT(is_aligned(node->data, 8));

		}
		node->next = b->head;
	}
	else {
		a->head = b->head;
	}

	node = b->head;

	DIGGI_ASSERT(node->size != 0);
	DIGGI_ASSERT(is_aligned(node->data, 8));

	node->ref++;
	while (node->next != nullptr) {
		node = node->next;

		DIGGI_ASSERT(node != nullptr);
		DIGGI_ASSERT(node->size != 0);
		DIGGI_ASSERT(is_aligned(node->data, 8));
		node->ref++;
	}
}
/**
 * @brief append buffer with given size to end of mbuf
 * If mbuf is longer than expected, because of deletes from tail. Deletes from tail only reduces length counter(for performance), and does not perform pruning
 * If the tail is longer than expected_size, the tail mbuf_node_t is replaced 
 * May specify if buffer lifetime should be handed over to mbuf reference counting through "alloc" param.
 * If so, data should not be free´d outside of mbuf.
 * @param mb mbuf to append to
 * @param data data to append to buffer
 * @param mbuf_size size of data.
 * @param alloc should data buffer be managed by mbuf
 * @param expected_size expected size after append
 * @return size_t actual size after append
 */
size_t mbuf_append_tail(mbuf_t *mb, char * data, size_t mbuf_size, bool alloc, size_t expected_size)
{
	if (mbuf_size == 0) {
		return 0;
	}
	size_t acctual_size = 0;
	DIGGI_ASSERT(mb != nullptr);
	DIGGI_ASSERT(data != nullptr);
	DIGGI_ASSERT(is_aligned(data, 8));

	auto mbuf = (mbuf_node_t *)malloc(sizeof(mbuf_node_t));
	mbuf->size = mbuf_size;
	mbuf->data = data;
	mbuf->ref = 1;
	mbuf->alloc = alloc;
	mbuf->next = nullptr;

	if (mb->head != nullptr) {
		auto node = mb->head;
		auto prev = mb->head;
		DIGGI_ASSERT(node->size != 0);
		DIGGI_ASSERT(is_aligned(node->data, 8));
		acctual_size += node->size;
		while (node->next != nullptr) {
			if (acctual_size > expected_size) {

				break;
			}
			prev = node;
			node = node->next;
			acctual_size += node->size;

			DIGGI_ASSERT(node != nullptr);
			DIGGI_ASSERT(node->size != 0);
			DIGGI_ASSERT(is_aligned(node->data, 8));

		}
		if (acctual_size > expected_size) {
			auto new_node = (mbuf_node_t *)malloc(sizeof(mbuf_node_t));
			new_node->size = (node->size - (acctual_size - expected_size));
			new_node->data = node->data;
			new_node->alloc = false;
			new_node->ref = 1;
			prev->next = new_node;
			node = new_node;
			acctual_size = expected_size;
		}
		node->next = mbuf;
	}
	else {
		mb->head = mbuf;
	}

	return acctual_size;
}
/**
 * @brief insert buffer of given size to header of mbuf
 * 
 * @param mb mbuf subject to insertion
 * @param data data to instert
 * @param mbuf_size size of data buffer
 * @return size_t new size of mbuf
 */
size_t mbuf_append_head(mbuf_t *mb, const char * data, size_t mbuf_size)
{
	return mbuf_append_head(mb, (char*)data, mbuf_size, false);
}
/**
 * @brief insert null terminated buffer to header
 * @warning forces buffer traversal through strlen
 * @param mb  mbuf subject to insertion
 * @param data pointer to zero terminated const char data.
 * @return size_t new size of mbuf
 */
size_t mbuf_append_head(mbuf_t *mb, const char * data)
{
	return mbuf_append_head(mb, data, strlen(data));
}
/**
 * @brief insert buffer of given size to header of mbuf
 * May specify if buffer lifetime should be handed over to mbuf reference counting through "alloc" param.
 * If so, data should not be free´d outside of mbuf.
 * @param mb mbuf subject to insertion
 * @param data pointer to data to insert
 * @param mbuf_size size of data to insert
 * @param alloc boolean specifying if mbuf should manage data buffer lifetime
 * @return size_t new size of mbuf
 */
size_t mbuf_append_head(mbuf_t *mb, char * data, size_t mbuf_size, bool alloc)
{
	DIGGI_ASSERT(mb != nullptr);
	DIGGI_ASSERT(data != nullptr);
	DIGGI_ASSERT(mbuf_size > 0);
	DIGGI_ASSERT(is_aligned(data, 8));

	auto mbuf = (mbuf_node_t *)malloc(sizeof(mbuf_node_t)); 
	mbuf->size = mbuf_size;
	mbuf->data = data;
	mbuf->alloc = alloc;
	mbuf->ref = 1;
	if (mb->head == nullptr) {
		mbuf->next = nullptr;
		mb->head = mbuf;
	}
	else {
		mbuf->next = mb->head;
		mb->head = mbuf;
	}

	return mbuf_size;
}
/**
 * @brief remove "size_mb" bytes from header.
 * must be identical to size of header node.
 * If not will throw assertion failure.
 * @warning does not increment reference count of mbuf_node_t, any further use should copy data or increase reference count.
 * @param mb mbuf to remove from
 * @param size_mb size returnable
 * @return void* return data in head.
 */
void *mbuf_remove_head(mbuf_t *mb, size_t size_mb)
{
	DIGGI_ASSERT(mb != nullptr);
	DIGGI_ASSERT(mb->head != nullptr);
	DIGGI_ASSERT(mb->head->size == size_mb);

	auto tmp = mb->head;
	void *ptr = tmp->data;
	mb->head = mb->head->next;
	mbuf_node_destroy(tmp);
	return ptr;
}
/**
 * @brief retrieve mbuf_node owning byte at a given position(index)
 * traverse mbuf to find mbuf_node_t owning the byte at "index" position.
 * If not first byte inside a mbuf_node, the node_base returns the first byte position
 * @warning does not increment reference count of mbuf_node_t, any further use should copy data or increase reference count.
 * @param mb mbuf to traverse
 * @param index index to find in mbuf
 * @param node_base base of mbuf_node_t where index resides.
 * @return mbuf_node_t* reference to mbuf_node_t
 */
mbuf_node_t *mbuf_node_at_pos(mbuf_t *mb, unsigned index, unsigned *node_base) {

	DIGGI_ASSERT(mb != nullptr);
	DIGGI_ASSERT(mb->head != nullptr);
	DIGGI_ASSERT(mb->head->size != 0);

	auto tmp = mb->head;
	auto count = 0;

	DIGGI_ASSERT(tmp->size != 0);
	DIGGI_ASSERT(is_aligned(tmp->data, 8));

	while (count + tmp->size <= index) {

		DIGGI_ASSERT(tmp != nullptr);
		DIGGI_ASSERT(tmp->size != 0);
		DIGGI_ASSERT(is_aligned(tmp->data, 8));
		count += tmp->size;
		tmp = tmp->next;
		if (tmp == nullptr) {
			break;
		}
	}

	*node_base = count;
	return tmp;

}
/**
 * @brief remove "size_mb" bytes from tail
 * Size must be same as last mbuf_node size
 * @warning does not increment reference count of mbuf_node_t, any further use should copy data or increase reference count.
 * @param mb mbuf to remove from
 * @param size_mb size of mbuf to remove
 * @return void* data content pointer in last node
 */
void *mbuf_remove_tail(mbuf_t *mb, size_t size_mb)
{
	DIGGI_ASSERT(mb != nullptr);
	DIGGI_ASSERT(mb->head != nullptr);

	auto node = mb->head;

	DIGGI_ASSERT(node->size != 0);
	DIGGI_ASSERT(is_aligned(node->data, 8));

	auto node_parrent = node;
	while (node->next != nullptr) {

		node_parrent = node;
		node = node->next;

		DIGGI_ASSERT(node != nullptr);
		DIGGI_ASSERT(node->size != 0);
		DIGGI_ASSERT(is_aligned(node->data, 8));
	}

	DIGGI_ASSERT(node->size == size_mb);

	void *ptr = node->data;
	node_parrent->next = nullptr;

	if (node == mb->head) {
		mb->head = nullptr;
	}

	mbuf_node_destroy(node);
	return ptr;
}

/*Valgrind reports memory leak at malloc below*/

/**
 * @brief duplicate mbuf at offset.
 * will fork mbuf at a particular offset.
 * mbuf node at position recreated to point at offset.
 * Returns new mbuf node with single refcount.
 * used to append zcstring with offset > 0 into other zcstring.
 * zcstring bytes preceeding
 * @param mbuf mbuf to duplicate at offset 
 * @param offset offset into mbuf.
 * @return mbuf_node_t* return new mbuf_node starting at offset.
 */
mbuf_node_t* mbuf_duplicate_offset(mbuf_t *mbuf, size_t offset) 
{
	unsigned int base = 0;

	auto node = mbuf_node_at_pos(mbuf, offset, &base);

	auto new_head_node = (mbuf_node_t *)malloc(sizeof(mbuf_node_t)); 

	auto size_alteration = (offset - base);
	new_head_node->data = node->data + size_alteration;
	new_head_node->size = node->size - size_alteration;
	new_head_node->alloc = false;
	new_head_node->ref = 1;
	new_head_node->next = node->next;

	return new_head_node;
}

/**
 * @brief serialize mbuf into STL imutable string
 * @warning forces copy of mbuf into string.
 * @param mb mbuf to serialize
 * @param length bytes to serialize
 * @param offset ofset into  mbuf to begin serialization
 * @param str pointer reference to target std::string object.
 */
void mbuf_to_printable(mbuf_t *mb, size_t length, size_t offset, std::string *str) {

	DIGGI_ASSERT(mb != nullptr);

	if (mb->head == nullptr) {
		return;
	}

	unsigned base = 0;
	auto node = mbuf_node_at_pos(mb, offset, &base);

	unsigned ofst = (offset - base);

	DIGGI_ASSERT(node->size != 0);
	DIGGI_ASSERT(is_aligned(node->data, 8));


	if (length + ofst < node->size) {
		str->append(node->data + ofst, length);
		return;
	}
	else {

		str->append(node->data + ofst, node->size - ofst);
	}
	length = length - (node->size - ofst);
	while (length > 0) {

		node = node->next;
		if (node == nullptr) {
			/*concat followed by popback can cause invalid length pointer*/
			break;
		}
		DIGGI_ASSERT(node->size != 0);
		DIGGI_ASSERT(is_aligned(node->data, 8));

		if (length < node->size) {

			str->append(node->data, length);
			break;
		}

		str->append(node->data, node->size);
		length -= node->size;
	}
}
/**
 * @brief insert content of mbuf within range of bytes into provided buffer.
 * @warning will cause copy operation into buffer.
 * range must not extend past end of buffer, undefined behaviour
 * @param mb mbuf to retrieve data from
 * @param length length of bytes to retrieve
 * @param offset offset into buffer to retriveve
 * @param buf buffer to insert into
 */
void mbuf_populate_buffer(mbuf_t *mb, size_t length, size_t offset, char* buf) {

	DIGGI_ASSERT(mb != nullptr);

	if (mb->head == nullptr) {
		return;
	}

	unsigned base = 0;
	auto node = mbuf_node_at_pos(mb, offset, &base);

	unsigned ofst = (offset - base);

	DIGGI_ASSERT(node->size != 0);
	DIGGI_ASSERT(is_aligned(node->data, 8));


	if (length + ofst < node->size) {
		memcpy(buf, node->data + ofst, length);
		return;
	}
	else {
		memcpy(buf, node->data + ofst, node->size - ofst);
		buf += node->size - ofst;
	}
	length = length - (node->size - ofst);
	while (length > 0) {

		node = node->next;
		if (node == nullptr) {
			/*concat followed by popback can cause invalid length pointer*/
			break;
		}
		DIGGI_ASSERT(node->size != 0);
		DIGGI_ASSERT(is_aligned(node->data, 8));

		if (length < node->size) {

			memcpy(buf, node->data, length);
			break;
		}

		memcpy(buf, node->data, node->size);
		buf += node->size;
		length -= node->size;
	}
}
/**
 * @brief replace range of bytes of mbuf with new content.
 * @warning will cause copy operation into buffer.
 * range must not extend past end of buffer, undefined behaviour
 * @param mb mbuf to replace into
 * @param length length of bytes to replace
 * @param offset offset into buffer to replace
 * @param buf buffer to insert
 */
void mbuf_replace(mbuf_t *mb, size_t length, size_t offset, char* buf) {

	DIGGI_ASSERT(mb != nullptr);

	if (mb->head == nullptr) {
		return;
	}

	unsigned base = 0;
	auto node = mbuf_node_at_pos(mb, offset, &base);

	unsigned ofst = (offset - base);

	DIGGI_ASSERT(node->size != 0);
	DIGGI_ASSERT(is_aligned(node->data, 8));


	if (length + ofst < node->size) {
		memcpy(node->data + ofst, buf, length);
		return;
	}
	else {
		memcpy(node->data + ofst, buf, node->size - ofst);
		buf += node->size - ofst;
	}
	length = length - (node->size - ofst);
	while (length > 0) {

		node = node->next;
		if (node == nullptr) {
			/*concat followed by popback can cause invalid length pointer*/
			break;
		}
		DIGGI_ASSERT(node->size != 0);
		DIGGI_ASSERT(is_aligned(node->data, 8));

		if (length < node->size) {

			memcpy(node->data, buf, length);
			break;
		}

		memcpy(node->data, buf, node->size);
		buf += node->size;
		length -= node->size;
	}
}

/**
 * @brief new mbuf.
 * will not have any mbuf_nodes
 * 
 * @return mbuf_t* new mbuf struct.
 */
mbuf_t * mbuf_new(void)
{
	auto retbuf = (mbuf_t *)malloc(sizeof(mbuf_t));
	retbuf->head = nullptr;

	return retbuf;
}
/**
 * @brief decrement reference count of node  and destroy if it reaches 0.
 * Depending on freedata, also delete data, if mbuf managed.
 * Sets struct to 0x0 in order to prevent later traversal prior to memory garbage collection (page unmap by OS).
 * @param nd node to destroy
 * @param freedata if memory managed, delete data
 * @return size_t 
 */
size_t mbuf_node_destroy(mbuf_node_t * nd, bool freedata)
{
	DIGGI_ASSERT(nd != nullptr);
	DIGGI_ASSERT(nd->ref > 0);
	if (--nd->ref == 0) {
		if (freedata) {
			if (nd->alloc) {
				/*If malloc backing, keep track of it*/
				free(nd->data);
			}
		}
		memset(nd, 0x0, sizeof(mbuf_node_t));
		free(nd);
		return 1;
	}
	return 0;
}
/**
 * @brief invoke destroy on mbuf or mbuf range.
 * Will traverse and attempt to destroy each node.
 * will only take effect if reference count for individual nodes reaches 0.
 * @param mb 
 * @param offset 
 * @param length 
 * @return size_t 
 */
size_t mbuf_destroy(mbuf_t *mb, size_t offset, size_t length)
{
	size_t retval = 0;
	if (length == 0) {
		free(mb);
		return 0;
	}
	DIGGI_ASSERT(mb->head);
	DIGGI_ASSERT(mb);

	unsigned base = 0;
	auto node = mbuf_node_at_pos(mb, offset, &base);
	auto count = 0u;
	while (node != nullptr && count < length) {
		count += node->size;
		auto tmp = node;
		//We rely on the invariant that for concatenated mbufs, all parts are deleted simoultanosly, 
		// meaning the individual parts arent referenced after one or the other have been deleted
		node = node->next;
		retval += mbuf_node_destroy(tmp, true);
	}

	memset(mb, 0x0, sizeof(mbuf_t));
	free(mb);
	return retval;
}