#include "copy_r.h"
/**
 * @brief for copying a buffer of a given size
 * 
 * @param ptr 
 * @param size 
 * @return void* 
 */
void* copy_r(void * ptr, size_t size)
{
	void * retval = malloc(size);
	memcpy(retval, ptr, size);
	return retval;
}