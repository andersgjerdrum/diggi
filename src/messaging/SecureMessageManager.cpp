/**
 * @file securemessagemanager.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief implementation of secure message manager, implemented on top of the asyncronous message manager. 
 * Supports message encryption, attestation, and reliable message ordering.
 * @version 0.1
 * @date 2020-01-30
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "messaging/SecureMessageManager.h"

/**
 * @brief Construct a new Secure Message Manager:: Secure Message Manager object
 * Initializes the SMM, one exists per physical thread.
 * Initialises a SESSION_REQUEST type for allowing attestation processing.
 * @param api reference to attestation api implementation, may be NOAttestation or Attestation.
 * @param threadpool diggi api threadpool interface
 * @param mngr Asynchronous Message Manager reference, used to relay messages.
 * @param nameservice_updates map containing human readable name to instance id mapping, allowing callers to specify human readable name for message destinations.
 * @param log diggi api logging reference
 * @param expected_thread thread id  of this SMM, used  to verify upcalls from AMM are for correct SMM
 * @param dynMR reference to dynamic enclave measurement object, which updates enclave measurement based on message state incomming to enclave.
 * @param crypto implementation api for cryptographic primitives used to encrypt and decrypt messages, only supports AES-256 GCM mode per now.
 * @param trusted_root_func_role if enabled, this SMM is used to attest other diggi instances, is itself concidered fully trustworthy.
 */
SecureMessageManager::SecureMessageManager(
    IDiggiAPI *dapi,
    IIASAPI *api,
    IAsyncMessageManager *mngr,
    std::map<std::string, aid_t> nameservice_updates,
    int expected_thread,
    IDynamicEnclaveMeasurement *dynMR,
    ICryptoImplementation *crypto,
    bool record_func,
    bool trusted_root_func_role)
    : diggiapi(dapi),
      iasapi(api),
      messageService(mngr),
      this_thread(expected_thread),
      dynamicmMasurement(dynMR),
      crypto(crypto),
      record_func(record_func),
      started_attest(false),
      trusted_root_func(trusted_root_func_role)
{
    self = diggiapi->GetId();
    self.fields.thread = this_thread;
    DIGGI_ASSERT(crypto);
    DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "Com instansiated\n");
    name_servicemap = nameservice_updates;
    DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "Secure message manager, Copied name service map\n");
    /*
        NB: Registered in async manager, 
        as secure manager is initializing.
        This is out of band registered by thread 0,
        potential race...
    */
    if (record_func)
    {
        tamperproofLog_inbound = new TamperProofLog(dapi);
        tamperproofLog_outbound = new TamperProofLog(dapi);
        tamperproofLog_inbound->initLog(
            WRITE_LOG,
            std::to_string(this_thread) + ".replay.input",
            nullptr,
            nullptr);
        tamperproofLog_outbound->initLog(
            WRITE_LOG,
            std::to_string(this_thread) + ".replay.output",
            nullptr,
            nullptr);
    }
    messageService->registerTypeCallback(SessionRequestHandler, SESSION_REQUEST, this);
}
/**
 * @brief Destroy the Secure Message Manager:: Secure Message Manager object
 * destroys callback_map storing crypto/integrity state about communicating diggi instances.
 * Alsom unregisteres typed callback, to ensure no new sessions are created.
 */
SecureMessageManager::~SecureMessageManager()
{
    messageService->UnregisterTypeCallback(SESSION_REQUEST);
    callback_map.clear();
}
/**
 * @brief internal method for encrypting a message
 * encrypts AES-128 GCM with nonse for replay prenvention.
 * Invokes crypto api for encrypt, may in theory be replaced in the future.
 * @param kec crypto/integrity context for target recipient of message
 * @param inp_buff plaintext to encrypt
 * @param inp_buff_len plaintext length
 * @param req_message target encrypted message structure.
 */
