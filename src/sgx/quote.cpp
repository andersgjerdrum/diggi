/**
 * @file quote.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief C api for retriving a quote for a particular enclave through the EREPORT instruction
 * @version 0.1
 * @date 2020-01-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "sgx/quote.h"
///exclusively for use in untrusted system
#ifdef UNTRUSTED_APP

#if defined(__cplusplus)
extern "C" {
#endif

/// Service Provider ID mocked for quote generation.
uint8_t spid[] =
{
	0x8e,0x64,0xde,0x5b,
	0x6f,0xdd,0xc1,0xf5,
	0xcc,0x0d,0xac,0x9e,
	0xbf,0xe8,0x53,0xe1
};


/**
 * @brief Get quote either linkable or non linkable for a particular enclave id.
 * 
 * 
 * @param linkable Is the enclave quote linkable or not
 * @param size the size of the resulting quote
 * @param enclaveid indentifier of enclave
 * @return sgx_quote_t* 
 */
sgx_quote_t* attestation_getquote(bool linkable, uint32_t* size, sgx_enclave_id_t enclaveid)
{

	sgx_status_t ret;

	sgx_spid_t *service_provider_id = (sgx_spid_t *)spid;
	sgx_target_info_t targetinfo = {};
	sgx_epid_group_id_t groupidentifier = {};
	sgx_report_t report = {};
	sgx_quote_t *quote;
	uint32_t quote_size;
	sgx_quote_sign_type_t quote_type = (linkable) ? SGX_LINKABLE_SIGNATURE : SGX_UNLINKABLE_SIGNATURE;
	int retval;

	ret = sgx_init_quote(&targetinfo, &groupidentifier);
	if (ret != SGX_SUCCESS) {
		printf("sgx_init_quote failed in %s with error code: 0x%x\n",
			__PRETTY_FUNCTION__,
			ret);

		return NULL;
	}

	ret = ecall_create_enclave_report(enclaveid, &report, &targetinfo);
	if (ret != SGX_SUCCESS) {
		printf("ecall_create_enclave_report failed in %s with error code: 0x%x\n",
			__PRETTY_FUNCTION__,
			ret);
		return NULL;
	}

	ret = sgx_calc_quote_size(NULL, 0, &quote_size);
	if (ret != SGX_SUCCESS) {
		printf("sgx_get_quote_size failed in %s with error code: 0x%x\n",
			__PRETTY_FUNCTION__,
			ret);
		return NULL;
	}

	quote = (sgx_quote_t*)calloc(1, (size_t)quote_size);
	if (quote == NULL)
	{
		printf("malloc failed in %s\n",
			__PRETTY_FUNCTION__);
		return NULL;
	}

	ret = sgx_get_quote(&report,
		quote_type,
		service_provider_id,
		NULL,
		NULL,
		0,
		NULL,
		quote,
		quote_size);

	if (ret != SGX_SUCCESS) {
		free(quote);
		printf("sgx_get_quote failed in %s with error code: 0x%x\n",
			__PRETTY_FUNCTION__,
			ret);
		return NULL;
	}

	*size = quote_size;
	return quote;
}



#if defined(__cplusplus)
}
#endif
#endif
