#include "messaging/AttestationServer.h"
#include "messaging/SecureMessageManager.h"
#include "messaging/Util.h"

/// dummy service name(ISV) used for msg2 in communicating with the Intel Attestation Service(IAS)
static sample_spid_t g_spid = {"Service X"};

/**
 * @brief Initiall callback, invoked in response to a attesation request by a diggi instance, hereby refered to as an attestation client.
 * Currently a no-op as we only simulate IAS interaction.
 * @param ptr msg_async_response_t (key_exchange_context_t, msg_t)
 * @param status unused parameter, error handling, future work
 */
void AttestationServer::dh_key_exchange_ra_msg0_enroll(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto kec = (key_exchange_context_t *)resp->context;
    DIGGI_ASSERT(kec);
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                "AttestationServer::dh_key_exchange_ra_msg0_enroll: recieved message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size);
    int ret = 0;
    /*
        TODO: Simulate enrollment
    */

    auto msg_n = kec->parent_manager->messageService->allocateMessage(resp->msg, sizeof(int));
    msg_n->delivery = resp->msg->delivery;

    msg_n->src = resp->msg->dest;
    msg_n->dest = resp->msg->src;
    *(int *)msg_n->data = ret;
    kec->parent_manager->messageService->sendMessageAsync(msg_n, AttestationServer::dh_key_exchange_ra_msg1_getsigir, kec);
}
/**
 * @brief Process the first message from attestation client, and request the signature revocation list from IAS.
 * Generate key agreement exponent for Diffie-Hellman, sign it using Eliptic curve 256, CMAC result message and send to attestaton client
 * 
 * @param ptr msg_async_response_t (key_exchange_context_t, msg_t)
 * @param status unused parameter, error handling, future work
 */
