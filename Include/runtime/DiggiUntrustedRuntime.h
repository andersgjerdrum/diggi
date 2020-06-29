#ifndef funcRUNTIME_H
#define funcRUNTIME_H
#ifdef UNTRUSTED_APP

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ZeroCopyString.h"
#include <unistd.h>
#include <pwd.h>
#include <fstream>
#include <streambuf>
#include <string>
#include "sgx_urts.h"
#include "enclave_u.h"
#include "messaging/Util.h"
#include "JSONParser.h"
#include "datatypes.h"
#include "DiggiAssert.h"
#include "threading/enclave_thread_entry.h"
#include "threading/ThreadPool.h"
#include "storage/StorageManager.h"
#include "sgx_quote.h"
#include "sgx/quote.h"
#include <dlfcn.h>
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "messaging/AsyncMessageManager.h"
#include <thread>
#include "telemetry.h"
#include "network/Connection.h"
#include "network/HTTP.h"
#include "ext_com.h"
#include "messaging/ThreadSafeMessageManager.h"
#include "messaging/SecureMessageManager.h"
#include "messaging/IIASAPI.h"
#define MAX_PATH FILENAME_MAX
/*4GB*/

typedef struct func_management_context_t
{
    DiggiAPI *acontext;
    sgx_enclave_id_t enc_id;
    aid_t id;
    bool enclave;
    std::string name;
    std::vector<std::thread *> enclave_thread;
    lf_buffer_t *input_queue;
    lf_buffer_t *output_queue;
    async_cb_t start_func;
    async_cb_t init_func;
    async_cb_t stop_func;
    json_node configuration;

} func_management_context_t;

void shutdown_funcs();

void shutdown_func(func_management_context_t ctx);

void runtime_init(void *ptr, int status);

void runtime_start(void *ptr, int status, bool exit_when_done);

void runtime_recieve(void *ptr, int status);

void runtime_stop(void *ptr, int status);

#endif
#endif