/**
 * @file ioStubs.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief Implementation of POSIX tranlation of asynchronous storagemanager operations.
 * Fully threadsafe, however concurrent access to the same file must be synchronized by caller, as per the regular POSIX contract.
 * 
 * The double-pointer-iostub_setresponse-callback pattern ensures that no internal state needs to be handled by this class, 
 * rather the underlying Asynchronous Message Manager is tasked with maintainging context objects and deliveing them to the correct callback.
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "DiggiAssert.h"
#include "storage/IStorageManager.h"
#include "runtime/DiggiAPI.h"
#include "DiggiGlobal.h"
#include "Seal.h"
#include "messaging/Pack.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef DIGGI_ENCLAVE
    /// Used for comparative measurement experiment between exitless encrypted IO and OCALL encrypted IO.
    static SGXSeal *ocall_seal = nullptr;
#endif

#include "posix/io_stubs.h"
#include <errno.h>
    /**
 * @brief ued to specify if POSIX api should be encrypted.
 * 
 */
    static bool encrypted = true;

    void iostub_setcontext(void *ctx, int enc)
    {
        set_errno(0);
#ifdef DIGGI_ENCLAVE
        if (ctx == nullptr && enc)
        {
            ocall_seal = new SGXSeal(CREATOR);
        }
#endif
        encrypted = enc;
    }

    /**
 * @brief set response callback from invocation.
 * Copies message result from asynchronous Storage Manager operation.
 * and sets double pointer input as msg_async_response_t to the resulting copied message.
 * Nofree postfix in function name is redundant as the stub calle should handle freeing the copied object.
 * 
 * @param ptr 
 * @param status 
 */
    void iostub_setresponse(void *ptr, int status)
    {
        auto rsp = (msg_async_response_t *)ptr;
        DIGGI_ASSERT(ptr);
        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);
        DIGGI_TRACE(acontext->GetLogObject(), LDEBUG, "Setting response from call\n");

        /*
		Not a real message from the async message manager
	*/
        msg_t **mr = (msg_t **)rsp->context;
        *mr = COPY(msg_t, rsp->msg, rsp->msg->size);
        auto mm = acontext->GetMessageManager();
        mm->endAsync(rsp->msg);
    }
    /**
 * @brief Synchronous blocking POSIX calls await response from asynchronous StorageManager via this function.
 * Yields to the threadpool, allowing concurrent operations to execute in the interim. 
 * Concurrent operations may include Asyncronous message manager polling for incomming message results.
 * Once the result is present, the iostub_setresponse callback will set the double pointer input to this method.
 * Resulting in the successfull return to calling POSIX stub.
 * 
 * Message is copied and allocated in iostub_setresponse but double pointer is allocated on stack in blocking POSIX call stub.
 * @param ptr double pointer for which to wait for assignment.
 * @return msg_t* 
 */
    msg_t *iostub_wait_for_response(msg_t **ptr)
    {
        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        while (*ptr == nullptr)
        {
            // DIGGI_TRACE(acontext->GetLogObject(), LDEBUG, "Yielding while waiting for response from call\n");

            acontext->GetThreadPool()->Yield();
            // DIGGI_TRACE(acontext->GetLogObject(), LDEBUG, "Returning from Yield\n");
        }
        return *ptr;
    }
    /**
 * @brief After the POSIX call has recieved a response from the storage manager, it processes the content of the message and frees the message content.
 * The message is a copied buffer, so a regular free operation will sufice.
 * if to many response messages are in-flight for processing, overconsumption of enclave heap memory may cause the enclave to crash due to memory exhaustion.
 * 
 * 
 * @param ptr double pointer containing message.
 */
    void iostub_freeresponse(msg_t **ptr)
    {
        DIGGI_ASSERT(*ptr != nullptr);
        free(*ptr);
        *ptr = nullptr;
    }

    /**
 * @brief convenicence method for extracting single int return value response.
 * 
 * @param mptr  double pointer to message object response
 * @return int return value to pass along to application.
 */
    int iostub_extract_retval(msg_t **mptr)
    {

        auto response = iostub_wait_for_response(mptr);
        DIGGI_ASSERT(response != nullptr);

        auto ptr = response->data;
        DIGGI_ASSERT(response->size == sizeof(msg_t) + sizeof(int));
        int retval = 0;
        memcpy(&retval, ptr, sizeof(int));

        iostub_freeresponse(mptr);
        if (retval < 0)
        {
            set_errno(ENOENT);
            errno = ENOENT;
        }
        else
        {
            errno = 0;
        }
        return retval;
    }

    /**
 * @brief check if file mode recieved is a link type
 * Allways returns falls
 * @param m 
 * @return int 
 */
    int s_isln(mode_t m)
    {
        return 0;
    }
    /**
 * @brief check if proc exited correctly
 * Allways returns affirmative (1)
 * 
 * @param n exit status
 * @return int 
 */
    int wifexited(int n)
    {
        return 1; /* Mock process exit condition */
    }

    /**
 * @brief check system configuration parameter 
 * Only supports interogating system page size.
 * Statically set to 4096, matching the encalve page size.
 * Will throw assertion failure if other parameter is checked.
 * @param name parameter to check
 * @return long 
 */
    long i_sysconf(int name)
    {
        if (name == _SC_PAGESIZE)
        {
            return 4096;
        }
        else
        {
            DIGGI_ASSERT(false);
        }
        return 0;
    }

    /**
 * @brief lstat translated to stat, we do not support links
 * @warning silently ignored, may perhaps cause bug hard to catch.
 * @param path path of file
 * @param buf stat buffer which stat populates
 * @return int success value
 */
    int i_lstat(const char *path, struct stat *buf)
    {
        return i_stat(path, buf);
    }
    /**
 * @brief places symbolic link in buf.
 * Allways the same as path, since no links exist
 * @warning applicaitons expecting a correct link implementation may behave incorrectly.
 * @param path path to check
 * @param buf link path result
 * @param bufsiz size of link path buffer input
 * @return ssize_t length of resulting link
 */
    ssize_t i_readlink(const char *path, char *buf, size_t bufsiz)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_readlink\n");

        DIGGI_ASSERT(bufsiz > strlen(path));
        memcpy(buf, path, strlen(path));
        return strlen(path);
    }
    /**
 * @brief change file permisssion bits
 * Noop, as we expect exclusive access for files per trusted runtime, no runtime internal file permissions
 * returns success(0) regardless.
 * @param fildes file descriptor 
 * @param mode mode to set
 * @return int sucess value 
 */
    int i_fchmod(int fildes, mode_t mode)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_fchmod\n");

        return 0;
    }

