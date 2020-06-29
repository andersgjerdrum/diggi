/**
 * @file baseStubs.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @author Lars Brenna (lars.brenna@uit.no)
 * @brief Misc posix api calls for errno, getpid, sleep, and various time apis.
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "DiggiGlobal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "posix/base_stubs.h"

    static struct tm internal_gmtime = {0};
    static unsigned int s_since_epoch = 1549885704;
    static time_t time_mock = 1574243164;

    /*errno must be thread local*/
    static int __thread internal_errno = 0;
    static int __thread *errnoptr = NULL;
    /**
 * @brief Set the errno object
 * Perthread, setting the correct value of the error value retrievable via errno.
 * @param val 
 * @return int 
 */
    int set_errno(int val)
    {
        if (errnoptr == NULL)
        {
            errnoptr = &internal_errno;
        }
        internal_errno = val;
        return val; // not sure if this is correct behavior, but required by compiler.
    }
    /**
 * @brief retrieve errno variable.
 * Thread local
 * 
 * @return int* 
 */
    int *i_errno(void)
    {
        return errnoptr;
    }

    /**
 * @brief return callers process identifier
 * Simulated quite crudely to return static variable
 * 
 * @return int 
 */
    int i_getpid(void)
    {
        return 26802;
    }

    /**
 * @brief sleep for seconds
 * @warning does not sleep the expected amount of time, 
 * rather yields into diggi runtime threadpool, allowing all other threads to execute.
 * After a roundrobin scheduling round, the instruction pointer returns here to continue execution
 * 
 */
    extern "C" unsigned int i_sleep(unsigned int seconds)
    {
        IDiggiAPI *ctx = GET_DIGGI_GLOBAL_CONTEXT();
        DIGGI_ASSERT(ctx);
        ctx->GetThreadPool()->Yield();
        return 0;
    }
#ifdef TEST_DEBUG
    void reset_time_mock()
    {
        time_mock = 1574243164;
        internal_gmtime = {0};
        s_since_epoch = 1549885704;
    }
#endif
    /**
 * @brief crudely simulating the passage of time by incrementing monotonic counter.
 * @warning not thread-safe.  Multiple writes may lead to ABA case where thread returns to old value, breaking monotonicity.
 * 
 * @param t 
 * @return time_t 
 */
    time_t i_time(time_t *t)
    {
        return ++time_mock;
    }
    /**
 * @brief crudely simulating clock, monotonmically incrementing time. 
 * Only supports 24 hours, not days or weeks
 * @warning not thread-safe.  Multiple writes may lead to ABA case where thread returns to old value, breaking monotonicity.
 * @param timer 
 * @return struct tm* 
 */
    struct tm *i_gmtime(const time_t *timer)
    {
        // increment internal_gmtime; a 24 hour clock that only supports 24 hours, not days, weeks etc.
        if (internal_gmtime.tm_sec == 59)
        {
            internal_gmtime.tm_sec = 0;
            if (internal_gmtime.tm_min == 59)
            {
                internal_gmtime.tm_min = 0;
                if (internal_gmtime.tm_hour == 23)
                {
                    internal_gmtime.tm_hour = 0;
                }
                else
                {
                    internal_gmtime.tm_hour++;
                }
            }
            else
            {
                internal_gmtime.tm_min++;
            }
        }
        else
        {
            internal_gmtime.tm_sec++;
        }
        return &internal_gmtime;
    }

    /**
 * @brief utime not implemented, always returns 0.
 * @warning does not cause assertion failiure, may lead to bugs which are hard to catch.
 * @param filename 
 * @param times 
 * @return int 
 */
    int i_utime(const char *filename, const struct utimbuf *times)
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    /**
 * @brief utimes not implemented, always returns 0.
 * @warning does not cause assertion failiure, may lead to bugs which are hard to catch.
 * @param filename 
 * @param times 
 * @return int 
 */
    int i_utimes(const char *filename, const struct timeval times[2])
    {
        DIGGI_ASSERT(false);
        return 0;
    }
    /**
 * @brief gettimeofday monotonically increments counter to simulate  passage of time
 * @warning not thread-safe.  Multiple writes may lead to ABA case where thread returns to old value, breaking monotonicity.
 * @param tv 
 * @param tz 
 * @return int 
 */
    int i_gettimeofday(struct timeval *tv, struct timezone *tz)
    {
        if (tv != nullptr)
        {
            tv->tv_sec = s_since_epoch++;
            tv->tv_usec = 0;
        }
        if (tz != nullptr)
        {
            tz->tz_minuteswest = 0; //minutes west of Greenwich
            tz->tz_dsttime = 1;     // DST applies
        }
        return 0;
    }

#ifdef __cplusplus
}
#endif /* __cplusplus */