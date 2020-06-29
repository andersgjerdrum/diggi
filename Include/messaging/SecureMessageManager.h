#ifndef SECUREMESSAGEMANAGER_H
#define SECUREMESSAGEMANAGER_H
/**
 * @file securemessagemanager.h
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief header file containing deffinitions for Secure message manager and accosiated datastructures.
 * @version 0.1
 * @date 2020-01-31
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "sgx_dh.h"
#include "sgx_trts.h"
#include "sgx_utils.h"
#include "sgx_eid.h"
#include "sgx_ecp_types.h"
#include "sgx_thread.h"
#include "sgx_dh.h"
#include "sgx_tcrypto.h"
#include "sgx_tkey_exchange.h"
#include "messaging/ecp.h"
#include <map>
#include <cassert>
#include <string.h>
#include "messaging/AsyncMessageManager.h"
#include "sgx/DynamicEnclaveMeasurement.h"
#include "messaging/network_ra.h"
#include "messaging/IIASAPI.h"
#include "messaging/service_provider.h"
#include "DiggiAssert.h"
#include "AttestationClient.h"
#include <memory>
#include "AsyncContext.h"
#include "datatypes.h"
#include "messaging/IMessageManager.h"
#include <inttypes.h>
#include "telemetry.h"
#include "Logging.h"
#include "runtime/DiggiAPI.h"
#include "messaging/Util.h"
#include "misc.h"
#include "storage/TamperProofLog.h"

//
//

/**
 * This is the private EC key of SP, the corresponding public EC key is hard coded in isv_enclave. It is based on NIST P-256 curve.
 */
static const sgx_ec256_private_t g_sp_priv_key = {
    {0x90, 0xe7, 0x6c, 0xbb, 0x2d, 0x52, 0xa1, 0xce,
     0x3b, 0x66, 0xde, 0x11, 0x43, 0x9c, 0x87, 0xec,
     0x1f, 0x86, 0x6a, 0x3b, 0x65, 0xb6, 0xae, 0xea,
     0xad, 0x57, 0x34, 0x53, 0xd1, 0x03, 0x8c, 0x01}};

/**
 * This is the public EC key of SP, this key is hard coded in isv_enclave.
 * It is based on NIST P-256 curve. Not used in the SP code.
 */
static const sgx_ec256_public_t g_sp_pub_key = {
    {0x72, 0x12, 0x8a, 0x7a, 0x17, 0x52, 0x6e, 0xbf,
     0x85, 0xd0, 0x3a, 0x62, 0x37, 0x30, 0xae, 0xad,
     0x3e, 0x3d, 0xaa, 0xee, 0x9c, 0x60, 0x73, 0x1d,
     0xb0, 0x5b, 0xe8, 0x62, 0x1c, 0x4b, 0xeb, 0x38},
    {0xd4, 0x81, 0x40, 0xd9, 0x50, 0xe2, 0x57, 0x7b,
     0x26, 0xee, 0xb7, 0x41, 0xe7, 0xc6, 0x14, 0xe2,
     0x24, 0xb7, 0xbd, 0xc9, 0x03, 0xf2, 0x9a, 0x28,
     0xa8, 0x3c, 0xc8, 0x10, 0x11, 0x14, 0x5e, 0x06}};

/**
 * @brief debug implementation of the cryptographic primitives used in SMM.
 * Used by unit and acceptance testing where actual crypto is not needed.
 * @warning provides no security guarantees!!
 * Simply does a copy operation from the input buffer to the output buffer.
 * some methods are deprecated, as we do not implement local attestation any more.
 */
class DebugCrypto : public ICryptoImplementation
{
public:
    sgx_status_t encrypt(
        const sgx_aes_gcm_128bit_key_t *p_key,
        const uint8_t *p_src,
        uint32_t src_len,
        uint8_t *p_dst,
        const uint8_t *p_iv,
        uint32_t iv_len,
        const uint8_t *p_aad,
        uint32_t aad_len,
        sgx_aes_gcm_128bit_tag_t *p_out_mac)
    {
        memcpy(p_dst, p_src, src_len);
        return SGX_SUCCESS;
    }

