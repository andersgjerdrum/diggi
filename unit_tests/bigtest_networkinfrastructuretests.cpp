
#define MG_ENABLE_SSL 1

#include <gtest/gtest.h>
#include "mongoose.h"
#include "threading/ThreadPool.h"
#include "messaging/AsyncMessageManager.h"
#include "messaging/SecureMessageManager.h"
#include "DiggiGlobal.h"
#include "network/NetworkServer.h"
#include "network/NetworkManager.h"
#include "storage/StorageManager.h"
#include "storage/StorageServer.h"
#include "runtime/DiggiUntrustedRuntime.h"
#include "posix/io_stubs.h"

class MockLog : public ILog
{
    LogLevel level;
    void SetFuncId(aid_t aid, std::string name = "testfunc")
    {
    }
    std::string GetfuncName()
    {
        return "testfunc";
    }
    void SetLogLevel(LogLevel lvl)
    {
        level = lvl;
    }
    void Log(LogLevel lvl, const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        printf(format, args);
        va_end(args);
    }
    void Log(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        printf(format, args);
        va_end(args);
    }
};

class MockStorageManager : public IStorageManager
{
    IDiggiAPI *func_context;
    ISealingAlgorithm *sealer;
    // this mockup returns the content of cert.pem, total 1554 bytes.
    string certfile = "-----BEGIN CERTIFICATE-----\n"
                      "REDACTED\n"
                      "-----END CERTIFICATE-----";

    string keyfile = "-----BEGIN PRIVATE KEY-----\n"
                     "REDACTED\n"
                     "-----END PRIVATE KEY-----";

    string htmlfile = "<html><title>hack!</title><body>Hello Lars, this is your web page in TEST</body></html>";
    string *currentfile = &htmlfile;

    int fd = 42;

public:
    MockStorageManager(IDiggiAPI *context, ISealingAlgorithm *seal)
    {
        func_context = context;
        sealer = seal;
    }

    void async_fopen(const char *filename, const char *mode, async_cb_t cb, void *context)
    {
        printf("MockStorageManager::async_fopen, %s : %s\n", filename, mode);

        if (strstr(filename, "crt") != NULL)
        {
            currentfile = &certfile;
        }
        else if (strstr(filename, "key") != NULL)
        {
            currentfile = &keyfile;
        }
        else
        {
            currentfile = &htmlfile;
            printf("mock fopen: unknown file being opened: %s\n", (char *)filename);
        }

        // reply:
        auto mngr = func_context->GetMessageManager();
        auto resp = new msg_async_response_t();
        resp->msg = mngr->allocateMessage("network_server_func", sizeof(int), CALLBACK, CLEARTEXT);
        resp->context = context;
        uint8_t *ptr = resp->msg->data;

        // mock reply for a file descriptor, will use this in the next step to return contents of file.
        Pack::pack<int>(&ptr, fd++);

        cb(resp, 1);
    }

    void async_fseek(FILE *stream, long offset, int whence, async_cb_t cb, void *context)
    {
        printf("MockStorageManager::async_fseek fd: %d, offset %d, whence %d\n", stream->_fileno, whence, (int)offset);

        auto mngr = func_context->GetMessageManager();
        auto resp = new msg_async_response_t();
        resp->context = context;
        resp->msg = mngr->allocateMessage("network_server_func", sizeof(int), CALLBACK, CLEARTEXT);
        resp->context = context;
        uint8_t *ptr = resp->msg->data;

        // returns 0 on success
        int retval = 0;
        Pack::pack<int>(&ptr, retval);

        cb(resp, 1);
    }

    void async_ftell(FILE *f, async_cb_t cb, void *context)
    {
        printf("MockStorageManager::async_ftell fd: %d\n", f->_fileno);

        auto mngr = func_context->GetMessageManager();
        auto resp = new msg_async_response_t();
        resp->context = context;
        resp->msg = mngr->allocateMessage("network_server_func", sizeof(long), CALLBACK, CLEARTEXT);
        uint8_t *ptr = resp->msg->data;

        // returns the size of currentfile, as revealed by the preceding fseek(0, SEEK_END) call.
        long retval = (long)currentfile->length();
        Pack::pack<long>(&ptr, retval);

        cb(resp, 1);
    }

