#ifndef ATTESTATION_H
#define ATTESTATION_H
#ifdef UNTRUSTED_APP
#include "messaging/Util.h"

#if defined(__cplusplus)
extern "C" {
#endif

#include "sgx_urts.h"
#include "sgx_utils.h"
#include "sgx_uae_service.h"
#include <stdio.h>
#include <stdlib.h>
#include "enclave_u.h"
#include <stdint.h>

sgx_quote_t* attestation_getquote(bool linkable, uint32_t* size, sgx_enclave_id_t enclaveid);


#if defined(__cplusplus)
}
#endif
#endif
#endif