#ifdef DIGGI_ENCLAVE
    /**
 * @brief conflicting stat symbol with stat struct, trusted func workaround definition for stat.
 * calls i_stat()
 * @see i_stat
 * 
 * @param path 
 * @param buf 
 * @return int 
 */
    int stat(const char *path, struct stat *buf)
    {
        i_stat(path, buf);
    }
#endif
    /**
 * @brief synchronous posix api call for stat
 * 
 * @param path 
 * @param buf 
 * @return int 
 */
    int i_stat(const char *path, struct stat *buf)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_stat path=%s \n", path);

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);
        auto retval = acontext->GetStorageManager()->async_stat(path, buf);

        if (retval < 0)
        {
            set_errno(ENOENT);
            errno = ENOENT;
        }
        else
        {
            errno = 0;
        }
        return retval;
    }

    /**
 * @brief synchronous posix call for fstat
 * 
 * @param fd 
 * @param buf 
 * @return int 
 */
    int i_fstat(int fd, struct stat *buf)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_fstat fd=%d \n", fd);

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);
        auto retval = acontext->GetStorageManager()->async_fstat(fd, buf);

        if (retval < 0)
        {
            set_errno(ENOENT);
            errno = ENOENT;
        }
        else
        {
            errno = 0;
        }
        return retval;
    }
    /**
 * @brief synchronous posix call for access
 * 
 * @param pathname 
 * @param mode 
 * @return int 
 */
    int i_access(const char *pathname, int mode)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_access\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);
        return acontext->GetStorageManager()->async_access(pathname, mode);
    }

    /**
 * @brief synchronous getcwd posix call
 * Allways have CWD be the same virtual directory
 * @param buf 
 * @param size 
 * @return char* 
 */
    char *i_getcwd(char *buf, size_t size)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_getcwd\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);

        return acontext->GetStorageManager()->async_getcwd(buf, size);
    }

    /**
 * @brief synchronous ftruncate call
 * 
 * @param fd 
 * @param length 
 * @return int 
 */
    int i_ftruncate(int fd, off_t length)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_ftruncate\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);

        return acontext->GetStorageManager()->async_ftruncate(fd, length);
    }

    /**
 * @brief synchronous fsync posix call
 * 
 * @param fd 
 * @return int 
 */
    int i_fsync(int fd)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_fsync\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);
        return acontext->GetStorageManager()->async_fsync(fd);
    }
    /**
 * @brief synchronous getenv call.
 * 
 * @param name 
 * @return char* 
 */
    char *i_getenv(const char *name)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_getenv\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);

        return acontext->GetStorageManager()->async_getenv(name);
    }
    /**
 * @brief synchronous getuid call
 * 
 * @return uid_t 
 */
    uid_t i_getuid(void)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_getuid\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);

        return acontext->GetStorageManager()->async_getuid();
    }
    /**
 * @brief synchronous geteuid call
 * 
 * @return uid_t 
 */
    uid_t i_geteuid(void)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_geteuid\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);

        return acontext->GetStorageManager()->async_geteuid();
    }
    /**
 * @brief synchronous fchown call
 * 
 * @param fd 
 * @param owner 
 * @param group 
 * @return int 
 */
    int i_fchown(int fd, uid_t owner, gid_t group)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_fchown\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);

        return acontext->GetStorageManager()->async_fchown(fd, owner, group);
    }
    /**
 * @brief synchronous call for lseek. Does not wait for response.
 * Expects lseek to work correctly, preserves request order at StorageServer
 * @see StorageServer::StorageServer
 * 
 * @param fd 
 * @param offset 
 * @param whence 
 * @return off_t 
 */
    off_t i_lseek(int fd, off_t offset, int whence)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_lseek\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        /*
		To be able to predict offset, we require whence to be SEEK_SET
	*/
