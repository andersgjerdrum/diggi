#ifndef UNISTD_STUBS_H
#define UNISTD_STUBS_H
#include "posix/syscall_def.h"

#ifdef __cplusplus
extern "C" {
#endif
#if defined(DIGGI_ENCLAVE)

/*	
	OBS! defined twice, 
	once here and once in ioStubTypes.h
*/
typedef int pid_t;


typedef unsigned long sigset_t;
typedef void(*__sighandler_t) (int);

#define SIG_IGN ((__sighandler_t) 1)            /* Ignore signal.  */
#define SIG_DFL  ((__sighandler_t) 0)            /* Default action.  */
typedef sigset_t __sigset_t;

typedef void(*sighandler_t)(int);

/* Structure describing the action to be taken when a signal arrives.  */
struct sigaction
{
	/* Signal handler.  */
#ifdef __USE_POSIX199309
	union
	{
		/* Used if SA_SIGINFO is not set.  */
		__sighandler_t sa_handler;
		/* Used if SA_SIGINFO is set.  */
		void(*sa_sigaction) (int, siginfo_t *, void *);
	}
	__sigaction_handler;
# define sa_handler     __sigaction_handler.sa_handler
# define sa_sigaction   __sigaction_handler.sa_sigaction
#else
	__sighandler_t sa_handler;
#endif

	/* Additional set of signals to be blocked.  */
	__sigset_t sa_mask;

	/* Special flags.  */
	int sa_flags;

	/* Restore handler.  */
	void(*sa_restorer) (void);
};
#else
#include <signal.h>
#endif
/*
	Not supported
*/

int				i_sigemptyset(sigset_t *set);
int				i_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
pid_t			i_fork(void);
sighandler_t	i_signal(int signum, sighandler_t handler);
unsigned int    i_alarm(unsigned int seconds);

int				i_execle(const char *path, const char *arg, ... /*, (char *) NULL, char * const envp[] */);
void			i__exit(int status);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif