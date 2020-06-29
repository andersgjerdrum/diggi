/**
 * @file misc.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief implements miscellaneous helper functions for use throughout diggi.
 * @version 0.1
 * @date 2020-02-05
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "misc.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef DIGGI_ENCLAVE
/**
 * @brief check equallity of two buffers in constant time (for certain compilers)
 * 
 * @param b1 
 * @param b2 
 * @param len 
 * @return int 
 */
    int
    consttime_memequal(uint8_t *b1, uint8_t *b2, size_t len)
    {
        uint8_t *c1 = b1, *c2 = b2;
        unsigned int res = 0;

        while (len--)
            res |= *c1++ ^ *c2++;

        /*
	 * Map 0 to 1 and [1, 256) to 0 using only constant-time
	 * arithmetic.
	 *
	 * This is not simply `!res' because although many CPUs support
	 * branchless conditional moves and many compilers will take
	 * advantage of them, certain compilers generate branches on
	 * certain CPUs for `!res'.
	 */
        return (1 & ((res - 1) >> 8));
    }

#endif
/**
 * @brief memset implementation wrapper, used by glibc
 * 
 * @param s 
 * @param smax 
 * @param c 
 * @param n 
 * @return int 
 */
    int memset_s(void *s, size_t smax, int c, size_t n)
    {
        int err = 0;

        if (s == NULL)
        {
            err = EINVAL;
            goto out;
        }

        if (n > smax)
        {
            err = EOVERFLOW;
            n = smax;
        }

        /* Calling through a volatile pointer should never be optimised away. */
        (*__memset_vp)(s, c, n);

    out:
        if (err == 0)
            return 0;
        else
        {
            errno = err;
            /* XXX call runtime-constraint handler */
            return err;
        }
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */

/**
 * @brief get id from thread local variable. set by set_thr_id()
 * @return 
*/
int thr_id()
{
    return __thr_id;
}

/**
 * @brief Set the physical thread id into thread local memory
 * @param id 
 */
void set_thr_id(int id)
{
/*
		verify that thread has not been initialized before
		We make an excemption for unit tests that assing threadids multiple times.
	*/
#ifndef TEST_DEBUG
    if (__thr_id != -1)
    {
        printf("FATAL: Thread id allready set");
        DIGGI_ASSERT(false);
    }
#endif
    __thr_id = id;
}

/**
 * @brief allocate a global message buffer pool usable by all diggi instances.
 * allocated into lockfree queue, with a count expected consumer producer threads.
 * messages are freed back onto lockfree queue, so all consumers are also producers.
 * if buffer is empty, threads will block on request.
 * Consumed by AsyncMessageManager
 * @see AsyncMessageManager::AsyncMessageManager
 * @param threads expected producers and consumeres
 * @return lf_buffer_t* 
 */
lf_buffer_t *provision_memory_buffer(size_t threads, size_t pool_size, size_t object_size)
{
    auto retval = lf_new(pool_size, threads, threads);
    for (size_t count = 0; count < pool_size; count++)
    {
        lf_send(retval, calloc(1, object_size), 0);
    }
    return retval;
}
/**
 * @brief delete global message buffer pool.
 * @warning ensure no threads are producing or consuming from pool.
 * 
 * @param buf buffer to delete.
 */
void delete_memory_buffer(lf_buffer_t *buf, size_t pool_size)
{
    size_t next_thread_emulate = 0;
    for (size_t count = 0; count < pool_size; count++)
    {
        auto retval = lf_try_recieve(buf, next_thread_emulate);
        if (retval)
        {
            free(retval);
        }
        else
        {
            next_thread_emulate = (next_thread_emulate + 1) % buf->n_consumers_;
        }
    }
}

/**
 * @brief round up an unsigned number to a multiple of a given number
 * 
 * @param numToRound number to round
 * @param multiple multiple of this number
 * @return size_t new number
 */
size_t roundUp_r(size_t numToRound, size_t multiple)
{
    if (multiple == 0)
        return numToRound;

    int remainder = numToRound % multiple;
    if (remainder == 0)
        return numToRound;

    return numToRound + multiple - remainder;
}

///only used outside of enclave
#if !defined(DIGGI_ENCLAVE)

#ifdef __cplusplus
extern "C"
{
#endif

    static int initrand = 0;
    /**
     * @brief mock SGX SDK method for supporting sgx random operation in untursted memory(for debugging purposes)
     * 
     * @param randbf 
     * @param length_in_bytes 
     * @return sgx_status_t 
     */
    sgx_status_t sgx_read_rand(unsigned char *randbf, size_t length_in_bytes)
    {
        if (!randbf)
        {
            return SGX_ERROR_INVALID_PARAMETER;
        }
        if (!initrand)
        {
            srand((unsigned int)time(NULL));
            initrand = 1;
        }
        for (size_t i = 0; i < length_in_bytes; i++)
        {
            randbf[i] = rand();
        }

        return SGX_SUCCESS;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
