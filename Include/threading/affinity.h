#ifndef AFFINITY_H
#define AFFINITY_H
#include <thread>
#include "datatypes.h"
#include "DiggiAssert.h"
#ifndef DIGGI_ENCLAVE
#include <sched.h>
#include <pthread.h>
#ifdef __cplusplus
extern "C" {
#endif

void reset_affinity();
std::thread new_thread_with_affinity(async_cb_t cb, void* ptr);
std::thread *new_thread_with_affinity_enc(async_cb_t cb, void* ptr);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

#endif