#include "storage/StorageManager.h"
#include "storage/StorageServer.h"
#include "messaging/Util.h"
#include "messaging/Pack.h"
//#include "mbed.h"
#include "mbedtls/sha256.h"

/** 
 * @file StorageManager.cpp
 * @brief Asynchronous Object Storage API
 * @details
 * The StorageManager acts as the client requestor for storage resources delivered by the untrusted operating system.
 * Requests for storage operations are translated and encrypted by the StorageManager,
 *  which is situated inside of the trust boundary. 
 * Requests are sent to the StorageServer which recides in regular process memory.
 * Diggi applications may use this class directly, or translated through the POSIX interface implemented in ioStubs.cpp
 * The StorageManager preserves no state across invocations of its member functions and is threadsafe. 
 * However, paralell access operations to the same objects may lead to unexpected behaviour.
 * Concurrent access to a single state object from multiple threads, must be synchronized by the caller.
 * All message passing uses cleartext mode of the SecureMessageManager, as block encryption handles confidentiallity.
 * @see SecureMessageManager::SecureMessageManager
 * @see ioStubs.cpp
 * @see StorageServer::StorageServer
 * @author Anders T. Gjerdrum
 * 
*/

/**
 * Creates an asynchronous Storage Object Manager. Situated inside enclave memory.
 * @param context Diggi API reference
 * @param seal Algorithm type reference for determining which algorithm to use for block encryption of storage.
 */
StorageManager::StorageManager(IDiggiAPI *context, ISealingAlgorithm *seal)
    : func_context(context),
      sealer(seal),
      monotonic_time_update(1566911621),
      next_virtual_inode(100000)

{
}
void StorageManager::GetCRCReplayVector(crc_vector_t **vectors)
{
    *vectors = &block_2_crc;
}

void StorageManager::SetCRCReplayVector(crc_vector_t *vectors)
{
    block_2_crc = *vectors;
}

/**
 * Function to translate from absolute to relative path for file accesses via POSIX interface.
 * We avoid absolute path references to the untrusted system, 
 * and assume all storage interaction occurs through a subfolder of the host process´s current working directory
 * Uses in-place replacement to avoid memory management, return pointer is the same.
 * @warning "path" should be const, do not use return for free operation as it increments pointer
 * 
 * @param path 
 * @return char* 
 */
char *StorageManager::normalizePath(char *path)
{
    DIGGI_ASSERT(path);
    /*
		Assume root directory and remove leading backspace to make request relative.
	*/

    if (path[0] == '/')
    {
        /*
			if request for current directory fd, let it through
		*/
        if (strlen(path) > 1)
        {
            path++;
        }
    }
    return path;
}
/**
 * async stat file operation request translated to request message.
 * argument path is marshaled onto message object and sent.
 * callback invoked upon request completion
 * @warning pathname is sent in cleartext, do not use filename for secrets!
 * @param path path to stat
 * @return callcack returns response message as parameter, must be un-marshalled by callback function.
*/
int StorageManager::async_stat(const char *path, struct stat *buf)
{
    char *path_n = normalizePath((char *)path);
    int fd = filepaths[std::string(path_n)];
    // printf("stat path =%s\n", path_n);
    return async_fstat(fd, buf);
}

/**
 * async fstat file operation request translated to request message.
 * argument path is marshaled onto message object and sent.
 * callback invoked upon request completion
 * @param fd filedescriptor of open file subject to fstat request
 * @param buf stat buf, ignored, requester must un-marshal responsemessage returned in completed callback
 * @param cb completed request callback, Must be async_cb_t type
 * @param context custom context pointer for surfacing on completed callback, at callers discression, can be NULL.
 * @param encrypted specify if the file requested an encrypted file, or plaintext, will cause assertion failiure if missmatched.
 */
int StorageManager::async_fstat(int fd, struct stat *buf)
{
    while (this->pending_write_map[fd] > 0)
    {
        func_context->GetThreadPool()->Yield();
    }
    memset(buf, 0, sizeof(struct stat));
    if (fd > 0)
    {
        buf->st_ino = inodes[fd];
        buf->st_nlink = 1;
        buf->st_mode = (filedes_to_path[fd].back() == '/') ? 33188 | S_IFDIR : 33188;
        buf->st_blksize = FILESYSTEM_BLK_SIZE;
        buf->st_uid = 1000;
        buf->st_gid = 1000;
        buf->st_dev = 2050;
        buf->st_ctim.tv_sec = ++monotonic_time_update;
        buf->st_ctim.tv_nsec = 137859984;
        buf->st_atim.tv_sec = monotonic_time_update;
        buf->st_atim.tv_nsec = 137859984;
        buf->st_mtim.tv_sec = monotonic_time_update;
        buf->st_mtim.tv_nsec = 137859984;
        // printf("putting size = %lld\n", size_of_file[fd]);
        buf->st_size = size_of_file[fd];
        buf->st_blocks = (roundUp_r(buf->st_size, FILESYSTEM_BLK_SIZE)) / FILESYSTEM_BLK_SIZE;
    }
    else
    {
        // printf("not found\n");
    }
    return (fd > 0) ? 0 : -1;
}

typedef struct AsyncContext<async_cb_t, void *, StorageManager *, string> fopen_ctx_t;