void SecureMessageManager::encrypt(
    key_exchange_context_t *kec,
    uint8_t *inp_buff,
    size_t inp_buff_len,
    secure_message_t *req_message)
{
    memset(req_message, 0, inp_buff_len);

    //Check if the nonce for the session has not exceeded 2^32-2 if so end session and start a new session
    DIGGI_ASSERT(kec->session_id < UINT32_MAX);
    const uint32_t data2encrypt_length = (uint32_t)inp_buff_len;
    //Set the payload size to data to encrypt length
    req_message->message_aes_gcm_data.payload_size = data2encrypt_length;

    //Use the session nonce as the payload IV

    kec->session_id++;

    memcpy(req_message->message_aes_gcm_data.reserved, &kec->session_id, sizeof(kec->session_id));

    //Set the session ID of the message to the current session id
    req_message->session_id = kec->session_id;

    //Prepare the request message with the encrypted payload
    auto sts = kec->parent_manager->crypto->encrypt(&kec->g_sp_db.sk_key, inp_buff, data2encrypt_length,
                                                    reinterpret_cast<uint8_t *>(&(req_message->message_aes_gcm_data.payload)),
                                                    reinterpret_cast<uint8_t *>(&(req_message->message_aes_gcm_data.reserved)),
                                                    sizeof(req_message->message_aes_gcm_data.reserved), NULL, 0,
                                                    &(req_message->message_aes_gcm_data.payload_tag));
    DIGGI_ASSERT(sts == SGX_SUCCESS);
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "encrypting req_message->session_id= %lu, kec->session_id = %lu\n",
                req_message->session_id,
                kec->session_id);
}
/**
 * @brief internal method to decrypt message inbound to instance
 * copies data into memory, checks and updates nonce, decrypts into memory buffer allocated in trusted memory.
 * @param resp_message encrypted message to decrypt.
 * @param kec crypto/integrity context for source of message
 * @param out_buff unencrypted target buffer for message
 * @param out_buff_len length of unencrypted buffer.
 */
void SecureMessageManager::decrypt(secure_message_t *resp_message,
                                   key_exchange_context_t *kec,
                                   char *out_buff,
                                   size_t *out_buff_len)
{

    //Code to process the response message from the Destination Enclave
    size_t decrypted_data_length = resp_message->message_aes_gcm_data.payload_size;
    uint8_t *decrypted_data = (uint8_t *)malloc(decrypted_data_length);
    DIGGI_ASSERT(decrypted_data);
    memset(decrypted_data, 0, decrypted_data_length);

    //Decrypt the response message payload

    auto status = kec->parent_manager->crypto->decrypt(
        &kec->g_sp_db.sk_key,
        resp_message->message_aes_gcm_data.payload,
        decrypted_data_length, decrypted_data,
        reinterpret_cast<uint8_t *>(&(resp_message->message_aes_gcm_data.reserved)),
        sizeof(resp_message->message_aes_gcm_data.reserved), NULL, 0,
        &(resp_message->message_aes_gcm_data.payload_tag));
    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(), "decryption returned status:%lx\n", status);

    DIGGI_ASSERT(status == SGX_SUCCESS);

    DIGGI_TRACE(kec->parent_manager->diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "decrypting resp_message->session_id= %lu, kec->session_id = %lu\n",
                resp_message->session_id,
                kec->session_id);

    // Verify if the nonce obtained in the response is equal to the session nonce + 1
    // (Prevents replay attacks)
    DIGGI_ASSERT(resp_message->session_id == (kec->session_id) + 1);
    kec->session_id++;
    //Update the value of the session nonce in the source enclave
    *out_buff_len = decrypted_data_length;

    memcpy(out_buff, decrypted_data, decrypted_data_length);
    free(decrypted_data);
}
/**
 * @brief Allocate message destined for address specified by human readable name.
 * Similar convention to AMM equivalent function.
 * does not allocate buffer on untrusted global message object queue.
 * Espects message to be encrypted before copying onto untrusted message object.
 * @see AsyncMessageManager::allocateMessage
 * @param destination HRN destination diggi instacne
 * @param payload_size size of payload
 * @param async convention, should the message expect a callback response.
 * @param delivery msg_delivery_t type, delivery may chose to not encrypt message (ENCRYPTED | CLEARTEXT)
 * @return msg_t* 
 */
msg_t *SecureMessageManager::allocateMessage(
    std::string destination,
    size_t payload_size,
    msg_convention_t async,
    msg_delivery_t delivery)
{
    DIGGI_ASSERT(name_servicemap.find(destination) != name_servicemap.end());
    return allocateMessage(name_servicemap[destination], payload_size, async, delivery);
}
/**
 * @brief allocate message based on instance id, used internally by HRN version above.
 * Similar convention to AMM equivalent function.
 * @see AsyncMessageManager::allocateMessage
 * does not allocate buffer on untrusted global message object queue.
 * Espects message to be encrypted before copying onto untrusted message object.
 * if message is not encrypted, buffer is allocated directly bu global message object queue, to save memory operations.
 * Generates message id if it expects a callback response.
 * @param destination destination id
 * @param payload_size payload size
 * @param async convention, should the message expect a callback response.
 * @param delivery msg_delivery_t type, delivery may chose to not encrypt message (ENCRYPTED | CLEARTEXT)
 * @return msg_t* 
 */
