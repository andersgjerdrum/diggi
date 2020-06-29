#include "posix/intercept.h"

/**
 * @file intercept.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @author Lars Brenna (lars.brenna@uit.no)
 * @brief stub implementaitons for POSIX intercept calls.
 * Used for untrusted runtime instances and unit tests to capture system call operations.
 * syscall.mk overrides linker symbols for select POSIX calls into this file instead.
 * Interposition state set via set_syscall_interposition() determine if calls are pass-through or are intercepted by the runtime.
 * @version 0.1
 * @date 2020-02-03
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "DiggiAssert.h"
#include "posix/time_stubs.h"
#include "posix/pthread_stubs.h"
#include "posix/unistd_stubs.h"
#include "posix/net_stubs.h"
#include "posix/io_stubs.h"
#include "posix/crypto_stubs.h"
#include "posix/net_utils.h"
#include "posix/io_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/// keep track of socket fd's to help route syscalls to correct handler
/// NOT THREAD SAFE :-O
/// TODO: make this thread safe
/// calls are either treated to file IO or network IO descriptors
#define MAX_SOCKET_FDS 10240
static int socket_fds[MAX_SOCKET_FDS] = {0};
static int socket_fd_index = 0;

static unsigned syscall_interposition = 0;
#define MAX_PRINTIF_SIZE 8092
/**
 * @brief Set the syscall interposition object
 * sets interposition state, and resets file descriptor accounting.
 * @param state 
 */
void set_syscall_interposition(unsigned int state) {
	syscall_interposition = state;
    /*
        reset for unit tests
    */
    memset(socket_fds,0,sizeof(int) * MAX_SOCKET_FDS);
}


/**
 * @brief check if fd is a file descriptor or network descriptor
 * 
 * @param fd 
 * @return true 
 * @return false 
 */
static bool is_fd_socket(int fd){
    for (int i=0;i<socket_fd_index;i++){
        if (socket_fds[i] == fd){
            return true;
        }
    }
    return false;
}
/**
 * @brief debug intercept calls.
 * usable if MAMA define is passed as compiler opt.
 * @param str 
 */
static inline void debug_printf(const char* str){
    #ifdef DEBUG_SYSCALL
        printf("DEBUG: %s\n", str);
    #endif
} 

/**
 * @brief only usable in unit tests or untrusted runtime instances
 * 
 */
#if  !defined(DIGGI_ENCLAVE) && !defined(UNTRUSTED_APP)

uint32_t __wrap_htonl(uint32_t hostlong) {
    debug_printf("htonl");
	if (syscall_interposition) { return i_htonl(hostlong); }
	else { return __real_htonl(hostlong); }
}
uint16_t __wrap_htons(uint16_t hostshort) {
    debug_printf("htons");
	if (syscall_interposition) { return i_htons(hostshort); }
	else { return __real_htons(hostshort); }
}
uint32_t __wrap_ntohl(uint32_t netlong) {
    debug_printf("ntohl");
	if (syscall_interposition) { return i_ntohl(netlong); }
	else { return __real_ntohl(netlong); }
}
uint16_t __wrap_ntohs(uint16_t netshort) {
    debug_printf("ntohs");
	if (syscall_interposition) { return i_ntohs(netshort); }
	else { return __real_ntohs(netshort); }
}
int __wrap_inet_aton(const char * cp, struct in_addr * inp) {
    debug_printf("aton");
	if (syscall_interposition) { return i_inet_aton(cp, inp); }
	else { return __real_inet_aton(cp, inp); }
}
in_addr_t __wrap_inet_addr(const char * cp) {
    debug_printf("inet_addr");
	if (syscall_interposition) { return i_inet_addr(cp); }
	else {return __real_inet_addr(cp); }
}
in_addr_t __wrap_inet_network(const char * cp) {
    debug_printf("inet_network");
	if (syscall_interposition) { return i_inet_network(cp); }
	else { return __real_inet_network(cp); }
}
char * __wrap_inet_ntoa(struct in_addr in) {
    debug_printf("inet_ntoa");
	if (syscall_interposition) { return i_inet_ntoa(in); }
	else { return __real_inet_ntoa(in); }
}
struct in_addr __wrap_inet_makeaddr(int net, int host) {
    debug_printf("inet_makeaddr");
	if (syscall_interposition) { return i_inet_makeaddr(net, host); }
	else { return __real_inet_makeaddr(net, host); }
}
in_addr_t __wrap_inet_lnaof(struct in_addr in) {
    debug_printf("inet_lnaof");
	if (syscall_interposition) { return i_inet_lnaof(in); }
	else { return __real_inet_lnaof(in); }
}
in_addr_t __wrap_inet_netof(struct in_addr in) {
    debug_printf("inet_netof");
	if (syscall_interposition) { return i_inet_netof(in); }
	else { return __real_inet_netof(in); }
}
const char * __wrap_inet_ntop(int af, const void * src, char * dst, socklen_t size) {
    debug_printf("inet_ntop");
	if (syscall_interposition) { return i_inet_ntop(af, src, dst, size); }
	else { return __real_inet_ntop(af, src, dst, size); }
}