    void async_fread(size_t size, size_t count, FILE *f, async_cb_t cb, void *context)
    {
        printf("MockStorageManager::async_fread fd: %d, size %d, count %d\n", f->_fileno, (int)size, (int)count);

        size_t requestlength = size * count;
        size_t replylength = 0;
        if (f->_fileno == 7)
        { //wtf magic number
            currentfile = &htmlfile;
        }
        if (requestlength < currentfile->length())
        {
            printf("Fread requested less than the file length (%d)\n", (int)requestlength);
            replylength = requestlength;
        }
        else if (requestlength == currentfile->length())
        {
            replylength = currentfile->length();
        }
        else
        {
            replylength = currentfile->length();
            printf("Fread requested more (%d) than the file length (%d)\n", (int)requestlength, (int)replylength);
        }
        auto mngr = func_context->GetMessageManager();
        auto resp = new msg_async_response_t();
        resp->context = context;
        resp->msg = mngr->allocateMessage("network_server_func", sizeof(size_t) + replylength, CALLBACK, CLEARTEXT);
        uint8_t *ptr = resp->msg->data;

        Pack::pack<size_t>(&ptr, replylength);
        Pack::packBuffer(&ptr, (uint8_t *)currentfile->c_str(), replylength);

        cb(resp, 1);
    }

    void async_fclose(FILE *f, async_cb_t cb, void *context)
    {
        printf("MockStorageManager::async_fclose fd: %d\n", f->_fileno);

        auto mngr = func_context->GetMessageManager();
        auto resp = new msg_async_response_t();
        resp->context = context;
        resp->msg = mngr->allocateMessage("network_server_func", sizeof(int), CALLBACK, CLEARTEXT);
        uint8_t *ptr = resp->msg->data;

        Pack::pack<int>(&ptr, 0);
        cb(resp, 1);
    }

    ~MockStorageManager() {}

    int async_stat(const char *path, struct stat *buf) { return 0; }
    int async_fstat(int fd, struct stat *buf) { return 0; }
    void async_close(int fd, bool omit_from_log) {}
    int async_access(const char *pathname, int mode) { return 0; }
    char *async_getcwd(char *buf, size_t size) { return nullptr; }
    int async_ftruncate(int fd, off_t length) { return 0; }
    int async_fcntl(int fd, int cmd, struct flock *lock) { return 0; }
    int async_fsync(int fd) { return 0; }
    char *async_getenv(const char *name) { return nullptr; }
    uid_t async_getuid(void) { return 0; }
    uid_t async_geteuid(void) { return 0; }
    int async_fchown(int fd, uid_t owner, gid_t group) { return 0; }
    off_t async_lseek(int fd, off_t offset, int whence) { return 0; }
    void async_open(const char *path, int oflags, mode_t mode, async_cb_t cb, void *context, bool encrypted, bool omit_from_log) {}
    static void async_open_cb(void *ptr, int status) {}
    static void async_read_internal_cb(void *ptr, int status) {}
    void async_read_internal(int fildes, read_type_t type, void *buf, size_t nbyte, async_cb_t cb, void *context, bool encrypted, bool omit_from_log) {}
    void async_read(int fildes, void *buf, size_t nbyte, async_cb_t cb, void *context, bool encrypted, bool omit_from_log) {}
    static void async_write_internal_cb(void *ptr, int status) {}
    void async_write(int fd, const void *buf, size_t count, async_cb_t cb, void *context, bool encrypted, bool omit_from_log) {}
    void async_unlink(const char *pathname, async_cb_t cb, void *context) {}
    int async_mkdir(const char *path, mode_t mode) { return 0; }
    int async_rmdir(const char *path) { return 0; }
    mode_t async_umask(mode_t mode) { return 0; }
};

static const char *http_port = "8000";
static struct mg_serve_http_opts http_server_opts;
static struct mg_bind_opts bind_opts = {0};
static const char *url = "diggi-2.cs.uit.no";
static volatile int end_of_execution = 0;

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
    // printf("mg_server receiving server event %d\n", ev);
    if (ev == MG_EV_HTTP_REQUEST)
    {
        struct http_message *hm = (struct http_message *)ev_data;
        mg_serve_http(nc, hm, http_server_opts);
    }
}

void mg_server_execution_callback(void *ptr, int status)
{
    // mongoose stuff:
    struct mg_mgr mgr;
    struct mg_connection *nc;

    mg_mgr_init(&mgr, NULL);
    // printf("Starting web server on port %s\n", http_port);

    memset(&bind_opts, 0, sizeof(bind_opts));
    bind_opts.ssl_cert = "diggi-2.cs.uit.no.crt";
    bind_opts.ssl_key = "diggi-2.cs.uit.no.key";
    nc = mg_bind_opt(&mgr, http_port, ev_handler, bind_opts); // with ssl
                                                              //nc = mg_bind(&mgr, http_port, ev_handler); // without ssl

    if (nc == NULL)
    {
        printf("Failed to create listener\n");
        DIGGI_ASSERT(false);
        return;
    }

    //// Set up HTTP server parameters
    mg_set_protocol_http_websocket(nc);
    http_server_opts.auth_domain = url;
    http_server_opts.document_root = "/home/larsb/www"; // Serve current directory
    http_server_opts.index_files = "index.html";
    http_server_opts.enable_directory_listing = "yes";

    printf("Entering mongoose poll...\n");
    for (int i = 0; i < 15; i++)
    {
        // printf("i=%d\n", i);
        mg_mgr_poll(&mgr, 1000);
    }
    mg_mgr_free(&mgr);

    end_of_execution = 1;
    __sync_synchronize();
}