msg_t *SecureMessageManager::allocateMessage(
    aid_t destination,
    size_t payload_size,
    msg_convention_t async,
    msg_delivery_t delivery)
{
    DIGGI_ASSERT(this_thread == diggiapi->GetThreadPool()->currentThreadId());
    /* 
        If message is not encrypted we allocate space directly into untrusted DRAM
    */
    if (delivery != ENCRYPTED)
    {
        auto retmsg = messageService->allocateMessage(self, destination, payload_size, async);
        retmsg->delivery = delivery;
        return retmsg;
    }

    auto msg = (msg_t *)malloc(sizeof(msg_t) + payload_size);
    msg->size = sizeof(msg_t) + payload_size;
    msg->session_count = 0;
    msg->omit_from_log = 0;
    msg->delivery = delivery;
    msg->src = self;
    msg->id = 0;
    msg->dest = destination;
    return msg;
}
/**
 * @brief allocate response message from recieved message as part of callback flow.
 * Similar to AsyncMessageManager
 * Encrypted if recived message is encrypted. 
 * If encrypted, allocated in trusted memory prior to encryption, if not, directly from global message object queue to avoid memory operations.
 * Delivery field may safely be modified before invocation, if perfered.
 * Auto increments session message count, for reliable ordered message delivery managed by SecureMessageManager::RecieveMessageHandlerAsync
 * @see SecureMessageManager::RecieveMessageHandlerAsync
 * @see AsyncMessageManager::allocateMessage
 * @param msg previous recieved message
 * @param payload_size payload size
 * @return msg_t* 
 */
msg_t *SecureMessageManager::allocateMessage(msg_t *msg, size_t payload_size)
{
    DIGGI_ASSERT(this_thread == diggiapi->GetThreadPool()->currentThreadId());
    /* 
        If message is not encrypted we allocate space directly into untrusted DRAM
    */
    if (msg->delivery != ENCRYPTED)
    {
        auto retmsg = messageService->allocateMessage(msg, payload_size);
        retmsg->delivery = msg->delivery;
        return retmsg;
    }

    auto msg_n = (msg_t *)malloc(sizeof(msg_t) + payload_size);
    msg_n->delivery = msg->delivery;
    msg_n->omit_from_log = msg->omit_from_log;
    msg_n->session_count = msg->session_count + 1;
    msg_n->size = sizeof(msg_t) + payload_size;
    msg_n->type = msg->type;
    msg_n->id = msg->id;
    msg_n->src = msg->src;
    msg_n->dest = msg->dest;
    return msg_n;
}
/**
 * @brief end an asynchronous flow.
 * Invokes AMM directly.
 * @see AsyncMessageManager::endAsync
 * @param msg 
 */
void SecureMessageManager::endAsync(msg_t *msg)
{
    messageService->endAsync(msg);
}
/**
 * @brief send a message with assoicated callback and context object
 * Similar convention to AsyncMessageManager::Send
 * @see AsyncMessageManager::Send
 * @warning if sent as non-encrypted message, callback message will recide in untrusted memory, do not modify response directly.
 * Message encrypted before sent through AMM send operation.
 * If uninitialized channel, attestaion and key exchange occurs before message is sent.
 * @param msg message to send
 * @param cb completion callback invoked on the recipt of a response, given that the message was allocate with a message id.
 * @param ptr Convenience context object  delived to callback along with message response.
 */
