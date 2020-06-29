
#include "posix/time_stubs.h"

/**
 * @brief retrieve local time.
 * @warning returns nullpointer, may cause undetected bugs.
 * 
 * @param timer 
 * @return struct tm* 
 */
struct tm *i_localtime(const time_t *timer) {
	return nullptr;
}
