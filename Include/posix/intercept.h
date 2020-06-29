#ifndef INTERCEPT_H
#define INTERCEPT_H
#include <signal.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif
void set_syscall_interposition(unsigned int state);
#if  !defined(DIGGI_ENCLAVE) && !defined(UNTRUSTED_APP)
#define SYSCALL_DEFINITION(rettype, func, ...) \
rettype __wrap_##func(__VA_ARGS__);\
extern rettype __real_##func(__VA_ARGS__);

SYSCALL_DEFINITION(int*, __errno_location,void);
//SYSCALL_DEFINITION(int, printf, const char * format, ...);
//SYSCALL_DEFINITION(int, vprintf, const char * format, va_list arg);
SYSCALL_DEFINITION(int, rand, void);
SYSCALL_DEFINITION(DIR *, opendir, const char *name);
SYSCALL_DEFINITION(struct dirent *, readdir, DIR *dirp);
SYSCALL_DEFINITION(int, closedir, DIR *dirp);
SYSCALL_DEFINITION(int, dup, int oldfd);
SYSCALL_DEFINITION(FILE *, fopen, const char * filename, const char * mode);
SYSCALL_DEFINITION(char *, fgets, char * str, int num, FILE * stream);
SYSCALL_DEFINITION(int, fclose, FILE * stream);
SYSCALL_DEFINITION(int, fputc, int character, FILE * stream);
SYSCALL_DEFINITION(int, fflush, FILE * stream);
SYSCALL_DEFINITION(size_t, fread, void * ptr, size_t size, size_t count, FILE * stream);
//SYSCALL_DEFINITION(int, fprintf, FILE * stream, const char * format, ...);
SYSCALL_DEFINITION(size_t, fwrite, const void * ptr, size_t size, size_t count, FILE * stream);
//SYSCALL_DEFINITION(int, vfprintf, FILE * stream, const char * format, va_list arg);
SYSCALL_DEFINITION(int, chdir, const char *path);
SYSCALL_DEFINITION(int, dup2, int oldfd, int newfd);
SYSCALL_DEFINITION(int, fseeko, FILE *stream, off_t offset, int whence);
SYSCALL_DEFINITION(int, fseek, FILE *stream, off_t offset, int whence);
SYSCALL_DEFINITION(long, ftell, FILE *stream);
SYSCALL_DEFINITION(int, fputs, const char * str, FILE * stream);
SYSCALL_DEFINITION(int, puts, const char * str);
SYSCALL_DEFINITION(int, fgetc, FILE * stream);
SYSCALL_DEFINITION(int, fileno, FILE *stream);

SYSCALL_DEFINITION(int, lstat, const char *path, struct stat *buf);

SYSCALL_DEFINITION(long, sysconf, int name);
SYSCALL_DEFINITION(ssize_t, readlink, const char *path, char *buf, size_t bufsiz);
SYSCALL_DEFINITION(int, fchmod, int fildes, mode_t mode);
SYSCALL_DEFINITION(unsigned int, sleep, unsigned int seconds);
SYSCALL_DEFINITION(int, getpid, void);
SYSCALL_DEFINITION(time_t, time, time_t *t);
SYSCALL_DEFINITION(struct tm *, gmtime, const time_t *timer);
SYSCALL_DEFINITION(int, utime, const char *filename, const struct utimbuf *times);
SYSCALL_DEFINITION(int, utimes, const char *filename, const struct timeval times[2]);
SYSCALL_DEFINITION(int, gettimeofday, struct timeval *tv, struct timezone *tz);
SYSCALL_DEFINITION(int, stat, const char *path, struct stat *buf);
SYSCALL_DEFINITION(int, fstat, int fd, struct stat *buf);



