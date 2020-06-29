#include "posix/io_types.h"
#include "posix/io_stubs.h"

int S_ISLNK(mode_t m);
#define S_IFSOCK	__S_IFSOCK
#define    __S_IFSOCK      0140000 /* Socket.  */