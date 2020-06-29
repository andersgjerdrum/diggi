/**
 * @file AttestationClient.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief Attestation client implementation for program authentication of client diggi instance against the trusted root.
 * @version 0.1
 * @date 2020-01-30
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "messaging/AttestationClient.h"
#include "messaging/SecureMessageManager.h"
#include "messaging/sample_messages.h"

/**
 * @brief final response callback flow response, signalling attestation completion.
 * In the case where an instance is configured to skip attestation, the intialization procedure callback after initial request skips to this function directly.
 * As a part of the multi step flow involved, pending outbound messages which should be delivered after successfull atteestation are stored.
 * This callback procedes to send each of theses messages as a final step of the completed attestation.
 * @warning gives no security properties if attestation is skipped
 * @param ptr msg_async_response_t
 * @param status 
 */
void AttestationClient::dh_key_exchange_report_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto kec = (key_exchange_context_t *)resp->context;
    DIGGI_ASSERT(kec);
    DIGGI_ASSERT(resp->msg);
    DIGGI_ASSERT(kec->parent_manager->this_thread == kec->parent_manager->diggiapi->GetThreadPool()->currentThreadId());

    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "AttestationClient::dh_key_exchange_report_cb:processing message 3 from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size);

    DIGGI_ASSERT(kec->parent_manager->diggiapi->GetThreadPool()->currentThreadId() == resp->msg->src.fields.thread);
    kec->parent_manager->messageService->endAsync(resp->msg);
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(), LDEBUG, "DH Key exchange done\n");
    for (auto ctx_item : kec->outputqueue)
    {
        DIGGI_ASSERT(ctx_item);
        DIGGI_ASSERT(kec->key_exchange_session_done != nullptr);
        DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                    LogLevel::LDEBUG,
                    "AttestationClient::dh_key_exchange_report_cb, retrying queue for dest=%lu, session_count=%lu\n", kec->other_id.raw, kec->session_id_outbound);
        kec->key_exchange_session_done(ctx_item, 1);
    }
    kec->outputqueue.clear();
    kec->initial = 0;
}
/**
 * @brief Response handler for instance request recipient in the event of attestaion not being configured.
 * @warning only used by instances which do not require attestation, provides no security guarantees.
 * Simply returns an empty acknowledgement to requestor instance
 * 
 * @param ptr 
 * @param status 
 */
void AttestationClient::dh_key_exchange_empty_ack(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto kec = (key_exchange_context_t *)resp->context;
    DIGGI_ASSERT(kec);
    DIGGI_ASSERT(resp->msg);
    DIGGI_ASSERT(kec->parent_manager);
    DIGGI_ASSERT(kec->parent_manager->this_thread == kec->parent_manager->diggiapi->GetThreadPool()->currentThreadId());

    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "AttestationClient::dh_key_exchange_empty_ack responding from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size);
    DIGGI_ASSERT(kec->parent_manager->diggiapi->GetThreadPool()->currentThreadId() == resp->msg->src.fields.thread);

    DIGGI_ASSERT(resp->msg->session_count == kec->session_id_inbound);

    auto msg_n = kec->parent_manager->messageService->allocateMessage(resp->msg, 0);
    msg_n->session_count = 0;

    msg_n->delivery = resp->msg->delivery;
    /*Empty ACK*/
    msg_n->src = kec->self_id;
    msg_n->dest = resp->msg->src;
    DIGGI_ASSERT(msg_n != resp->msg);
    kec->parent_manager->messageService->sendMessage(msg_n);
}
/**
 * @brief either instance client or server, bootstraps an attestation procedure to verify each other.
 * Client requests are paused and redirect communciation to the trusted_root before  requests to server may proceed.
 * Servers requests attestation after attempting to register a typed callback.
 * Each request an attestation from the trusted root.
 * This callback is invoked as a response to the initial request,  processing msg0 and intitializing the RA protocol.
 * The callback prepares msg1 via an ocall to the untrusted SGX SDK and sends it to the trusted root via the AMM.
 * 
 * @param ptr 
 * @param status 
 */
