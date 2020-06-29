/**
 * @file mmStubs.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief stubs for memory mapping posix calls.
 * These are not implemented, as SGXv1 does not support dynamic memory.
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "DiggiAssert.h"
#include "posix/io_types.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief mmap call not implemented, will throw assertion failure on use
 * 
 * @param addr 
 * @param len 
 * @param prot 
 * @param flags 
 * @param fildes 
 * @param off 
 * @return void* 
 */
	void *i_mmap(void *addr, size_t len, int prot, int flags,
		int fildes, off_t off) {
		DIGGI_ASSERT(false);
		return NULL;
	}
    /**
     * @brief munmap call not implemented, will throw assertion failure on use
     * 
     * @param __addr 
     * @param __len 
     * @return int 
     */
	int i_munmap(void *__addr, size_t __len) {
		DIGGI_ASSERT(false);
		return 0;

	}
    /**
     * @brief mremap call not implemented, will throw assertion failure on use
     * 
     * @param __addr 
     * @param __old_len 
     * @param __new_len 
     * @param __flags 
     * @param ... 
     * @return void* 
     */
	void *i_mremap(void *__addr, size_t __old_len, size_t __new_len,
		int __flags, ...) {
		DIGGI_ASSERT(false);
		return NULL;
	}
#ifdef __cplusplus
}
#endif /* __cplusplus */