void SecureMessageManager::Send(msg_t *msg, async_cb_t cb, void *ptr)
{
    DIGGI_ASSERT(this_thread == diggiapi->GetThreadPool()->currentThreadId());
    /*
			Initially we  attempt to send to same thread on recieving end
			If not then rework is required in callbackmap to account for correct thread.
			which is not currently implemented.
		*/
    msg->dest.fields.thread = diggiapi->GetThreadPool()->currentThreadId();
    msg->src.fields.thread = diggiapi->GetThreadPool()->currentThreadId();
    auto ctx = new secure_message_context_t(msg, cb, ptr, this, false);

    if (callback_map.find(msg->dest.raw) == callback_map.end())
    {

        DIGGI_TRACE(diggiapi->GetLogObject(), LDEBUG, "attempting server handshake request\n");
        DIGGI_ASSERT(msg->session_count == 0);
        callback_map[msg->dest.raw].self_id = self;
        callback_map[msg->dest.raw].other_id = msg->dest;
        callback_map[msg->dest.raw].key_exchange_session_done =
            (async_cb_t)SecureMessageManager::SendMessageAsyncInternal;
        callback_map[msg->dest.raw].outputqueue.push_back(ctx);
        callback_map[msg->dest.raw].parent_manager = this;
        callback_map[msg->dest.raw].session_id = 0;
        callback_map[msg->dest.raw].session_id_outbound = 0;
        callback_map[msg->dest.raw].session_id_inbound = 0;
        callback_map[msg->dest.raw].initial = 1;
        callback_map[msg->dest.raw].attestation_initialized = 0;
        dh_key_exchange_initiator(&callback_map[msg->dest.raw]);
    }
    else
    {
        ///if instance attempts to send messages before key exchange and attestation is completed, the message is stored for deffered delivery.
        if (callback_map[msg->dest.raw].initial)
        {
            DIGGI_ASSERT(callback_map[msg->dest.raw].outputqueue.size() > 0);
            callback_map[msg->dest.raw].outputqueue.push_back(ctx);
        }
        else
        {
            DIGGI_ASSERT(callback_map[msg->dest.raw].outputqueue.size() == 0);
            SecureMessageManager::SendMessageAsyncInternal(ctx, 1);
        }
    }
}

/**
 * @brief invoked directly from send operation or as a deffered message delivery following a completed attestation process.
 * 
 * @param context context object capturing potential deffered message send information
 * @param status callback status, unused parameter, future work.
 */
void SecureMessageManager::SendMessageAsyncInternal(void *context, int status)
{
    auto ctx = (secure_message_context_t *)context;
    auto cb = ctx->item2;
    auto _this = ctx->item4;
    DIGGI_ASSERT(_this->this_thread == _this->diggiapi->GetThreadPool()->currentThreadId());

    size_t payload_size = sizeof(secure_message_t) + (ctx->item1->size - sizeof(msg_t));
    DIGGI_ASSERT(ctx->item1);
    DIGGI_ASSERT(ctx->item4);
    DIGGI_ASSERT(ctx->item1->src.raw == _this->self.raw);
    DIGGI_ASSERT(payload_size >= (ctx->item1->size - sizeof(msg_t)));
    msg_t *send = ctx->item1;
    /* 
        Support encryption without enclaves but not enclaves without encryption
    */

    if (ctx->item1->dest.fields.type == ENCLAVE && _this->self.fields.type == ENCLAVE)
    {
        DIGGI_ASSERT(ctx->item1->delivery == ENCRYPTED);
    }

    if (ctx->item1->delivery == ENCRYPTED)
    {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                    LogLevel::LDEBUG,
                    "encrypting message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                    ctx->item1->src.raw,
                    ctx->item1->dest.raw,
                    ctx->item1->id,
                    ctx->item1->size);

        DIGGI_ASSERT(ctx->item1->dest.fields.enclave < MAX_ENCLAVE_ID);
        send = _this->messageService->allocateMessage(ctx->item1, payload_size);
        send->delivery = ENCRYPTED;
        auto secure_message = (secure_message_t *)send->data;
        encrypt(
            &(_this->callback_map[send->dest.raw]),
            ctx->item1->data,
            payload_size - sizeof(secure_message_t), /*only data is encrypted, not headers*/
            secure_message);
        free(ctx->item1);
    }

    DIGGI_ASSERT(send);
    send->session_count = _this->callback_map[send->dest.raw].session_id_outbound;
    DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "sending message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu, session_count=%lu\n",
                send->src.raw,
                send->dest.raw,
                send->id,
                send->size,
                send->session_count);
    _this->callback_map[send->dest.raw].session_id_outbound++;
    if (_this->dynamicmMasurement)
    {
        memcpy(send->sha256_current_evidence_hash, _this->dynamicmMasurement->get(), ATTESTATION_HASH_SIZE);
    }
    if (_this->record_func && send->omit_from_log == 0)
    {
        _this->tamperproofLog_outbound->appendLogEntry(send);
    }
    (cb == nullptr)
        ? _this->messageService->sendMessage(send)
        : _this->messageService->sendMessageAsync(send, RecieveMessageHandlerAsync, ctx);

    if (cb == nullptr)
    {
        delete ctx;
    }
}
/**
 * @brief Callback handler for all messages inbound. 
 * Registered as intermediate recipient for all callbacks(both typed and one-off as part of a flow)
 * correct callback context is captured through the secure_message_context_t object, created for all message send operations, or type registrations.
 * This callback handles and preserves correct ordering of messages according to the sender. Senders mark messages with a series counter, indicating the ordering.
 * If the AMM or untrusted runtime deffers scheduling of a message onto the recipient queue, a reordering may occur.
 * Messages which do not match the currently expected, are copied and stored until the correct is recieved.
 * The callback must copy messages because AMM purges message objects following the callback invocation.
 * We preserve the delete-after-invoke convention for copied messages aswell on behalf of callbacks invoked here.
 * The fast path is still invoked in the event of no rescheduling, without major algoritmic operations or copy operations.
 * 
 * @param info msg_async_response_t containing message and secure_message_context_t
 * @param status unused parameter, error handling, future work.
 */