SYSCALL_DEFINITION(int, close, int fd);
SYSCALL_DEFINITION(int, access, const char *pathname, int mode);
SYSCALL_DEFINITION(char *, getcwd, char *buf, size_t size);
SYSCALL_DEFINITION(int, ftruncate, int fd, off_t length);
SYSCALL_DEFINITION(int, fcntl, int fd, int cmd, ... /*args */);
SYSCALL_DEFINITION(int, fsync, int fd);
SYSCALL_DEFINITION(char *, getenv, const char *name);
SYSCALL_DEFINITION(uid_t, getuid, void);
SYSCALL_DEFINITION(uid_t, geteuid, void);
SYSCALL_DEFINITION(int, fchown, int fd, uid_t owner, gid_t group);
SYSCALL_DEFINITION(off_t, lseek, int fd, off_t offset, int whence);
SYSCALL_DEFINITION(int, open, const char *path, int oflags, mode_t mode);
SYSCALL_DEFINITION(ssize_t, read, int fildes, void *buf, size_t nbyte);
SYSCALL_DEFINITION(ssize_t, write, int fd, const void *buf, size_t count);
SYSCALL_DEFINITION(int, unlink, const char *pathname);
SYSCALL_DEFINITION(int, mkdir, const char *path, mode_t mode);
SYSCALL_DEFINITION(int, rmdir, const char *path);
SYSCALL_DEFINITION(mode_t, umask, mode_t mask);
SYSCALL_DEFINITION(int, socket, int domain, int type, int protocol);
SYSCALL_DEFINITION(int, getaddrinfo, const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
SYSCALL_DEFINITION(void, freeaddrinfo, struct addrinfo *res);
SYSCALL_DEFINITION(int, setsockopt, int sockfd, int level, int optname, const void *optval, socklen_t optlen);
SYSCALL_DEFINITION(int, getpeername, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
SYSCALL_DEFINITION(ssize_t, send, int sockfd, const void *buf, size_t len, int flags);
SYSCALL_DEFINITION(ssize_t, sendto, int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
SYSCALL_DEFINITION(ssize_t, recvfrom, int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
SYSCALL_DEFINITION(int, bind, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
SYSCALL_DEFINITION(int, getsockname, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
SYSCALL_DEFINITION(ssize_t, recv, int sockfd, void *buf, size_t len, int flags);
SYSCALL_DEFINITION(int, connect, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
SYSCALL_DEFINITION(int, accept, int sockfd, struct sockaddr *addr, socklen_t *addrlen);
SYSCALL_DEFINITION(int, listen, int sockfd, int backlog);
SYSCALL_DEFINITION(int, select, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
SYSCALL_DEFINITION(int, getsockopt, int sockfd, int level, int optname, void *optval, socklen_t *optlen);
SYSCALL_DEFINITION(int, sigemptyset, sigset_t *set);
SYSCALL_DEFINITION(int, sigaction, int signum, const struct sigaction *act, struct sigaction *oldact);
SYSCALL_DEFINITION(pid_t, fork, void);
SYSCALL_DEFINITION(sighandler_t, signal, int signum, sighandler_t handler);
SYSCALL_DEFINITION(int, execle, const char *path, const char *arg, ... /*, ,char *) NULL, char * const envp[] */);
SYSCALL_DEFINITION(void, _exit, int status);
//SYSCALL_DEFINITION(int, sscanf, const char *str, const char *format, ...);
SYSCALL_DEFINITION(int, sprintf, char * str, const char * format, ...);
SYSCALL_DEFINITION(uint32_t, htonl, uint32_t hostlong);
SYSCALL_DEFINITION(uint16_t, htons, uint16_t hostshort);
SYSCALL_DEFINITION(uint32_t, ntohl, uint32_t netlong);
SYSCALL_DEFINITION(uint16_t, ntohs, uint16_t netshort);
SYSCALL_DEFINITION(int, inet_aton, const char *cp, struct in_addr *inp);
SYSCALL_DEFINITION(in_addr_t, inet_addr, const char *cp);
SYSCALL_DEFINITION(in_addr_t, inet_network, const char *cp);
SYSCALL_DEFINITION(char *, inet_ntoa, struct in_addr in);
SYSCALL_DEFINITION(struct in_addr, inet_makeaddr, int net, int host);
SYSCALL_DEFINITION(in_addr_t, inet_lnaof, struct in_addr in);
SYSCALL_DEFINITION(in_addr_t, inet_netof, struct in_addr in);
SYSCALL_DEFINITION(const char *, inet_ntop, int af, const void *src, char *dst, socklen_t size);
SYSCALL_DEFINITION(struct tm *, localtime, const time_t *timer);


SYSCALL_DEFINITION(int, pthread_mutexattr_init,pthread_mutexattr_t *attr);
SYSCALL_DEFINITION(int, pthread_mutexattr_settype,pthread_mutexattr_t * attr, int type);
SYSCALL_DEFINITION(int, pthread_mutexattr_destroy,pthread_mutexattr_t *attr);
SYSCALL_DEFINITION(int, pthread_mutex_init,pthread_mutex_t *mutext, const pthread_mutexattr_t *attr);
SYSCALL_DEFINITION(int, pthread_mutex_lock,pthread_mutex_t *mutex);
SYSCALL_DEFINITION(int, pthread_mutex_unlock,pthread_mutex_t *mutex);
SYSCALL_DEFINITION(int, pthread_mutex_trylock,pthread_mutex_t *mutex);
SYSCALL_DEFINITION(int, pthread_mutex_destroy,pthread_mutex_t *mutex);
SYSCALL_DEFINITION(int, pthread_create,pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
SYSCALL_DEFINITION(int, pthread_join,pthread_t thread, void **value_ptr);
SYSCALL_DEFINITION(int, pthread_cond_wait,pthread_cond_t *cond, pthread_mutex_t *mutex);
SYSCALL_DEFINITION(int, pthread_cond_broadcast,pthread_cond_t *cond);
SYSCALL_DEFINITION(int, pthread_cond_signal,pthread_cond_t *cond);
SYSCALL_DEFINITION(int, pthread_cond_init,pthread_cond_t *cond, const pthread_condattr_t *attr);


#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif