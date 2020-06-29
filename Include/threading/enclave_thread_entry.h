#ifndef U_ASYNC_SCHEDULER_H
#define U_ASYNC_SCHEDULER_H
#if  defined(UNTRUSTED_APP)
#include <thread>
#include <vector>
#include "messaging/AsyncMessageManager.h"
#include "DiggiAssert.h"
#include "enclave_u.h"
#include "Logging.h"



void enclave_thread_entry(void *ptr, int status);


#endif
#endif