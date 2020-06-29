#ifndef DYN_MEASURE_H
#define DYN_MEASURE_H
#include <stddef.h>
#include <inttypes.h>
#include <sgx_tseal.h>
#include <runtime/DiggiAPI.h>

/*
    Attestation kernel implementation
*/
#define ATTESTATION_HASH_SIZE 32

class IDynamicEnclaveMeasurement
{
public:
    IDynamicEnclaveMeasurement() {}
    virtual ~IDynamicEnclaveMeasurement() {}
    virtual void update(uint8_t *next, size_t size) = 0;
    /*
        per thread retrieval.
        Total gathered by traversing all threads.
    */
    virtual uint8_t *get() = 0;
};

/*
    (proofapi, originating thread)
*/
class DynamicEnclaveMeasurement : public IDynamicEnclaveMeasurement
{
    uint8_t *current;
    IDiggiAPI *api;

public:
    DynamicEnclaveMeasurement(uint8_t *init, size_t init_size, IDiggiAPI *dapi);
    DynamicEnclaveMeasurement(IDiggiAPI *dapi);

    ~DynamicEnclaveMeasurement();
    /*
        per thread update.
        Total retrieved by traversing all threads.
    */
    void update(uint8_t *next, size_t size);
    /*
        per thread retrieval.
        Total gathered by traversing all threads.
    */
    uint8_t *get();
};
#endif