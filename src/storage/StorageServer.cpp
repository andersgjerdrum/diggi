/**
 * @file StorageServer.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief Untrusted Server recipient of storage requests sent by StorageManager
 * @see StorageManager::StorageManager
 * @version 0.1
 * @date 2020-01-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */

/*only used in libfunc*/
#if !defined(DIGGI_ENCLAVE) && !defined(UNTRUSTED_APP)

#include "storage/StorageServer.h"
#include "messaging/Util.h"
#include "messaging/Pack.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <grp.h>
#include <pwd.h>
#include <stddef.h>
#include <sys/stat.h>

/**
 * @brief Construct a new Storage Server:: Storage Server object
 * Implemented in untrusted runtime, for interfacing encrypted storage requests sent from StorageManager, and translating them to sycalls.
 * A single StorageServer object is not ThreadSafe but multiple instances may be created simultaneously. 
 * Provides identical concurrency guarantees as regular POSIX, implying that threads must manage synchronization of writes explicitly.
 * All requests and responses are handled using unencrypted SecureStorageManager, where each thread is uniquely addressable from client.
 * @param api diggi api 
 */
StorageServer::StorageServer(IDiggiAPI *api) : diggiapi(api),
                                               in_memory(false),
                                               next_fd(4)
{
    in_memory = diggiapi->GetFuncConfig().contains("in-memory");
}

/**
 * Initializes all message types recievable by this class, by registering (callback,type) pairs with the SecureMessageManager.
 * One callback for each of the expected file operations. 
 * Each additionally adds the server object itself as context, since all callbacks are static functions.
 * Must be invoked on thread expected to recieve callbacks. 
 * Will throw assertion if invoked on thread not managed by ThreadPool. Examples include main thread, and google test threads.
 * StorageServer virtualize file position based on expectancy by clients, through the lseekstatemap map, separate from the actual encrypted storage block structure.
 * @see SecureStorageManager::SecureStorageManager
 * @see ThreadPool::ThreadPool
 */
void StorageServer::initializeServer()
{
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoOpen, FILEIO_OPEN, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoRead, FILEIO_READ, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoWrite, FILEIO_WRITE, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoClose, FILEIO_CLOSE, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoUnlink, FILEIO_UNLINK, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoFopen, FILEIO_FOPEN, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoFseek, FILEIO_FSEEK, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoFtell, FILEIO_FTELL, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoFread, FILEIO_FREAD, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::fileIoFclose, FILEIO_FCLOSE, this);
    diggiapi->GetMessageManager()->registerTypeCallback(StorageServer::ServerRand, NET_RAND_MSG_TYPE, this);
}

/**
 * Open request to new or existing file either O_APPEND or at beginning of file.
 * Incomming message includes path relative to CWD, Flags and encrypted flag.
 * If encrypted, O_APPEND must translate encrypted block representation into expected virtual file position
 * If StorageServer is created with in-memory flag set, virtual filedescriptor, inode and path mappings must be initialized.
 * response message includes newly created file descriptor. Used by StorageServer to keep per-descriptor state while file is open.
 * 
 * @param msg incomming request message
 * @param status status flag (unused) future work
 */
void StorageServer::fileIoOpen(void *msg, int status)
{
    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;
    auto ptr = ctx->msg->data;
    mode_t md = Pack::unpack<mode_t>(&ptr);
    int oflags = Pack::unpack<int>(&ptr);
    int encrypted = Pack::unpack<int>(&ptr);
    /* assumes null terminated character */
    const char *path = (const char *)ptr;

    /*direct syscall*/
    int fd = _this->filepaths[std::string(path)];
    if (_this->in_memory)
    {
        if (fd == 0)
        {
            fd = _this->next_fd++;
            _this->filedes_to_path[fd] = std::string(path); //copy
            _this->filepaths[std::string(path)] = fd;
        }
    }
    else
    {
        fd = __real_open(path, oflags, md);
    }
    DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG, "fileIoOpen(path = %s, fd=%d)\n", path, fd);

    off_t start_position = 0;
    if (encrypted)
    {
        uint32_t added_tail_size = 0;
        off_t truestart = 0;
        if (!_this->in_memory)
        {
            char *buf[ENCRYPTION_HEADER_SIZE];

            truestart = __real_lseek(fd, 0, SEEK_END);
            if (truestart > 0)
            {
                if ((truestart % ENCRYPTED_BLK_SIZE) == 0)
                {
                    __real_lseek(fd, truestart - ENCRYPTED_BLK_SIZE, SEEK_SET);
                    ssize_t ret = 0;
                    ret = __real_read(fd, buf, ENCRYPTION_HEADER_SIZE);
                    DIGGI_ASSERT(ENCRYPTION_HEADER_SIZE == ret);
                    added_tail_size = ((sgx_sealed_data_t *)buf)->aes_data.payload_size;
                }
            }
        }
        else
        {
            truestart = _this->in_memory_data[fd].size();
            if (truestart > 0)
            {
                auto buf = _this->in_memory_data[fd].substr(truestart - ENCRYPTED_BLK_SIZE).getptr(ENCRYPTED_BLK_SIZE);
                added_tail_size = ((sgx_sealed_data_t *)buf)->aes_data.payload_size;
            }
        }

        DIGGI_ASSERT(added_tail_size <= SPACE_PER_BLOCK);
        if (truestart > 0)
        {
            start_position = (((truestart / ENCRYPTED_BLK_SIZE) - 1) * SPACE_PER_BLOCK) + added_tail_size;
        }
    }
    else
    {
        start_position = __real_lseek(fd, 0, SEEK_END);
    }
    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int) + sizeof(off_t));
    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;
    auto ptrt = msg_n->data;
    Pack::pack<int>(&ptrt, fd);
    Pack::pack<off_t>(&ptrt, start_position);
    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void StorageServer::fileIoFopen(void *msg, int status)
{
    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;
    uint8_t *ptr = ctx->msg->data;

    size_t namelength = Pack::unpack<size_t>(&ptr);
    char filename[namelength + 1];
    memset((void *)filename, 0, namelength + 1);
    Pack::unpackBuffer(&ptr, (uint8_t *)filename, (unsigned int)namelength);
    size_t modelength = Pack::unpack<size_t>(&ptr);
    char mode[modelength + 1];
    memset((void *)mode, 0, modelength + 1);
    Pack::unpackBuffer(&ptr, (uint8_t *)mode, (unsigned int)modelength);

    FILE *f = __real_fopen(filename, mode);

    short retval = 0;
    if (f != NULL)
    {
        _this->openfilesmap[f->_fileno] = f;
        retval = f->_fileno; // should be an int representing fd, me thinks.
    }
    else
    {
        printf("StorageServer: fopen failed with errno %d\n", errno);
    }
    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;
    auto *dataPtr = msg_n->data;
    Pack::pack<int>(&dataPtr, retval);

    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void StorageServer::fileIoFseek(void *msg, int status)
{
    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;
    uint8_t *ptr = ctx->msg->data;

    short fd = Pack::unpack<short>(&ptr);
    long offset = Pack::unpack<long>(&ptr);
    int whence = Pack::unpack<int>(&ptr);

    int retval = __real_fseek(_this->openfilesmap[fd], offset, whence);

    if (retval != 0)
    {
        printf("StorageServer: fseek failed with errno %d\n", errno);
    }

    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;
    auto *dataPtr = msg_n->data;
    Pack::pack<int>(&dataPtr, retval);

    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void StorageServer::fileIoFtell(void *msg, int status)
{
    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;
    uint8_t *ptr = ctx->msg->data;

    short fd = Pack::unpack<short>(&ptr);

    long retval = __real_ftell(_this->openfilesmap[fd]);

    if (retval == -1)
    {
        printf("StorageServer: fseek failed with errno %d\n", errno);
    }

    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(long));
    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;
    auto *dataPtr = msg_n->data;
    Pack::pack<long>(&dataPtr, retval);

    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void StorageServer::fileIoFread(void *msg, int status)
{
    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;
    uint8_t *ptr = ctx->msg->data;

    size_t size = Pack::unpack<size_t>(&ptr);
    size_t count = Pack::unpack<size_t>(&ptr);
    short fd = Pack::unpack<short>(&ptr);

    char buffer[size * count];

    size_t actual_read = __real_fread((void *)buffer, size, count, _this->openfilesmap[fd]);
    if (actual_read != size * count)
    {
        printf("StorageServer: fread did not serve requested amount of bytes (%d requested vs %d served. errno is %d\n", (int)(size * count), (int)actual_read, errno);
    }

    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(size_t) + actual_read);
    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;
    auto *dataPtr = msg_n->data;

    Pack::pack<size_t>(&dataPtr, actual_read);
    Pack::packBuffer(&dataPtr, (uint8_t *)buffer, actual_read);

    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void StorageServer::fileIoFclose(void *msg, int status)
{
    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;
    uint8_t *ptr = ctx->msg->data;

    short fd = Pack::unpack<short>(&ptr);
    int retval = __real_fclose(_this->openfilesmap[fd]);

    if (retval == 0)
    {
        int erased = _this->openfilesmap.erase(fd);
        if (erased == 0)
        {
            printf("StorageServer: Error in removing fd %d from open files map\n", fd);
        }
    }
    else
    {
        printf("StorageServer: fclose could not close file %d. errno is %d\n", fd, errno);
    }

    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;
    auto *dataPtr = msg_n->data;
    Pack::pack<int>(&dataPtr, retval);

    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

/**
 * Read request operation for open file.
 * Request message contains file descriptor, number of bytes to read, and read type, specified as read_type_t.
 * Read type may be SEEKBACK or NOSEEK. SEEKBACK used for read preceding write operation, where lseek rewinds file position before write.
 * @see StorageManager::async_read_internal
 * lseek translates read between virtual file position specified by lseekstatemap and the physical representation in encrypted block storage.
 * If read to end of file, return message specifies partial block, to ensure correc decryption and coaleasing procedure inside the trusted runtime.
 * Does not move lseek forward if read_type_t is SEEKBACK.
 * Return message also specify file position.
 * @param msg incomming request message
 * @param status status flag (unused) future work
 */
void StorageServer::fileIoRead(void *msg, int status)
{

    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;
    DIGGI_ASSERT(ctx->msg->size == (sizeof(msg_t) + sizeof(int) + sizeof(size_t) + sizeof(size_t) + sizeof(int)));

    auto ptr = ctx->msg->data;
    int fd = Pack::unpack<int>(&ptr);
    size_t total_read_size = Pack::unpack<size_t>(&ptr);
    size_t phys_pos = Pack::unpack<size_t>(&ptr);
    Pack::unpack<int>(&ptr);
    DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG, "fileIoRead fd=%d, total_read_size=%lu\n", fd, total_read_size);

    int end_of_file = 0;
    off_t origsize = 0;
    if (!_this->in_memory)
    {
        origsize = __real_lseek(fd, 0, SEEK_END);
        DIGGI_ASSERT(origsize >= 0);
        if ((size_t)origsize <= phys_pos + total_read_size)
        {
            end_of_file = 1;
        }
        if (origsize == 0)
        {
            total_read_size = 0;
        }
        else
        {
            __real_lseek(fd, phys_pos, SEEK_SET);
        }
    }
    else
    {
        origsize = _this->in_memory_data[fd].size();
        DIGGI_ASSERT(origsize >= 0);

        if ((size_t)origsize <= phys_pos + total_read_size)
        {
            end_of_file = 1;
        }
        if (origsize == 0)
        {
            total_read_size = 0;
        }
    }

    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, total_read_size + sizeof(size_t) + sizeof(int));

    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;

    auto ptr_data = msg_n->data + sizeof(size_t) + sizeof(int);
    size_t retval = 0;
    if (_this->in_memory)
    {

        retval = std::min(total_read_size, _this->in_memory_data[fd].size() - phys_pos);
        if (retval > 0)
        {
            _this->in_memory_data[fd].substr(phys_pos, retval).copy((char *)ptr_data, total_read_size);
        }
    }
    else
    {
        retval = __real_read(fd, ptr_data, total_read_size);
    }

    auto ptrret = msg_n->data;
    Pack::pack<size_t>(&ptrret, retval);
    Pack::pack<int>(&ptrret, end_of_file);
    /* 
        Piggyback file positon action on read to avoid lseek issued 
        from between read and write when doing an encrypted write
    */
    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

/**
 * Write operation request handler
 * Recieves a write request from StorageManager.
 * request message includes file descriptor, and original unencrypted write size and encryotion boolean flag.
 * If in-memory mode active, write to mutable memory buffer, either encrypted or plaintext, depending on mode.
 * Will in regular mode, write encrypted blocks to disc via syscall, and emulate corresponding fileposition update according to expected application behaviour.
 * Returns written bytes as seen by application.
 * 
 * @param msg incomming request message
 * @param status status flag (unused) future work
 */
void StorageServer::fileIoWrite(void *msg, int status)
{

    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;

    auto ptr = ctx->msg->data;
    int fd = Pack::unpack<int>(&ptr);
    DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG, "fileIoWrite fd=%d\n", fd);

    int encrypted = Pack::unpack<int>(&ptr);
    size_t phys_pos = Pack::unpack<size_t>(&ptr);
    size_t writesize = ctx->msg->size - (sizeof(msg_t) + sizeof(int) + sizeof(size_t) + sizeof(int));

    if (!_this->in_memory)
    {
        __real_lseek(fd, phys_pos, SEEK_SET);
    }
    if (encrypted)
    {
        DIGGI_ASSERT(writesize % ENCRYPTED_BLK_SIZE == 0);
    }
    ssize_t retval = 0;
    if (!_this->in_memory)
    {
        retval = __real_write(fd, ptr, writesize);
    }
    else
    {
        if (_this->in_memory_data[fd].size() < phys_pos + writesize)
        {
            auto chunk_data = _this->in_memory_data[fd].reserve(writesize);
            memset(chunk_data, 0, writesize);
            _this->in_memory_data[fd].append((char *)chunk_data, writesize);
        }
        _this->in_memory_data[fd].substr(phys_pos, writesize).replace((char *)ptr, writesize);
        retval = writesize;
    }
    DIGGI_ASSERT((size_t)retval == ctx->msg->size - (sizeof(msg_t) + sizeof(int) + sizeof(size_t) + sizeof(int)));
    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(ssize_t));

    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;
    ptr = msg_n->data;
    Pack::pack<ssize_t>(&ptr, retval);
    /*must update with original write size*/

    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

/**
 * Close file request handler.
 * input request message contains file descriptor.
 * Deallocates file position info for particular file.
 * If in memory, does not close file.
 * In memory files persist beyond close operations, but if StorageServer is destroyed, the files will be purged.
 * Asynchronous, caller does not wait for response, expects successfull operation.
 * 
 * @param msg incomming request message
 * @param status status flag (unused) future work
 */
void StorageServer::fileIoClose(void *msg, int status)
{

    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;

    auto ptr = ctx->msg->data;
    int fd = Pack::unpack<int>(&ptr);

    DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG, "fileIoClose fd=%d\n", fd);
    _this->filepaths[_this->filedes_to_path[fd]] = 0;

    if (!_this->in_memory)
    {
        __real_close(fd);
    }
}