void StorageManager::async_fopen(const char *filename, const char *mode, async_cb_t cb, void *context)
{
    // printf("StorageManager::async_fopen, %s : %s\n", filename, mode);
    DIGGI_ASSERT(func_context);
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "fopen: filename = %s\n", filename);
    auto mngr = func_context->GetMessageManager();

    auto hash = func_context->GetFuncConfig()[filename];

    if (hash.value.size() == 0)
    { //
        // printf("file does not exist in configuration\n");
        //zcstring file_hash = global_context->GetFuncConfig()[filename].value.tostring();
        // send null message back to ioStubs without passing through server
        auto resp = new msg_async_response_t();
        resp->msg = ALLOC_P(msg_t, sizeof(int));
        resp->msg->size = sizeof(msg_t) + sizeof(int);
        resp->msg->id = 1979; //magic
        resp->context = context;
        auto *dataPtr = resp->msg->data;

        Pack::pack<int>(&dataPtr, 0); // 0 means could not open file
        cb(resp, 1);

        free(resp->msg);
        resp->msg = nullptr;
        delete (resp);
        resp = nullptr;
    }
    else
    {

        auto msg = mngr->allocateMessage("file_io_func", sizeof(size_t) + strlen(filename) + sizeof(size_t) + strlen(mode), CALLBACK, CLEARTEXT);
        msg->type = FILEIO_FOPEN;

        uint8_t *ptr = msg->data;
        Pack::pack<unsigned long>(&ptr, strlen(filename));
        Pack::packBuffer(&ptr, (uint8_t *)filename, (unsigned int)strlen(filename));
        Pack::pack<unsigned long>(&ptr, strlen(mode));
        Pack::packBuffer(&ptr, (uint8_t *)mode, (unsigned int)strlen(mode));

        string filename_str(filename);

        mngr->Send(msg, async_fopen_cb, new fopen_ctx_t(cb, context, this, filename));
    }
}

void StorageManager::async_fopen_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto async_id = resp->msg->id;
    DIGGI_ASSERT(resp);
    auto ctx = (fopen_ctx_t *)resp->context;
    DIGGI_ASSERT(ctx);
    auto _this = ctx->item3;
    DIGGI_ASSERT(_this);
    auto msg = resp->msg;
    DIGGI_ASSERT(msg);
    std::string filename = (std::string)ctx->item4;

    short fd;
    memcpy(&fd, resp->msg->data, sizeof(short));

    // printf("async_fopen_cb connecting fd %d to filename %s\n", fd, filename.c_str());

    _this->fd_to_filename_map[fd] = filename;

    resp->msg->id = async_id; // wtf?
    resp->context = ctx->item2;
    ctx->item1(resp, status);

    delete ctx; // is this the right place to delete?
}

void StorageManager::async_fseek(FILE *f, off_t offset, int whence, async_cb_t cb, void *context)
{
    // printf("StorageManager::async_fseek\n");
    DIGGI_ASSERT(func_context);
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "fseek: f = %s\n", f);

    auto mngr = func_context->GetMessageManager();
    auto msg = mngr->allocateMessage("file_io_func", sizeof(short) + sizeof(off_t) + sizeof(int), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FSEEK;

    uint8_t *ptr = msg->data;

    Pack::pack<short>(&ptr, f->_fileno);
    Pack::pack<off_t>(&ptr, offset);
    Pack::pack<int>(&ptr, whence);

    mngr->Send(msg, cb, context);
}

void StorageManager::async_ftell(FILE *f, async_cb_t cb, void *context)
{
    // printf("StorageManager::async_ftell\n");
    DIGGI_ASSERT(func_context);
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "ftell: f = %s\n", f);

    auto mngr = func_context->GetMessageManager();
    auto msg = mngr->allocateMessage("file_io_func", sizeof(short), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FTELL;

    uint8_t *ptr = msg->data;
    Pack::pack<short>(&ptr, f->_fileno);

    mngr->Send(msg, cb, context);
}

typedef struct AsyncContext<async_cb_t, void *, StorageManager *, short> fread_ctx_t;

void StorageManager::async_fread(size_t size, size_t count, FILE *f, async_cb_t cb, void *context)
{
    DIGGI_ASSERT(func_context);
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "async_fread\n");
    auto mngr = func_context->GetMessageManager();

    auto msg = mngr->allocateMessage("file_io_func", sizeof(size_t) + sizeof(size_t) + sizeof(int), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FREAD;

    uint8_t *ptr = msg->data;
    Pack::pack<size_t>(&ptr, size);
    Pack::pack<size_t>(&ptr, count);
    Pack::pack<short>(&ptr, f->_fileno);

    // cb is where the returned message will go eventually
    // ascync_fread_cb is a detour that checks the returned file content
    // towards the file checksum
    auto cb_context = new fread_ctx_t(cb, context, this, f->_fileno);

    mngr->Send(msg, async_fread_cb, cb_context);
}

