#ifndef STORAGE_SERVER_H
#define STORAGE_SERVER_H
/**
 * @file StorageServer.h
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief header file for StorageServer
 * @version 0.1
 * @date 2020-01-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include <map>
#include "messaging/IMessageManager.h"
#include "AsyncContext.h"
#include "datatypes.h"
#include "DiggiAssert.h"
#include "DiggiGlobal.h"
#include "Logging.h"
#include "posix/intercept.h"
#include "misc.h"

#ifndef DIGGI_ENCLAVE

#include <sys/types.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#endif
#include "runtime/DiggiAPI.h"

class StorageServer
{

#ifdef TEST_DEBUG
#include <gtest/gtest_prod.h>
    /**
    * Declaring friend classes which may invoke private methods and fields on StorageServer objects.
    */
    FRIEND_TEST(storageservertests, openmessage);
    FRIEND_TEST(storageservertests, readmessage_empty_file);
    FRIEND_TEST(storageservertests, writemessage);
    FRIEND_TEST(storageservertests, closemessage);
    FRIEND_TEST(storageservertests, openmessage_append);
    FRIEND_TEST(storageservertests, lseek_beginning);
    FRIEND_TEST(storageservertests, readmessage_back);
    FRIEND_TEST(storageservertests, readmessage_seekback);

#endif
    /// Diggi api reference
    IDiggiAPI *diggiapi;

    /**
	* Lseek is a O(1) operation in ext4 so skipping back and forth between 
    */
    /// in memory file map, for DRAM storage, manages a mutable buffer structure per file descriptor
    std::map<int, zcstring> in_memory_data;
    /// maps paths to file descriptors.
    std::map<std::string, int> filepaths;
    /// maps descriptors to file paths
    std::map<int, std::string> filedes_to_path;
    /// map between file descriptor and virtual inode number (used for in memory files)
    bool in_memory;
    /// next virtual file descriptor, for in-memory storage.
    int next_fd;

    /*
    File pointers can't be passed from untrusted to trusted memory.
    SS thus needs to keep a map of FILE pointers and file descriptors,
    and then pass just the file descriptor to trusted memory.
    StorageManager then holds a "fake" FILE struct to emulate this for 
    applications running in trusted space.
    */
    std::map<short, FILE *> openfilesmap;

public:
    StorageServer(IDiggiAPI *mman);
    void initializeServer();

    static void fileIoFopen(void *ctx, int status);
    static void fileIoFread(void *ctx, int status);
    static void fileIoFseek(void *ctx, int status);
    static void fileIoFtell(void *ctx, int status);
    static void fileIoFclose(void *ctx, int status);
    static void fileIoOpen(void *ctx, int status);
    static void fileIoRead(void *ctx, int status);
    static void fileIoWrite(void *msg, int status);
    static void fileIoClose(void *msg, int status);
    static void fileIoUnlink(void *msg, int status);
    static void ServerRand(void *msg, int status);
};
#endif