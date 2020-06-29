#pragma once

#include <cstddef>
#if defined(DIGGI_ENCLAVE)

#include "sgx_tseal.h"
#include "sgx_urts.h"

#endif
#include "misc.h"
#include "datatypes.h"
#include "DiggiAssert.h"

class ISealingAlgorithm
{

public:
    ISealingAlgorithm(){};
    virtual ~ISealingAlgorithm() {}
    virtual void decrypt(uint8_t *ciphertext, size_t ciphertextsize, uint8_t *plaintext, size_t plaintextsize, uint32_t crc) = 0;
    virtual size_t getciphertextsize(size_t plaintextsize) = 0;
    virtual uint8_t *encrypt(uint8_t *plaintext, size_t size, size_t encrypted_size, uint32_t *crc) = 0;
};

class NoSeal : public ISealingAlgorithm
{
    bool crc_val;
public:
    NoSeal();
    NoSeal(bool do_crc);

    void decrypt(uint8_t *ciphertext, size_t ciphertextsize, uint8_t *plaintext, size_t plaintextsize, uint32_t crc);
    size_t getciphertextsize(size_t plaintextsize);
    uint8_t *encrypt(uint8_t *plaintext, size_t size, size_t encrypted_size, uint32_t *crc);
};

#if defined(DIGGI_ENCLAVE)

typedef enum seal_key_type_t
{
    INSTANCE,
    MEASURED,
    CREATOR,
} seal_key_type_t;

class SGXSeal : public ISealingAlgorithm
{
    seal_key_type_t type;
    bool crc_val;
public:
    SGXSeal(seal_key_type_t tp);
    SGXSeal(seal_key_type_t tp, bool do_crc);

    void decrypt(uint8_t *ciphertext, size_t ciphertextsize, uint8_t *plaintext, size_t plaintextsize, uint32_t crc);
    size_t getciphertextsize(size_t plaintextsize);
    uint8_t *encrypt(uint8_t *plaintext, size_t size, size_t encrypted_size, uint32_t *crc);
};

#endif