void StorageManager::async_fread_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto async_id = resp->msg->id;
    DIGGI_ASSERT(resp);
    auto ctx = (fread_ctx_t *)resp->context;
    DIGGI_ASSERT(ctx);
    auto _this = ctx->item3;
    DIGGI_ASSERT(_this);
    auto msg = resp->msg;
    DIGGI_ASSERT(msg);
    short fd = (short)ctx->item4;
    uint8_t *data = resp->msg->data;
    size_t bytes_read = Pack::unpack<size_t>(&data);

    void *buffer;
    if (bytes_read > 0)
    {
        buffer = malloc(bytes_read);
        Pack::unpackBuffer(&data, (uint8_t *)buffer, bytes_read);
    }

    std::string filename = _this->fd_to_filename_map[fd];
    std::string content_str((char *)buffer);

    unsigned char output[32]; // SHA-256 outputs 32 bytes
    int retval = mbedtls_sha256_ret((const unsigned char *)buffer, bytes_read, output, 0);
    DIGGI_ASSERT(retval == 0);

    auto config = _this->func_context->GetFuncConfig();

    // printf("async_fread_cb: comparing checksums for fd=%d and filename '%s'\n", fd, filename.c_str());

    char *charoutput = (char *)malloc(64); // 64 characters + null term from sprintf
    char *tmptr = charoutput;

    for (int i = 0; i < 32; i++)
    {
        sprintf(tmptr, "%02x", output[i]);
        tmptr += 2;
    }

    //printf("async_fread_cb: stored checksum is: '%s'\n", );
    //printf("async_fread_cb: calced checksum is: '%s'\n", charoutput);
    //printf("%s\n", config_hash);

    if (strcmp(charoutput, config[filename].value.tostring().c_str()) != 0)
    {
        // printf("async_fread_cb: Checksum is INCORRECT. Test FAILED!\n");
        DIGGI_ASSERT(false);
    }
    //else {printf("async_fread_cb: Checksum is correct. Test passed!\n");}

    free(charoutput);
    free(buffer);

    resp->msg->id = async_id; // wtf?
    resp->context = ctx->item2;
    ctx->item1(resp, status);

    delete ctx;
}

void StorageManager::async_fclose(FILE *f, async_cb_t cb, void *context)
{
    // printf("StorageManager::async_fclose\n");
    DIGGI_ASSERT(func_context);
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "fclose: f = %s\n", f);

    auto mngr = func_context->GetMessageManager();
    auto msg = mngr->allocateMessage("file_io_func", sizeof(short), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FCLOSE;

    uint8_t *ptr = msg->data;
    Pack::pack<short>(&ptr, f->_fileno);

    mngr->Send(msg, cb, context);
}

/**
 * Close file operation request, expects no response info.
 * Callback invoked upon successfull completion.
 * 
 * @param fd file subject to close
 * @param cb completion callback
 * @param context calle defined context object, served to completion callback
 */

void StorageManager::async_close(int fd, bool omit_from_log)
{
    while (this->pending_write_map[fd] > 0)
    {
        func_context->GetThreadPool()->Yield();
    }
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "close\n");
    auto mngr = func_context->GetMessageManager();
    auto msg = mngr->allocateMessage("file_io_func", sizeof(int), REGULAR, CLEARTEXT);
    DIGGI_ASSERT(lseekstatemap.find(fd) != lseekstatemap.end());

    lseekstatemap.erase(fd);
    filepaths[filedes_to_path[fd]] = 0;
    msg->type = FILEIO_CLOSE;
    uint8_t *ptr = msg->data;
    msg->omit_from_log = omit_from_log;
    /*Marshall*/
    Pack::pack<int>(&ptr, fd);
    mngr->Send(msg, nullptr, nullptr);
}
/**
 * Access file request operation.
 * Callback invoked upon response.
 * Callback must manage un-marshaling arguments.
 * @param pathname path to access ( cleartext, do not add secrets in this parameter)
 * @param mode mode of access
 * @param cb completion callback
 * @param context calle defined context object, served to completion callback.
 */
int StorageManager::async_access(const char *pathname, int mode)
{

    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "access\n");
    char *path_n = normalizePath((char *)pathname);

    return (filepaths[std::string(path_n)] > 0) ? 0 : 1;
}
/**
 * Simulates an file system mounted under the current working directory, for posix requests.
 * Synchronous, returns immediately.
 * @param buf buffer populated by path
 * @param size size of populated buffer
 * @return char* returns buf
 */
char *StorageManager::async_getcwd(char *buf, size_t size)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "getcwd\n");
    /*waiting for reply*/
    const char cwd_ptr[] = "";

    if (size < sizeof(cwd_ptr))
    {
        return NULL;
    }

    memcpy(buf, cwd_ptr, sizeof(cwd_ptr));

    return buf;
}
/**
 * Not implemented
 * Will throw assertion if invoked.
 * 
 * @param fd 
 * @param length 
 * @return int 
 */
int StorageManager::async_ftruncate(int fd, off_t length)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "ftruncate\n");
    DIGGI_ASSERT(false);

    return 0;
}

/**
 * request for  fcntl operation on open file.
 * Currently only supports file lock operations.
 * Cannot duplicate file descriptors or set filedescriptor flags.
 * As long as fcntl returns 0, caller will be happy
 * @param fd file descriptor 
 * @param cmd lock or unlock command
 * @param lock lock structure, marshaled and sent to storage server
 */
int StorageManager::async_fcntl(int fd, int cmd, struct flock *lock)
{
    return 0;
}
/**
 * fsync operation forcing filesystem to flush blocks belonging to file to disk.
 * We mock it for convenience
 * @param fd file to flush
 */
int StorageManager::async_fsync(int fd)
{
    while (this->pending_write_map[fd] > 0)
    {
        func_context->GetThreadPool()->Yield();
    }
    return 0;
}
/**
 * Not implemented
 * will throw assertion if invoked
 * 
 * @param name 
 * @return char* 
 */
char *StorageManager::async_getenv(const char *name)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "getenv\n");
    DIGGI_ASSERT(false);

    return nullptr;
}
/**
 * Not implemented
 * will throw assertion if invoked
 * 
 * @return uid_t 
 */
uid_t StorageManager::async_getuid(void)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "getuid\n");
    DIGGI_ASSERT(false);

    return 0;
}
/**
 * Simulates user id 1 for all invocations.
 * returns immediately
 * 
 * @return uid_t 
 */