/*int __wrap_sscanf(const char *str, const char *format, ...) {
    debug_printf("sscanf");
	int retval = 0;
	//TODO: implement this.
	va_list args;
	va_start(args, format);
	if (syscall_interposition) {
		retval = i_vsscanf(str, format, args);
	}
	else {
		retval = vsscanf(str, format, args);
	}
	va_end(args);

	return retval;
}*/

int	__wrap_sprintf(char * str, const char * format, ...) 
{
    debug_printf("sprintf");
	int retval = 0;
 	va_list args;
	va_start(args, format);
	if (syscall_interposition) {
		retval = i_vsprintf(str, format, args);
	}
	else {
		retval = vsprintf(str, format, args);
	}
	va_end(args); 

	return retval;
}



int	__wrap_fcntl(int fd, int cmd, ... /*args */) {
    debug_printf("fcntl");
	va_list argp;
	struct flock *lock = NULL;
    int flag = 0;
	int retval = 0;
	va_start(argp, cmd);
    if (is_fd_socket(fd)){
        flag = va_arg(argp, int);
        if (syscall_interposition){
            // printf("FCNTL for fd=%d going to network implementation\n", fd);
            retval = network_fcntl(fd, cmd, flag);
        } else {
            retval = __real_fcntl(fd, cmd, flag);
        }
    } 
	else if ((cmd == F_SETLK) || (cmd == F_SETLKW) || (cmd == F_GETLK)) {
		lock = va_arg(argp, struct flock *);
		DIGGI_ASSERT(lock);

		if (syscall_interposition) {
			retval = i_fcntl(fd, cmd, lock);
		}
		else {
			retval = __real_fcntl(fd, cmd, lock);
		}

	}

    
	else{
		/*
			Do not support emulation of file descriptor manipulation yet
		*/
		DIGGI_ASSERT(!syscall_interposition);
		if (cmd == F_GETFL) {
			retval = __real_fcntl(fd, cmd);
		}
		else if(cmd == F_SETFL) {
			int op = va_arg(argp, int);
			retval = __real_fcntl(fd, cmd, op);
		}
		else {
			/*
				Unknown operation
			*/
			DIGGI_ASSERT(false);
		}
	}

	va_end(argp);
	return retval;
}

int __wrap_rand(void) {
    //debug_printf("rand");  // commenting out because it is called so often during TLS it makes the log hard to read.
	if (syscall_interposition) {
		return i_rand();
	}
	else {
		return __real_rand();
	}
}

