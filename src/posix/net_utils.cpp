#include "DiggiAssert.h"
#include "posix/net_utils.h"

 /*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
 
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "posix/tlibc/byteswap.h"


uint32_t i_htonl(uint32_t hostlong){ 
    #if BYTE_ORDER == LITTLE_ENDIAN
    u_char *s = (u_char *)&hostlong;
        return (u_int32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
    #else
        return hostlong;
    #endif

 }

uint16_t i_htons(uint16_t hostshort){
    #if BYTE_ORDER == BIG_ENDIAN
        return hostshort;
    #elif BYTE_ORDER == LITTLE_ENDIAN
        return __bswap_16 (hostshort);
    #else
    #error "What kind of system is this?"
    #endif
}
uint32_t i_ntohl(uint32_t netlong){
        return i_htonl(netlong);
}

uint16_t i_ntohs(uint16_t netshort){
        return i_htons(netshort);
}

int i_inet_aton(const char *cp, struct in_addr *inp){DIGGI_ASSERT(false);return 0;}

in_addr_t i_inet_addr(const char *cp){DIGGI_ASSERT(false);return 0;}

in_addr_t i_inet_network(const char *cp){DIGGI_ASSERT(false);return 0;}

char *i_inet_ntoa(struct in_addr in){
        // TODO: buffer is not thread safe! 

        static char buffer[18];
        unsigned char *bytes = (unsigned char *) &in;
        snprintf (buffer, sizeof (buffer), "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);

return buffer;

}

struct in_addr i_inet_makeaddr(int net, int host){DIGGI_ASSERT(false);return {0} ;}

in_addr_t i_inet_lnaof(struct in_addr in){DIGGI_ASSERT(false);return 0;}

in_addr_t i_inet_netof(struct in_addr in){DIGGI_ASSERT(false);return 0;}





 
#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) ((size_t)sprintf x)
#endif
 
/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */
 
static const char *inet_ntop4 (const u_char *src, char *dst, socklen_t size);
static const char *inet_ntop6 (const u_char *src, char *dst, socklen_t size);
 
/* char *
 * inet_ntop(af, src, dst, size)
 *        convert a network format address to presentation format.
 * return:
 *        pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *        Paul Vixie, 1996.
 */

//libc_hidden_def (inet_ntop)
 
/* const char *
 * inet_ntop4(src, dst, size)
 *        format an IPv4 address
 * return:
 *        `dst' (as a const)
 * notes:
 *        (1) uses no statics
 *        (2) takes a u_char* not an in_addr as input
 * author:
 *        Paul Vixie, 1996.
 * 
 * edit:
 *      replaced strcpy with memcpy
 *      author: Lars Brenna, 2019
 */
static const char *
inet_ntop4 (const u_char *src, char *dst, socklen_t size)
{
        static const char fmt[] = "%u.%u.%u.%u";
        char tmp[sizeof "255.255.255.255"];
 
        if (SPRINTF((tmp, fmt, src[0], src[1], src[2], src[3])) >= size) {
                // TODO: implement errno
                //__set_errno (ENOSPC);
                return (NULL);
        }
        // TODO: replace strcp with no copy... 
        memcpy((void*)dst, (void*) tmp, (size_t)size);
        return dst;
}
 

/* const char *
 * inet_ntop6(src, dst, size)
 *        convert IPv6 binary address into presentation (printable) format
 * author:
 *        Paul Vixie, 1996.
 */
static const char *
inet_ntop6 (const u_char *src, char *dst, socklen_t size)
{
        /*
         * Note that int32_t and int16_t need only be "at least" large enough
         * to contain a value of the specified size.  On some systems, like
         * Crays, there is no such thing as an integer variable with 16 bits.
         * Keep this in mind if you think this function should have been coded
         * to use pointer overlays.  All the world's not a VAX.
         */
        char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
        struct { int base, len; } best, cur;
        u_int words[NS_IN6ADDRSZ / NS_INT16SZ];
        int i;
 
        /*
         * Preprocess:
         *        Copy the input (bytewise) array into a wordwise array.
         *        Find the longest run of 0x00's in src[] for :: shorthanding.
         */
        memset(words, '\0', sizeof words);
        for (i = 0; i < NS_IN6ADDRSZ; i += 2)
                words[i / 2] = (src[i] << 8) | src[i + 1];
        best.base = -1;
        cur.base = -1;
        best.len = 0;
        cur.len = 0;
        for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
                if (words[i] == 0) {
                        if (cur.base == -1)
                                cur.base = i, cur.len = 1;
                        else
                                cur.len++;
                } else {
                        if (cur.base != -1) {
                                if (best.base == -1 || cur.len > best.len)
                                        best = cur;
                                cur.base = -1;
                        }
                }
        }
        if (cur.base != -1) {
                if (best.base == -1 || cur.len > best.len)
                        best = cur;
        }
        if (best.base != -1 && best.len < 2)
                best.base = -1;
 
        /*
         * Format the result.
         */
        tp = tmp;
        for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
                /* Are we inside the best run of 0x00's? */
                if (best.base != -1 && i >= best.base &&
                    i < (best.base + best.len)) {
                        if (i == best.base)
                                *tp++ = ':';
                        continue;
                }
                /* Are we following an initial run of 0x00s or any real hex? */
                if (i != 0)
                        *tp++ = ':';
                /* Is this address an encapsulated IPv4? */
                if (i == 6 && best.base == 0 &&
                    (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
                        if (!inet_ntop4(src+12, tp, sizeof tmp - (tp - tmp)))
                                return (NULL);
                        tp += strlen(tp);
                        break;
                }
                tp += SPRINTF((tp, "%x", words[i]));
        }
        /* Was it a trailing run of 0x00's? */
        if (best.base != -1 && (best.base + best.len) ==
            (NS_IN6ADDRSZ / NS_INT16SZ))
                *tp++ = ':';
        *tp++ = '\0';
 
        /*
         * Check for overflow, copy, and we're done.
         */
        if ((socklen_t)(tp - tmp) > size) {
                // TODO: implement errno
                //__set_errno (ENOSPC);
                return (NULL);
        }
        memcpy((void*)dst, (void*) tmp, (size_t)size);
        return dst;
}

const char *i_inet_ntop(int af, const void *src, char *dst, socklen_t size){
    switch (af) {
        case AF_INET:
                return (inet_ntop4((const u_char*)src, (char*) dst, size));
        case AF_INET6:
                return (inet_ntop6((const u_char*)src, (char*)dst, size));
        default:
                //TODO: implement errno
                //__set_errno (EAFNOSUPPORT);
                return (NULL);
    }
        /* NOTREACHED */

}