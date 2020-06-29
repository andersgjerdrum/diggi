#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <sys/types.h>
#include <math.h>
/**
 * @file StorageManager.h
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief header file for StorageManager api, for persistant encrypted storage.
 * @version 0.1
 * @date 2020-01-29
 * @copyright Copyright (c) 2020
 * 
 */
#ifndef DIGGI_ENCLAVE
#include <unistd.h>
#include <fcntl.h>
#else
#include "posix/io_types.h"
#endif

#include <string.h>
#include <stdio.h>
#include <map>
#include <vector>

#include "posix/stdio_stubs.h"
#include "storage/IStorageManager.h"
#include "runtime/DiggiAPI.h"
#include "Seal.h"
#include "Logging.h"
#include "misc.h"
#include "storage/crc.h"

typedef struct std::map<std::string, std::map<size_t, uint32_t>> crc_vector_t;

class StorageManager : public IStorageManager
{
    /**
    * Reference to diggi api
    */
    IDiggiAPI *func_context;
    /** 
     * Reference to encryption algorithm
     */
    ISealingAlgorithm *sealer;
    std::map<int, int> pending_write_map;

    /// map maintaining the virtual offset as expected by applications
    std::map<int, off_t> lseekstatemap;
    crc_vector_t block_2_crc;
    std::map<int, off_t> size_of_file;
    /// maps paths to file descriptors.
    std::map<std::string, int> filepaths;
    /// maps descriptors to file paths
    std::map<int, std::string> filedes_to_path;
    /// map between file descriptor and virtual inode number (used for in memory files)
    std::map<int, size_t> inodes;
    /// counter to deliver monotonicaly increasing timestamps, used for stat/fstat
    size_t monotonic_time_update;
    /// next virtual inode, monotonically increasing number, used for in memory mode.
    size_t next_virtual_inode;

public:
    StorageManager(IDiggiAPI *context, ISealingAlgorithm *seal);
    void GetCRCReplayVector(crc_vector_t **vectors);
    void SetCRCReplayVector(crc_vector_t *vectors);

    static char *normalizePath(char *path);

    int async_stat(const char *path, struct stat *buf);
    int async_fstat(int fd, struct stat *buf);

    void async_fopen(const char *filename, const char *mode, async_cb_t cb, void *context);
    static void async_fopen_cb(void *ptr, int status);
    void async_fseek(FILE *f, off_t offset, int whence, async_cb_t cb, void *context);
    void async_ftell(FILE *f, async_cb_t cb, void *context);
    void async_fread(size_t size, size_t count, FILE *f, async_cb_t cb, void *context);
    static void async_fread_cb(void *ptr, int status);

    void async_fclose(FILE *, async_cb_t cb, void *context);

    void async_close(int fd, bool omit_from_log);

    int async_access(const char *pathname, int mode);

    char *async_getcwd(char *buf, size_t size);

    int async_ftruncate(int fd, off_t length);

    int async_fcntl(int fd, int cmd, struct flock *lock);

    int async_fsync(int fd);

    char *async_getenv(const char *name);

    uid_t async_getuid(void);

    uid_t async_geteuid(void);

    int async_fchown(int fd, uid_t owner, gid_t group);

    off_t async_lseek(int fd, off_t offset, int whence);

    void async_open(const char *path, int oflags, mode_t mode, async_cb_t cb, void *context, bool encrypted, bool omit_from_log);

    static void async_open_cb(void *ptr, int status);

    static void async_read_internal_cb(void *ptr, int status);

    void async_read_internal(int fd, read_type_t type, void *buf, size_t nbyte, async_cb_t cb, void *context, bool encrypted, bool omit_from_log);

    void async_read(int fd, void *buf, size_t nbyte, async_cb_t cb, void *context, bool encrypted, bool omit_from_log);

    static void async_write_internal_cb(void *ptr, int status);

    void async_write(int fd, const void *buf, size_t count, async_cb_t cb, void *context, bool encrypted, bool omit_from_log);

    void async_unlink(const char *pathname, async_cb_t cb, void *context);

    int async_mkdir(const char *path, mode_t mode);

    int async_rmdir(const char *path);

    mode_t async_umask(mode_t mode);
    ~StorageManager();

private:
    std::map<short, std::string> fd_to_filename_map;
};
#endif