DIR * __wrap_opendir(const char * name) {
    debug_printf("opendir");
	if (syscall_interposition) {
		return i_opendir(name);
	}
	else {
		return __real_opendir(name);
	}
}
struct dirent * __wrap_readdir(DIR * dirp) {
    debug_printf("readdir");
	if (syscall_interposition) {
		return i_readdir(dirp);
	}
	else {
		return __real_readdir(dirp);
	}
}
int __wrap_closedir(DIR * dirp) {
    debug_printf("closedir");
	if (syscall_interposition) {
		return i_closedir(dirp);
	}
	else {
		return __real_closedir(dirp);
	}
}
int __wrap_dup(int oldfd) {
    debug_printf("dup");
	if (syscall_interposition) {
		return i_dup(oldfd);
	}
	else {
		return __real_dup(oldfd);
	}
}
FILE * __wrap_fopen(const char * filename, const char * mode) {
    debug_printf("fopen");
	if (syscall_interposition) {
		return i_fopen(filename, mode);
	}
	else {
		return __real_fopen(filename, mode);
	}
}
char * __wrap_fgets(char * str, int num, FILE * stream) {
    debug_printf("fgets");
	if (syscall_interposition) {
		return i_fgets(str, num, stream);
	}
	else {
		return __real_fgets(str, num, stream);
	}
}
int __wrap_fclose(FILE * stream) {
    debug_printf("fclose");
	if (syscall_interposition) {
		return i_fclose(stream);
	}
	else {
		return __real_fclose(stream);
	}
}
int __wrap_fputc(int character, FILE * stream) {
    debug_printf("fputc");
	if (syscall_interposition) {
		return i_fputc(character, stream);
	}
	else {
		return __real_fputc(character, stream);
	}
}
int __wrap_fflush(FILE * stream) {
    debug_printf("fflush");
	if (syscall_interposition) {
		return i_fflush(stream);
	}
	else {
		return __real_fflush(stream);
	}
}
size_t __wrap_fread(void * ptr, size_t size, size_t count, FILE * stream) {
    debug_printf("fread");
	if (syscall_interposition) {
		return i_fread(ptr, size, count, stream);
	}
	else {
		return __real_fread(ptr, size, count, stream);
	}
}


size_t __wrap_fwrite(const void * ptr, size_t size, size_t count, FILE * stream) {
    debug_printf("fwrite");
	if (syscall_interposition) {
		return i_fwrite(ptr, size, count, stream);
	}
	else {
		return __real_fwrite(ptr, size, count, stream);
	}
}


int __wrap_chdir(const char * path) {
    debug_printf("chdir");
	if (syscall_interposition) {
		return i_chdir(path);
	}
	else {
		return __real_chdir(path);
	}
}
int __wrap_dup2(int oldfd, int newfd) {
    debug_printf("dup2");
	if (syscall_interposition) {
		return i_dup2(oldfd, newfd);
	}
	else {
		return __real_dup2(oldfd, newfd);
	}
}

int __wrap_fseeko(FILE * stream, off_t offset, int whence) {
    debug_printf("fseeko");
	if (syscall_interposition) {
		return i_fseeko(stream, offset, whence);
	}
	else {
		return __real_fseeko(stream, offset, whence);
	}
}

int __wrap_fseek(FILE * stream, off_t offset, int whence) {
    debug_printf("fseek");
	if (syscall_interposition) {
		return i_fseek(stream, offset, whence);
	}
	else {
		return __real_fseek(stream, offset, whence);
	}
}

long __wrap_ftell(FILE * stream) {
    debug_printf("ftell");
	if (syscall_interposition) {
		return i_ftell(stream);
	}
	else {
		return __real_ftell(stream);
	}
}


int __wrap_fputs(const char * str, FILE * stream) {
    debug_printf("fputs");
	if (syscall_interposition) {
		return i_fputs(str, stream);
	}
	else {
		return __real_fputs(str, stream);
	}
}
int __wrap_fgetc(FILE * stream) {
    debug_printf("fgetc");
	if (syscall_interposition) {
		return i_fgetc(stream);
	}
	else {
		return __real_fgetc(stream);
	}
}
int __wrap_fileno(FILE * stream) {
    debug_printf("fileno");
	if (syscall_interposition) {
		return i_fileno(stream);
	}
	else {
		return __real_fileno(stream);
	}
}

