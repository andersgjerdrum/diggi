#include "posix/syscall_def.h"

#include "posix/io_types.h"
typedef struct sched_param {
		int sched_ss_low_priority; 
		struct timespec sched_ss_repl_period;
		struct timespec sched_ss_init_budget;
		int             sched_ss_max_repl;
}sched_param;