/**
 * Unlink request handler unlinks file according to path specified in request message.
 * Path allways relative to CWD.
 * If in-memory all file data and metadata stored in DRAM is purged.
 * Returns success flag.
 * 
 * @param msg incomming request message
 * @param status status flag (unused) future work
 */
void StorageServer::fileIoUnlink(void *msg, int status)
{
    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;

    auto ptr = ctx->msg->data;

    const char *path = (const char *)ptr;
    DIGGI_TRACE(_this->diggiapi->GetLogObject(), LDEBUG, "fileIoUnlink path %s\n", path);

    int retval = 0;
    int fd = _this->filepaths[std::string(path)];
    _this->filedes_to_path.erase(fd);
    _this->filepaths.erase(std::string(path));
    if (!_this->in_memory)
    {
        retval = __real_unlink(path);
    }

    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
    ptr = msg_n->data;
    Pack::pack<int>(&ptr, retval);
    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;
    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

void StorageServer::ServerRand(void *msg, int status)
{
    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    auto _this = (StorageServer *)ctx->context;

    int retval = __real_rand();
    if (retval < 0)
    {
        _this->diggiapi->GetLogObject()->Log(LogLevel::LRELEASE, "StorageServer failed in rand, with error %d: %s\n", errno, strerror(errno));
    }

    auto msg_n = _this->diggiapi->GetMessageManager()->allocateMessage(ctx->msg, sizeof(int));
    msg_n->src = ctx->msg->dest;
    msg_n->dest = ctx->msg->src;

    auto *dataPtr = msg_n->data;
    Pack::pack<int>(&dataPtr, retval);

    _this->diggiapi->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}

#endif