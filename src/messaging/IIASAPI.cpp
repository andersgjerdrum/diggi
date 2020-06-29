/**
 * @file IIASAPI.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief Implementations of the IAttestationApi interface
 * @version 0.1
 * @date 2020-01-31
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "messaging/IIASAPI.h"

/**
 * @brief wrapper to send attestation verification request to the Intel Attestation Service
 * 
 * @param p_isv_quote quote data
 * @param pse_manifest Platform services manifest
 * @param p_attestation_verification_report report response from IAS
 * @return int returns errorcodde SGX_SUCCESS(0) if ok.
 */
int AttestationAPI::verify_attestation_evidence(
    const sgx_quote_t *p_isv_quote,
    uint8_t *pse_manifest,
    ias_att_report_t *p_attestation_verification_report)
{
    return 0;
}
/**
 * @brief retrieve signature revocation list from ISA
 * 
 * @param gid Intel Enhanched Protection Identity group id
 * @param p_sig_rl_size signature list size returned
 * @param p_sig_rl signature list buffer returned
 * @return int returns errorcodde SGX_SUCCESS(0) if ok.
 */
int AttestationAPI::get_sigrl(
    const sample_epid_group_id_t gid,
    uint32_t *p_sig_rl_size,
    uint8_t **p_sig_rl)
{
    return 0;
}
/**
 * @brief start attestation flow for trusted root, given in response to a request from a attestation client
 * Invoked by
 * @return async_cb_t 
 */
async_cb_t AttestationAPI::get_server_attestation_flow()
{
    return AttestationServer::dh_key_exchange_ra_msg0_enroll;
}
/**
 * @brief start a server attestation flow interfacing with the trusted root.
 * @return async_cb_t 
 */
async_cb_t AttestationAPI::get_client_response_attestation_flow()
{
    return AttestationClient::dh_key_exchange_ra_response_msg0_cb;
}
/**
 * @brief start a client side attestation flow against the trusted root before proceding with request.
 * also invoked for serves via RegisterTypedCallback to ensure passive servers are also attested.
 * @return async_cb_t 
 */
async_cb_t AttestationAPI::get_client_initiator_attestation_flow()
{
    return AttestationClient::dh_key_exchange_ra_response_msg0_cb;
}
/**
 * @brief check if Attestation api allows the current instance to be attested
 * if false, the communciation channel provides no security guarantees. 
 * This implementation supports attestation and will allways return true.
 * Only relevant for system instance communication with StorageServer, NetworkManager and untrusted runtime.
 * @return true 
 * @return false 
 */
bool AttestationAPI::attestable()
{
    return true;
}
/**
 * @brief noop for debug or non-attested communication
 * Should not be invoked if non-atested com, will cause assertion failiure
 * @param p_isv_quote 
 * @param pse_manifest 
 * @param p_attestation_verification_report 
 * @return int 
 */
int NoAttestationAPI::verify_attestation_evidence(
    const sgx_quote_t *p_isv_quote,
    uint8_t *pse_manifest,
    ias_att_report_t *p_attestation_verification_report)
{
    /*
        Should not happen
    */

    DIGGI_ASSERT(false);
    return -1;
}

/**
 * @brief noop for debug or non-attested communication
 * Should not be invoked if non-atested com, will cause assertion failiure
 * @param gid 
 * @param p_sig_rl_size 
 * @param p_sig_rl 
 * @return int 
 */
int NoAttestationAPI::get_sigrl(
    const sample_epid_group_id_t gid,
    uint32_t *p_sig_rl_size,
    uint8_t **p_sig_rl)
{
    /*
        Should not happen
    */

    DIGGI_ASSERT(false);
    return -1;
}

/**
 * @brief noop for debug or non-attested communication
 * Should not be invoked if non-atested com, will cause assertion failiure
 * @return async_cb_t 
 */
async_cb_t NoAttestationAPI::get_server_attestation_flow()
{
    DIGGI_ASSERT(false);
    return NULL;
}
/**
 * @brief flow for noop attestation, for non-attested communication, such as StorageServer, NetworkServer, etc.
 * 
 * @return async_cb_t 
 */
async_cb_t NoAttestationAPI::get_client_response_attestation_flow()
{
    return AttestationClient::dh_key_exchange_empty_ack;
}

/**
 * @brief flow for noop attestation, for non-attested communication, such as StorageServer, NetworkServer, etc.
 * 
 * @return async_cb_t 
 */
async_cb_t NoAttestationAPI::get_client_initiator_attestation_flow()
{
    return AttestationClient::dh_key_exchange_report_cb;
}
/**
 * @brief check if api allows attestation.
 * Hint: this implementation(NoAttesation) does not and will allways return false
 * 
 * @return true 
 * @return false 
 */
bool NoAttestationAPI::attestable(){
    return false;
}