uid_t StorageManager::async_geteuid(void)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "geteuid\n");

    return 1;
}
/**
 * Not implemented
 * will throw assertion if invoked
 * 
 * @param fd 
 * @param owner 
 * @param group 
 * @return int 
 */
int StorageManager::async_fchown(int fd, uid_t owner, gid_t group)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "fchown\n");
    DIGGI_ASSERT(false);

    return 0;
}
/**
 * Request for lseek operation on open file.
 * Only supports SEEK_SET operations, other requests will throw assertions.
 * 
 * @param fd file to seek into
 * @param offset offset relative to begining of file
 * @param whence mode allways SEEK_SET
 * @param cb Completion callback, returns actual position, differs if file smaller than offset, must be unmarshaled by completion callback.
 * @param context discressionary context object pointer for state handling in callback
 */
off_t StorageManager::async_lseek(int fd, off_t offset, int whence)
{
    DIGGI_ASSERT(whence == SEEK_SET);
    while (this->pending_write_map[fd] > 0)
    {
        func_context->GetThreadPool()->Yield();
    }
    lseekstatemap[fd] = offset;
    return offset;
}

/**
 * asynchronous request for open call
 * 
 * @param path path for open, all paths are translated relative to CWD
 * @param oflags flags
 * @param mode mode
 * @param cb Completion callback
 * @param context context object for completion callback, managed by calle
 * @param encrypted is this an encrypted file, if existing this must be correct, will throw assertion if missmatch
 * @return file descriptor, must be unmarshalled by completion callback
 */
typedef struct AsyncContext<StorageManager *, async_cb_t, void *, std::string, mode_t> open_ctx_t;
void StorageManager::async_open(const char *path, int oflags, mode_t mode, async_cb_t cb, void *context, bool encrypted, bool omit_from_log)
{
    char *path_n = normalizePath((char *)path);
    // if (std::string(path).find("test.db-journal") != std::string::npos)
    // {
    //     DIGGI_DEBUG_BREAK();
    // }
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "open path=%s, oflags=%u, mode=%u\n", path_n, oflags, mode);
    auto mngr = func_context->GetMessageManager();
    size_t path_length = strlen(path_n);
    DIGGI_ASSERT(cb != nullptr);
    size_t request_size = sizeof(int) + sizeof(mode_t) + sizeof(int) + path_length + 1;
    msg_convention_t type = (cb == nullptr) ? REGULAR : CALLBACK;

    auto msg = mngr->allocateMessage("file_io_func", request_size, type, CLEARTEXT);
    msg->type = FILEIO_OPEN;

    /*Marshall*/
    auto ptr = msg->data;
    msg->omit_from_log = omit_from_log;
    Pack::pack<mode_t>(&ptr, mode);
    Pack::pack<int>(&ptr, oflags);
    Pack::pack<int>(&ptr, (int)encrypted);
    memcpy(ptr, path_n, path_length + 1);
    auto ctx = new open_ctx_t(this, cb, context, std::string(path_n), mode);
    mngr->Send(msg, StorageManager::async_open_cb, ctx);
}
void StorageManager::async_open_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto ctx = (open_ctx_t *)resp->context;
    auto _this = ctx->item1;
    auto ptrm = resp->msg->data;
    int fd = Pack::unpack<int>(&ptrm);
    auto end_file_point = Pack::unpack<off_t>(&ptrm);
    auto path = ctx->item4;
    auto mode = ctx->item5;
    _this->lseekstatemap[fd] = (mode & O_APPEND) ? (end_file_point) : 0;
    _this->pending_write_map[fd] = 0;
    _this->size_of_file[fd] = end_file_point;
    // printf("setting size = %lld\n", _this->size_of_file[fd]);

    _this->inodes[fd] = _this->next_virtual_inode++;
    _this->filedes_to_path[fd] = path; //copy
    _this->filepaths[path] = fd;
    resp->context = ctx->item3;
    DIGGI_ASSERT(ctx->item2);
    ctx->item2(resp, 1);
    delete ctx;
    ctx = nullptr;
}
/**
 * internal context object used in multi-step read request.
 * Encrypted writes require blocks to be read and decrypted into trusted runtime
 * If read is invoked as part of encrypted write, this context object stores inter-callback information for each particular call.
 * Reduces need for class level state and ensures concurrent callbacks may occur without synchronization.
 */
typedef struct AsyncContext<async_cb_t, void *, StorageManager *, read_type_t, size_t, bool, int> read_ctx_t;

/**
 * Read internal callback, invoked as pure read, or as read preceeding a write.
 * If read preceeding write, lseek ensures that file position remains unchanged, 
 * via the read_type_t struct being set to SEEKBACK.
 * Reads all full blocks touched by the range speicified through file position and size of read.
 * Decrypts and concatenates result into message buffer. 
 * Regardless of if callback is from a write or read operation, Recipient completion callback must unmarshal result.
 * @see StorageManager::async_write
 * @see StorageManager::async_read
 *
 * @param ptr pointer to msg_async_response_t object as is the interface through all SecureMessageManager callback functions. this object expects a read_ctx_t as the context field.
 * @param status Unused parameter for error handling (future work)
 * @return as part of callback returns: read buffer, offset in file and 
 */
