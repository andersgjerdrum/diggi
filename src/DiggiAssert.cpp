#include "DiggiAssert.h"

/**
 * @brief resolved by define DIGGI_ASSERT, and directs assert handling.
 * Handled by STD assert if executing in untrusted diggi.
 * Leads to a ocall operation if executing inside an enclave.
 * @see ocalls.cpp
 * 
 * @param condition 
 * @param condition_text 
 * @param _caller_name 
 * @param line 
 * @param filename 
 */
void _enclave_assert(
	bool condition,
	const char *condition_text,
	const char *_caller_name,
	int line,
	const char * filename)
{
	if (!condition) {
		printf("%s: Assertion failure: %s line: %d thread: %d \nDIGGI_ASSERT(%s);\n", filename, _caller_name, line, thr_id(), condition_text);
#ifdef UNTRUSTED_APP
		printf("Assertion fired outside of enclave!\n");
		assert(condition);
#endif
#if defined(DIGGI_ENCLAVE)

		ocall_sig_assert();
#endif
		assert(condition);
	}
}
