#ifndef BASESTUBS_H
#define BASESTUBS_H
#include "posix/syscall_def.h"
#include <stddef.h>
#include <time.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
* Stuff that is common to all *Stubs.
* Note: C-code!!
* @author: lars.brenna@uit.no
*/

#ifdef TEST_DEBUG
    void reset_time_mock();
#endif

    int set_errno(int val);
    int *i_errno(void);

    // time related syscalls:
    time_t i_time(time_t *t);
    struct tm *i_gmtime(const time_t *timer);
    int i_utime(const char *filename, const struct utimbuf *times);
    int i_utimes(const char *filename, const struct timeval times[2]);
    int i_gettimeofday(struct timeval *tv, struct timezone *tz);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