void StorageManager::async_read_internal_cb(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto resp = (msg_async_response_t *)ptr;
    auto async_id = resp->msg->id;
    DIGGI_ASSERT(resp);
    auto ctx = (read_ctx_t *)resp->context;
    DIGGI_ASSERT(ctx);
    auto _this = ctx->item3;
    DIGGI_ASSERT(_this);
    auto msg = resp->msg;
    DIGGI_ASSERT(msg);
    auto original_size = ctx->item5;
    DIGGI_ASSERT(original_size);
    auto encrypted = ctx->item6;
    auto fd = ctx->item7;
    /*decrypt and return*/
    auto ptrm = resp->msg->data;
    size_t retval = Pack::unpack<size_t>(&ptrm);
    off_t offset = _this->lseekstatemap[fd] % SPACE_PER_BLOCK;
    int end_of_file = Pack::unpack<int>(&ptrm);

    DIGGI_ASSERT(offset >= 0);

    if (encrypted)
    {
        size_t phys_pos = (encrypted) ? (_this->lseekstatemap[fd] / SPACE_PER_BLOCK) * ENCRYPTED_BLK_SIZE : _this->lseekstatemap[fd];
        size_t blocknum = phys_pos / ENCRYPTED_BLK_SIZE;
        size_t chunks = retval / ENCRYPTED_BLK_SIZE;
        size_t totaldecrypted = chunks * SPACE_PER_BLOCK;
        size_t totalplaintext = 0;
        auto totalmsg = ALLOC_P(msg_t, totaldecrypted + sizeof(size_t) + sizeof(off_t));
        totalmsg->size = sizeof(msg_t) + totaldecrypted + sizeof(size_t) + sizeof(off_t);
        auto destblobptr = totalmsg->data + sizeof(size_t) + sizeof(off_t);
        ptrm = totalmsg->data;

        if (totaldecrypted > 0)
        {
            auto chunkptr = resp->msg->data + sizeof(size_t) + sizeof(int);
            auto startchunk = 0;
            if (ctx->item4 == NOSEEK)
            {
                startchunk = 1;
                auto customchunk = (size_t)((sgx_sealed_data_t *)chunkptr)->aes_data.payload_size;
                DIGGI_ASSERT(customchunk <= SPACE_PER_BLOCK);

                if (customchunk != 0)
                {
                    /*
						if end of file and one chunk, handle partial end
					*/
                    if (end_of_file && (chunks < 2))
                    {

                        if (customchunk >= (size_t)offset)
                        {

                            auto plaintextchunk = (uint8_t *)calloc(1, customchunk);
                            // printf("read path=%s fd = %d blocknum = %lu, crc=%lu\n", _this->filedes_to_path[fd].c_str(), fd, blocknum, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);
                            _this->sealer->decrypt(chunkptr, _this->sealer->getciphertextsize(customchunk), plaintextchunk, customchunk, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);
                            auto orig_chunkstart = plaintextchunk;
                            plaintextchunk += offset; /* wont work */
                            auto cappedsize = customchunk - offset;
                            Pack::packBuffer(&destblobptr, plaintextchunk, cappedsize);
                            free(orig_chunkstart);
                            chunkptr += ENCRYPTED_BLK_SIZE;
                            totalplaintext += cappedsize;
                        }
                        else
                        {
                            ///aborted read, because offset was beyond file
                            totalplaintext = 0;
                            offset = 0;
                            ///Ensure we do not attempt more reads.
                            DIGGI_ASSERT((size_t)startchunk == chunks);
                        }
                    }
                    else if (end_of_file && (chunks > 1))
                    {
                        // printf("read path=%s fd = %d blocknum = %lu, crc=%lu\n", _this->filedes_to_path[fd].c_str(), fd, blocknum, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);

                        auto plaintextchunk = (uint8_t *)calloc(1, customchunk);
                        _this->sealer->decrypt(chunkptr, _this->sealer->getciphertextsize(customchunk), plaintextchunk, customchunk, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);
                        auto orig_chunkstart = plaintextchunk;
                        plaintextchunk += offset; /* wont work */
                        auto cappedsize = SPACE_PER_BLOCK - offset;
                        DIGGI_ASSERT((size_t)offset < customchunk);
                        DIGGI_ASSERT(SPACE_PER_BLOCK == customchunk);
                        Pack::packBuffer(&destblobptr, plaintextchunk, cappedsize);
                        free(orig_chunkstart);
                        chunkptr += ENCRYPTED_BLK_SIZE;
                        totalplaintext += cappedsize;
                    }
                    else if (customchunk > (size_t)offset)
                    {
                        // printf("read path=%s fd = %d blocknum = %lu, crc=%lu\n", _this->filedes_to_path[fd].c_str(), fd, blocknum, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);

                        auto plaintextchunk = (uint8_t *)calloc(1, customchunk);
                        _this->sealer->decrypt(chunkptr, _this->sealer->getciphertextsize(customchunk), plaintextchunk, customchunk, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);
                        auto orig_chunkstart = plaintextchunk;
                        plaintextchunk += offset; /* wont work */
                        DIGGI_ASSERT((size_t)offset < customchunk);
                        DIGGI_ASSERT(SPACE_PER_BLOCK == customchunk);
                        auto cappedsize = customchunk - offset;
                        Pack::packBuffer(&destblobptr, plaintextchunk, cappedsize);
                        free(orig_chunkstart);
                        chunkptr += ENCRYPTED_BLK_SIZE;
                        totalplaintext += cappedsize;
                    }
                    else
                    {
                        DIGGI_ASSERT(false);
                    }
                }
                else
                {
                    DIGGI_ASSERT((size_t)offset < SPACE_PER_BLOCK);
                    chunkptr += ENCRYPTED_BLK_SIZE;
                    destblobptr += (SPACE_PER_BLOCK - offset);
                    totalplaintext += (SPACE_PER_BLOCK - offset);
                }
                blocknum++;
            }

            for (unsigned i = startchunk; i < chunks; i++)
            {
                size_t customchunk = (size_t)((sgx_sealed_data_t *)chunkptr)->aes_data.payload_size;
                DIGGI_ASSERT(customchunk <= SPACE_PER_BLOCK);

                /*
					Sparse files may not work out
				*/
                size_t mmset = 0;

                if (customchunk == 0)
                {
                    mmset = SPACE_PER_BLOCK - customchunk;
                }
                if (i + 1 < chunks)
                {
                    mmset = SPACE_PER_BLOCK - customchunk;
                }
                if (customchunk > 0)
                {
                    // printf("read path=%s fd = %d blocknum = %lu, crc=%lu\n", _this->filedes_to_path[fd].c_str(), fd, blocknum, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);

                    auto plaintextchunk = (uint8_t *)calloc(1, customchunk);
                    _this->sealer->decrypt(chunkptr, _this->sealer->getciphertextsize(customchunk), plaintextchunk, customchunk, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);
                    Pack::packBuffer(&destblobptr, plaintextchunk, customchunk);
                    free(plaintextchunk);
                }
                memset(destblobptr, 0, mmset);
                chunkptr += ENCRYPTED_BLK_SIZE;
                destblobptr += mmset;
                DIGGI_ASSERT((customchunk + mmset) <= SPACE_PER_BLOCK);
                totalplaintext += (customchunk + mmset);
                blocknum++;
            }
            if (ctx->item4 == SEEKBACK && !end_of_file)
            {
                Pack::pack<size_t>(&ptrm, totaldecrypted);
            }
            else
            {
                if (totalplaintext > original_size)
                {
                    Pack::pack<size_t>(&ptrm, original_size);
                }
                else
                {
                    Pack::pack<size_t>(&ptrm, totalplaintext);
                }
            }
            Pack::pack<off_t>(&ptrm, offset);
        }
        else
        {
            size_t empty = 0;
            Pack::pack<size_t>(&ptrm, empty);
            Pack::pack<off_t>(&ptrm, offset);
        }

        resp->msg = totalmsg;
        if (ctx->item4 != SEEKBACK)
        {
            _this->lseekstatemap[fd] += (totalplaintext < original_size) ? totalplaintext : original_size;
        }
    }
    else
    {
        DIGGI_ASSERT(ctx->item4 != SEEKBACK);
        /*
			if reading regular non encrypted file
		*/
        auto totalmsg = ALLOC_P(msg_t, retval + sizeof(size_t) + sizeof(off_t));
        ptrm = totalmsg->data;
        Pack::pack<size_t>(&ptrm, retval);
        Pack::pack<off_t>(&ptrm, 0);
        auto chunkptr = resp->msg->data + sizeof(size_t) + sizeof(int);
        Pack::packBuffer(&ptrm, chunkptr, retval);
        totalmsg->size = sizeof(msg_t) + retval + sizeof(size_t) + sizeof(off_t);
        resp->msg = totalmsg;
        _this->lseekstatemap[fd] += retval;
    }

    resp->msg->id = async_id;
    resp->context = ctx->item2;
    ctx->item1(resp, 1);
    if (ctx->item4 == NOSEEK)
    {
        free(resp->msg);
        resp->msg = nullptr;
    }
    delete ctx;
    ctx = nullptr;
    /*
		When can i delete the message(total_message)?
	*/
}
/**
 * Internal wrapper call for marshaling and sending read request, may be called from write operation.
 * 
 * @param fd file descriptor of open file for read operation
 * @param type SEEKBACK or NOSEEK depending on if read may move file pointer position. If the read precedes a write, the type is SEEKBACK
 * @param buf target read buffer (not used as result is returned through callback)
 * @param nbyte bytes to read
 * @param cb completion callback, responsible for unmarshaling result
 * @param context context object used by calle
 * @param encrypted encrypted read, must match file mode.
 * @return read target buffer, size of read and offset in file as message delivered to completion callback.
 */
