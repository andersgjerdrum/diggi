/**
 * @file unistdStubs.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief various signal and process syscalls, none of whuch are implemented.
 * @version 0.1
 * @date 2020-02-06
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "posix/unistd_stubs.h"
#include "DiggiAssert.h"


int				i_sigemptyset(sigset_t *set){DIGGI_ASSERT(false);return 0;}
int				i_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact){DIGGI_ASSERT(false);return 0;}
pid_t			i_fork(void){DIGGI_ASSERT(false);return 0;}
sighandler_t	i_signal(int signum, sighandler_t handler){DIGGI_ASSERT(false);return 0;}
unsigned int    i_alarm(unsigned int seconds){DIGGI_ASSERT(false); return 0;};
int				i_execle(const char *path, const char *arg, ... /*, (char *) NULL, char * const envp[] */){DIGGI_ASSERT(false);return 0;}
void			i__exit(int status){DIGGI_ASSERT(false);}