void test_ns_init(void *ptr, int status)
{
    auto network_server = (NetworkServer *)ptr;
    network_server->InitializeServer();
}

TEST(bigtest_networkinfrastructure, DISABLED_singlethread_ssl)
{
    set_syscall_interposition(1);
    end_of_execution = 0;

    ThreadPool *threadpool_man = new ThreadPool(1);
    ThreadPool *threadpool_srv = new ThreadPool(1);
    auto mlog_man = new StdLogger(threadpool_man);
    auto mlog_srv = new StdLogger(threadpool_srv);

    aid_t serv;
    aid_t cli;
    serv.raw = 0;
    serv.fields.lib = 1;
    serv.fields.type = LIB;
    cli.raw = 0;
    cli.fields.lib = 2;
    cli.fields.type = LIB;

    mlog_man->SetFuncId(cli, "network_manager");
    mlog_srv->SetFuncId(serv, "network_server");
    mlog_man->SetLogLevel(LRELEASE);
    mlog_srv->SetLogLevel(LRELEASE);
    auto in_b = lf_new(RING_BUFFER_SIZE, 2, 2);
    auto out_b = lf_new(RING_BUFFER_SIZE, 2, 2);
    auto globuff = provision_memory_buffer(3, MAX_DIGGI_MEM_ITEMS, MAX_DIGGI_MEM_SIZE);
    DiggiAPI *acontext_man = new DiggiAPI(threadpool_man, nullptr, nullptr, nullptr, nullptr, mlog_man, cli, nullptr);
    DiggiAPI *acontext_srv = new DiggiAPI(threadpool_srv, nullptr, nullptr, nullptr, nullptr, mlog_srv, serv, nullptr);
    auto amm_srv = new AsyncMessageManager(acontext_srv, in_b, out_b, std::vector<name_service_update_t>(), 0, globuff);
    auto amm_man = new AsyncMessageManager(acontext_man, out_b, in_b, std::vector<name_service_update_t>(), 1, globuff);
    amm_srv->Start();
    amm_man->Start();

    auto mapns = std::map<std::string, aid_t>();
    mapns["network_server_func"] = serv;

    iostub_setcontext(acontext_man, 1);

    auto nsl = new NoSeal();
    auto network_manager = new NetworkManager(acontext_man, nsl);
    acontext_man->SetNetworkManager(network_manager);

    auto mm_manager = new SecureMessageManager(
        acontext_man,
        new NoAttestationAPI(),
        amm_man,
        mapns,
        0,
        new DynamicEnclaveMeasurement(acontext_man),
        new DebugCrypto(),
        false,
        false);
    acontext_man->SetMessageManager(mm_manager);

    auto sm_manager = new MockStorageManager(acontext_man, nsl); // introducing mock here to mock the reply from fopen
    acontext_man->SetStorageManager(sm_manager);

    auto mm_srv = new SecureMessageManager(
        acontext_srv,
        new NoAttestationAPI(),
        amm_srv,
        mapns,
        0,
        new DynamicEnclaveMeasurement(acontext_srv),
        new DebugCrypto(),
        false,
        false);
    acontext_srv->SetMessageManager(mm_srv);
    auto network_server = new NetworkServer(acontext_srv);

    SET_DIGGI_GLOBAL_CONTEXT(acontext_man);

    threadpool_srv->Schedule(test_ns_init, network_server, __PRETTY_FUNCTION__);

    threadpool_srv->Schedule(mg_server_execution_callback, nullptr, __PRETTY_FUNCTION__);

    while (!end_of_execution)
        ;

    threadpool_man->Stop();
    threadpool_srv->Stop();

    delete mm_srv;
    delete mm_manager;
    amm_srv->Stop();
    delete amm_srv;
    amm_man->Stop();
    delete amm_man;
    delete threadpool_man;
    delete threadpool_srv;

    lf_destroy(in_b);
    lf_destroy(out_b);
    delete_memory_buffer(globuff, MAX_DIGGI_MEM_ITEMS);

    delete nsl;
    delete mlog_man;
    delete mlog_srv;
    set_syscall_interposition(0);
}