void StorageManager::async_read_internal(int fd, read_type_t type, void *buf, size_t nbyte, async_cb_t cb, void *context, bool encrypted, bool omit_from_log)
{
    // DIGGI_ASSERT(fildes < FILE_DESCRIPTOR_LIMIT);

    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "read fd=%d, nbyte=%lu\n", fd, nbyte);
    auto mngr = func_context->GetMessageManager();
    size_t request_size = sizeof(int) + sizeof(size_t) + sizeof(size_t) + sizeof(int);
    auto msg = mngr->allocateMessage("file_io_func", request_size, CALLBACK, CLEARTEXT);
    msg->type = FILEIO_READ;
    auto blocks_to_read = roundUp_r((lseekstatemap[fd] % SPACE_PER_BLOCK) + nbyte, SPACE_PER_BLOCK) / SPACE_PER_BLOCK;
    auto total_read_size = nbyte;
    if (encrypted)
    {
        total_read_size = (blocks_to_read * ENCRYPTED_BLK_SIZE);
    }

    size_t phys_pos = (encrypted) ? (lseekstatemap[fd] / SPACE_PER_BLOCK) * ENCRYPTED_BLK_SIZE : lseekstatemap[fd];

    auto ptr = msg->data;
    msg->omit_from_log = omit_from_log;
    Pack::pack<int>(&ptr, fd);
    Pack::pack<size_t>(&ptr, total_read_size);
    Pack::pack<size_t>(&ptr, phys_pos);
    Pack::pack<int>(&ptr, encrypted);

    mngr->Send(msg, async_read_internal_cb, new read_ctx_t(cb, context, this, type, nbyte, encrypted, fd));
}
/**
 * Asynchronous read request. The correct function for req requesting a read.
 * 
 * @param fd file descriptor of open file.
 * @param buf read buffer (not used)
 * @param nbyte number of bytes to read
 * @param cb completion callback
 * @param context calle managed context object, delivered to callback.
 * @param encrypted encrypted file access or not, must be identical to expected file mode, will throw assertion if not.
 */