int __wrap_lstat(const char * path, struct stat * buf) {
    debug_printf("lstat");
	if (syscall_interposition) {
		return i_lstat(path, buf);
	}
	else {
		return __real_lstat(path, buf);
	}
}
long __wrap_sysconf(int name) {
    debug_printf("sysconf");
	if (syscall_interposition) {
		return i_sysconf(name);
	}
	else {
		return __real_sysconf(name);
	}
}
ssize_t __wrap_readlink(const char * path, char * buf, size_t bufsiz) {
    debug_printf("readlink");
	if (syscall_interposition) {
		return i_readlink(path, buf, bufsiz);
	}
	else {
		return __real_readlink(path, buf, bufsiz);
	}
}
int __wrap_fchmod(int fildes, mode_t mode) {
    debug_printf("fchmod");
	if (syscall_interposition) {
		return i_fchmod(fildes, mode);
	}
	else {
		return __real_fchmod(fildes, mode);
	}
}
unsigned int __wrap_sleep(unsigned int seconds) {
    debug_printf("sleep");
	if (syscall_interposition) {
		return i_sleep(seconds);
	}
	else {
		return __real_sleep(seconds);
	}
}
int __wrap_getpid(void) {
    debug_printf("getpid");
	if (syscall_interposition) {
		return i_getpid();
	}
	else {
		return __real_getpid();
	}
}
time_t __wrap_time(time_t * t) {
        debug_printf("time");
	if (syscall_interposition) {
		return i_time(t);
	}
	else {
		return __real_time(t);
	}
}
struct tm * __wrap_gmtime(const time_t * timer) {
    debug_printf("gmtime");
	if (syscall_interposition) {
		return i_gmtime(timer);
	}
	else {
		return __real_gmtime(timer);
	}
}
int __wrap_utime(const char * filename, const struct utimbuf * times) {
    debug_printf("utime");
	if (syscall_interposition) {
		return i_utime(filename, times);
	}
	else {
		return __real_utime(filename, times);
	}
}
int __wrap_utimes(const char * filename, const struct timeval times[2]) {
    debug_printf("utimes");
	if (syscall_interposition) {
		return i_utimes(filename, times);
	}
	else {
		return __real_utimes(filename, times);
	}
}
int __wrap_gettimeofday(struct timeval * tv, struct timezone * tz) {
    debug_printf("gettimeofday");
	if (syscall_interposition) {
		return i_gettimeofday(tv, tz);
	}
	else {
		return __real_gettimeofday(tv, tz);
	}
}
int __wrap_stat(const char * path, struct stat * buf) {
    debug_printf("stat");
	if (syscall_interposition) {
		return i_stat(path, buf);
	}
	else {
		return __real_stat(path, buf);
	}
}

int __wrap_fstat(int fd, struct stat * buf) {
    debug_printf("fstat");
	if (syscall_interposition) {
		return i_fstat(fd, buf);
	}
	else {
		return __real_fstat(fd, buf);
	}
}
int __wrap_close(int fd) {
    debug_printf("close");
	if (syscall_interposition) {
        if (is_fd_socket(fd)){
    		return network_close(fd);
        }
        else {
            return i_close(fd);
        }
	}
	else {
		return __real_close(fd);
	}
}
int __wrap_access(const char * pathname, int mode) {
    debug_printf("access");
	if (syscall_interposition) {
		return i_access(pathname, mode);
	}
	else {
		return __real_access(pathname, mode);
	}
}
/*
Allways have CWD be the same virtual directory
*/
char * __wrap_getcwd(char * buf, size_t size) {
    debug_printf("getcwd");
	if (syscall_interposition) {
		return i_getcwd(buf, size);
	}
	else {
		return __real_getcwd(buf, size);
	}
}
int __wrap_ftruncate(int fd, off_t length) {
        debug_printf("ftruncate");
	if (syscall_interposition) {
		return i_ftruncate(fd, length);
	}
	else {
		return __real_ftruncate(fd, length);
	}
}

int __wrap_fsync(int fd) {
    debug_printf("fsync");
	if (syscall_interposition) {
		return i_fsync(fd);
	}
	else {
		return __real_fsync(fd);
	}
}
char * __wrap_getenv(const char * name) {
    debug_printf("getenv");
	if (syscall_interposition) {
		return i_getenv(name);
	}
	else {
		return __real_getenv(name);
	}
}
uid_t __wrap_getuid(void) {
    debug_printf("getuid");
	if (syscall_interposition) {
		return i_getuid();
	}
	else {
		return __real_getuid();
	}
}
uid_t __wrap_geteuid(void) {
    debug_printf("geteuid");
	if (syscall_interposition) {
		return i_geteuid();
	}
	else {
		return __real_geteuid();
	}
}
int __wrap_fchown(int fd, uid_t owner, gid_t group) {
    debug_printf("fchown");
	if (syscall_interposition) {
		return i_fchown(fd, owner, group);
	}
	else {
		return __real_fchown(fd, owner, group);
	}
}
off_t __wrap_lseek(int fd, off_t offset, int whence) {
    debug_printf("lseek");
	if (syscall_interposition) {
		return i_lseek(fd, offset, whence);
	}
	else {
		return __real_lseek(fd, offset, whence);
	}
}

