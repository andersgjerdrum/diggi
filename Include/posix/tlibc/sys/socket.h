#ifndef SOCKET_DIGGI_H
#define SOCKET_DIGGI_H

#include "posix/io_types.h"
#include "posix/net_utils.h"
#include "posix/net_stubs.h"
#include "posix/intercept.h"
/*
 *  Desired design of maximum size and alignment.
 */
#define MSG_PEEK	0x02
#define _SS_MAXSIZE 128
    /* Implementation-defined maximum size. */
#define _SS_ALIGNSIZE (sizeof(int64_t))
    /* Implementation-defined desired alignment. */


/*
 *  Definitions used for sockaddr_storage structure paddings design.
 */
#define _SS_PAD1SIZE (_SS_ALIGNSIZE - sizeof(sa_family_t))
#define _SS_PAD2SIZE (_SS_MAXSIZE - (sizeof(sa_family_t)+ \
                      _SS_PAD1SIZE + _SS_ALIGNSIZE))
struct sockaddr_storage {
    sa_family_t  ss_family;  /* Address family. */
/*
 *  Following fields are implementation-defined.
 */
    char _ss_pad1[_SS_PAD1SIZE];
        /* 6-byte pad; this is to make implementation-defined
           pad up to alignment field that follows explicit in
           the data structure. */
    int64_t _ss_align;  /* Field to force desired structure
                           storage alignment. */
    char _ss_pad2[_SS_PAD2SIZE];
        /* 112-byte pad to achieve desired size,
           _SS_MAXSIZE value minus size of ss_family
           __ss_pad1, __ss_align fields is 112. */
};

#define SOMAXCONN        128
#define AF_UNSPEC	0

#ifndef AI_PASSIVE
# define AI_PASSIVE	0x0001	/* Socket address is intended for `bind'.  */

#endif
#endif