    sgx_status_t decrypt(
        const sgx_aes_gcm_128bit_key_t *p_key,
        const uint8_t *p_src,
        uint32_t src_len,
        uint8_t *p_dst,
        const uint8_t *p_iv,
        uint32_t iv_len,
        const uint8_t *p_aad,
        uint32_t aad_len,
        const sgx_aes_gcm_128bit_tag_t *p_in_mac)
    {
        memcpy(p_dst, p_src, src_len);
        return SGX_SUCCESS;
    }
};

#ifndef TEST_DEBUG
/**
 * @brief Implementation of crypto api interface for use in Secure Message Manager
 * A secure implementation which interfaces with the SGX SDK api for encryption/decryption.
 * Some deprecated message preparation methods for local attestaion, not used, we do not support localc attestation anymore.
 * Uses AES-128 GCM for encryption.
 * Only used in enclaves.
 */
class SGXCryptoImpl : public ICryptoImplementation
{
    // Inherited via ICryptoImplementation
public:
    sgx_status_t encrypt(
        const sgx_aes_gcm_128bit_key_t *p_key,
        const uint8_t *p_src,
        uint32_t src_len,
        uint8_t *p_dst,
        const uint8_t *p_iv,
        uint32_t iv_len,
        const uint8_t *p_aad,
        uint32_t aad_len,
        sgx_aes_gcm_128bit_tag_t *p_out_mac)
    {
        return sgx_rijndael128GCM_encrypt(
            p_key,
            p_src,
            src_len,
            p_dst,
            p_iv,
            iv_len,
            p_aad,
            aad_len,
            p_out_mac);
    }

    sgx_status_t decrypt(
        const sgx_aes_gcm_128bit_key_t *p_key,
        const uint8_t *p_src,
        uint32_t src_len,
        uint8_t *p_dst,
        const uint8_t *p_iv,
        uint32_t iv_len,
        const uint8_t *p_aad,
        uint32_t aad_len,
        const sgx_aes_gcm_128bit_tag_t *p_in_mac)
    {
        return sgx_rijndael128GCM_decrypt(
            p_key,
            p_src,
            src_len,
            p_dst,
            p_iv,
            iv_len,
            p_aad,
            aad_len,
            p_in_mac);
    }
};

#endif
///forward declaration of SecureMessageManager to allow context object typedef.
class SecureMessageManager;

///Context object for capturing message and callback info, in use when doing order preserving processing of incomming messages in SecureStorageManager::RecieveMessageHandlerAsync
typedef struct AsyncContext<msg_t *, async_cb_t, void *, SecureMessageManager *, bool> secure_message_context_t;

/**
 * @brief Cryptographic and integrity context per target recipient.
 * 
 * 
 */

typedef struct key_exchange_context_t_packd
{
    /// Own unique identity
    aid_t self_id;
    /// recipient target identity
    aid_t other_id;
    /// Shared secret key used for encryption
    sp_db_item_t g_sp_db;
    /// inbound session id used for preserving input message ordering
    uint64_t session_id_inbound;
    /// outbound session id used for determining message ordering.
    uint64_t session_id_outbound;
    /// nonce used in encrypted message to preserve integrity(replay prevention)
    uint32_t session_id;
    /// determine if attestation handshake is completed
    uint32_t initial;
    /// used by trusted root to determine if client is attested properly
    uint32_t attestation_initialized;
} key_exchange_context_t_packd;

typedef struct key_exchange_context_t
{
    /// Own unique identity
    aid_t self_id;
    /// recipient target identity
    aid_t other_id;
    /// Shared secret key used for encryption
    sp_db_item_t g_sp_db;
    /// inbound session id used for preserving input message ordering
    uint64_t session_id_inbound;
    /// outbound session id used for determining message ordering.
    uint64_t session_id_outbound;
    /// nonce used in encrypted message to preserve integrity(replay prevention)
    uint32_t session_id;
    /// determine if attestation handshake is completed
    uint32_t initial;
    /// used by trusted root to determine if client is attested properly
    uint32_t attestation_initialized;
    /// context handle to own SMM
    SecureMessageManager *parent_manager;
    /// output queue for prepared messages pending successfull attesation handshake, should be empty once initially depleted
    std::vector<secure_message_context_t *> outputqueue;
    /// input message map for preserving ordering according to message session_id, key-ed on session id.
    std::map<uint64_t, secure_message_context_t *> inputqueue;
    ///callback specified for handling output queue once attestation handshake is done.
    async_cb_t key_exchange_session_done;
    /// remote attestation context used during handshake
    sgx_ra_context_t ra_context;
} key_exchange_context_t;

