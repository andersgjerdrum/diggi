#ifndef SYSCALL_DEF_H
#define SYSCALL_DEF_H
#ifdef __cplusplus
extern "C" {
#endif
#if defined(DIGGI_ENCLAVE)


#ifdef errno
	#undef errno
#endif
#define     mmap	            i_mmap
#define     munmap	            i_munmap
#define     mremap	            i_mremap
#define		errno				(*i_errno())
#define		printf				i_printf
#define		vprintf				i_vprintf
#define		vsprintf			i_vsprintf		
#define		vsscanf				i_vsscanf		
#define		lstat				i_lstat			
#define		sysconf				i_sysconf		
#define		readlink			i_readlink		
#define		fchmod				i_fchmod		
#define		sleep				i_sleep			
#define		getpid				i_getpid		
#define		time				i_time			
#define		gmtime				i_gmtime		
#define		utime				i_utime			
#define		utimes				i_utimes		
#define		gettimeofday		i_gettimeofday  
/*
	stat is explisitly defined in iostubs.h
*/
#define		fstat				i_fstat			
#define		close				i_close			
#define		access				i_access		
#define		getcwd				i_getcwd		
#define		ftruncate			i_ftruncate		
#define		fcntl				i_fcntl			
#define		fsync				i_fsync			
#define		getenv				i_getenv		
#define		getuid				i_getuid		
#define		geteuid				i_geteuid		
#define		fchown				i_fchown		
#define		lseek				i_lseek			
#define		open				i_open			
#define		read				i_read			
#define		write				i_write			
#define		unlink				i_unlink		
#define		mkdir				i_mkdir			
#define		rmdir				i_rmdir			
#define		umask				i_umask			
#define     chdir				i_chdir			
#define		fputs				i_fputs
#define     ferror              i_ferror			
#define		puts				i_puts			
#define		getenv				i_getenv		
#define		socket				i_socket	
#define     getaddrinfo         i_getaddrinfo
#define	    freeaddrinfo        i_freeaddrinfo
#define		accept				i_accept		
#define		dup2				i_dup2			
#define		getsockopt			i_getsockopt	
#define		fflush				i_fflush		
#define		fprintf				i_fprintf		
#define		setsockopt			i_setsockopt	
#define		kill				i_kill			
#define		connect				i_connect		
#define		bind				i_bind			
#define		vfprintf			i_vfprintf		
#define		fputc				i_fputc			
#define		signal				i_signal	
#define     alarm               i_alarm	
#define		sscanf				i_sscanf		
#define		send				i_send			
#define		recv				i_recv			
#define		connect				i_connect		
#define		sendto				i_sendto		
#define		recvfrom			i_recvfrom		
#define		bind				i_bind			
#define		listen				i_listen		
#define		getsockname			i_getsockname	
#define		select				i_select		
#define		shutdown			i_shutdown		

#define		getpeername			i_getpeername	
#define		sprintf				i_sprintf		
#define		fclose				i_fclose		
#define		fseeko				i_fseeko
#define		fseek				i_fseek	
#define		ftell				i_ftell		
#define		fgets				i_fgets			
#define		opendir				i_opendir		
#define		readdir				i_readdir		
#define		closedir			i_closedir		
#define		fork				i_fork			
#define		execle				i_execle		
#define		_exit				i__exit			
#define		sigemptyset			i_sigemptyset	
#define		sigaction(...)		i_sigaction(__VA_ARGS__)
#define		fileno				i_fileno		
#define		fgetc				i_fgetc			
#define		rand				i_rand			
#define		fopen				i_fopen			
#define		fread				i_fread			
#define		fwrite				i_fwrite		
#define		dup					i_dup			
#define		htonl				i_htonl			
#define		htons				i_htons			
#define		ntohl				i_ntohl			
#define		ntohs				i_ntohs			
#define		inet_aton			i_inet_aton		
#define		inet_addr			i_inet_addr		
#define		inet_network		i_inet_network	
#define		inet_ntoa			i_inet_ntoa		
#define		inet_makeaddr		i_inet_makeaddr	
#define		inet_lnaof			i_inet_lnaof	
#define		inet_netof			i_inet_netof	
#define		inet_ntop			i_inet_ntop
#define		localtime			i_localtime

#define pthread_mutexattr_init				i_pthread_mutexattr_init
#define pthread_mutexattr_settype			i_pthread_mutexattr_settype
#define pthread_mutexattr_destroy			i_pthread_mutexattr_destroy
#define pthread_mutex_init					i_pthread_mutex_init
#define pthread_mutex_lock					i_pthread_mutex_lock
#define pthread_mutex_unlock				i_pthread_mutex_unlock
#define pthread_mutex_trylock				i_pthread_mutex_trylock
#define pthread_mutex_destroy				i_pthread_mutex_destroy
#define pthread_create						i_pthread_create
#define pthread_join						i_pthread_join
#define pthread_cond_wait					i_pthread_cond_wait
#define pthread_cond_broadcast				i_pthread_cond_broadcast
#define pthread_cond_signal					i_pthread_cond_signal
#define pthread_cond_init					i_pthread_cond_init
#define pthread_self						i_pthread_self

#endif
#ifdef __cplusplus
}
#endif // __cplusplus
#endif