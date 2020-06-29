#if  defined(UNTRUSTED_APP)

#include "threading/enclave_thread_entry.h"
/**
 * @brief entry point for threads allocated to a particluar enclave.
 * use async_cb_t format of callback.
 * After an enclave instance is destroyed, the threads return out of the enclave at this point in the code.
 * If enclave is destroyed before all threads have exited this function recieves SGX_ERROR_ENCLAVE_LOST.
 * @param ptr pointer encoded enclave identifier
 * @param status unused parameter, error handling, future work
 */
void enclave_thread_entry(void *ptr, int status) {
	auto id = (uint64_t*)ptr;
	auto sts = ecall_new_thread(*id);
	delete id;
	if (sts == SGX_ERROR_ENCLAVE_LOST) {
		/*
			Reconfiguration of funcs causes sgx_enclave_destroy() to be called
			and therefore causes threads to exit enclave with SGX_ERROR_ENCLAVE_LOST status code
		*/
		return;
	}
	DIGGI_ASSERT(sts == SGX_SUCCESS);
}

#endif
