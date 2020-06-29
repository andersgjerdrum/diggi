#include <gtest/gtest.h>
#include "messaging/IMessageManager.h"
#include "Logging.h"
#include "messaging/Pack.h"
#include "storage/StorageServer.h"
#include <string>
#include "runtime/DiggiAPI.h"

class MockLog : public ILog
{
    std::string GetfuncName()
    {
        return "testfunc";
    }
    void SetFuncId(aid_t aid, std::string name = "func")
    {
    }
    void SetLogLevel(LogLevel lvl)
    {
    }
    void Log(LogLevel lvl, const char *fmt, ...)
    {
    }
    void Log(const char *fmt, ...)
    {
    }
};

class SMockMessageManager : public IMessageManager
{
    msg_t *outbound;

public:
    SMockMessageManager() : outbound(nullptr)
    {
    }

    msg_t *GetOutboundMessage()
    {
        return outbound;
    }
    void Send(msg_t *msg, async_cb_t cb, void *cb_context)
    {
        DIGGI_ASSERT(cb == nullptr);
        DIGGI_ASSERT(cb_context == nullptr);
        outbound = msg;
    }

    msg_t *allocateMessage(msg_t *msg, size_t payload_size)
    {
        auto msgret = ALLOC_P(msg_t, payload_size);
        memset(msgret, 0x0, sizeof(msg_t) + payload_size);
        msgret->size = sizeof(msg_t) + payload_size;
        return msgret;
    }
    void endAsync(msg_t *msg)
    {
        return;
    }
    msg_t *allocateMessage(std::string destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
    {
        return allocateMessage(aid_t(), payload_size, async, delivery);
    }

    msg_t *allocateMessage(aid_t destination, size_t payload_size, msg_convention_t async, msg_delivery_t delivery)
    {
        auto msgret = ALLOC_P(msg_t, payload_size);
        memset(msgret, 0x0, sizeof(msg_t) + payload_size);

        msgret->size = sizeof(msg_t) + payload_size;
        return msgret;
    }
    void registerTypeCallback(async_cb_t cb, msg_type_t type, void *ctx)
    {
    }
    std::map<std::string, aid_t> getfuncNames()
    {
        return std::map<std::string, aid_t>();
    }
};
static int fileDescriptor = 0;

TEST(storageservertests, openmessage)
{
    auto mm = new SMockMessageManager();
    auto log = new MockLog();

    auto actx = new DiggiAPI();
    actx->SetMessageManager(mm);
    actx->SetLogObject(log);
    auto ss = new StorageServer(actx);

    auto resp = new msg_async_response_t();
    resp->context = ss;

    int oflags = O_RDWR | O_CREAT | O_TRUNC;
    mode_t mode = S_IRWXU;
    const char *path_n = "test.file.test";

    size_t path_length = strlen(path_n);
    size_t request_size = sizeof(mode_t) + sizeof(int) + sizeof(int) + path_length + 1;
    auto msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
    msg->type = FILEIO_OPEN;

    auto ptr = msg->data;
    Pack::pack<mode_t>(&ptr, mode);
    Pack::pack<int>(&ptr, oflags);
    Pack::pack<int>(&ptr, 1);
    memcpy(ptr, path_n, path_length + 1);
    resp->msg = msg;
    StorageServer::fileIoOpen(resp, 1);

    auto resp_msg = mm->GetOutboundMessage();
    EXPECT_TRUE(resp_msg->size == sizeof(msg_t) + sizeof(int) + sizeof(off_t));
    auto respptr = resp_msg->data;
    fileDescriptor = Pack::unpack<int>(&respptr);
    auto off = Pack::unpack<off_t>(&respptr);
    EXPECT_TRUE(off == 0);
    EXPECT_TRUE(fileDescriptor > 0);
    free(resp_msg);

    delete mm;
    delete log;
    delete ss;
    delete resp;
    free(msg);
}

TEST(storageservertests, readmessage_empty_file)
{
    auto mm = new SMockMessageManager();
    auto log = new MockLog();

    auto actx = new DiggiAPI();
    actx->SetMessageManager(mm);
    actx->SetLogObject(log);
    auto ss = new StorageServer(actx);
    auto resp = new msg_async_response_t();
    resp->context = ss;
    size_t request_size = sizeof(int) + sizeof(size_t) + sizeof(size_t) + sizeof(int);

    auto msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
    msg->type = FILEIO_READ;

    auto ptr = msg->data;
    Pack::pack<int>(&ptr, fileDescriptor);
    size_t nbyte = ENCRYPTED_BLK_SIZE;
    Pack::pack<size_t>(&ptr, nbyte);
    size_t phys_pos = 0;
    Pack::pack<size_t>(&ptr, phys_pos);
    int enc = 1;
    Pack::pack<int>(&ptr, enc);
    resp->msg = msg;
    StorageServer::fileIoRead(resp, 1);

    auto resp_msg = mm->GetOutboundMessage();
    EXPECT_TRUE(resp_msg->size == (sizeof(msg_t) + sizeof(size_t) + sizeof(int)));

    free(resp_msg);

    delete mm;
    delete log;
    delete ss;
    delete resp;
    free(msg);
}

TEST(storageservertests, writemessage)
{
    auto mm = new SMockMessageManager();
    auto log = new MockLog();
    auto actx = new DiggiAPI();
    actx->SetMessageManager(mm);
    actx->SetLogObject(log);
    auto ss = new StorageServer(actx);

    auto resp = new msg_async_response_t();
    resp->context = ss;
    size_t request_size = sizeof(int) + sizeof(size_t) + sizeof(int) + ENCRYPTED_BLK_SIZE;

    auto msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
    msg->type = FILEIO_WRITE;
    auto ptrresp = msg->data;
    Pack::pack<int>(&ptrresp, fileDescriptor);
    Pack::pack<int>(&ptrresp, 1);
    Pack::pack<size_t>(&ptrresp, 0);

    char buf[ENCRYPTION_HEADER_SIZE] = {0};
    auto dt = (sgx_sealed_data_t *)buf;
    dt->aes_data.payload_size = 30;

    memcpy(ptrresp, dt, ENCRYPTION_HEADER_SIZE);
    memcpy(ptrresp + ENCRYPTION_HEADER_SIZE, "testing.testing.one.two.three", 30);

    resp->msg = msg;
    StorageServer::fileIoWrite(resp, 1);

    auto resp_msg = mm->GetOutboundMessage();
    EXPECT_TRUE(resp_msg->size == (sizeof(msg_t) + sizeof(ssize_t)));
    auto ptrtt = resp_msg->data;
    EXPECT_TRUE(Pack::unpack<ssize_t>(&ptrtt) == ENCRYPTED_BLK_SIZE);
    free(resp_msg);

    delete mm;
    delete log;
    delete ss;
    delete resp;
    free(msg);
}

TEST(storageservertests, closemessage)
{
    auto mm = new SMockMessageManager();
    auto log = new MockLog();

    auto actx = new DiggiAPI();
    actx->SetMessageManager(mm);
    actx->SetLogObject(log);
    auto ss = new StorageServer(actx);

    auto resp = new msg_async_response_t();
    resp->context = ss;
    size_t request_size = sizeof(int);

    auto msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
    msg->type = FILEIO_CLOSE;
    auto ptrresp = msg->data;
    memcpy(ptrresp, &fileDescriptor, sizeof(int));
    resp->msg = msg;
    StorageServer::fileIoClose(resp, 1);

    auto resp_msg = mm->GetOutboundMessage();

    EXPECT_TRUE(resp_msg == nullptr);
    delete mm;
    delete log;
    delete ss;
    delete resp;
    free(msg);
}

TEST(storageservertests, openmessage_append)
{
    auto mm = new SMockMessageManager();
    auto log = new MockLog();

    auto actx = new DiggiAPI();
    actx->SetMessageManager(mm);
    actx->SetLogObject(log);
    auto ss = new StorageServer(actx);

    auto resp = new msg_async_response_t();
    resp->context = ss;

    int oflags = O_RDWR | O_APPEND;
    mode_t mode = S_IRWXU;
    const char *path_n = "test.file.test";

    size_t path_length = strlen(path_n);
    size_t request_size = sizeof(mode_t) + sizeof(int) + sizeof(int) + path_length + 1;
    auto msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
    msg->type = FILEIO_OPEN;
    auto ptr = msg->data;

    Pack::pack<mode_t>(&ptr, mode);
    Pack::pack<int>(&ptr, oflags);
    Pack::pack<int>(&ptr, 1);
    memcpy(ptr, path_n, path_length + 1);
    resp->msg = msg;
    StorageServer::fileIoOpen(resp, 1);

    auto resp_msg = mm->GetOutboundMessage();
    EXPECT_TRUE(resp_msg->size == sizeof(msg_t) + sizeof(int) + sizeof(off_t));

    auto respptr = resp_msg->data;
    fileDescriptor = Pack::unpack<int>(&respptr);
    auto off = Pack::unpack<off_t>(&respptr);
    EXPECT_TRUE(off == 30);
    EXPECT_TRUE(fileDescriptor > 0);
    free(resp_msg);

    delete mm;
    delete log;
    delete ss;
    delete resp;
    free(msg);
}

TEST(storageservertests, readmessage_back)
{
    auto mm = new SMockMessageManager();
    auto log = new MockLog();

    auto actx = new DiggiAPI();
    actx->SetMessageManager(mm);
    actx->SetLogObject(log);
    auto ss = new StorageServer(actx);

    auto resp = new msg_async_response_t();
    resp->context = ss;
    size_t request_size = sizeof(int) + sizeof(size_t) + sizeof(size_t) + sizeof(int);

    auto msg = mm->allocateMessage(aid_t(), request_size, CALLBACK, CLEARTEXT);
    msg->type = FILEIO_READ;

    auto ptr = msg->data;
    Pack::pack<int>(&ptr, fileDescriptor);
    Pack::pack<size_t>(&ptr, ENCRYPTED_BLK_SIZE);
    Pack::pack<size_t>(&ptr, 0);
    Pack::pack<int>(&ptr, 1);
    resp->msg = msg;
    StorageServer::fileIoRead(resp, 1);
    auto resp_msg = mm->GetOutboundMessage();

    auto respptr = resp_msg->data;
    auto retval = Pack::unpack<size_t>(&respptr);
    auto endoffile = Pack::unpack<int>(&respptr);
    EXPECT_TRUE(endoffile == 1);
    EXPECT_TRUE(retval == ENCRYPTED_BLK_SIZE);

    EXPECT_TRUE(resp_msg->size == (sizeof(msg_t) + sizeof(size_t) + sizeof(int) + ENCRYPTED_BLK_SIZE));
    EXPECT_TRUE(memcmp(resp_msg->data + sizeof(size_t) + sizeof(int) + ENCRYPTION_HEADER_SIZE, "testing.testing.one.two.three", 30) == 0);
    free(resp_msg);
    delete mm;
    delete log;
    delete ss;
    delete resp;
    free(msg);
}

TEST(storageservertests, tls_setup)
{
    auto mm = new SMockMessageManager();
    auto log = new MockLog();

    auto actx = new DiggiAPI();
    actx->SetMessageManager(mm);
    actx->SetLogObject(log);
    auto ss = new StorageServer(actx);

    std::string filename = "cert.pem";
    std::string mode = "rb";
    short fd;
    auto resp = new msg_async_response_t();
    resp->context = ss;
    auto msg = mm->allocateMessage("file_io_func", sizeof(size_t) + filename.length() + sizeof(size_t) + mode.length(), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FOPEN;

    uint8_t *ptr = msg->data;
    Pack::pack<size_t>(&ptr, filename.length());
    Pack::packBuffer(&ptr, (uint8_t *)filename.c_str(), (unsigned int)filename.length());
    Pack::pack<size_t>(&ptr, mode.length());
    Pack::packBuffer(&ptr, (uint8_t *)mode.c_str(), (unsigned int)mode.length());

    resp->msg = msg;
    StorageServer::fileIoFopen(resp, 1);

    auto resp_msg = mm->GetOutboundMessage();
    EXPECT_TRUE(resp_msg->size == (sizeof(msg_t) + sizeof(int))); // + ENCRYPTED_BLK_SIZE));
    ptr = resp_msg->data;
    fd = Pack::unpack<short>(&ptr);

    //printf("fopen test returned %d\n", fd);
    EXPECT_TRUE(fd != 0);

    free(resp_msg);
    free(msg);
    delete resp;

    // test fseek END
    resp = new msg_async_response_t();
    resp->context = ss;
    msg = mm->allocateMessage("file_io_func", sizeof(short) + sizeof(off_t) + sizeof(int), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FOPEN;

    ptr = msg->data;

    Pack::pack<short>(&ptr, fd);
    Pack::pack<off_t>(&ptr, 0);
    Pack::pack<int>(&ptr, SEEK_END);

    resp->msg = msg;
    StorageServer::fileIoFseek(resp, 1);
    resp_msg = mm->GetOutboundMessage();
    EXPECT_TRUE(resp_msg->size == (sizeof(msg_t) + sizeof(int)));
    ptr = resp_msg->data;
    int retval = Pack::unpack<int>(&ptr);
    EXPECT_TRUE(retval == 0);

    free(resp_msg);
    free(msg);
    delete resp;

    // test ftell
    resp = new msg_async_response_t();
    resp->context = ss;
    msg = mm->allocateMessage("file_io_func", sizeof(short), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FOPEN;

    ptr = msg->data;
    Pack::pack<short>(&ptr, fd);

    resp->msg = msg;
    StorageServer::fileIoFtell(resp, 1);

    resp_msg = mm->GetOutboundMessage();
    EXPECT_TRUE(resp_msg->size == (sizeof(msg_t) + sizeof(long)));
    ptr = resp_msg->data;
    long filesize = Pack::unpack<long>(&ptr);
    EXPECT_TRUE(filesize != -1);

    free(resp_msg);
    free(msg);
    delete resp;

    //test fseek 0 (SEEK_SET)
    resp = new msg_async_response_t();
    resp->context = ss;
    msg = mm->allocateMessage("file_io_func", sizeof(short) + sizeof(off_t) + sizeof(int), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FOPEN;

    ptr = msg->data;

    Pack::pack<short>(&ptr, fd);
    Pack::pack<off_t>(&ptr, 0);
    Pack::pack<int>(&ptr, SEEK_SET);

    resp->msg = msg;
    StorageServer::fileIoFseek(resp, 1);
    resp_msg = mm->GetOutboundMessage();
    EXPECT_TRUE(resp_msg->size == (sizeof(msg_t) + sizeof(int)));
    ptr = resp_msg->data;
    retval = Pack::unpack<int>(&ptr);
    EXPECT_TRUE(retval == 0);

    free(resp_msg);
    free(msg);
    delete resp;

    // test fread
    resp = new msg_async_response_t();
    resp->context = ss;
    msg = mm->allocateMessage("file_io_func", sizeof(size_t) + sizeof(size_t) + sizeof(int), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_FREAD;

    ptr = msg->data;
    Pack::pack<size_t>(&ptr, 1);
    Pack::pack<size_t>(&ptr, (size_t)filesize);
    Pack::pack<int>(&ptr, fd);
    resp->msg = msg;
    StorageServer::fileIoFread(resp, 1);

    resp_msg = mm->GetOutboundMessage();
    ptr = resp_msg->data;
    size_t readcount = Pack::unpack<size_t>(&ptr);
    char filecontent[readcount + 1];
    memset((void *)filecontent, 0, readcount + 1);
    Pack::unpackBuffer(&ptr, (uint8_t *)filecontent, (int)readcount);

    EXPECT_TRUE(readcount != 0);

    free(resp_msg);
    free(msg);
    delete resp;

    // test fclose
    resp = new msg_async_response_t();
    resp->context = ss;
    msg = mm->allocateMessage("file_io_func", sizeof(int), CALLBACK, CLEARTEXT);
    msg->type = FILEIO_CLOSE;

    ptr = msg->data;
    Pack::pack<short>(&ptr, fd);
    resp->msg = msg;
    StorageServer::fileIoFclose(resp, 1);

    resp_msg = mm->GetOutboundMessage();
    ptr = resp_msg->data;
    retval = Pack::unpack<int>(&ptr);

    EXPECT_TRUE(retval == 0);

    free(resp_msg);
    free(msg);
    delete resp;

    delete mm;
    delete log;
    delete ss;
}
