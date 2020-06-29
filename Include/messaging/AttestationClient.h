/**
 * @file AttestationClient.h
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief header file for defininf message callback flow for instances attesting themselves to the trusted root.
 * @version 0.1
 * @date 2020-01-30
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#ifndef ATTESTATION_CLIENT_H
#define ATTESTATION_CLIENT_H
#include "misc.h"
#include "datatypes.h"
#include "DiggiAssert.h"
#include "DiggiGlobal.h"
#include "messaging/network_ra.h"

class AttestationClient
{

public:
    static void dh_key_exchange_report_cb(void *ptr, int status);
    static void dh_key_exchange_empty_ack(void *ptr, int status);
    static void dh_key_exchange_ra_sp_response_msg1_cb(void *ptr, int status);
    static void dh_key_exchange_ra_response_msg0_cb(void *ptr, int status);
    static void dh_key_exchange_ra_response_msg1_cb(void *ptr, int status);
    static void dh_key_exchange_ra_response_final_cb(void *ptr, int status);
};

#endif