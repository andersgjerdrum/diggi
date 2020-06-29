#include "posix/syscall_def.h"


void *	i_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
int		i_munmap(void *__addr, size_t __len);
void *	i_mremap(void *__addr, size_t __old_len, size_t __new_len, int __flags, ...);