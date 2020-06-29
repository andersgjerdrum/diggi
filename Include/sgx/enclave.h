#ifndef ENCLAVE_H
#define ENCLAVE_H
#ifdef DIGGI_ENCLAVE
#include "posix/syscall_def.h"
#include "posix/stdio_stubs.h"
#include "messaging/SecureMessageManager.h"
#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include "enclave.h"
#include "JSONParser.h"
#include "network/HTTP.h"
#include "network/Connection.h"
#include "network/Connection.h"
#include <string>
#include "messaging/Util.h"
#include "sgx/ecalls.h"
#include "runtime/func.h"
#include "Logging.h"
#include "threading/ThreadPool.h"
#include "storage/StorageManager.h"
#include "DiggiGlobal.h"
#include "messaging/ThreadSafeMessageManager.h"
#include "runtime/attested_config.h"

void enclave_thread_entry_function();
void client(aid_t self,
            sgx_enclave_id_t id,
            size_t base_thread,
            lf_buffer_t *global_memory_buffer,
			lf_buffer_t *input_q,
			lf_buffer_t *output_q,
			name_service_update_t * mapdata,
			size_t count,
			const char* func_name,
			size_t expected_threads);
void client_stop();

#endif
#endif