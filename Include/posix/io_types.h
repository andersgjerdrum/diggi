#ifndef	IOSTUB_TYPES_H
#define IOSTUB_TYPES_H
#include "posix/syscall_def.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include "datatypes.h"
#if defined(DIGGI_ENCLAVE) /* Debuggability of untrusted funcs */



#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
/*
#define SIGLOST		29
*/
#define SIGPWR		30
#define SIGSYS		31
#define	SIGUNUSED	31

/* These should not be considered constants from userland.  */
#define SIGRTMIN	32
#define SIGRTMAX	_NSIG

/*
* SA_FLAGS values:
*
* SA_ONSTACK indicates that a registered stack_t will be used.
* SA_RESTART flag to get restarting signals (which were the default long ago)
* SA_NOCLDSTOP flag to turn off SIGCHLD when children stop.
* SA_RESETHAND clears the handler when the signal is delivered.
* SA_NOCLDWAIT flag on SIGCHLD to inhibit zombies.
* SA_NODEFER prevents the current signal from being masked in the handler.
*
* SA_ONESHOT and SA_NOMASK are the historical Linux names for the Single
* Unix names RESETHAND and NODEFER respectively.
*/
#define SA_NOCLDSTOP	0x00000001u
#define SA_NOCLDWAIT	0x00000002u
#define SA_SIGINFO	0x00000004u
#define SA_ONSTACK	0x08000000u
#define SA_RESTART	0x10000000u
#define SA_NODEFER	0x40000000u
#define SA_RESETHAND	0x80000000u

#define SA_NOMASK	SA_NODEFER
#define SA_ONESHOT	SA_RESETHAND

#define SA_RESTORER	0x04000000

#define MINSIGSTKSZ	2048
#define SIGSTKSZ	8192

/* This turns out to be the MinGW definition. Switching now to GNU libc definition, which is what is used on the cluster
typedef struct FILE
{
    char*   _ptr;
    int _cnt;
    char*   _base;
    int _flag;
    int _file;
    int _charbuf;
    int _bufsiz;
    char*   _tmpfname;
    short _fileno;
    short _flags;
    char hash[32]; //SHA265 hash of content, used by fopen and fread. read from config during runtime.
} FILE;
*/
#define _IO_lock_t void
// copied _IO_FILE struct from /usr/include/x86_64-linux-gnu/bits/libio.h and renamed to FILE
typedef struct FILE {
  int _flags;           /* High-order word is _IO_MAGIC; rest is flags. */
#define _IO_file_flags _flags
#define _IO_off_t __off_t
  /* The following pointers correspond to the C++ streambuf protocol. */
  /* Note:  Tk uses the _IO_read_ptr and _IO_read_end fields directly. */
  char* _IO_read_ptr;   /* Current read pointer */
  char* _IO_read_end;   /* End of get area. */
  char* _IO_read_base;  /* Start of putback+get area. */
  char* _IO_write_base; /* Start of put area. */
  char* _IO_write_ptr;  /* Current put pointer. */
  char* _IO_write_end;  /* End of put area. */
  char* _IO_buf_base;   /* Start of reserve area. */
  char* _IO_buf_end;    /* End of reserve area. */
  /* The following fields are used to support backing up and undo. */
  char *_IO_save_base; /* Pointer to start of non-current get area. */
  char *_IO_backup_base;  /* Pointer to first valid character of backup area */
  char *_IO_save_end; /* Pointer to end of non-current get area. */

  struct _IO_marker *_markers;

  struct FILE *_chain;

  int _fileno;
#if 0
  int _blksize;
#else
  int _flags2;
#endif
  _IO_off_t _old_offset; /* This used to be _offset but it's too small.  */

#define __HAVE_COLUMN /* temporary */
  /* 1+column number of pbase(); 0 is unknown. */
  unsigned short _cur_column;
  signed char _vtable_offset;
  char _shortbuf[1];

  /*  char* _save_gptr;  char* _save_egptr; */

  _IO_lock_t *_lock;
//#ifdef _IO_USE_OLD_IO_FILE
}FILE;



typedef int pid_t;

typedef long int time_t;
typedef long	suseconds_t;
typedef unsigned long int dev_t;
typedef unsigned long int __dev_t;
typedef unsigned long int ino_t;
typedef unsigned long int __ino_t;
typedef unsigned int mode_t;
typedef unsigned int __mode_t;
typedef unsigned long int nlink_t;
typedef unsigned long int __nlink_t;
typedef unsigned int uid_t;
typedef unsigned int __uid_t;
typedef unsigned int gid_t;
typedef unsigned int __gid_t;
typedef long int blksize_t;
typedef long int __blksize_t;
typedef long int blkcnt_t;
typedef long int __blkcnt_t;
typedef int __pid_t;
typedef long int __off64_t;
typedef long int __syscall_slong_t;
typedef unsigned long __syscall_ulong_t;

typedef long int off_t;
typedef long int ssize_t;
typedef unsigned int socklen_t;
typedef unsigned int useconds_t;
// Used for poll()
typedef unsigned int nfds_t;

#define POLLIN		0x0001
#define POLLPRI		0x0002
#define POLLOUT		0x0004
#define POLLERR		0x0008
#define POLLHUP		0x0010
#define POLLNVAL	0x0020

#define	S_IRWXU	0000700			/* RWX mask for owner */


typedef	unsigned long	__fd_mask;

#define __NFDBITS (8 * (int) sizeof (__fd_mask))
#define NFDBITS          __NFDBITS
#define __FD_SETSIZE          1024
#define	FD_SETSIZE	1024

#define   __FD_MASK(d)    ((__fd_mask) (1UL << ((d) % __NFDBITS)))
#define   __FD_ELT(d)     ((d) / __NFDBITS)

typedef struct fd_set
{
    /* XPG4.2 requires this member name.  Otherwise avoid the name
    from the global namespace.  */
#ifdef __USE_XOPEN
    __fd_mask fds_bits[__FD_SETSIZE / __NFDBITS];
# define __FDS_BITS(set) ((set)->fds_bits)
#else
    __fd_mask __fds_bits[__FD_SETSIZE / __NFDBITS];
# define __FDS_BITS(set) ((set)->__fds_bits)
#endif
} fd_set;



#if defined __GNUC__ && __GNUC__ >= 2

# if __WORDSIZE == 64
#  define __FD_ZERO_STOS "stosq"
# else
#  define __FD_ZERO_STOS "stosl"
# endif

# define __FD_ZERO(fdsp) \
  do {                                                                        \
    int __d0, __d1;                                                           \
    __asm__ __volatile__ ("cld; rep; " __FD_ZERO_STOS                         \
                          : "=c" (__d0), "=D" (__d1)                          \
                          : "a" (0), "0" (sizeof (fd_set)                     \
                                          / sizeof (__fd_mask)),              \
                            "1" (&__FDS_BITS (fdsp)[0])                       \
                          : "memory");                                        \
  } while (0)

#else   /* ! GNU CC */

/* We don't use `memset' because this would require a prototype and
the array isn't too big.  */
# define __FD_ZERO(set)  \
  do {                                                                        \
    unsigned int __i;                                                         \
    fd_set *__arr = (set);                                                    \
    for (__i = 0; __i < sizeof (fd_set) / sizeof (__fd_mask); ++__i)          \
      __FDS_BITS (__arr)[__i] = 0;                                            \
  } while (0)

#endif  /* GNU CC */

#define FD_SET(fd, fdsetp)      __FD_SET (fd, fdsetp)
#define FD_CLR(fd, fdsetp)      __FD_CLR (fd, fdsetp)
#define FD_ISSET(fd, fdsetp)    __FD_ISSET (fd, fdsetp)
#define FD_ZERO(fdsetp)         __FD_ZERO (fdsetp)

#define __FD_SET(d, set) \
  ((void) (__FDS_BITS (set)[__FD_ELT (d)] |= __FD_MASK (d)))
#define __FD_CLR(d, set) \
  ((void) (__FDS_BITS (set)[__FD_ELT (d)] &= ~__FD_MASK (d)))
#define __FD_ISSET(d, set) \
  ((__FDS_BITS (set)[__FD_ELT (d)] & __FD_MASK (d)) != 0)


/* fd_set for select and pselect.  */

#define	_NFDBITS	(sizeof(__fd_mask) * 8)	/* bits per mask */

#ifndef _howmany
#define	_howmany(x, y)	(((x) + ((y) - 1)) / (y))
#endif

#define __S_IFMT        0170000 /* These bits determine file type.  */
#define __S_IFREG       0100000 /* Regular file.  */

#define __S_ISTYPE(mode, mask)  (((mode) & __S_IFMT) == (mask))

#define     S_ISREG(mode)    __S_ISTYPE((mode), __S_IFREG)


struct pollfd
{
    int fd;
    short events;
    short revents;
};

struct iovec
{
    void  *iov_base;
    ssize_t iov_len;
};

struct sockaddr
{
    unsigned short sa_family;
    char           sa_data[14];
};

struct linger
{
    int l_onoff;
    int l_linger;
};

struct msghdr
{
    void	 *msg_name;
    socklen_t msg_namelen;
    struct	  iovec *msg_iov;
    int		  msg_iovlen;
    void	 *msg_control;
    socklen_t msg_controllen;
    int		  msg_flags;
};

/*
 * Types
 */
#define	SOCK_STREAM	1		/* stream socket */
#define	SOCK_DGRAM	2		/* datagram socket */
#define	SOCK_RAW	3		/* raw-protocol interface */
#define	SOCK_RDM	4		/* reliably-delivered message */
#define	SOCK_SEQPACKET	5		/* sequenced packet stream */
#define SOL_SOCKET 0xffff

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER	0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define	SO_REUSEPORT	0x0200		/* allow local address & port reuse */
#define SO_TIMESTAMP	0x0800		/* timestamp received dgram traffic */
#define SO_BINDANY	0x1000		/* allow bind to any address */
#define SO_ZEROIZE	0x2000		/* zero out all mbufs sent over socket */


/*
 * Additional options, not kept in so_options.
 */
#define	SO_SNDBUF	0x1001		/* send buffer size */
#define	SO_RCVBUF	0x1002		/* receive buffer size */
#define	SO_SNDLOWAT	0x1003		/* send low-water mark */
#define	SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define	SO_SNDTIMEO	0x1005		/* send timeout */
#define	SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */
#define	SO_NETPROC	0x1020		/* multiplex; network processing */
#define	SO_RTABLE	0x1021		/* routing table to be used */
#define	SO_PEERCRED	0x1022		/* get connect-time credentials */
#define	SO_SPLICE	0x1023		/* splice data to other socket */

#define AF_LOCAL 1
#define AF_UNIX AF_LOCAL
#define AF_INET 2
#define AF_INET6 24

#define __libc_lock_define(CLASS,NAME)


struct __dirstream
{
    int fd;			/* File descriptor.  */

    __libc_lock_define(, lock) /* Mutex lock for this structure.  */

        size_t allocation;		/* Space allocated for the block.  */
    size_t size;		/* Total valid data in the block.  */
    size_t offset;		/* Current offset into the block.  */

    off_t filepos;		/* Position of next entry to read.  */

                        /* Directory block.  */
    char data[0] __attribute__((aligned(__alignof__(void*))));
};

typedef struct __dirstream DIR;


struct addrinfo
{
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};

typedef unsigned short int sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;




struct dirent {
    ino_t          d_ino;       /* Inode number */
    off_t          d_off;       /* Not an offset; see below */
    unsigned short d_reclen;    /* Length of this record */
    unsigned char  d_type;      /* Type of file; not supported
                                by all filesystem types */
    char           d_name[256]; /* Null-terminated filename */
};


//typedef int64_t ssize_t;
struct timeval {
    time_t		tv_sec;		/* seconds */
    suseconds_t tv_usec;	/* microseconds */
};

/*
    Because untrusted funcs rely on signal without exiting enclave as an important part of debugging, 
    we only define timespec again if trusted func
*/
struct timespec
{
    __time_t tv_sec;            /* Seconds.  */
    __syscall_slong_t tv_nsec;  /* Nanoseconds.  */
};
struct flock
{
    short int l_type;	/* Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.	*/
    short int l_whence; /* Where `l_start' is relative to (like `lseek').  */
    __off64_t l_start;	/* Offset where the lock begins.  */
    __off64_t l_len;	/* Size of the locked area; zero means until EOF.  */
    __pid_t l_pid;		/* Process holding the lock.  */
};


#define F_DUPFD         0       /* dup */
#define F_GETFD         1       /* get close_on_exec */
#define F_SETFD         2       /* set/clear close_on_exec */
#define F_GETFL         3       /* get file->f_flags */
#define F_SETFL         4       /* set file->f_flags */
#  define F_GETLK	5	/* Get record locking info.  */
#  define F_SETLK	6	/* Set record locking info (non-blocking).	*/
#  define F_SETLKW	7	/* Set record locking info (blocking).	*/

# define F_RDLCK		0	/* Read lock.  */
# define F_WRLCK		1	/* Write lock.	*/
# define F_UNLCK		2	/* Remove lock.	 */
# define SEEK_SET	0	/* Seek from beginning of file.  */
# define SEEK_CUR	1	/* Seek from current position.	*/
# define SEEK_END	2	/* Seek from end of file.  */



/* for F_[GET|SET]FL 
copied from https://github.com/torvalds/linux/blob/master/include/uapi/asm-generic/fcntl.h */
#define FD_CLOEXEC	1	/* actually anything with low bit set goes */

/* Values for the second argument to `fcntl'.  */
#define F_DUPFD         0       /* Duplicate file descriptor.  */
#define F_GETFD         1       /* Get file descriptor flags.  */
#define F_SETFD         2       /* Set file descriptor flags.  */
#define F_GETFL         3       /* Get file status flags.  */
#define F_SETFL         4       /* Set file status flags.  */

