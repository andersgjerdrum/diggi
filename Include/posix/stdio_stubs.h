#ifndef SYSCALLWRAPPER_H
#define SYSCALLWRAPPER_H
#include "posix/syscall_def.h"
#include "posix/io_stubs.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <stdarg.h>
#include <stdarg.h>
#include <stdio.h>
#define PRINTF_BUFFERSIZE 16384
/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
int i_printf(const char *fmt, ...);
int i_vprintf(const char *fmt, va_list args);
int	i_sscanf(const char *str, const char *format, ...);
int	i_sprintf(char * str, const char * format, ...);
int i_vsscanf(const char * s, const char * format, va_list arg);
int i_vsprintf(char * s, const char * format, va_list arg);


int snprintf(char * s, size_t count, const char* format, ...);
int vsnprintf_(char* buffer, size_t count, const char* format, va_list va);

#ifndef vsnsprintf
#define vsnsprintf vsnsprintf_
#endif


#ifdef __cplusplus
}
#endif // __cplusplus

#endif