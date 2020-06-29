

#ifndef EXT_COM_H
#define EXT_COM_H
#ifdef UNTRUSTED_APP

#include "network/Connection.h"
#include "network/HTTP.h"


void tcp_incomming(void *ptr, int status);
void tcp_outgoing(void *ctx, int status);
void tcp_outgoing_cb(void *ptr, int status);
void tcp_incomming_read_header_cb(void *ptr, int status);
void tcp_incomming_read_body_cb(void *ptr, int status);

void http_incomming(void *ptr, int status);
void http_incomming_cb(void *ptr, int status);
void http_respond_ok(IConnection *connection);
void http_respond_ok_done_cb(void *ptr, int status);
#endif
#endif