#ifdef DIGGI_ENCLAVE
        if (acontext == nullptr)
        {
            off_t ret = 0;
            ocall_lseek(&ret, fd, offset, whence);
            return ret;
        }
#endif
        return acontext->GetStorageManager()->async_lseek(fd, offset, whence);
    }
    /**
 * @brief synchronous posix call for open 
 * 
 * @param path 
 * @param oflags 
 * @param mode 
 * @return int 
 */
    int i_open(const char *path, int oflags, mode_t mode)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_open\n");
        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        /*
        Does not support host os character devices such as dev/random etc.
        (Security voulnerability)
    */
        if (std::string(path).find("dev/") != std::string::npos)
        {
            DIGGI_TRACE(acontext->GetLogObject(), LDEBUG, "open=%s, no character devices allowed\n", path);
            return -1;
        }
#ifdef DIGGI_ENCLAVE

        if (acontext == nullptr)
        {
            int ret = 0;
            ocall_open(&ret, path, oflags, mode);
            return ret;
        }
#endif
        DIGGI_ASSERT(acontext);
        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;
        acontext->GetStorageManager()->async_open(path, oflags, mode, iostub_setresponse, put, encrypted, false);
        auto response = iostub_wait_for_response(put);
        auto ptr = response->data;
        int fd = Pack::unpack<int>(&ptr);
        iostub_freeresponse(put);
        return fd;
    }
    /**
 * @brief synchronus posix call for read
 * 
 * @param fildes 
 * @param buf 
 * @param nbyte 
 * @return ssize_t 
 */
    ssize_t i_read(int fildes, void *buf, size_t nbyte)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_read  fd = %d\n", fildes);

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
#ifdef DIGGI_ENCLAVE

        if (acontext == nullptr)
        {
            int ret = 0;
            auto retval = malloc(ocall_seal->getciphertextsize(nbyte));

            ocall_read(&ret, fildes, (char *)retval, ocall_seal->getciphertextsize(nbyte));
            DIGGI_ASSERT(ret > 0);
            auto value_to_return = (uint8_t *)malloc(nbyte);
            ocall_seal->decrypt((uint8_t *)retval, ocall_seal->getciphertextsize(nbyte), value_to_return, nbyte, 0);
            free(retval);
            memcpy(buf, value_to_return, nbyte);
            free(value_to_return);
            return (ssize_t)nbyte;
        }