void AttestationServer::dh_key_exchange_ra_msg1_getsigir(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto kec = (key_exchange_context_t *)resp->context;
    DIGGI_ASSERT(kec);
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                "AttestationServer::dh_key_exchange_ra_msg1_getsigir: recieved message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size);
    int ret = 0;
    ra_samp_response_header_t *p_msg2_full = NULL;
    sgx_ra_msg2_t *p_msg2 = NULL;
    sgx_ecc_state_handle_t ecc_state = NULL;
    sgx_status_t sgx_ret = SGX_SUCCESS;
    bool derive_ret = false;
    sgx_ec256_public_t pub_key = {{0}, {0}};
    sgx_ec256_private_t priv_key = {{0}};
    sgx_ra_msg1_t *p_msg1 = (sgx_ra_msg1_t *)resp->msg->data;

    // Get the sig_rl from attestation server using GID.
    // GID is Base-16 encoded of EPID GID in little-endian format.
    // In the product, the SP and attesation server uses an established channel for
    // communication.
    uint8_t *sig_rl;
    uint32_t sig_rl_size = 0;

    // The product interface uses a REST based message to get the SigRL.
    DIGGI_ASSERT(kec->parent_manager->iasapi != nullptr);
    ret = kec->parent_manager->iasapi->get_sigrl(p_msg1->gid, &sig_rl_size, &sig_rl);
    if (0 != ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("\nError, ias_get_sigrl [%s].", __FUNCTION__);
        DIGGI_ASSERT(false);
    }

    // Need to save the client's public ECCDH key to local storage
    if (memcpy_s(&kec->g_sp_db.g_a, sizeof(kec->g_sp_db.g_a), &p_msg1->g_a,
                 sizeof(p_msg1->g_a)))
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, cannot do memcpy in [%s].\n", __FUNCTION__);
        DIGGI_ASSERT(false);
    }

    // Generate the Service providers ECCDH key pair.
    sgx_ret = sgx_ecc256_open_context(&ecc_state);
    if (SGX_SUCCESS != sgx_ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }
    sgx_ret = sgx_ecc256_create_key_pair(&priv_key, &pub_key,
                                         ecc_state);
    if (SGX_SUCCESS != sgx_ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    // Need to save the SP ECCDH key pair to local storage.
    if (memcpy_s(&kec->g_sp_db.b, sizeof(kec->g_sp_db.b), &priv_key, sizeof(priv_key)) || memcpy_s(&kec->g_sp_db.g_b, sizeof(kec->g_sp_db.g_b),
                                                                                                   &pub_key, sizeof(pub_key)))
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    // Generate the client/SP shared secret
    sample_ec_dh_shared_t dh_key = {{0}};
    sgx_ret = sgx_ecc256_compute_shared_dhkey(&priv_key,
                                              (sgx_ec256_public_t *)&p_msg1->g_a,
                                              (sgx_ec256_dh_shared_t *)&dh_key,
                                              ecc_state);
    if (SGX_SUCCESS != sgx_ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    // smk is only needed for msg2 generation.
    derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_SMK,
                            &kec->g_sp_db.smk_key);
    if (derive_ret != true)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    // The rest of the keys are the shared secrets for future communication.
    derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_MK,
                            &kec->g_sp_db.mk_key);
    if (derive_ret != true)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }
    // Mock key if not enclave
    if (kec->other_id.fields.type == ENCLAVE)
    {
        derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_SK,
                                &kec->g_sp_db.sk_key);
        if (derive_ret != true)
        {
            kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
            DIGGI_ASSERT(false);
        }
    }

    derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_VK,
                            &kec->g_sp_db.vk_key);
    if (derive_ret != true)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    uint32_t msg2_size = (uint32_t)sizeof(sgx_ra_msg2_t) + sig_rl_size;
    auto msg_n = kec->parent_manager->messageService->allocateMessage(resp->msg,
                                                                      msg2_size + sizeof(ra_samp_response_header_t));
    msg_n->delivery = resp->msg->delivery;

    msg_n->src = resp->msg->dest;
    msg_n->dest = resp->msg->src;
    msg_n->session_count = 0;
    p_msg2_full = (ra_samp_response_header_t *)msg_n->data;
    if (!p_msg2_full)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }
    memset(p_msg2_full, 0, msg2_size + sizeof(ra_samp_response_header_t));
    p_msg2_full->type = TYPE_RA_MSG2;
    p_msg2_full->size = msg2_size;
    // The simulated message2 always passes.  This would need to be set
    // accordingly in a real service provider implementation.
    p_msg2_full->status[0] = 0;
    p_msg2_full->status[1] = 0;
    p_msg2 = (sgx_ra_msg2_t *)p_msg2_full->body;

    // Assemble MSG2
    if (memcpy_s(&p_msg2->g_b, sizeof(p_msg2->g_b), &kec->g_sp_db.g_b,
                 sizeof(kec->g_sp_db.g_b)) ||
        memcpy_s(&p_msg2->spid, sizeof(sample_spid_t),
                 &g_spid, sizeof(g_spid)))
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    // The service provider is responsible for selecting the proper EPID
    // signature type and to understand the implications of the choice!
    p_msg2->quote_type = SAMPLE_QUOTE_LINKABLE_SIGNATURE;

    p_msg2->kdf_id = SAMPLE_AES_CMAC_KDF_ID;
    // Create gb_ga
    sample_ec_pub_t gb_ga[2];
    if (memcpy_s(&gb_ga[0], sizeof(gb_ga[0]), &kec->g_sp_db.g_b,
                 sizeof(kec->g_sp_db.g_b)) ||
        memcpy_s(&gb_ga[1], sizeof(gb_ga[1]), &kec->g_sp_db.g_a,
                 sizeof(kec->g_sp_db.g_a)))
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    // Sign gb_ga
    sgx_ret = sgx_ecdsa_sign((uint8_t *)&gb_ga, sizeof(gb_ga),
                             (sgx_ec256_private_t *)&g_sp_priv_key,
                             (sgx_ec256_signature_t *)&p_msg2->sign_gb_ga,
                             ecc_state);
    if (SGX_SUCCESS != sgx_ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    // Generate the CMACsmk for gb||SPID||TYPE||KDF_ID||Sigsp(gb,ga)
    uint8_t mac[SAMPLE_EC_MAC_SIZE] = {0};
    uint32_t cmac_size = offsetof(sgx_ra_msg2_t, mac);
    sgx_ret = sgx_rijndael128_cmac_msg(&kec->g_sp_db.smk_key,
                                       (uint8_t *)&p_msg2->g_b, cmac_size, &mac);
    if (SGX_SUCCESS != sgx_ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }
    if (memcpy_s(&p_msg2->mac, sizeof(p_msg2->mac), mac, sizeof(mac)))
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }

    if (memcpy_s(&p_msg2->sig_rl[0], sig_rl_size, sig_rl, sig_rl_size))
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("ERROR: dh_key_exchange_ra_sp_response_msg1_cb\n");
        DIGGI_ASSERT(false);
    }
    p_msg2->sig_rl_size = sig_rl_size;

    if (ecc_state)
    {
        sgx_ecc256_close_context(ecc_state);
    }
    // kec->parent_manager->diggiapi->GetLogObject()->Log(LRELEASE, "message 2 size = %lu\n", msg_n->size);

    // Utils::print_byte_array((void *)p_msg2, msg2_size);

    kec->parent_manager->messageService->sendMessageAsync(msg_n, AttestationServer::dh_key_exchange_ra_msg3, kec);
}