void StorageManager::async_read(int fd, void *buf, size_t nbyte, async_cb_t cb, void *context, bool encrypted, bool omit_from_log)
{
    DIGGI_ASSERT(lseekstatemap.find(fd) != lseekstatemap.end());

    while (this->pending_write_map[fd] > 0)
    {
        func_context->GetThreadPool()->Yield();
    }
    async_read_internal(fd, NOSEEK, buf, nbyte, cb, context, encrypted, omit_from_log);
}

/**
 * write context object used internally to preserve state across message flows. 
 * All encrypted writes require a preceding read, and this struct captures context in between asynchronous message requests.
 * The pattern avoids state managemnt in class and synchronization for concurrent operations by multiple threads.
 * callback, context, buffer, this, count, fd
 */
typedef struct AsyncContext<async_cb_t, void *, void *, StorageManager *, size_t, int, bool, bool> write_ctx_t;

/**
 * Internal write callback, returning from preceding read.
 * input message as specified by SecureMessageManager is an msg_async_response_t.
 * context field of msg_async_response_t contains write_ctx_t type.
 * Message object contains read size, offset into file and read buffer.
 * The buffer read is used as a basis for which the modifed bytes of the write buffer are inserted, 
 * before reencrypting the resulting coaleased buffer.
 * @param ptr msg_async_response_t
 * @param status for error handling (unused) future work.
 * @return callback specified in input by write_ctx_t handles return values. request response returns bytes written.
 */
void StorageManager::async_write_internal_cb(void *ptr, int status)
{
    auto resp = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(resp);
    auto context = (write_ctx_t *)resp->context;
    DIGGI_ASSERT(context);
    auto count = context->item5;
    DIGGI_ASSERT(count > 0);
    auto _this = context->item4;
    DIGGI_ASSERT(_this);
    auto cb = context->item1;
    DIGGI_ASSERT(cb);
    auto ctx = context->item2;
    auto outbuffer = context->item3;
    DIGGI_ASSERT(outbuffer);
    auto fd = context->item6;
    bool encrypted = context->item7;
    bool omit_from_log = context->item8;
    DIGGI_ASSERT(fd > 0);
    // DIGGI_ASSERT(fd < FILE_DESCRIPTOR_LIMIT);

    bool free_intermediate = false;
    uint8_t *intermediatepointer = nullptr;
    auto base = resp->msg->data;
    size_t read = Pack::unpack<size_t>(&base);
    off_t offset = Pack::unpack<off_t>(&base);
    DIGGI_ASSERT(encrypted);
    // DIGGI_ASSERT(!(read % SPACE_PER_BLOCK));

    uint8_t *dest_write_pointer = base + offset;
    if (read < count + offset)
    {
        /*
			if read is smaller than write
			we are writing to end of file
		*/

        base = (uint8_t *)malloc(count + offset);
        memcpy(base, resp->msg->data + sizeof(size_t) + sizeof(off_t), offset);
        free_intermediate = true;
        intermediatepointer = base;
        dest_write_pointer = base + offset;
    }

    memcpy(dest_write_pointer, outbuffer, count);

    /*
		Encrypt and send out of enclave
	*/
    auto mngr = _this->func_context->GetMessageManager();
    auto chuncksize = _this->sealer->getciphertextsize(SPACE_PER_BLOCK);
    size_t chunks = (size_t)(ceil((double)(count + offset) / (double)SPACE_PER_BLOCK));
    size_t request_size = sizeof(int) + sizeof(size_t) + sizeof(int) + chuncksize * chunks;
    msg_t *msg = mngr->allocateMessage("file_io_func", request_size, CALLBACK, CLEARTEXT);
    msg->omit_from_log = omit_from_log;
    msg->type = FILEIO_WRITE;
    size_t phys_pos = (encrypted) ? (_this->lseekstatemap[fd] / SPACE_PER_BLOCK) * ENCRYPTED_BLK_SIZE : _this->lseekstatemap[fd];

    auto ptrresp = msg->data;
    Pack::pack<int>(&ptrresp, fd);
    Pack::pack<int>(&ptrresp, (int)encrypted);
    Pack::pack<size_t>(&ptrresp, phys_pos);
    auto bytes_left = (read < count + offset) ? count + offset : read;
    size_t blocknum = phys_pos / ENCRYPTED_BLK_SIZE;
    for (unsigned i = 0; i < chunks; i++)
    {
        uint8_t *ciphertext = nullptr;
        if (bytes_left < SPACE_PER_BLOCK)
        {
            // printf("write path=%s fd = %d blocknum = %lu, crc=%lu\n", _this->filedes_to_path[fd].c_str(), fd, blocknum, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);

            ciphertext = _this->sealer->encrypt(base, bytes_left, _this->sealer->getciphertextsize(bytes_left), &_this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);
            Pack::packBuffer(&ptrresp, ciphertext, _this->sealer->getciphertextsize(bytes_left));
        }
        else
        {
            // printf("write path=%s fd = %d blocknum = %lu, crc=%lu\n", _this->filedes_to_path[fd].c_str(), fd, blocknum, _this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);

            ciphertext = _this->sealer->encrypt(base, SPACE_PER_BLOCK, chuncksize, &_this->block_2_crc[_this->filedes_to_path[fd]][blocknum]);
            Pack::packBuffer(&ptrresp, ciphertext, chuncksize);
        }

        free(ciphertext);

        /*
			Incremented after last use but not referenced, so we are fine
		*/
        blocknum++;
        base += SPACE_PER_BLOCK;
        bytes_left -= SPACE_PER_BLOCK;
    }

    if (free_intermediate)
    {
        DIGGI_ASSERT(intermediatepointer);
        free(intermediatepointer);
    }
    if (((size_t)_this->lseekstatemap[fd] + count) > ((size_t)_this->size_of_file[fd]))
    {
        _this->size_of_file[fd] = _this->lseekstatemap[fd] + count;
    }
    _this->lseekstatemap[fd] += count;
    _this->pending_write_map[fd]--;
    mngr->Send(msg, cb, ctx);
    free(resp->msg);
    resp->msg = nullptr;
    delete context;
    context = nullptr;

    /*Can i delete the read internal message here?*/
}