void AttestationClient::dh_key_exchange_ra_response_msg0_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto kec = (key_exchange_context_t *)resp->context;
    DIGGI_ASSERT(kec);
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                "AttestationClient::dh_key_exchange_ra_response_msg0_cb: recieved message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size);

    kec->ra_context = INT_MAX;
    auto msg_n = kec->parent_manager->messageService->allocateMessage(resp->msg, sizeof(sgx_ra_msg1_t));
    msg_n->session_count = 0;
    msg_n->src = resp->msg->dest;
    msg_n->dest = resp->msg->src;

    /*
    define here to allow compile and code deduplication.
 */

#ifdef DIGGI_ENCLAVE
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    DIGGI_ASSERT(kec->self_id.fields.type == ENCLAVE);
    ret = sgx_ra_init(&g_sp_pub_key, false, &kec->ra_context);
    DIGGI_ASSERT(kec->ra_context < INT_MAX);
    if (ret != SGX_SUCCESS)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log(LDEBUG, "ERROR, dh_key_exchange_ra_response_msg0_cb ra_init failed\n");
        DIGGI_ASSERT(false);
    }
    ocall_prepare_msg1(msg_n->data, GET_DIGGI_GLOBAL_CONTEXT()->GetEnclaveId(), kec->ra_context);

#else

    memcpy_s(msg_n->data, sizeof(sgx_ra_msg1_t),
             msg1_sample1,
             sizeof(sgx_ra_msg1_t));
#endif

    kec->parent_manager->messageService->sendMessageAsync(msg_n, AttestationClient::dh_key_exchange_ra_response_msg1_cb, kec);
}
/**
 * @brief callback triggerd upon the response from sending msg1 to the trusted root.
 * Prepares msg3 and sends to trusted root.
 * 
 * @param ptr 
 * @param status 
 */