#endif
        DIGGI_ASSERT(acontext);
        DIGGI_ASSERT(buf);
        char *ptr = (char *)buf;
        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;
        acontext->GetStorageManager()->async_read(fildes, nullptr, nbyte, iostub_setresponse, put, encrypted, false);
        auto response = iostub_wait_for_response(put);
        DIGGI_ASSERT(response != nullptr);
        auto dtptr = response->data;
        size_t read = 0;
        memcpy(&read, dtptr, sizeof(size_t));

        if (read == 0)
        {
            iostub_freeresponse(put);

            return read;
        }
        DIGGI_ASSERT(read <= nbyte);
        dtptr += sizeof(size_t);
        dtptr += sizeof(off_t);
        DIGGI_ASSERT(read <= (response->size - sizeof(msg_t) - sizeof(size_t) - sizeof(off_t)));
        memcpy(ptr, dtptr, read);
        iostub_freeresponse(put);
        /*short reads must set remaining to 0*/
        memset(ptr + read, 0, nbyte - read);
        return read;
    }
    /**
 * @brief synchronus posix call for write
 * 
 * @param fd 
 * @param buf 
 * @param count 
 * @return ssize_t 
 */
#include "messaging/Util.h"
    ssize_t i_write(int fd, const void *buf, size_t count)
    {

        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_write\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
#ifdef DIGGI_ENCLAVE

        if (acontext == nullptr)
        {
            int ret = 0;
            off_t reto = 0;
            auto retval = malloc(ocall_seal->getciphertextsize(count));

            ocall_read(&ret, fd, (char *)retval, ocall_seal->getciphertextsize(count));
            if (ret)
            {
                auto value_to_return = (uint8_t *)malloc(count);
                ocall_seal->decrypt((uint8_t *)retval, ocall_seal->getciphertextsize(count), value_to_return, count, 0);
                ocall_lseek(&reto, fd, -(ocall_seal->getciphertextsize(count)), SEEK_CUR);
            }
            uint32_t crcmock = 0;
            auto enc_data = ocall_seal->encrypt((uint8_t *)buf, count, ocall_seal->getciphertextsize(count), &crcmock);
            ocall_write(&ret, fd, (char *)enc_data, ocall_seal->getciphertextsize(count));
            free(enc_data);
            free(retval);
            return (ssize_t)count;
        }
#endif
        DIGGI_ASSERT(acontext);
        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;
        acontext->GetStorageManager()->async_write(fd, buf, count, iostub_setresponse, put, encrypted, false);
        auto response = iostub_wait_for_response(put);
        DIGGI_ASSERT(response != nullptr);
        DIGGI_ASSERT(response->size == sizeof(msg_t) + sizeof(ssize_t));
        iostub_freeresponse(put);
        return count;
    }
    /**
 * @brief synchronus posix call for unlink
 * 
 * @param pathname 
 * @return int 
 */
    int i_unlink(const char *pathname)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_unlink\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);
        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;
        acontext->GetStorageManager()->async_unlink(pathname, iostub_setresponse, put);

        return iostub_extract_retval(put);
    }
    /**
 * @brief synchronus posix call for mdir
 * allways relative to CWD even with absolute path
 * does not wait for response, expects success.
 * 
 * @param path 
 * @param mode 
 * @return int 
 */
    int i_mkdir(const char *path, mode_t mode)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_mkdir\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);

        return acontext->GetStorageManager()->async_mkdir(path, mode);
    }
    /**
 * @brief synchronus posix call for rmdir.
 * does not wait for response, expects success.
 * allways relative to CWD, even with absolute path
 * @param path 
 * @return int 
 */
    int i_rmdir(const char *path)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_rmdir");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);

        return acontext->GetStorageManager()->async_rmdir(path);
    }
    /**
 * @brief synchronous posix call for umask
 * 
 * @param mask 
 * @return mode_t 
 */
    mode_t i_umask(mode_t mask)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_umask\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);
        return acontext->GetStorageManager()->async_umask(mask);
    }

    /*
This implementation "cheats" and does not return the FILE* actually created. 
The reason is to keep the FILE struct outside of the enclave, and we are 
assuming enclave code will not need it to function. 
A "cheat" FILE struct will be created on the inside, using the fd from the 
outside call. 
WARNING
*/
    FILE *i_fopen(const char *filename, const char *mode)
    {
        IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(global_context);

        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;

        global_context->GetStorageManager()->async_fopen(filename, mode, iostub_setresponse, put);

        auto response = iostub_wait_for_response(put);
        DIGGI_ASSERT(response != nullptr);

        uint8_t *ptr = response->data;
        DIGGI_ASSERT(response->size == sizeof(msg_t) + sizeof(int));
        int retval = Pack::unpack<int>(&ptr);
        iostub_freeresponse(put);
        if (retval != 0)
        {
            FILE *f = (FILE *)malloc(sizeof(FILE));
            f->_fileno = retval;
            return f;
        }
        else
        {
            return nullptr;
        }
    }

    char *i_fgets(char *str, int num, FILE *stream)
    {
        DIGGI_ASSERT(false);
        return nullptr;
    }

    int i_fclose(FILE *stream)
    {
        DIGGI_ASSERT(stream);
        IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(global_context);
        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;
        // note that the FILE* struct is "fake", see fopen for info.
        global_context->GetStorageManager()->async_fclose(stream, iostub_setresponse, put);

        auto response = iostub_wait_for_response(put);
        DIGGI_ASSERT(response != nullptr);

        uint8_t *ptr = response->data;
        DIGGI_ASSERT(response->size == sizeof(msg_t) + sizeof(int));
        int retval = Pack::unpack<int>(&ptr);
        iostub_freeresponse(put);

        if (stream != nullptr)
        {
            free(stream);
        }

        return retval;
    }
    /**
 * @brief synchronous posix call close.
 * does not wait for response, expects close to succeed.
 * 
 * @param fd 
 * @return int 
 */
    int i_close(int fd)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_close\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(acontext);
        acontext->GetStorageManager()->async_close(fd, false);
        return 0;
    }
    /**
 * @brief synchronous fcntl
 * Noop for file locks, as diggi is single process emulated.
 * @warning GETFD and SETFD is noop aswell, may lead to strange bugs
 * @param fd 
 * @param cmd 
 * @param ... 
 * @return int 
 */
    int i_fcntl(int fd, int cmd, ... /*args */)
    {
        DIGGI_TRACE(GET_DIGGI_GLOBAL_CONTEXT()->GetLogObject(), LDEBUG, "i_fcntl\n");

        auto acontext = GET_DIGGI_GLOBAL_CONTEXT();
        if ((cmd == F_SETLK) || (cmd == F_SETLKW) || (cmd == F_GETLK))
        {
            /*
            diggi only supports single process emualtion,
            therefore file-locks are useless
        */
            return 0;
        }
        if ((cmd == F_GETFD) || ((cmd == F_SETFD)))
        {
            return 0;
        }

        DIGGI_TRACE(acontext->GetLogObject(), LDEBUG, "ERROR: Unexpected fcntl argument cmd=%d\n", cmd);
        DIGGI_ASSERT(false);
        return -1;
    }
    /**
 * @brief synchonous putc posix call.
 * @warning routs call to standard out regardless of file stream.
 * if debugging is turned off, this call will fail silently.
 * @param character 
 * @param stream 
 * @return int 
 */
    int i_fputc(int character, FILE *stream)
    {
        IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
        auto logger = global_context->GetLogObject();
        DIGGI_TRACE(logger, LDEBUG, "%c", (unsigned char)character);
        return character;
    }
    /**
 * @brief synchronous fflush posix call.
 * @warning routs call to standard out regardless of file stream.
 * if debugging is turned off, this call will fail silently.
 * simply adds a new line to standard out.
 * @param stream 
 * @return int 
 */
    int i_fflush(FILE *stream)
    {
        IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_TRACE(global_context->GetLogObject(), LDEBUG, "\n");
        return 0;
    }
    size_t i_fread(void *target, size_t size, size_t count, FILE *stream)
    {
        IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;
        global_context->GetStorageManager()->async_fread(size, count, stream, iostub_setresponse, put);

        auto response = iostub_wait_for_response(put);
        DIGGI_ASSERT(response != nullptr);

        uint8_t *ptr = response->data;
        size_t actual_count = Pack::unpack<size_t>(&ptr);
        if (actual_count > 0)
        {
            Pack::unpackBuffer(&ptr, (uint8_t *)target, actual_count);
        }

        iostub_freeresponse(put);

        return actual_count;
    }

    int i_fseek(FILE *stream, long offset, int whence)
    {
        DIGGI_ASSERT(stream);
        IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(global_context);
        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;
        // note that the FILE* struct is "fake", see fopen for info.
        global_context->GetStorageManager()->async_fseek(stream, offset, whence, iostub_setresponse, put);

        return iostub_extract_retval(put);
    }

    long i_ftell(FILE *stream)
    {
        DIGGI_ASSERT(stream);
        IDiggiAPI *global_context = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(global_context);
        msg_t *retmsg;
        msg_t **put = &retmsg;
        retmsg = nullptr;
        // note that the FILE* struct is "fake", see fopen for info.
        global_context->GetStorageManager()->async_ftell(stream, iostub_setresponse, put);

        auto response = iostub_wait_for_response(put);
        DIGGI_ASSERT(response != nullptr);

        uint8_t *ptr = response->data;
        DIGGI_ASSERT(response->size == sizeof(msg_t) + sizeof(long));
        long retval = Pack::unpack<long>(&ptr);
        iostub_freeresponse(put);

        return retval;
    }
    /// all of the below are not implemented and will throw assertion failiure on use.
    int i_fprintf(FILE *stream, const char *format, ...)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    size_t i_fwrite(const void *ptr, size_t size, size_t count, FILE *stream)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    int i_vfprintf(FILE *stream, const char *format, va_list arg)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    int i_chdir(const char *path)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    int i_dup2(int oldfd, int newfd)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    int i_fseeko(FILE *stream, off_t offset, int whence)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    int i_fputs(const char *str, FILE *stream)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    /* int				i_puts(const char * str){} */

    int i_fgetc(FILE *stream)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    int i_fileno(FILE *stream)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    int i_ferror(FILE *stream)
    {
        DIGGI_ASSERT(false);
        return 0;
    }

    DIR *i_opendir(const char *name)
    {
        DIGGI_ASSERT(false);
        return nullptr;
    }
    struct dirent *i_readdir(DIR *dirp)
    {
        DIGGI_ASSERT(false);
        return nullptr;
    }
    int i_closedir(DIR *dirp)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    int i_dup(int oldfd)
    {
        DIGGI_ASSERT(false);
        return 0;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */
