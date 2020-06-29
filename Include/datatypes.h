#ifndef DATATYPES_H_
#define DATATYPES_H_
#include "sgx_tseal.h"

#include "sgx_dh.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "copy_r.h"

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(!!(COND))*2-1]
#define COMPILE_TIME_ASSERT3(X,L) STATIC_ASSERT(X,static_assertion_at_line_##L)
#define COMPILE_TIME_ASSERT2(X,L) COMPILE_TIME_ASSERT3(X,L)
#define COMPILE_TIME_ASSERT(X)    COMPILE_TIME_ASSERT2(X,__LINE__)

#define MAX_ENCLAVE_ID 1000
#define ALLOC(type) (type*)malloc(sizeof(type))
#define ALLOC_P(type, addedsize) (type*)malloc(sizeof(type) + addedsize)
#define GETPARRENTFRAME(ptr, type) (type*)((uint8_t*)ptr - sizeof(type))
#define COPY(type, ptr, size) (type*)copy_r((void*)ptr, size)

#define ENCRYPTION_HEADER_SIZE (size_t) 4096
#define ENCRYPTED_BLK_SIZE (size_t) (SPACE_PER_BLOCK + ENCRYPTION_HEADER_SIZE)
#define SPACE_PER_BLOCK (size_t) 4096
#define FILESYSTEM_BLK_SIZE (size_t) 4096
typedef enum read_type_t {
	SEEKBACK,
	NOSEEK,
}read_type_t;

typedef void(*async_cb_t)(void*, int status);

/*func identifier type*/
typedef union aid_t {
	uint64_t raw;
	struct id {
		uint8_t proc;
		uint8_t enclave;
		uint8_t lib;
		uint8_t thread;
		uint8_t att_group;
		uint8_t host;
		uint8_t type;
		uint8_t pad1;
	}fields;
} aid_t;

COMPILE_TIME_ASSERT(sizeof(aid_t) == 8);


typedef enum message_role_t {
	SERVER,
	CLIENT
}message_role_t;

typedef struct async_work_t {
	async_cb_t cb;
	void *arg;
	int status;
	const char* label;
	int thread_id;
	size_t rate_limit;
	size_t current_rate;
	size_t pad;
}async_work_t;

COMPILE_TIME_ASSERT(sizeof(async_work_t) == (64));

typedef enum msg_convention_t {
	CALLBACK,
	REGULAR
}msg_convention_t;

typedef enum msg_type_t {
	SESSION_REQUEST,
	REGULAR_MESSAGE,
	FILEIO_OPEN,
    FILEIO_FOPEN,
    FILEIO_FSEEK,
    FILEIO_FTELL,
    FILEIO_FREAD,
    FILEIO_FCLOSE,
	FILEIO_READ,
	FILEIO_STAT,
	FILEIO_STAT64,
	FILEIO_FSTAT,
	FILEIO_FSTAT64,
	FILEIO_LSEEK,
	FILEIO_FCNTL,
	FILEIO_ACCESS,
	FILEIO_UMASK,
	FILEIO_WRITE,
	FILEIO_CLOSE,
	FILEIO_UNLINK,
	FILEIO_FSYNC,
	NETIO_ACCEPT,
	SQL_QUERY_MESSAGE_TYPE,
    SQL_QUERY_MESSAGE_BLOB_TYPE,
	NET_SOCKET_MSG_TYPE,
	NET_BIND_MSG_TYPE,
	NET_GETSOCKNAME_MSG_TYPE,
	NET_CONNECT_MSG_TYPE,
	NET_FCNTL_MSG_TYPE,
	NET_SETSOCKOPT_MSG_TYPE,
    NET_GETSOCKOPT_MSG_TYPE,
	NET_LISTEN_MSG_TYPE,
	NET_SELECT_MSG_TYPE,
	NET_RECV_MSG_TYPE,
	NET_SEND_MSG_TYPE,
	NET_ACCEPT_MSG_TYPE,
	NET_CLOSE_MSG_TYPE,
    NET_RAND_MSG_TYPE,
    DIGGI_SIGNAL_TYPE_EXIT,
} msg_type_t;

typedef enum msg_payload_type_t {
	BUFFER_TYPE,
	ZERO_COPY_TYPE
}msg_payload_type_t;



#define SGX_MAX_CLIENT_ENCLAVES 10

#define RING_BUFFER_SIZE (1024 * 1024) /*1 MB*/ 

typedef enum destination_type_t
{
	ENCLAVE,
	LIB,
	PROC, /* Reordering will cause diggi_base_executable incomming routing to break */

}destination_type_t;



typedef void(*lib_harness_start_t)(async_cb_t, async_cb_t, async_cb_t, async_cb_t, aid_t, aid_t);

typedef enum msg_delivery_t{
    CLEARTEXT, //0
    ENCRYPTED
}msg_delivery_t;

/*
TODO: enforce 8 byte alignment*/
typedef struct msg_t
{
	/*4*/
	msg_type_t type;
	/*8*/
	aid_t dest;
	/*12*/
	aid_t src;
	/*16*/
	unsigned long id;
	/*20*/
	size_t size;
	/*24*/
	size_t session_count;
	/*30*/
	msg_delivery_t delivery;
	uint8_t sha256_current_evidence_hash[32];
    size_t omit_from_log;
    size_t pad[4];

	uint8_t data[];


}msg_t;

COMPILE_TIME_ASSERT(sizeof(msg_t) == 128);


typedef struct msg_async_response_t {
	msg_t *msg;
	void *context;
}msg_async_response_t;


//Format of the AES-GCM message being exchanged between the source and the destination enclaves
typedef struct _secure_message_t
{
    uint32_t session_id; //Session ID identifyting the session to which the message belongs
    sgx_aes_gcm_data_t message_aes_gcm_data;    
}secure_message_t;


#define MEASUREMENT_SIZE 128

typedef struct quote_pair_t{
    aid_t src;
    uint8_t measurement[MEASUREMENT_SIZE];
} quote_pair_t;

#endif

