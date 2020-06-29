/**
 * @file affinty.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief Implementation of assigning cores to diggi instances, assigned in round robin.
 * Enclave threads are assigned from upper core count and downward, while non enclave instances are assigned from core 1 and upward.
 * Core 0 is dedicated to untrusted runtime package scheduling.
 * @version 0.1
 * @date 2020-02-04
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "threading/affinity.h"

#ifndef DIGGI_ENCLAVE

#ifdef __cplusplus
extern "C"
{
#endif

    /// next core to assing to  diggi instance
    static volatile size_t next_core = 0;
    /// next enclave core to assing
    static size_t next_enclave_core = 1;
    void reset_affinity()
    {
        next_core = 0;
        next_enclave_core = 1;
    }

    /**
 * @brief allocate thread and affinitize to next availible core. used for non-enclave diggi instances.
 * Begins at logical core 1 going uppwards
 * @param cb callback executing new thread.
 * @param ptr context pointer for sending thread initial data.
 * @return std::thread return thread object (for accounting, join etc.).
 */
    std::thread new_thread_with_affinity(async_cb_t cb, void *ptr)
    {

        auto thrd = std::thread(cb, ptr, 1);

#ifndef TEST_DEBUG /*test code does not need affinitization*/
        size_t num_cpus = std::thread::hardware_concurrency();

        if (next_enclave_core == num_cpus)
        {
            next_enclave_core = 1;
        }

        num_cpus = (num_cpus - next_enclave_core);

        // DIGGI_ASSERT(num_cpus);
        size_t local_id = next_core;
        next_core = (next_core + 1) % num_cpus;

        DIGGI_ASSERT(local_id < num_cpus);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        printf("setting thread to core%lu\n", local_id % num_cpus);
        CPU_SET(local_id % num_cpus, &cpuset);
        int rc = pthread_setaffinity_np(thrd.native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
        DIGGI_ASSERT(rc == 0);
#endif
        return thrd;
    }

    /**
 * @brief Allocate thread and affintize to next availible core. used for enclave diggi instance
 * Begins at upper bound core going downward.
 * 
 * 
 * @param cb callback executing new thread
 * @param ptr context pointer for sending thread initial data
 * @return std::thread* return thread reference (for accounting, join etc.)
 */
    std::thread *new_thread_with_affinity_enc(async_cb_t cb, void *ptr)
    {
        size_t num_cpus = std::thread::hardware_concurrency();
        if (next_enclave_core == num_cpus)
        {
            next_enclave_core = 1;
        }
        size_t local_id = num_cpus - next_enclave_core;

        next_enclave_core++;
        // DIGGI_ASSERT(local_id <= num_cpus);

        auto thrd = new std::thread(cb, ptr, 1);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        printf("setting secure thread to core%lu\n", local_id % num_cpus);
        CPU_SET(local_id % num_cpus, &cpuset);
        int rc = pthread_setaffinity_np(thrd->native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
        DIGGI_ASSERT(rc == 0);
        return thrd;
    }

#ifdef __cplusplus
}
#endif // __cplusplus

#endif