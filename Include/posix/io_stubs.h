#ifndef	IOSTUB_H
#define IOSTUB_H
#include "posix/syscall_def.h"
#include <stdio.h>

#include "datatypes.h"
#include "copy_r.h"

#ifdef __cplusplus
extern "C" {
#endif
#define	I_SC_PAGESIZE			11

#include "datatypes.h"
#include "base_stubs.h"


void iostub_setcontext(void *ctx, int enc);
#if defined(DIGGI_ENCLAVE)
	#include "posix/io_types.h"
	#include <stdio.h>
	#include <signal.h>
	#include <sys/types.h>
	#include <sys/socket.h>




/*	
	Workaround for untrusted contexts not running in enclave with custom STD lib
*/
int s_isln(mode_t m);

#define S_ISLNK(mode)  s_isln(mode)
int wifexited(int);
#define WIFEXITED(stat_val) wifexited(stat_val)
#define WIFSIGNALED(stat_val)	wifexited(stat_val)
#define WEXITSTATUS(stat_val) wifexited(stat_val)
#define WTERMSIG(stat_val) wifexited(stat_val) /* allways SIGHUP because we dont care*/

#define _SC_PAGESIZE I_SC_PAGESIZE

#else		
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <sys/time.h>
	#include <stddef.h>
	#include <stdio.h>
	#include <dirent.h>
	#include <sys/select.h>
#endif

/*
	In use
*/
//int set_errno(int val); // now defined in baseStubs
//int *			i_errno(void);
DIR *			i_opendir(const char *name);
struct dirent *	i_readdir(DIR *dirp);
int				i_closedir(DIR *dirp);
int				i_dup(int oldfd);

FILE *			i_fopen(const char * filename, const char * mode);
char *			i_fgets(char * str, int num, FILE * stream);
int				i_fclose(FILE * stream);
int				i_fputc(int character, FILE * stream);
int				i_fflush(FILE * stream);
size_t			i_fread(void * ptr, size_t size, size_t count, FILE * stream);
int				i_fprintf(FILE * stream, const char * format, ...);
size_t			i_fwrite(const void * ptr, size_t size, size_t count, FILE * stream);
int				i_vfprintf(FILE * stream, const char * format, va_list arg);
int				i_chdir(const char *path);
int				i_dup2(int oldfd, int newfd);
int				i_fseeko(FILE *stream, off_t offset, int whence);
int				i_fseek(FILE *stream, off_t offset, int whence);
long			i_ftell(FILE * stream);
int				i_fputs(const char * str, FILE * stream);
int				i_fgetc(FILE * stream);
int				i_fileno(FILE *stream);
int             i_ferror ( FILE * stream );

/* Implemented */
int				i_lstat(const char *path, struct stat *buf);
long			i_sysconf(int name);
ssize_t			i_readlink(const char *path, char *buf, size_t bufsiz);
int				i_fchmod(int fildes, mode_t mode);
unsigned int	i_sleep(unsigned int seconds);
int				i_getpid(void);
int				i_stat(const char *path, struct stat *buf);
#ifdef DIGGI_ENCLAVE
int				stat(const char *path, struct stat *buf);

#endif // DIGGI_ENCLAVE

int				i_fstat(int fd, struct stat *buf);
int				i_access(const char *pathname, int mode);
/*
Allways have CWD be the same virtual directory
*/
char *			i_getcwd(char *buf, size_t size);
int				i_ftruncate(int fd, off_t length);
int				i_fsync(int fd);
char *			i_getenv(const char *name);
uid_t			i_getuid(void);
uid_t			i_geteuid(void);
int				i_fchown(int fd, uid_t owner, gid_t group);
off_t			i_lseek(int fd, off_t offset, int whence);
int				i_open(const char *path, int oflags, mode_t mode);
ssize_t			i_read(int fildes, void *buf, size_t nbyte);
ssize_t			i_write(int fd, const void *buf, size_t count);
int				i_unlink(const char *pathname);
int				i_mkdir(const char *path, mode_t mode);
int				i_rmdir(const char *path);
mode_t			i_umask(mode_t mask);
int             i_close(int fd);
int             i_fcntl(int fd, int cmd, ... /*args */);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