#define O_ACCMODE		   0003
#define O_RDONLY			 00
#define O_WRONLY			 01
#define O_RDWR				 02
#ifndef O_CREAT
# define O_CREAT		   0100 /* Not fcntl.  */
#endif
#ifndef O_EXCL
# define O_EXCL			   0200 /* Not fcntl.  */
#endif
#ifndef O_NOCTTY
# define O_NOCTTY		   0400 /* Not fcntl.  */
#endif
#ifndef O_TRUNC
# define O_TRUNC		  01000 /* Not fcntl.  */
#endif
#ifndef O_APPEND
# define O_APPEND		  02000
#endif
#ifndef O_NONBLOCK
# define O_NONBLOCK		  04000
#endif
#ifndef O_NDELAY
# define O_NDELAY		O_NONBLOCK
#endif
#ifndef O_SYNC
# define O_SYNC		   04010000
#endif
#define O_FSYNC			O_SYNC
#ifndef O_ASYNC
# define O_ASYNC		 020000
#endif



/* access function */
#define	F_OK		0	/* test for existence of file */
#define	X_OK		0x01	/* test for execute or search permission */
#define	W_OK		0x02	/* test for write permission */
#define	R_OK		0x04	/* test for read permission */

struct timezone {
    int tz_minuteswest;		/* minutes west of Greenwich */
    int tz_dsttime;			/* type of DST correction */
};
struct stat
{
    __dev_t st_dev;             /* Device.  */
    __ino_t st_ino;             /* File serial number.  */
    __nlink_t st_nlink;         /* Link count.  */
    __mode_t st_mode;           /* File mode.  */
    __uid_t st_uid;             /* User ID of the file's owner. */
    __gid_t st_gid;             /* Group ID of the file's group.*/
    int __pad0;
    __dev_t st_rdev;            /* Device number, if device.  */
    __off_t st_size;                    /* Size of file, in bytes.  */
    __blksize_t st_blksize;     /* Optimal block size for I/O.  */
    __blkcnt_t st_blocks;               /* Number 512-byte blocks allocated. */
    /* rest of struct copied from:
    https://code.woboq.org/gcc/include/bits/stat.h.html */
    /* Nanosecond resolution timestamps are stored in a format
       equivalent to 'struct timespec'.  This is the type used
       whenever possible but the Unix namespace rules do not allow the
       identifier 'timespec' to appear in the <sys/stat.h> header.
       Therefore we have to handle the use of this header in strictly
       standard-compliant sources special.  */
    struct timespec st_atim;                /* Time of last access.  */
    struct timespec st_mtim;                /* Time of last modification.  */
    struct timespec st_ctim;            /* Time of last status change.  */
#ifdef __x86_64__
    __syscall_slong_t __glibc_reserved[3];
#else
# ifndef __USE_FILE_OFFSET64
    unsigned long int __glibc_reserved4;
    unsigned long int __glibc_reserved5;
# else
    __ino64_t st_ino;                        /* File serial number.        */
# endif
#endif
};

/*
    If stat size is not 144, we break with the linux x86-64 syscall abi
*/
COMPILE_TIME_ASSERT(sizeof(struct stat) == 144);

struct stat64 {
    unsigned long long      st_dev;
    unsigned char   __pad0[4];

    unsigned long   __st_ino;

    unsigned int    st_mode;
    unsigned int    st_nlink;

    unsigned long   st_uid;
    unsigned long   st_gid;

    unsigned long long      st_rdev;
    unsigned char   __pad3[4];

    long long       st_size;
    unsigned long   st_blksize;

    /* Number 512-byte blocks allocated. */
    unsigned long long      st_blocks;

    unsigned long   st_atime;
    unsigned long   st_atime_nsec;

    unsigned long   st_mtime;
    unsigned int    st_mtime_nsec;

    unsigned long   st_ctime;
    unsigned long   st_ctime_nsec;

    unsigned long long      st_ino;
};

struct utimbuf {
    time_t actime;		 /* access time */
    time_t modtime;		 /* modification time */
};

#define __S_IFDIR		0040000 /* Directory.  */
#define __S_IFMT		0170000 /* These bits determine file type.	*/
#define S_IFDIR __S_IFDIR

#define __S_ISTYPE(mode, mask)	(((mode) & __S_IFMT) == (mask))

#define S_ISDIR(mode)	 __S_ISTYPE((mode), __S_IFDIR)

#endif

#ifdef __cplusplus
}
#endif // __cplusplus
#endif
