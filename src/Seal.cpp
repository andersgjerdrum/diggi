/**
 * @file seal.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief implementation of encryption algorithm used in storage manager
 * currently built on the SGX seal key unique to the enclave invoker.
 * Implies that only an indentical enclave(measurement and signature) may decrypt blocks encrypted using this algorithm
 * @see StorageManager::StorageManager
 * @version 0.1
 * @date 2020-02-05
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "Seal.h"
#include "messaging/Util.h"
#include "DiggiGlobal.h"
#include "storage/crc.h"
#if defined(DIGGI_ENCLAVE)
/**
 * @brief Construct a new SGXSeal::SGXSeal object
 * Create crypto api instance input to StorageManager constructor
 * @param tp non used parameter, future work to support MRSIGNER policy, allowing updated enclaves with identical signatures to decrypt.
 */
SGXSeal::SGXSeal(seal_key_type_t tp) : type(tp)
{
    /*
		future work, default key type used in SGX API is MRSIGNER
		TODO: replace with api call:
			https://software.intel.com/en-us/node/709129
	*/
}
SGXSeal::SGXSeal(seal_key_type_t tp, bool do_crc) : type(tp), crc_val(do_crc)
{
    /*
		future work, default key type used in SGX API is MRSIGNER
		TODO: replace with api call:
			https://software.intel.com/en-us/node/709129
	*/
}
/**
 * @brief decrypt a ciphertext with a given size
 * Copies data to ensure its inside enclave memory. may be directly invoked from message object reciding in unrtrusted memory.
 * sets decrypted payload size to ensure partially filled blocks are able to be correctly un-marshalled afterwards.
 * @param ciphertext 
 * @param ciphertextsize 
 * @param plaintextsize 
 * @return uint8_t* 
 */
void SGXSeal::decrypt(uint8_t *ciphertext, size_t ciphertextsize, uint8_t *plaintext, size_t plaintextsize, uint32_t crc)
{
    DIGGI_ASSERT(ciphertextsize == ENCRYPTED_BLK_SIZE);
    uint32_t p_decrypted_text_length = ciphertextsize - sizeof(sgx_sealed_data_t);

    /*
		If empty blocks are returned a preceeding lseek caused these to be empty.
		We therefore emulate empty blocks
	*/
    auto outdata = (uint8_t *)calloc(1, p_decrypted_text_length);
    DIGGI_ASSERT(plaintextsize);

    /*
		Ciphertext located on ringbuffer outside of enclave can not be input to unseal funciton
	*/
    auto ciphertext_within_enclave = COPY(sgx_sealed_data_t, ciphertext, ciphertextsize);
    ciphertext_within_enclave->aes_data.payload_size = p_decrypted_text_length;

    auto status = sgx_unseal_data(
        ciphertext_within_enclave,
        NULL,
        NULL,
        outdata,
        &p_decrypted_text_length);
    free(ciphertext_within_enclave);
    uint32_t actual_crc = 0;
    memcpy(&actual_crc, outdata, sizeof(uint32_t));
    if (status != SGX_SUCCESS)
    {
        GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject()->Log(LRELEASE, "ERROR: sgxstatus = 0x%x, ciphertext size= %lu, cleantextsize = %lu\n", status, ciphertextsize, p_decrypted_text_length);
        DIGGI_ASSERT(status == SGX_SUCCESS);
    }
    if (crc_val)
    {
        DIGGI_ASSERT(crc == actual_crc);
    }

    auto offset_plaintext = (ENCRYPTION_HEADER_SIZE - sizeof(sgx_sealed_data_t));
    DIGGI_ASSERT(p_decrypted_text_length - offset_plaintext == SPACE_PER_BLOCK);
    memcpy(plaintext, outdata + offset_plaintext, plaintextsize);
    free(outdata);
}
/**
 * @brief retrive expecte ciphertext size given a plaintext
 * each block adds a static encryption header to the total size.
 * @param plaintextsize 
 * @return size_t 
 */
