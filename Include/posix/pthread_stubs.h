#ifndef PTHREADSTUBS_H
#define PTHREADSTUBS_H
#include "posix/syscall_def.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifndef DIGGI_ENCLAVE
#include <pthread.h>
#include <sys/errno.h>
#endif

#ifndef UNTRUSTED_APP

#include "posix/pthread_types.h"
#include <errno.h>

void	pthread_stubs_set_thread_manager(void * tpool);
void pthread_stubs_unset_thread_manager();

int		i_pthread_mutexattr_init(pthread_mutexattr_t *);
int		i_pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int		i_pthread_mutexattr_destroy(pthread_mutexattr_t *);
int		i_pthread_mutex_init(pthread_mutex_t *mutext, const pthread_mutexattr_t *attr);
int		i_pthread_mutex_lock(pthread_mutex_t *mutex);
int		i_pthread_mutex_unlock(pthread_mutex_t *mutex);
int		i_pthread_mutex_trylock(pthread_mutex_t *mutex);
int		i_pthread_mutex_destroy(pthread_mutex_t *mutex);
int		i_pthread_create(pthread_t *thread, const pthread_attr_t *attr,void *(*start_routine)(void*), void *arg);
int		i_pthread_join(pthread_t thread, void **value_ptr);
int		i_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int		i_pthread_cond_broadcast(pthread_cond_t *cond);
int		i_pthread_cond_signal(pthread_cond_t *cond);
int		i_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int     i_pthread_self(void);

#endif

#ifdef __cplusplus
}
#endif // __cplusplus


#endif