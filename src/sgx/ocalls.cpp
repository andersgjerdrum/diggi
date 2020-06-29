/**
 * @file ocalls.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief all outbound call exit points for diggi enclave code executing in the untrusted runtime
 * @version 0.1
 * @date 2020-01-29
 *
 * @copyright Copyright (c) 2020
 *
 */
#if defined(UNTRUSTED_APP)

#include "sgx/ocalls.h"
#include "messaging/Util.h"

/**
 * @brief print string facility for logging infrastructure, called by StdLogger
 * @see StdLogger::StdLogger
 *
 * @param name enclave invokers name
 * @param str string to write
 * @param threadid id of thread inside enclave (enclave relative thread)
 * @param enc_id Identity of enclave
 */
void ocall_print_string_diggi(const char *name, const char *str, int threadid, uint64_t enc_id)
{
    /* Proxy/Bridge will check the length and null-terminate
	* the input string to prevent buffer overflow.
	*/

    printf("[func-S: %s\t (%" PRIu64 "), Thread: %d] -: %s", name, enc_id, threadid, str);
    fflush(stdout);
}
/**
 * @brief print string for enclaves without proper diggi runtime support.
 *
 * @param str string to print
 * @return int success value
 */
int ocall_print_string(const char *str)
{

    ocall_print_string_diggi("unknown_func", str, thr_id(), 0);
    return 0;
}
/**
 * @brief assertion call for raising signals in the event of an assertion failure inside the trusted runtime.
 *
 */
void ocall_sig_assert(void)
{
    /*assert occured in enclave code, cause assertion outside*/
    raise(SIGABRT);
}
/**
 * Sleep function for allowing unused threads to be reclaimed by host operating system in the event of a dormant trusted runtime.
 * Used for linear backof algorithm in AsyncMessageManager.
 *
 * @see AsyncMessageManager::async_message_pump
 * @param usec microseconds for sleep
 */
void ocall_sleep(uint64_t usec)
{
    usleep(usec);
}

/**
 * @brief for experimental measurements of intervals in runtime internals, tags identify point of sampling.
 *
 * @param tag
 */
void ocall_telemetry_capture(const char *tag)
{
    telemetry_capture(tag);
}

/**
 * @brief Constructing first message in remote attestation protocol.
 *
 * @param message message inbound to trusted runtime.
 */
void ocall_prepare_msg0(void *message)
{
    uint32_t extended_epid_group_id = 0;
    auto a_con = GET_DIGGI_GLOBAL_CONTEXT();
    DIGGI_ASSERT(a_con);
    DIGGI_ASSERT(message);

    sgx_status_t ret = sgx_get_extended_epid_group_id(&extended_epid_group_id);
    if (SGX_SUCCESS != ret)
    {
        a_con->GetLogObject()->Log(LRELEASE, "ERROR, call sgx_get_extended_epid_group_id failed");
        DIGGI_ASSERT(false);
    }
    ra_samp_request_header_t *p_msg0_full = (ra_samp_request_header_t *)(((msg_t *)message)->data);

    p_msg0_full->type = TYPE_RA_MSG0;
    p_msg0_full->size = sizeof(uint32_t);

    *(uint32_t *)((uint8_t *)p_msg0_full + sizeof(ra_samp_request_header_t)) = extended_epid_group_id;
}

/**
 * @brief Constructing 2nd message in remote attestation protocol
 *
 * @param message message inbound to trusted runtime
 * @param enclave_id enclave id in question
 * @param context remote attestation context
 */
void ocall_prepare_msg1(void *message, sgx_enclave_id_t enclave_id, sgx_ra_context_t context)
{
    auto a_con = GET_DIGGI_GLOBAL_CONTEXT();
    DIGGI_ASSERT(a_con);
    DIGGI_ASSERT(message);
    DIGGI_ASSERT(context < INT_MAX);

    sgx_status_t ret = sgx_ra_get_msg1(context, enclave_id, sgx_ra_get_ga,
                                       (sgx_ra_msg1_t *)message);
    if (SGX_SUCCESS != ret)
    {
        a_con->GetLogObject()->Log(LRELEASE, "ERROR=0x%x, enc_id=%lu call sgx_ra_get_msg1 failed\n", ret, enclave_id);
        DIGGI_ASSERT(false);
    }
}

/**
 * @brief Process 2nd and prepare 3rd message in remote attestation protocol
 *
 * @param context remote attestation context
 * @param enclave_id enclave identity
 * @param msg3 3nd message body
 * @param msg2 2rd message buffer
 * @param msg2_size length of 2nd message body
 */
sgx_ra_msg3_t* ocall_prepare_msg2(sgx_ra_context_t context,
                            sgx_enclave_id_t enclave_id,
                            void *msg2,
                            size_t msg2_size)
{
    auto a_con = GET_DIGGI_GLOBAL_CONTEXT();

    DIGGI_ASSERT(a_con);

    uint32_t msg3_size = 0;
    sgx_ra_msg3_t *ptr = NULL;

    sgx_status_t ret = sgx_ra_proc_msg2(context,
                                        enclave_id,
                                        sgx_ra_proc_msg2_trusted,
                                        sgx_ra_get_msg3_trusted,
                                        (sgx_ra_msg2_t *)msg2,
                                        msg2_size,
                                        &ptr,
                                        &msg3_size);
    if (!ptr)
    {
        a_con->GetLogObject()->Log(LRELEASE, "\nError, call sgx_ra_proc_msg2 fail. "
                                             "p_msg3 = 0x%p [%s].",
                                   ptr, __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    if (SGX_SUCCESS != ret)
    {
        a_con->GetLogObject()->Log(LRELEASE, "\nError, call sgx_ra_proc_msg2 fail. "
                                             "ret = 0x%08x [%s].",
                                   ret, __FUNCTION__);
        DIGGI_ASSERT(false);
    }
    DIGGI_ASSERT(msg3_size);
    return ptr;
}

/**
 * @brief test call for comparing exit-less io and ocalls
 * NB: for testing purposes only, not to be used in prod
 * @param path
 * @param oflags
 * @param mode
 * @return int
 */
int ocall_open(const char *path, int oflags, mode_t mode)
{
    return open(path, oflags, mode);
}

/**
 * @brief test call for comparing exit-less io and ocalls
 * NB: for testing purposes only, not to be used in prod
 * @param fildes
 * @param buf
 * @param nbyte
 * @return int
 */
int ocall_read(int fildes, char *buf, size_t nbyte)
{
    return read(fildes, buf, nbyte);
}

/**
 * @brief test call for comparing exit-less io and ocalls
 * NB: for testing purposes only, not to be used in prod
 * @param fd
 * @param buf
 * @param count
 * @return int
 */
int ocall_write(int fd, char *buf, size_t count)
{
    return write(fd, buf, count);
}

/**
 * @brief test call for comparing exit-less io and ocalls
 * NB: for testing purposes only, not to be used in prod
 * @param fd
 * @param offset
 * @param whence
 * @return off_t
 */
off_t ocall_lseek(int fd, off_t offset, int whence)
{
    return lseek(fd, offset, whence);
}

#endif