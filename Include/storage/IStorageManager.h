#ifndef ISTORAGEMANAGER_H
#define ISTORAGEMANAGER_H

#ifndef DIGGI_ENCLAVE

#include <unistd.h>
#include <fcntl.h>

#else

#include "posix/io_types.h"

#endif
#include "datatypes.h"
class IStorageManager {
public:
	IStorageManager() {};
    virtual int async_stat(const char *path, struct stat *buf) = 0;
    virtual int async_fstat(int fd, struct stat *buf) = 0;
    virtual void async_fopen(const char* filename, const char* mode, async_cb_t cb, void *context) = 0;
    virtual void async_fseek(FILE* f, off_t offset, int whence, async_cb_t cb, void *context) = 0;
    virtual void async_ftell(FILE* f, async_cb_t cb, void *context) = 0;
    virtual void async_fread(size_t size, size_t count, FILE* f, async_cb_t cb, void *context) = 0;
    virtual void async_fclose(FILE *, async_cb_t cb, void *context) = 0;
    virtual void async_close(int fd, bool omit_from_log) = 0;
    virtual int async_access(const char *pathname, int mode) = 0;
    virtual char *async_getcwd(char *buf, size_t size) = 0;
    virtual int async_ftruncate(int fd, off_t length) = 0;
    virtual int async_fcntl(int fd, int cmd, struct flock *lock) = 0;
    virtual int async_fsync(int fd) = 0;
    virtual char *async_getenv(const char *name) = 0;
    virtual uid_t async_getuid(void) = 0;
    virtual uid_t async_geteuid(void) = 0;
    virtual int async_fchown(int fd, uid_t owner, gid_t group) = 0;
    virtual off_t async_lseek(int fd, off_t offset, int whence) = 0;
    virtual void async_open(const char *path, int oflags, mode_t mode, async_cb_t cb, void *context, bool encrypted, bool omit_from_log) = 0;
    virtual void async_read_internal(int fildes, read_type_t type, void * buf, size_t nbyte, async_cb_t cb, void * context, bool encrypted, bool omit_from_log) = 0;
    virtual void async_read(int fd, void *buf, size_t nbyte, async_cb_t cb, void *context, bool encrypted, bool omit_from_log) = 0;
    virtual void async_write(int fd, const void *buf, size_t count, async_cb_t cb, void * context, bool encrypted, bool omit_from_log) = 0;
    virtual void async_unlink(const char *pathname, async_cb_t cb, void * context) = 0;
    virtual int async_mkdir(const char *path, mode_t mode) = 0;
    virtual int async_rmdir(const char *path) = 0;
    virtual mode_t async_umask(mode_t mode) = 0;
	virtual ~IStorageManager() {};
};



#endif
