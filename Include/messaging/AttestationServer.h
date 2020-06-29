#ifndef ATTESTATION_SERVER_H
#define ATTESTATION_SERVER_H
#include "messaging/service_provider.h"
#include <algorithm>

class AttestationServer
{
public:
    static void dh_key_exchange_ra_msg0_enroll(void *ptr, int status);
    static void dh_key_exchange_ra_msg1_getsigir(void *ptr, int status);
    static void dh_key_exchange_ra_msg3(void *ptr, int status);
};

#endif