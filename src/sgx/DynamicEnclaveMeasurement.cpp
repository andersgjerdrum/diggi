/**
 * @file dattestation.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief implements dynamic attestation of diggi instances based on inbound messages.
 * @version 0.1
 * @date 2020-01-29
 * TODO: work in progress, document completely once implemented.
 * @copyright Copyright (c) 2020
 * 
 */
#include "sgx/DynamicEnclaveMeasurement.h"

/**
 * @brief Construct a new Dynamic Enclave Measurement:: Dynamic Enclave Measurement object
 * accepts as input the initial enclave measurement.
 * Threadsafe, as each thread uses its own hash store.
 * @param init 
 * @param init_size 
 * @param dapi  diggi api reference.
 */
DynamicEnclaveMeasurement::DynamicEnclaveMeasurement(uint8_t *init, size_t init_size, IDiggiAPI *dapi) : api(dapi)
{
    DIGGI_ASSERT(init);
    DIGGI_ASSERT(init_size);
    DIGGI_ASSERT(dapi);
    DIGGI_ASSERT(ATTESTATION_HASH_SIZE == init_size);
    DIGGI_ASSERT(sizeof(sgx_sha256_hash_t) == ATTESTATION_HASH_SIZE);
    auto thrid_ = api->GetThreadPool()->physicalThreadCount();
    current = (uint8_t *)calloc(1, thrid_ * ATTESTATION_HASH_SIZE);
    for (size_t i = 0; i < thrid_; i++)
    {
        memcpy(&current[i], init, ATTESTATION_HASH_SIZE);
    }
}
/**
 * @brief Construct a new Dynamic Enclave Measurement:: Dynamic Enclave Measurement object
 * without initial input, begins with empty hash.
 * Separate hash for each thread.
 * @param dapi diggi api reference
 */
DynamicEnclaveMeasurement::DynamicEnclaveMeasurement(IDiggiAPI *dapi) : api(dapi)
{
    DIGGI_ASSERT(dapi);
    DIGGI_ASSERT(sizeof(sgx_sha256_hash_t) == ATTESTATION_HASH_SIZE);
    current = (uint8_t *)calloc(api->GetThreadPool()->physicalThreadCount(), ATTESTATION_HASH_SIZE);
}

/**
 * @brief update measurement with new input buffer data.
 * peforms a hash of current hash + input data to chain content.
 * updates for current thread
 * @param next 
 * @param size 
 */
void DynamicEnclaveMeasurement::update(uint8_t *next, size_t size)
{

    auto total_upd = size + ATTESTATION_HASH_SIZE;
    auto intermediary = calloc(1, total_upd);
    memcpy(intermediary, next, size);
    /*
        only interact with current thread
    */
    auto current_offset = &current[api->GetThreadPool()->currentThreadId()];
    memcpy((void *)((uintptr_t)intermediary + size), current_offset, ATTESTATION_HASH_SIZE);
    DIGGI_ASSERT(SGX_SUCCESS == sgx_sha256_msg((const uint8_t *)intermediary, (uint32_t)total_upd, (sgx_sha256_hash_t *)current_offset));
    free(intermediary);
}
/**
 * @brief retrieve current hash
 * Retrieves for current calling thread
 * @return uint8_t* 
 */
uint8_t *DynamicEnclaveMeasurement::get()
{
    return &current[api->GetThreadPool()->currentThreadId()];
}

DynamicEnclaveMeasurement::~DynamicEnclaveMeasurement()
{
    free(current);
}