void AttestationClient::dh_key_exchange_ra_response_msg1_cb(void *ptr, int status)
{

    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto kec = (key_exchange_context_t *)resp->context;
    DIGGI_ASSERT(kec);
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(), LDEBUG,
                "AttestationClient::dh_key_exchange_ra_response_msg1_cb: recieved message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size);
    ra_samp_response_header_t *p_msg2_full = (ra_samp_response_header_t *)resp->msg->data;
    DIGGI_ASSERT(resp->msg->size >= sizeof(ra_samp_response_header_t) + sizeof(msg_t));
    sgx_ra_msg3_t *p_msg3 = NULL;

    DIGGI_ASSERT(p_msg2_full->size > 0);

    if (TYPE_RA_MSG2 != p_msg2_full->type)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log(LRELEASE, "Error, didn't get MSG2 in response to MSG1. "
                                                                     "[%s].",
                                                           __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    uint32_t msg3_size = MSG3_BODY_SIZE;

    /*
        We rely on p_msg3 comming from untrusted memory
    */
/*
    define here to allow compile and code deduplication.
 */
#ifdef DIGGI_ENCLAVE
    sgx_ra_msg2_t *p_msg2_body = (sgx_ra_msg2_t *)p_msg2_full->body;
    DIGGI_ASSERT(p_msg2_full->size == resp->msg->size - (sizeof(ra_samp_response_header_t) + sizeof(msg_t)));
    ocall_prepare_msg2(&p_msg3,
                       kec->ra_context,
                       GET_DIGGI_GLOBAL_CONTEXT()->GetEnclaveId(),
                       p_msg2_body,
                       p_msg2_full->size);
    ;
    if (sgx_ra_get_keys(kec->ra_context, SGX_RA_KEY_SK, &kec->g_sp_db.sk_key) != SGX_SUCCESS)
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log(LRELEASE, "Error: INTERNAL ERROR - ra_get_keys failed in [%s].",
                                                           __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    DIGGI_ASSERT(kec->self_id.fields.type == ENCLAVE);
#else
    /*
        Mock message
    */
    p_msg3 = (sgx_ra_msg3_t *)msg3_sample1;
    msg3_size = MSG3_BODY_SIZE;

#endif
    DIGGI_ASSERT(msg3_size > 0);
    auto msg_n = kec->parent_manager->messageService->allocateMessage(resp->msg, msg3_size);
    msg_n->session_count = 0;
    msg_n->delivery = resp->msg->delivery;

    msg_n->src = resp->msg->dest;
    msg_n->dest = resp->msg->src;

    if (memcpy_s(msg_n->data, msg3_size, p_msg3, msg3_size))
    {
        kec->parent_manager->diggiapi->GetLogObject()->Log(LRELEASE, "Error: INTERNAL ERROR - memcpy failed in [%s].",
                                                           __FUNCTION__);
        DIGGI_ASSERT(false);
    }

    // #ifdef DIGGI_ENCLAVE
    // free(p_msg3);
    // #endif

    kec->parent_manager->messageService->sendMessageAsync(msg_n, AttestationClient::dh_key_exchange_ra_response_final_cb, kec);
}

/**
 * @brief callback triggered as a result of trusted root processing msg3
 * Final callback recieieving symetric keys for attestation group.
 * The attestation group specifies which other attested participants this instance may authentically and securely communicate with.
 * Finally AttestationClient::dh_key_exchange_report_cb is called to send previously prepared messages.
 * Once the symetric keys are recieved, all participants in the attestation group are concidered trusted.
 * @param ptr 
 * @param status 
 */
void AttestationClient::dh_key_exchange_ra_response_final_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto kec = (key_exchange_context_t *)resp->context;
    DIGGI_ASSERT(kec);
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                "AttestationClient::dh_key_exchange_ra_response_final_cb: recieved message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                resp->msg->src.raw,
                resp->msg->dest.raw,
                resp->msg->id,
                resp->msg->size);

    if (resp->msg->size != sizeof(msg_t))
    {
        size_t count = (resp->msg->size - (sizeof(secure_message_t) + sizeof(msg_t))) / sizeof(key_exchange_context_t_packd);
        key_exchange_context_t_packd *keys = (key_exchange_context_t_packd *)malloc(sizeof(key_exchange_context_t_packd) * count);

        size_t decrypt_size;
        kec->parent_manager->decrypt((secure_message_t *)resp->msg->data, kec, (char *)keys, &decrypt_size);
        auto attestation_group = keys[0].other_id.fields.att_group;
        for (size_t i = 0; i < count; i++)
        {
            DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(), LRELEASE,
                        "Preparing session keys for: %" PRIu64 ".\n",
                        keys[i].other_id.raw);

            DIGGI_ASSERT(attestation_group == keys[i].other_id.fields.att_group);
            if (keys[i].other_id.fields.type == LIB)
            {
                kec->parent_manager->callback_map[keys[i].other_id.raw].g_sp_db = {0};
            }
            else
            {
                kec->parent_manager->callback_map[keys[i].other_id.raw].g_sp_db = keys[0].g_sp_db; //allways chose the first for sessions
            }
            // Utils::print_byte_array(&kec->parent_manager->callback_map[keys[i].other_id.raw].g_sp_db.sk_key, sizeof(sgx_ec_key_128bit_t));

            kec->parent_manager->callback_map[keys[i].other_id.raw].self_id = kec->self_id;
            kec->parent_manager->callback_map[keys[i].other_id.raw].other_id = keys[i].other_id;
            kec->parent_manager->callback_map[keys[i].other_id.raw].key_exchange_session_done =
                (async_cb_t)SecureMessageManager::SendMessageAsyncInternal;
            kec->parent_manager->callback_map[keys[i].other_id.raw].parent_manager = kec->parent_manager;
            kec->parent_manager->callback_map[keys[i].other_id.raw].session_id = 0;
            // kec->parent_manager->callback_map[keys[i].other_id.raw].session_id_outbound = 0;
            // kec->parent_manager->callback_map[keys[i].other_id.raw].session_id_inbound = 0;
            kec->parent_manager->callback_map[keys[i].other_id.raw].initial = 0;
            kec->parent_manager->callback_map[keys[i].other_id.raw].attestation_initialized = 1;
        }
        free(keys);
    }
    AttestationClient::dh_key_exchange_report_cb(ptr, status);
}