void SecureMessageManager::RecieveMessageHandlerAsync(void *info, int status)
{
    DIGGI_ASSERT(info);
    auto resp = (msg_async_response_t *)info;
    auto ctx_base = (secure_message_context_t *)resp->context;
    DIGGI_ASSERT(ctx_base);
    auto ctx = new secure_message_context_t(ctx_base->item1, ctx_base->item2, ctx_base->item3, ctx_base->item4, ctx_base->item5);
    if (ctx_base->item5 != true)
    {
        delete ctx_base;
    }

    auto _this = ctx->item4;
    ctx->item1 = resp->msg;
    DIGGI_ASSERT(_this);

    /*
			Check message session count and compensate to avoid out of order delivery
			which may happen on highly concurrent diggi nodes.
			This is caused by 1) the main func message scheduler
			which may deffer delivery of individual messages if contention on ringbuffer.
			2) If multiple sends allocate to ringbuffer contention may cause out of order
			allocation on outbound buffer.
			3) Picking messages off the ringbuffer may result in wrong thread recieving it,
			and message must therefore be rescheduled on correct thread, causing it to be potentially delivered out of band.

			reorder logic only invoked if an out of order message is detected.
		*/
    DIGGI_ASSERT(ctx->item1->type != SESSION_REQUEST);
    key_exchange_context_t &source_slot = _this->callback_map.find(ctx->item1->src.raw)->second;

    DIGGI_ASSERT(source_slot.inputqueue.find(ctx->item1->session_count) == source_slot.inputqueue.end());

    auto lowerbound = source_slot.session_id_inbound;
    auto upperbound = ctx->item1->session_count;
    DIGGI_ASSERT(lowerbound <= upperbound);
    for (
        uint64_t i = lowerbound;
        i <= upperbound;
        i++)
    {
        /*
				We set response for each iteration of loop as we are reusing 
				the asyncmessagemanagers response struct.
			*/

        /*
				If we reached current message
			*/
        if ((i == ctx->item1->session_count) &&
            (i == source_slot.session_id_inbound))
        {

            DIGGI_ASSERT(source_slot.inputqueue.find(i) == source_slot.inputqueue.end());
            DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                        LogLevel::LDEBUG,
                        "deliver current message from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                        ctx->item1->src.raw,
                        ctx->item1->dest.raw,
                        ctx->item1->id,
                        ctx->item1->type,
                        ctx->item1->size);
            resp->context = ctx;
            resp->msg = ctx->item1;
            RecieveMessageHandlerInternal(resp, status);

            /*
					In the event where messages following the most
					current is held up, we deliver them aswell.
				*/

            while (source_slot.inputqueue.find(i + 1) != source_slot.inputqueue.end())
            {
                auto context = source_slot.inputqueue[i + 1];
                DIGGI_ASSERT(context);
                DIGGI_ASSERT(context->item1 != nullptr);
                DIGGI_ASSERT(context->item1->size > 0);
                DIGGI_ASSERT(context->item4);
                DIGGI_ASSERT(context->item2);
                resp->context = context;
                resp->msg = context->item1;
                DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                            LogLevel::LDEBUG,
                            "deliver future messages from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                            context->item1->src.raw,
                            context->item1->dest.raw,
                            context->item1->id,
                            context->item1->type,
                            context->item1->size);
                source_slot.inputqueue.erase(i + 1);
                RecieveMessageHandlerInternal(resp, status);
                free(context->item1);
                delete context;
                i++;
            }

            delete ctx;
            break;
        }
        /*
			else if in map, deliver it
			*/
        else if (source_slot.inputqueue.find(i) != source_slot.inputqueue.end() &&
                 (i == source_slot.session_id_inbound))
        {

            auto ctx_new = source_slot.inputqueue[i];
            DIGGI_ASSERT(ctx_new);
            DIGGI_ASSERT(ctx_new->item1 != nullptr);
            DIGGI_ASSERT(ctx_new->item1->size > 0);
            DIGGI_ASSERT(ctx_new->item4);
            DIGGI_ASSERT(ctx_new->item2);
            DIGGI_ASSERT(i == ctx_new->item1->session_count);
            source_slot.inputqueue.erase(i);
            resp->context = ctx_new;
            resp->msg = ctx_new->item1;
            DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                        LogLevel::LDEBUG,
                        "deliver past message from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                        ctx_new->item1->src.raw,
                        ctx_new->item1->dest.raw,
                        ctx_new->item1->id,
                        ctx_new->item1->type,
                        ctx_new->item1->size);
            RecieveMessageHandlerInternal(resp, status);
            free(ctx_new->item1);
            ctx_new->item1 = nullptr;
            /*
					delete regardless of wether this is a typed,
					as deffered typed message delivery will copy the message context instead of reuse
				*/
            delete ctx_new;
        }
        else
        {
            /*
					If not stored in the map,
					we return without serving a message to the upwards layer

					If typed message, we copy the context used aswell, since all typed originally share context,
					further reuse would cause inconsistent global message context.
				*/
            DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                        LogLevel::LDEBUG,
                        "Storing out of bounds message with expecting %lu, session_count %lu,  from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu \n",
                        source_slot.session_id_inbound,
                        ctx->item1->session_count,
                        ctx->item1->src.raw,
                        ctx->item1->dest.raw,
                        ctx->item1->id,
                        ctx->item1->type,
                        ctx->item1->size);
            DIGGI_ASSERT(source_slot.inputqueue.find(ctx->item1->session_count) == source_slot.inputqueue.end());
            DIGGI_ASSERT(ctx->item1 != nullptr);
            DIGGI_ASSERT(ctx->item4);
            DIGGI_ASSERT(ctx->item2);
            auto cpymsg = COPY(msg_t, ctx->item1, ctx->item1->size);

            source_slot.inputqueue[ctx->item1->session_count] = ctx;
            ctx->item1 = cpymsg;

            break;
        }
    }
}
/**
 * @brief invoked on a correctly ordered message, either typed or one-off callback as part of flow.
 * Increments lowerbound ordering count, and calls decrypt operations.
 * @param info msg_async_response_t containing message and secure_message_context_t
 * @param status unused parameter, error handling, future work
 */
