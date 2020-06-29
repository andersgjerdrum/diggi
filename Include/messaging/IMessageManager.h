#ifndef IMESSAGE_MANAGER_H
#define IMESSAGE_MANAGER_H
#include "datatypes.h"
#include <string>
#include <vector>
#include <map>

class ICryptoImplementation {
public:
	ICryptoImplementation(){}
	virtual ~ICryptoImplementation() {};

	virtual sgx_status_t encrypt(
		const sgx_aes_gcm_128bit_key_t *p_key,
		const uint8_t *p_src,
		uint32_t src_len,
		uint8_t *p_dst,
		const uint8_t *p_iv,
		uint32_t iv_len,
		const uint8_t *p_aad,
		uint32_t aad_len,
		sgx_aes_gcm_128bit_tag_t *p_out_mac
	) = 0;

	virtual sgx_status_t decrypt(
		const sgx_aes_gcm_128bit_key_t *p_key,
		const uint8_t *p_src,
		uint32_t src_len,
		uint8_t *p_dst,
		const uint8_t *p_iv,
		uint32_t iv_len,
		const uint8_t *p_aad,
		uint32_t aad_len,
		const sgx_aes_gcm_128bit_tag_t *p_in_mac
	) = 0;

};

class IMessageManager {
public:

	IMessageManager() {}
	virtual ~IMessageManager() {};
	virtual void endAsync(msg_t *msg) = 0;
	virtual void Send(msg_t *msg, async_cb_t cb, void *cb_context) = 0;
	virtual msg_t *allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery) = 0;
    virtual msg_t *allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery) = 0;
	virtual msg_t *allocateMessage(msg_t *msg, size_t payload_size) = 0;
	virtual void registerTypeCallback(async_cb_t cb, msg_type_t type, void * ctx) = 0;
	virtual std::map<std::string, aid_t> getfuncNames () = 0;
};

#endif
