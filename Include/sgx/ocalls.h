#ifndef OCALLS_H
#define OCALLS_H
#if  defined(UNTRUSTED_APP)

#include "sgx_urts.h"
#include "sgx_utils.h"
#include "sgx_uae_service.h"
#include "sgx_ukey_exchange.h"
#include "enclave_u.h"
#include <signal.h>
#include "DiggiAssert.h"
#include "threading/enclave_thread_entry.h"
#include "datatypes.h"
#include "misc.h"
#include "telemetry.h"
#include <inttypes.h>
#include <stdio.h>
#include "DiggiGlobal.h"
#include "messaging/network_ra.h"

void ocall_print_string_diggi(const char *name, const char *str, int threadid, uint64_t enc_id);

int ocall_print_string(const char *str);

void ocall_sleep(uint64_t usec);
void ocall_sig_assert(void);

void ocall_telemetry_capture(const char* tag);


#endif
#endif