void SecureMessageManager::RecieveMessageHandlerInternal(void *info, int status)
{
    DIGGI_ASSERT(info);
    auto secmsg = (msg_async_response_t *)info;
    auto ctx = (secure_message_context_t *)secmsg->context;

    DIGGI_ASSERT(ctx);
    /*
			Not checking item3 in context since
			callback may not have context.
		*/
    DIGGI_ASSERT(ctx->item4);
    DIGGI_ASSERT(ctx->item2);
    auto _this = ctx->item4;
    DIGGI_ASSERT(_this->this_thread == _this->diggiapi->GetThreadPool()->currentThreadId());

    DIGGI_TRACE(_this->diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "recieving message from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                secmsg->msg->src.raw,
                secmsg->msg->dest.raw,
                secmsg->msg->id,
                secmsg->msg->size);
    _this->callback_map[secmsg->msg->src.raw].session_id_inbound++;
    DIGGI_TRACE(_this->diggiapi->GetLogObject(), "Delivering session_count: %lu, next_id_inbound:%lu\n", ctx->item1->session_count, _this->callback_map[ctx->item1->src.raw].session_id_inbound);

    _this->decryptAndDeliver(secmsg, ctx, ctx->item5);
#ifndef RELEASE
    DIGGI_TRACE(_this->diggiapi->GetLogObject(), "Messages left on input queue from: %" PRIu64 " count:%lu \n", ctx->item1->src.raw, _this->callback_map[ctx->item1->src.raw].inputqueue.size());
    for (auto itm : _this->callback_map[ctx->item1->src.raw].inputqueue)
    {
        DIGGI_TRACE(_this->diggiapi->GetLogObject(), "Left session items: %lu\n", itm.second->item1->session_count);
    }
#endif
}
/**
 * @brief Registers type callback, funnels all types into RecieveMessageHandlerAsync for order-presered processing.
 * Captures source expected completion callback as part of secure_message_context_t object, 
 * causing decryptAndDeliver to correctly invoke the callback input to this method, with the expected convenience context pointer.
 * @param cb callback to invoke on the reciept of a message with the given type
 * @param type type of message to register
 * @param ctx convenience context pointer deliverd to callback
 */