bool completedCoAttestation(key_exchange_context_t *kec);
char *packGroupKeys(const key_exchange_context_t *kec, size_t *ksize, bool non_enclave);

/**
 * @brief process message 3 from attestation client
 * recieves the client exponent anc computes the shared symetric secret.
 * Verify quote by asking IAS to verify, then compares mesurement against known good measurement, which verifies the initial state of attestation client.
 * attestation clients are organized in attestation-groups, which are jointly co-attested. 
 * Once the last expected participant in a attestation-group performs the attesaton process, 
 * the DH keys are distributed securely to all particpipants, 
 * along with addressing info, allowing authenticated communication within the group.
 */
void AttestationServer::dh_key_exchange_ra_msg3(void *ptr, int status)
{

    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto kec = (key_exchange_context_t *)resp->context;
    DIGGI_ASSERT(kec);
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                "AttestationServer::dh_key_exchange_ra_msg3: recieved message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size);
    int ret = 0;
    sgx_status_t sample_ret = SGX_SUCCESS;
    const uint8_t *p_msg3_cmaced = NULL;
    const sgx_quote_t *p_quote = NULL;
    sgx_sha_state_handle_t sha_handle = NULL;
    sgx_report_data_t report_data = {0};
    sgx_ra_msg3_t *p_msg3 = (sgx_ra_msg3_t *)resp->msg->data;
    uint32_t msg3_size = sizeof(sgx_ra_msg3_t);
    // Compare g_a in message 3 with local g_a.
    ret = memcmp(&kec->g_sp_db.g_a, &p_msg3->g_a, sizeof(sample_ec_pub_t));
    if (ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, g_a is not same [%s].", __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    //Make sure that msg3_size is bigger than sample_mac_t.
    uint32_t mac_size = msg3_size - (uint32_t)sizeof(sgx_mac_t);
    p_msg3_cmaced = reinterpret_cast<const uint8_t *>(p_msg3);
    p_msg3_cmaced += sizeof(sgx_mac_t);

    // Verify the message mac using SMK
    sgx_cmac_128bit_tag_t mac = {0};
    sample_ret = sgx_rijndael128_cmac_msg(&kec->g_sp_db.smk_key,
                                          p_msg3_cmaced,
                                          mac_size,
                                          &mac);
    if (SGX_SUCCESS != sample_ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, cmac fail in [%s].", __FUNCTION__);
        DIGGI_ASSERT(false);
    }
// In real implementation, should use a time safe version of memcmp here,
// in order to avoid side channel attack.
#ifdef DIGGI_ENCLAVE
    ret = memcmp(&p_msg3->mac, mac, sizeof(mac));
    if (ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, verify cmac fail [%s].", __FUNCTION__);
        DIGGI_ASSERT(false);
    }
#endif
    if (memcpy_s(&kec->g_sp_db.ps_sec_prop, sizeof(kec->g_sp_db.ps_sec_prop),
                 &p_msg3->ps_sec_prop, sizeof(p_msg3->ps_sec_prop)))
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, memcpy failed in [%s].", __FUNCTION__);
        DIGGI_ASSERT(false);
    }

    p_quote = (const sgx_quote_t *)p_msg3->quote;

    // Verify the report_data in the Quote matches the expected value.
    // The first 32 bytes of report_data are SHA256 HASH of {ga|gb|vk}.
    // The second 32 bytes of report_data are set to zero.
    sample_ret = sgx_sha256_init(&sha_handle);
    if (sample_ret != SGX_SUCCESS)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, init hash failed in [%s].", __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    sample_ret = sgx_sha256_update((uint8_t *)&(kec->g_sp_db.g_a),
                                   sizeof(kec->g_sp_db.g_a), sha_handle);
    if (sample_ret != SGX_SUCCESS)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, udpate hash failed in [%s].",
                                                           __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    sample_ret = sgx_sha256_update((uint8_t *)&(kec->g_sp_db.g_b),
                                   sizeof(kec->g_sp_db.g_b), sha_handle);
    if (sample_ret != SGX_SUCCESS)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, udpate hash failed in [%s].",
                                                           __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    sample_ret = sgx_sha256_update((uint8_t *)&(kec->g_sp_db.vk_key),
                                   sizeof(kec->g_sp_db.vk_key), sha_handle);
    if (sample_ret != SGX_SUCCESS)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, udpate hash failed in [%s].",
                                                           __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    sample_ret = sgx_sha256_get_hash(sha_handle,
                                     (sgx_sha256_hash_t *)&report_data);
    if (sample_ret != SGX_SUCCESS)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, Get hash failed in [%s].", __FUNCTION__);
        DIGGI_ASSERT(false);
    }
