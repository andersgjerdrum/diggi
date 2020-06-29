#ifndef ECALLS_H
#define ECALLS_H
#if  defined(DIGGI_ENCLAVE)

#include "posix/syscall_def.h"
#include "datatypes.h"
#include "threading/ThreadPool.h"
#include "messaging/SecureMessageManager.h"
#include "enclave.h"


void ecall_create_enclave_report(sgx_report_t *report, sgx_target_info_t *targetinfo);

void ecall_enclave_stop();

void ecall_new_thread();

#endif
#endif