/**
 * write request method, correct funciton for requesting a write through the diggi api.
 * Write to encrypted or unencrypte file.
 * TODO: rewrite operations to handle lseek inside enclave.
 * @param fd file descriptor open file
 * @param buf write buffer
 * @param count write byte size
 * @param cb completion callback
 * @param context calle managed context object
 * @param encrypted field for specifying encrypted write, must match expected for file.
 * @return bytes written, unmarshalled in completion callback.
 */
void StorageManager::async_write(int fd, const void *buf, size_t count, async_cb_t cb, void *context, bool encrypted, bool ommit_from_log)
{
    DIGGI_ASSERT(lseekstatemap.find(fd) != lseekstatemap.end());

    while (pending_write_map[fd] > 0)
    {
        func_context->GetThreadPool()->Yield();
    }
    DIGGI_TRACE(func_context->GetLogObject(), LogLevel::LDEBUG,
                "StorageManager::async_write( path = %s fd = %d, count = %lu)\n",
                filedes_to_path[fd].c_str(),
                fd,
                count);

    /*
		If encrypted, we must first read the blocks affected by write
	*/
    if (encrypted)
    {
        pending_write_map[fd]++;
        async_read_internal(fd, SEEKBACK, nullptr, count, async_write_internal_cb, new write_ctx_t(cb, context, (void *)buf, this, count, fd, encrypted, ommit_from_log), encrypted, ommit_from_log);
    }
    else
    {
        auto mngr = func_context->GetMessageManager();
        size_t request_size = sizeof(int) + sizeof(size_t) + sizeof(int) + count;

        msg_t *msg = mngr->allocateMessage("file_io_func", request_size, CALLBACK, CLEARTEXT);
        msg->type = FILEIO_WRITE;
        msg->omit_from_log = ommit_from_log;
        auto ptrresp = msg->data;
        Pack::pack<int>(&ptrresp, fd);
        Pack::pack<int>(&ptrresp, (int)encrypted);
        Pack::pack<size_t>(&ptrresp, (size_t)lseekstatemap[fd]);
        Pack::packBuffer(&ptrresp, (uint8_t *)buf, count);
        mngr->Send(msg, cb, context);
        if (((size_t)lseekstatemap[fd] + count) > ((size_t)size_of_file[fd]))
        {
            size_of_file[fd] = lseekstatemap[fd] + count;
        }
        lseekstatemap[fd] += count;
    }
}

/**
 * Request for file unlink
 * 
 * @param pathname pathname, allways relative to CWD
 * @param cb completion callback
 * @param context calle managed context object.
 * @return int specifying success, must be unmarshaled by completion callback.
 */
void StorageManager::async_unlink(const char *pathname, async_cb_t cb, void *context)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "unlink\n");
    char *path_n = normalizePath((char *)pathname);
    auto mngr = func_context->GetMessageManager();
    size_t path_length = strlen(path_n);
    size_t request_size = path_length + 1;
    auto msg = mngr->allocateMessage("file_io_func", request_size, CALLBACK, CLEARTEXT);
    msg->type = FILEIO_UNLINK;

    int fd = filepaths[std::string(path_n)];
    lseekstatemap.erase(fd);
    filedes_to_path.erase(fd);
    filepaths.erase(std::string(path_n));
    inodes.erase(fd);
    /*Marshall*/
    auto ptr = msg->data;
    Pack::packBuffer(&ptr, (uint8_t *)path_n, path_length + 1);
    mngr->Send(msg, cb, context);
}

/**
 * Not implemented, will throw assertion if invoked
 * 
 * @param path 
 * @param mode 
 * @return int 
 */
int StorageManager::async_mkdir(const char *path, mode_t mode)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "mkdir\n");
    DIGGI_ASSERT(false);
    return 0;
}

/**
 * Not implemented, will throw assertion if invoked
 * 
 * @param path 
 * @return int 
 */
int StorageManager::async_rmdir(const char *path)
{
    DIGGI_TRACE(func_context->GetLogObject(), LDEBUG, "rmdir\n");
    DIGGI_ASSERT(false);

    return 0;
}

/**
 * Asynchronous request for unmask file permission setting for current proc.
 * 
 * @param mode mode to set 
 * @param cb completion callback
 * @param context calle managed context object.
 * @return previous mask, must be unmarshalled by completion callback.
 */
mode_t StorageManager::async_umask(mode_t mode)
{
    return 0777;
}
/**
 * @brief Destroy the Storage Manager:: Storage Manager object
 * Noop as StorageManager does not allocate any internal state.
 */
StorageManager::~StorageManager()
{
}
