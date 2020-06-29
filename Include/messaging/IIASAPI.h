#ifndef IIASAPI_H
#define IIASAPI_H

#include "messaging/ias_ra.h"
#include "messaging/AttestationClient.h"
#include "messaging/AttestationServer.h"

/*
    Interface for Attestation Api
*/
class IIASAPI
{
public:
    IIASAPI() {}
    virtual ~IIASAPI() {}

    virtual int verify_attestation_evidence(const sgx_quote_t *p_isv_quote,
                                            uint8_t *pse_manifest,
                                            ias_att_report_t *p_attestation_verification_report) = 0;
    virtual int get_sigrl(const sample_epid_group_id_t gid,
                          uint32_t *p_sig_rl_size,
                          uint8_t **p_sig_rl) = 0;
    virtual async_cb_t get_client_response_attestation_flow() = 0;
    virtual async_cb_t get_client_initiator_attestation_flow() = 0;
    virtual async_cb_t get_server_attestation_flow() = 0;
    virtual bool attestable() = 0;
};

/* func attestation api */
class AttestationAPI : public IIASAPI
{
public:
    int verify_attestation_evidence(
        const sgx_quote_t *p_isv_quote,
        uint8_t *pse_manifest,
        ias_att_report_t *p_attestation_verification_report);
    int get_sigrl(
        const sample_epid_group_id_t gid,
        uint32_t *p_sig_rl_size,
        uint8_t **p_sig_rl);
    async_cb_t get_client_response_attestation_flow();
    async_cb_t get_client_initiator_attestation_flow();
    async_cb_t get_server_attestation_flow();
    bool attestable();
};

/* 
    Dummy api for skipping attestation
*/

class NoAttestationAPI : public IIASAPI
{
public:
    int verify_attestation_evidence(
        const sgx_quote_t *p_isv_quote,
        uint8_t *pse_manifest,
        ias_att_report_t *p_attestation_verification_report);
    int get_sigrl(
        const sample_epid_group_id_t gid,
        uint32_t *p_sig_rl_size,
        uint8_t **p_sig_rl);
    async_cb_t get_client_response_attestation_flow();
    async_cb_t get_client_initiator_attestation_flow();
    async_cb_t get_server_attestation_flow();
    bool attestable();
};

#endif