int __wrap_open(const char * path, int oflags, mode_t mode) {
    debug_printf("open");
	if (syscall_interposition) {
		return i_open(path, oflags, mode);
	}
	else {
		return __real_open(path, oflags, mode);
	}
}
ssize_t __wrap_read(int fildes, void * buf, size_t nbyte) {
    debug_printf("read");
	if (syscall_interposition) {
		return i_read(fildes, buf, nbyte);
	}
	else {
		return __real_read(fildes, buf, nbyte);
	}
}
ssize_t __wrap_write(int fd, const void * buf, size_t count) {
    debug_printf("write");
	if (syscall_interposition) {
		return i_write(fd, buf, count);
	}
	else {
		return __real_write(fd, buf, count);
	}
}
int __wrap_unlink(const char * pathname) {
    debug_printf("unlink");
	if (syscall_interposition) {
		return i_unlink(pathname);
	}
	else {
		return __real_unlink(pathname);
	}
}
int __wrap_mkdir(const char * path, mode_t mode) {
    debug_printf("mkdir");
	if (syscall_interposition) {
		return i_mkdir(path, mode);
	}
	else {
		return __real_mkdir(path, mode);
	}
}
int __wrap_rmdir(const char * path) {
    debug_printf("rmdir");
	if (syscall_interposition) {
		return i_rmdir(path);
	}
	else {
		return __real_rmdir(path);
	}
}
mode_t __wrap_umask(mode_t mask) {
    debug_printf("umask");
	if (syscall_interposition) {
		return i_umask(mask);
	}
	else {
		return __real_umask(mask);
	}
}

void __wrap_freeaddrinfo(struct addrinfo *res)
{
    if (syscall_interposition) {
        i_freeaddrinfo(res);
    }
    else{
        __real_freeaddrinfo(res);
    }
}

 int __wrap_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **res)
{
    if (syscall_interposition) {
      return i_getaddrinfo(node, service, hints, res);
    }
    else
    {
        return __real_getaddrinfo(node, service, hints, res);
    }
}

