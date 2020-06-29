#ifndef PSX_WAIT_H
#define PSX_WAIT_H

pid_t wait(int *stat_loc);
pid_t waitpid(pid_t pid, int *stat_loc, int options);

#endif