void SecureMessageManager::registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx)
{
    /*
        TODO: why is this line commented out? is it violated somewhere?
    */
    DIGGI_ASSERT(this_thread == diggiapi->GetThreadPool()->currentThreadId());
    if (iasapi->attestable()  && !this->started_attest)
    {
        this->started_attest = true;
        DIGGI_TRACE(diggiapi->GetLogObject(),
                    LogLevel::LDEBUG,
                    "Attesting server from registerTypeCallback\n");
        callback_map[self.raw].self_id = self;
        callback_map[self.raw].parent_manager = this;
        callback_map[self.raw].session_id = 0;
        callback_map[self.raw].session_id_outbound = 0;
        callback_map[self.raw].session_id_inbound = 0;
        callback_map[self.raw].initial = 1;
        callback_map[self.raw].attestation_initialized = 0;
        dh_key_exchange_initiator(&callback_map[self.raw]);
    }
    else
    {
        DIGGI_TRACE(diggiapi->GetLogObject(),
                    LogLevel::LDEBUG,
                    "registerTypeCallback: Warning, not attestable type%d\n", type);
    }
    auto ctx_n = new secure_message_context_t(nullptr, cb, ctx, this, true);
    messageService->registerTypeCallback(RecieveMessageHandlerAsync, type, ctx_n);
}

/**
 * @brief internal method to decrypt message and invoke the correct corresponding callback
 * Copies message into trusted memory before decryption. if cleartext, delivered directly.
 * @param ctxmsg msg_async_response_t (context,message)
 * @param ctx secure message to decrypt
 * @param typed non used parameter
 */
void SecureMessageManager::decryptAndDeliver(msg_async_response_t *ctxmsg, secure_message_context_t *ctx, bool typed)
{
    DIGGI_ASSERT(this_thread == diggiapi->GetThreadPool()->currentThreadId());
    if (ctxmsg->msg->src.fields.type == ENCLAVE && self.fields.type == ENCLAVE)
    {
        DIGGI_ASSERT(ctxmsg->msg->delivery == ENCRYPTED);
    }
    auto encrypted = (ctxmsg->msg->delivery == ENCRYPTED);

    size_t decrypt_msg_size = 0;
    if (encrypted)
    {
        /*
            We must copy message from buffer into encalve
            as the decryption api will not work on untrusted memory directly
            
            TODO: evaulate security risk of decrypting directly from unsecure memory to avoid an extra copy operation.
                and implement custom crypto.
        */

        auto recv = COPY(msg_t, ctxmsg->msg, ctxmsg->msg->size);

        auto secure_message = (secure_message_t *)recv->data;

        decrypt(secure_message, &callback_map[recv->src.raw], sgx_recieve_buffer, &decrypt_msg_size);

        // DIGGI_ASSERT(decrypt_msg_size > 0);
        DIGGI_ASSERT(decrypt_msg_size <= SGX_RECIEVEBUFSIZE);

        memcpy(recv->data, sgx_recieve_buffer, decrypt_msg_size);
        recv->size = decrypt_msg_size + sizeof(msg_t);
        ctxmsg->msg = recv;
    }

    ctxmsg->context = ctx->item3;
    if (record_func && ctxmsg->msg->omit_from_log == 0)
    {

        DIGGI_TRACE(diggiapi->GetLogObject(),
                    LogLevel::LDEBUG,
                    "Incomming from: %" PRIu64 ", to: %" PRIu64 ", id: %lu, type: %d , size: %lu session_id = %lu\n",
                    ctxmsg->msg->src.raw,
                    ctxmsg->msg->dest.raw,
                    ctxmsg->msg->id,
                    ctxmsg->msg->type,
                    ctxmsg->msg->size,
                    ctxmsg->msg->session_count);
        tamperproofLog_inbound->appendLogEntry(ctxmsg->msg);
    }
    if (dynamicmMasurement)
    {
        dynamicmMasurement->update((uint8_t *)ctxmsg->msg, ctxmsg->msg->size);
    }
    ctx->item2(ctxmsg, 1);
    if (encrypted)
    {
        free(ctxmsg->msg);
    }
}
/**
 * @brief intitial session setup handler, used for attestation flow init procedure.
 * called by a requestor instance, will cause both to engage in communication flow with trusted root defined in AttestationClient.cpp
 * @param ptr 
 * @param status 
 */
void SecureMessageManager::SessionRequestHandler(void *ptr, int status)
{
    auto secmsg = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(secmsg);
    DIGGI_ASSERT(secmsg->context);
    DIGGI_ASSERT(secmsg->msg);
    auto _this = (SecureMessageManager *)secmsg->context;

    DIGGI_ASSERT(_this->self.raw != UINT64_MAX);
    DIGGI_ASSERT(_this->this_thread == _this->diggiapi->GetThreadPool()->currentThreadId());

    auto msg = secmsg->msg;

    _this->callback_map[msg->src.raw].self_id = _this->self;
    _this->callback_map[msg->src.raw].other_id = msg->src;
    _this->callback_map[msg->src.raw].initial = _this->iasapi->attestable();
    _this->callback_map[msg->src.raw].attestation_initialized = 0;
    _this->callback_map[msg->src.raw].session_id = 0;

    _this->callback_map[msg->src.raw].key_exchange_session_done = nullptr;
    _this->callback_map[msg->src.raw].parent_manager = _this;
    _this->callback_map[msg->src.raw].session_id_inbound = 0;
    _this->callback_map[msg->src.raw].session_id_outbound = 0;
    secmsg->context = &_this->callback_map[msg->src.raw];

    /* The callbacks below should copy message for delayed initial delivery, 
        pending attestation
    */
    if (_this->trusted_root_func)
    {

        _this->iasapi->get_server_attestation_flow()(secmsg, 1);
    }
    else
    {
        _this->iasapi->get_client_response_attestation_flow()(secmsg, 1);
    }
}
/**
 * @brief called by send function to initiate atestation process.
 * If configuration says attestation process should commence, this function retrieves address of trusted root and initiates the protocol.
 * Invoked by registertypedcallback for servers not involved in ongoing message exchange.
 * 
 * @param kec context of deffered target, not realy used for encryption/integrity as part of this call.
 */
void SecureMessageManager::dh_key_exchange_initiator(
    key_exchange_context_t *kec)
{
    DIGGI_ASSERT(this_thread == diggiapi->GetThreadPool()->currentThreadId());
    auto recipient = (iasapi->attestable()) ? name_servicemap["trusted_root_func"] : kec->other_id;
    if (recipient.raw == 0)
    {
        diggiapi->GetLogObject()->Log(LRELEASE, "ERROR:dh_key_exchange_initiator() - expected trusted_root_func, concider disabling attestation");
        DIGGI_ASSERT(false);
    }
    auto msg = messageService->allocateMessage(kec->self_id, recipient, sizeof(ra_samp_request_header_t) + sizeof(uint32_t), CALLBACK);

    DIGGI_TRACE(diggiapi->GetLogObject(),
                LogLevel::LDEBUG,
                "sending dh_key_exchange_initiator request from: %" PRIu64 ", to: %" PRIu64 ", id:%lu size: %lu\n",
                msg->src.raw,
                msg->dest.raw,
                msg->id,
                msg->size);
    DIGGI_ASSERT(diggiapi->GetThreadPool()->currentThreadId() == msg->src.fields.thread);

    msg->session_count = 0;
    msg->type = SESSION_REQUEST;
    msg->delivery = CLEARTEXT;
#ifdef DIGGI_ENCLAVE
    ocall_prepare_msg0(msg);
#else
    memset(msg->data, 0, sizeof(ra_samp_request_header_t) + sizeof(uint32_t));
#endif
    messageService->sendMessageAsync(msg, iasapi->get_client_initiator_attestation_flow(), kec);
}
/**
 * @brief api call for retrieving aid->human readable name map for instances.
 * returns all possible instance targets allowed for this instance.
 * May include system instances, such as networking and storage instances.
 * Should not be used to send messages to system instances, will lead to undefined behaviour.
 * May be used for group broadcast messages.
 * 
 * @return std::map<std::string, aid_t> 
 */
std::map<std::string, aid_t> SecureMessageManager::getfuncNames()
{
    DIGGI_ASSERT(this_thread == diggiapi->GetThreadPool()->currentThreadId());
    return name_servicemap;
}
void SecureMessageManager::StopRecording()
{
    record_func = false;
    tamperproofLog_inbound->Stop();
    tamperproofLog_outbound->Stop();
}