int __wrap_socket(int domain, int type, int protocol) {
    debug_printf("socket");
    int new_fd = 0;
	if (syscall_interposition) {
		new_fd = i_socket(domain, type, protocol);
	}
	else {
		new_fd = __real_socket(domain, type, protocol);
	}
    if (socket_fd_index == MAX_SOCKET_FDS){
        socket_fd_index = 0;
    }
    socket_fds[socket_fd_index] = new_fd;
    socket_fd_index++;

    return new_fd;
}
int __wrap_setsockopt(int sockfd, int level, int optname,
const void * optval, socklen_t optlen) {
    debug_printf("setsockopt");
	if (syscall_interposition) {
		return i_setsockopt(sockfd, level, optname, optval, optlen);
	}
	else {
		return __real_setsockopt(sockfd, level, optname, optval, optlen);
	}
}
int __wrap_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    debug_printf("getpeername");
	if (syscall_interposition) {
		return i_getpeername(sockfd, addr, addrlen);
	}
	else {
		return __real_getpeername(sockfd, addr, addrlen);
	}
}
ssize_t __wrap_send(int sockfd, const void * buf, size_t len, int flags) {
    debug_printf("send");
	if (syscall_interposition) {
		return i_send(sockfd, buf, len, flags);
	}
	else {
		return __real_send(sockfd, buf, len, flags);
	}
}
ssize_t __wrap_sendto(int sockfd,
	const void * buf, size_t len, int flags,
	const struct sockaddr * dest_addr, socklen_t addrlen) {
    debug_printf("sendto");
	if (syscall_interposition) {
		return i_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
	}
	else {
		return __real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
	}
}
ssize_t __wrap_recvfrom(int sockfd, void * buf, size_t len, int flags, struct sockaddr * src_addr, socklen_t * addrlen) {
    debug_printf("recvfrom");
	if (syscall_interposition) {
		return i_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
	}
	else {
		return __real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
	}
}
int __wrap_bind(int sockfd,
	const struct sockaddr * addr, socklen_t addrlen) {
    debug_printf("bind");
	if (syscall_interposition) {
		return i_bind(sockfd, addr, addrlen);
	}
	else {
		return __real_bind(sockfd, addr, addrlen);
	}
}
int __wrap_getsockname(int sockfd, struct sockaddr * addr, socklen_t * addrlen) {
    debug_printf("getsockname");
	if (syscall_interposition) {
		return i_getsockname(sockfd, addr, addrlen);
	}
	else {
		return __real_getsockname(sockfd, addr, addrlen);
	}
}
ssize_t __wrap_recv(int sockfd, void * buf, size_t len, int flags) {
    debug_printf("recv");
	if (syscall_interposition) {
		return i_recv(sockfd, buf, len, flags);
	}
	else {
		return __real_recv(sockfd, buf, len, flags);
	}
}
int __wrap_connect(int sockfd, const struct sockaddr * addr, socklen_t addrlen) {
    debug_printf("connect");
	if (syscall_interposition) {
		return i_connect(sockfd, addr, addrlen);
	}
	else {
		return __real_connect(sockfd, addr, addrlen);
	}
}
int __wrap_accept(int sockfd, struct sockaddr * addr, socklen_t * addrlen) {
    debug_printf("accept");
    int new_fd = 0;
	if (syscall_interposition) {
		new_fd = i_accept(sockfd, addr, addrlen);
	}
	else {
		new_fd = __real_accept(sockfd, addr, addrlen);
	}
    // add to socket_fd_index:
    if (socket_fd_index == MAX_SOCKET_FDS){
        socket_fd_index = 0;
    }
    socket_fds[socket_fd_index] = new_fd;
    socket_fd_index++;

    return new_fd;
}
int __wrap_listen(int sockfd, int backlog) {
    debug_printf("listen");
	if (syscall_interposition) {
		return i_listen(sockfd, backlog);
	}
	else {
		return __real_listen(sockfd, backlog);
	}
}
int __wrap_select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval * timeout) {
    debug_printf("select");
	if (syscall_interposition) {
		return i_select(nfds, readfds, writefds, exceptfds, timeout);
	}
	else {
		return __real_select(nfds, readfds, writefds, exceptfds, timeout);
	}
}
int __wrap_getsockopt(int sockfd, int level, int optname, void * optval, socklen_t * optlen) {
    debug_printf("getsockopt");
	if (syscall_interposition) {
		return i_getsockopt(sockfd, level, optname, optval, optlen);
	}
	else {
		return __real_getsockopt(sockfd, level, optname, optval, optlen);
	}
}

int __wrap_sigemptyset(sigset_t * set) {
    debug_printf("sigemptyset");
	if (syscall_interposition) {
		return i_sigemptyset(set);
	}
	else {
		return __real_sigemptyset(set);
	}
}
int __wrap_sigaction(int signum,
	const struct sigaction * act, struct sigaction * oldact) {
    debug_printf("sigaction");
	if (syscall_interposition) {
		return i_sigaction(signum, act, oldact);
	}
	else {
		return __real_sigaction(signum, act, oldact);
	}
}
pid_t __wrap_fork(void) {
    debug_printf("fork");
	if (syscall_interposition) {
		return i_fork();
	}
	else {
		return __real_fork();
	}
}
sighandler_t __wrap_signal(int signum, sighandler_t handler) {
    debug_printf("signal");
	if (syscall_interposition) {
		return i_signal(signum, handler);
	}
	else {
		return __real_signal(signum, handler);
	}
}

void __wrap__exit(int status) {
    debug_printf("exit");
	if (syscall_interposition) {
		i__exit(status);
	}
	else {
		__real__exit(status);
	}
}

int __wrap_execle(const char *path, const char *arg, ...){
    debug_printf("execle");
	DIGGI_ASSERT(false);
	return -1;
}

struct tm *__wrap_localtime(const time_t *timer) {
    debug_printf("localtime");
	if (syscall_interposition) {
		return i_localtime(timer);
	}
	else {
		return __real_localtime(timer);
	}
}