size_t SGXSeal::getciphertextsize(size_t plaintextsize)
{
    DIGGI_ASSERT(plaintextsize);
    size_t retval = ENCRYPTED_BLK_SIZE;
    return retval;
}
/**
 * @brief encrypt a plaintext of given size
 * 
 * @param plaintext 
 * @param size 
 * @param encrypted_size 
 * @return uint8_t* 
 */
uint8_t *SGXSeal::encrypt(uint8_t *plaintext, size_t size, size_t encrypted_size, uint32_t *crc)
{
    *crc = crc32_fast(plaintext, size, *crc);
    auto outdata = (sgx_sealed_data_t *)calloc(1, encrypted_size);
    auto plaintext1 = (uint8_t *)calloc(1, encrypted_size);
    memcpy(plaintext1 + (ENCRYPTION_HEADER_SIZE - sizeof(sgx_sealed_data_t)), plaintext, size);
    memcpy(plaintext1, crc, sizeof(uint32_t));
    auto status = sgx_seal_data(
        0,
        NULL,
        encrypted_size - sizeof(sgx_sealed_data_t),
        plaintext1,
        encrypted_size,
        outdata);
    free(plaintext1);
    outdata->aes_data.payload_size = (uint32_t)size;

    if (status != SGX_SUCCESS)
    {
        GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject()->Log(LRELEASE, "ERROR: sgxstatus = 0x%x, ciphertext size= %lu, cleantextsize = %lu\n", status, encrypted_size, size);
        DIGGI_ASSERT(status == SGX_SUCCESS);
    }
    return (uint8_t *)outdata;
}

#endif

/**
 * @brief Construct a new No Seal:: No Seal object
 * dummy api implementing noops on data blocks used for debugging purposes
 */
NoSeal::NoSeal() : crc_val(true)
{
}
NoSeal::NoSeal(bool do_crc) : crc_val(do_crc)
{
}
/**
 * @brief dummy implementing noops on data blocks used for debugging purposes
 * Copies data from ciphertext buffer to plaintext return
 * @param ciphertext 
 * @param ciphertextsize 
 * @param plaintextsize 
 * @return uint8_t* 
 */
void NoSeal::decrypt(uint8_t *ciphertext, size_t ciphertextsize, uint8_t *plaintext, size_t plaintextsize, uint32_t crc)
{
    DIGGI_ASSERT(plaintextsize);

    memcpy(plaintext, ciphertext + ENCRYPTION_HEADER_SIZE, plaintextsize);
    uint32_t test = 0;
    memcpy(&test, ((sgx_sealed_data_t *)ciphertext)->aes_data.payload_tag, sizeof(uint32_t));
    if (crc_val)
    {
        DIGGI_ASSERT(memcmp(((sgx_sealed_data_t *)ciphertext)->aes_data.payload_tag, &crc, sizeof(uint32_t)) == 0);
    }
}
/**
 * @brief dummy implementing noops on data blocks used for debugging purposes
 * 
 * @param plaintextsize 
 * @return size_t 
 */
size_t NoSeal::getciphertextsize(size_t plaintextsize)
{
    DIGGI_ASSERT(plaintextsize);
    size_t retval = ENCRYPTED_BLK_SIZE;
    return retval;
}
/**
 * @brief dummy implementing noops on data blocks used for debugging purposes
 * copies plaintext buffer to encrypted return
 * @param plaintext 
 * @param size 
 * @param encrypted_size 
 * @return uint8_t* 
 */
uint8_t *NoSeal::encrypt(uint8_t *plaintext, size_t size, size_t encrypted_size, uint32_t *crc)
{
    *crc = crc32_fast(plaintext, size, *crc);

    auto outdata = (uint8_t *)calloc(1, encrypted_size);
    //memset(outdata, 0, encrypted_size);
    ((sgx_sealed_data_t *)outdata)->aes_data.payload_size = size;
    memcpy(((sgx_sealed_data_t *)outdata)->aes_data.payload_tag, crc, sizeof(uint32_t));
    auto offset = outdata + ENCRYPTION_HEADER_SIZE;
    memcpy(offset, plaintext, size);
    return outdata;
}