/// max decrypted message size, used as temporary buffer in trusted memory to decrypt message into.
#define SGX_RECIEVEBUFSIZE 1024 * 1024

/**
 * @brief class definition for SMM
 * 
 */
class SecureMessageManager : public IMessageManager
{
    /// friend class definitionf for unit tests, allow access to private members.
#ifdef TEST_DEBUG
#include <gtest/gtest_prod.h>
    FRIEND_TEST(threadsafe_messagemanager, secure_test_many_send_recieve);
#endif
    /// friend class definitions for attestation call flows, which require internals of SMM to work.
    friend class AttestationClient;
    friend class AttestationServer;

private:
    IDiggiAPI *diggiapi;
    /// attestation api reference
    IIASAPI *iasapi;
    /// AMM api reference, used to process incomming and outgoing messages, handle all callback deliveries.
    IAsyncMessageManager *messageService;

    char sgx_recieve_buffer[SGX_RECIEVEBUFSIZE];
    /// own uniqe diggi instance id
    aid_t self;
    /// own thread id, each SMM and AMM is associated with a unique id. AMM guarantees delivery to same thread.
    int this_thread;
    // Implements dynamic measurement of enclave state based on incomming messages, measurement attached to outgoing messages.
    IDynamicEnclaveMeasurement *dynamicmMasurement;
    /// message crypto algorithm api reference
    ICryptoImplementation *crypto;
    bool record_func;
    bool started_attest;
    /// Bool specifying if SMM is in trusted root mode, only usable by trusted root. field specified in configuration.json, built into binary to disallow modification.
    bool trusted_root_func;

    void dh_key_exchange_initiator(key_exchange_context_t *kec);

    static void RecieveMessageHandlerAsync(void *info, int status);
    void decryptAndDeliver(msg_async_response_t *ctxmsg, secure_message_context_t *ctx, bool typed);
    static void SessionRequestHandler(void *ptr, int status);
    static void SendMessageAsyncInternal(void *ptr, int status);
    static void RecieveMessageHandlerInternal(void *info, int status);
    void registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx);
    static void encrypt(key_exchange_context_t *kec, uint8_t *inp_buff, size_t inp_buff_len, secure_message_t *req_message);
    static void decrypt(secure_message_t *resp_message, key_exchange_context_t *kec, char *out_buff, size_t *out_buff_len);

public:
    /// map holding aid to crypto and integrity context per target recipient of outbound messages
    std::map<uint64_t, key_exchange_context_t> callback_map;
    /// mapping of Human Readable Name(HRD) to unique diggi instance identifier. Contains recipients allowed for this enclave, encoded in configuration as part of binary.
    std::map<std::string, aid_t> name_servicemap;
    TamperProofLog *tamperproofLog_inbound;
    TamperProofLog *tamperproofLog_outbound;

    SecureMessageManager(
        IDiggiAPI *dapi,
        IIASAPI *api,
        IAsyncMessageManager *mngr,
        std::map<std::string, aid_t> nameservice_updates,
        int expected_thread,
        IDynamicEnclaveMeasurement *dynMR,
        ICryptoImplementation *crypto,
        bool record_func,
        bool trusted_root_func_role);
    ~SecureMessageManager();
    msg_t *allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery);
    msg_t *allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery);
    msg_t *allocateMessage(msg_t *msg, size_t payload_size);
    void endAsync(msg_t *msg);
    void Send(msg_t *msg, async_cb_t cb, void *ptr);
    std::map<std::string, aid_t> getfuncNames();
    void StopRecording();
};

#endif