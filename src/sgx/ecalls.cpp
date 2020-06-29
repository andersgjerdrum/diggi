/**
 * @file ecalls.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief wrappers for all ecalls inbound to enclave.
 * @version 0.1
 * @date 2020-01-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */

///Only define wrapper functions if not test compile and not untrusted runtime and not untrusted instance
#if defined(DIGGI_ENCLAVE)

#include "sgx/ecalls.h"

/**
 * entry call to retrieve enclave report for local attestation.
 * 
 * @param report report outbound to calle
 * @param targetinfo target enclave for whom the report is destined.
 */
void ecall_create_enclave_report(sgx_report_t *report, sgx_target_info_t *targetinfo)
{
	sgx_status_t ret = sgx_create_report(targetinfo, NULL, report);
	DIGGI_ASSERT(ret == SGX_SUCCESS);
}
/**
 * @brief wrapper for client() enclave entry function
 * @see enclave.cpp
 * @param self 
 * @param id 
 * @param base_thread_id 
 * @param global_memory_buffer 
 * @param input_q 
 * @param output_q 
 * @param mapdata 
 * @param count 
 * @param func_name 
 * @param expected_threads 
 */
void ecall_enclave_entry_client(
	aid_t self,
    sgx_enclave_id_t id,
    size_t base_thread_id,
    void *global_memory_buffer,
	void *input_q,
	void *output_q,	
	void * mapdata,
	size_t count,
	const char* func_name,
	size_t expected_threads)
{
	client(self, id, base_thread_id, (lf_buffer_t*)global_memory_buffer, (lf_buffer_t*)input_q, (lf_buffer_t*)output_q,  (name_service_update_t*)mapdata, count, func_name, expected_threads);
}
/**
 * @brief wrapper for client_stop() in enclave.cpp
 * @see enclave.cpp
 */
void ecall_enclave_stop() {
	client_stop();
}

/**
 * @brief wrapper for enclave_thread_entry_function() in enclave.cpp
 * @see enclave.cpp
 */
void ecall_new_thread() {
	enclave_thread_entry_function();
	/*
		thread is lost by enclave if it reaches here
	*/
}
#endif