#ifdef DIGGI_ENCLAVE
    ret = memcmp((uint8_t *)&report_data,
                 &(p_quote->report_body.report_data),
                 sizeof(report_data));
    if (ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Error, verify hash fail [%s].", __FUNCTION__);
        DIGGI_ASSERT(false);
    }
#endif

    // Verify quote with attestation server.
    // In the product, an attestation server could use a REST message and JSON formatting to request
    ias_att_report_t attestation_report;
    memset(&attestation_report, 0, sizeof(attestation_report));
    DIGGI_ASSERT(kec->parent_manager->iasapi != nullptr);

    ret = kec->parent_manager->iasapi->verify_attestation_evidence(p_quote, NULL,
                                                                   &attestation_report);
    if ((IAS_QUOTE_OK == attestation_report.status) &&
        (IAS_PSE_OK == attestation_report.pse_status) && 0 != ret)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("error: verify_attestation_evidence\n");

        DIGGI_ASSERT(false);
    }

    if (kec->other_id.fields.att_group == 0)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Send empty ack to participant in attestation group 0\n");

        DIGGI_ASSERT(kec->other_id.fields.type == LIB);
        /*
            Delivering empty response to client so we skip waiting for ready participants, as it is not attestable.
        */

        auto msg_n = kec->parent_manager->messageService->allocateMessage(resp->msg, 0);
        msg_n->delivery = resp->msg->delivery;

        msg_n->src = resp->msg->dest;
        msg_n->dest = resp->msg->src;
        msg_n->session_count = 0;
        kec->attestation_initialized = 1;
        kec->initial = 0;
        kec->parent_manager->messageService->sendMessage(msg_n);
    }
    else
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log("Packing keys for clients.\n");

        kec->attestation_initialized = 1;
        kec->initial = 0;
        kec->outputqueue.push_back(new secure_message_context_t(COPY(msg_t, resp->msg, resp->msg->size), nullptr, nullptr, nullptr));
        if (completedCoAttestation(kec))
        {
            for (
                std::map<uint64_t, key_exchange_context_t>::const_iterator it = kec->parent_manager->callback_map.begin();
                it != kec->parent_manager->callback_map.end();
                it++)
            {
                DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                            "handling keys bound for: %" PRIu64 " this id is:%" PRIu64 "  \n", it->second.other_id.raw, it->second.self_id.raw);
                for (auto itm_msg : it->second.outputqueue)
                {
                    DIGGI_ASSERT(itm_msg);
                    size_t size = 0;
                    auto keys = packGroupKeys(&it->second, &size, itm_msg->item1->src.fields.type != ENCLAVE);
                    auto msg_n = kec->parent_manager->messageService->allocateMessage(itm_msg->item1,
                                                                                      size + sizeof(secure_message_t));
                    msg_n->delivery = itm_msg->item1->delivery;

                    msg_n->session_count = 0;
                    msg_n->src = itm_msg->item1->dest;
                    msg_n->dest = itm_msg->item1->src;
                    /*
                        Reset session state between trusted root and participant when new membership is announced
                    */
                    kec->parent_manager->callback_map[msg_n->dest.raw].session_id = 0;
                    kec->parent_manager->encrypt(&(kec->parent_manager->callback_map[msg_n->dest.raw]), (uint8_t *)keys, size, (secure_message_t *)msg_n->data);
                    free(keys);
                    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                                "Sending keys to: %" PRIu64 "\n", msg_n->dest.raw);

                    kec->parent_manager->messageService->sendMessage(msg_n);
                    kec->parent_manager->messageService->endAsync(msg_n);
                    free(itm_msg->item1);
                    delete itm_msg;
                }
                /*
                avoid compiler complaining about const.
            */
                ((key_exchange_context_t *)&it->second)->outputqueue.clear();
            }
        }
    }
}
/**
 * @brief counts completed participants in the attestaiton group associated with the kec 
 * Determine if attestation is complete for a given group.
 * 
 * @param kec context of client belongig to group
 * @return true 
 * @return false 
 */