int __wrap_pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    debug_printf("pthread_mutexattr_init");
	if (syscall_interposition) {
		return i_pthread_mutexattr_init(attr);
	}
	else {
		return __real_pthread_mutexattr_init(attr);
	}
}

int __wrap_pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    debug_printf("pthread_mutexattr_settype");
	if (syscall_interposition) {
		return i_pthread_mutexattr_settype(attr, type);
	}
	else {
		return __real_pthread_mutexattr_settype(attr, type);
	}
}

int __wrap_pthread_mutexattr_destroy(pthread_mutexattr_t * attr) {
    debug_printf("pthread_mutexattr_destroy");
	if (syscall_interposition) {
		return i_pthread_mutexattr_destroy(attr);
	}
	else {
		return __real_pthread_mutexattr_destroy(attr);

	}
}

int __wrap_pthread_mutex_init(pthread_mutex_t * mutext,
	const pthread_mutexattr_t * attr) {
    debug_printf("pthread_mutex_init");
	if (syscall_interposition) {
		return i_pthread_mutex_init(mutext, attr);
	}
	else {
		return __real_pthread_mutex_init(mutext, attr);

	}
}

int __wrap_pthread_mutex_lock(pthread_mutex_t * mutex) {
    debug_printf("pthread_mutex_lock");
	if (syscall_interposition) {
		return i_pthread_mutex_lock(mutex);
	}
	else {
		return __real_pthread_mutex_lock(mutex);
	}
}

int __wrap_pthread_mutex_unlock(pthread_mutex_t * mutex) {
    debug_printf("pthread_mutex_unlock");
	if (syscall_interposition) {
		return i_pthread_mutex_unlock(mutex);
	}
	else {
		return __real_pthread_mutex_unlock(mutex);
	}
}

int __wrap_pthread_mutex_trylock(pthread_mutex_t * mutex) {
    debug_printf("pthread_mutex_trylock");
	if (syscall_interposition) {
		return i_pthread_mutex_trylock(mutex);
	}
	else {
		return __real_pthread_mutex_trylock(mutex);
	}
}

int __wrap_pthread_mutex_destroy(pthread_mutex_t * mutex) {
    debug_printf("pthread_mutex_destroy");
	if (syscall_interposition) {
		return i_pthread_mutex_destroy(mutex);
	}
	else {
		return __real_pthread_mutex_destroy(mutex);
	}
}

int __wrap_pthread_create(pthread_t * thread,
	const pthread_attr_t * attr, void * (*start_routine)(void *), void * arg) {
        debug_printf("pthread_create");
	if (syscall_interposition) {
		return i_pthread_create(thread, attr, start_routine, arg);
	}
	else {
		return __real_pthread_create(thread, attr, start_routine, arg);
	}
}

int __wrap_pthread_join(pthread_t thread, void ** value_ptr) {
    debug_printf("pthread_join");
	if (syscall_interposition) {
		return i_pthread_join(thread, value_ptr);
	}
	else {
		return __real_pthread_join(thread, value_ptr);
	}
}

int __wrap_pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex) {
    debug_printf("pthread_cond_wait");
	if (syscall_interposition) {
		return i_pthread_cond_wait(cond, mutex);
	}
	else {
		return __real_pthread_cond_wait(cond, mutex);
	}
}

int __wrap_pthread_cond_broadcast(pthread_cond_t * cond) {
    debug_printf("pthread_cond_broadcast");
	if (syscall_interposition) {
		return i_pthread_cond_broadcast(cond);
	}
	else {
		return __real_pthread_cond_broadcast(cond);
	}
}

int __wrap_pthread_cond_signal(pthread_cond_t * cond) {
    debug_printf("pthread_cond_signal");
	if (syscall_interposition) {
		return i_pthread_cond_signal(cond);
	}
	else {
		return __real_pthread_cond_signal(cond);
	}
}

int __wrap_pthread_cond_init(pthread_cond_t * cond,
	const pthread_condattr_t * attr) {
        debug_printf("pthread_cont_init");
	if (syscall_interposition) {
		return i_pthread_cond_init(cond, attr);
	}
	else {
		return __real_pthread_cond_init(cond, attr);
	}
}

#endif

#ifdef __cplusplus
}
#endif // __cplusplus
