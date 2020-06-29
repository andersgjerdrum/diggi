#ifndef TIMESTUBS_H
#define TIMESTUBS_H
#include "posix/syscall_def.h"

#ifdef __cplusplus
extern "C" {
#endif



#include <sys/time.h>
#include <unistd.h>

struct tm *i_localtime(const time_t *timer);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // TIMESTUBS_H