bool completedCoAttestation(key_exchange_context_t *kec)
{
    int coattestation_participants = 0;
    for (
        std::map<std::string, aid_t>::const_iterator it = kec->parent_manager->name_servicemap.begin();
        it != kec->parent_manager->name_servicemap.end();
        it++)
    {
        /*
            Attestation group 0 should represent no attestation.
        */
        if (it->second.fields.att_group == kec->other_id.fields.att_group)
        {
            coattestation_participants++;
        }
        else
        {
            /*
                If not in this attestation group, 
                must be a untrusted lib system func.
            */
            DIGGI_ASSERT(it->second.fields.att_group == 0 && it->second.fields.type == LIB);
        }
    }
    int ready_participants = 0;
    for (
        std::map<uint64_t, key_exchange_context_t>::const_iterator it = kec->parent_manager->callback_map.begin();
        it != kec->parent_manager->callback_map.end();
        it++)
    {
        if ((it->second.other_id.fields.att_group == kec->other_id.fields.att_group) && it->second.attestation_initialized)
        {
            ready_participants++;
        }
    }
    DIGGI_ASSERT(ready_participants <= coattestation_participants);

    return (coattestation_participants == ready_participants);
}
/**
 * @brief packs keys belonging to a given attestation group as buffer to send.
 * 
 * @param kec context of attestation client for whitch to pack for.
 * @param keypacksize return: size of packed key buffer.
 * @return char* packed buffer.
 */
char *packGroupKeys(const key_exchange_context_t *kec, size_t *keypacksize, bool non_enclave)
{

    size_t items_to_pack = 0;

    for (
        std::map<uint64_t, key_exchange_context_t>::const_iterator it = kec->parent_manager->callback_map.begin();
        it != kec->parent_manager->callback_map.end();
        it++)
    {
        if (it->second.other_id.fields.att_group == kec->other_id.fields.att_group)
        {
            items_to_pack++;
        }
    }

    *keypacksize = items_to_pack * sizeof(key_exchange_context_t_packd);
    char *retval = (char *)malloc(*keypacksize);
    char *next_ptr = retval;
    for (
        std::map<uint64_t, key_exchange_context_t>::const_iterator it = kec->parent_manager->callback_map.begin();
        it != kec->parent_manager->callback_map.end();
        it++)
    {
        if (it->second.other_id.fields.att_group == kec->other_id.fields.att_group)
        {
            memcpy(next_ptr, &it->second, sizeof(key_exchange_context_t_packd));
            // Mock key if not enclave
            if (non_enclave)
            {
                memset(&(((key_exchange_context_t_packd *)next_ptr)->g_sp_db.sk_key), 0x0, sizeof(sgx_ec_key_128bit_t));
            }
            next_ptr += sizeof(key_exchange_context_t_packd);
        }
    }
    return retval;
}