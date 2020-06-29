#include <stdio.h>
#include <inttypes.h>
#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "posix/stdio_stubs.h"
#include "runtime/DiggiAPI.h"
#include "mongoose.h"
#include "posix/intercept.h"
/*
	Mongoose server func
*/

static const char *s_http_port = "0000";
static struct mg_serve_http_opts s_http_server_opts = {0};
static struct mg_bind_opts bind_opts = {0};

static void ev_handler(struct mg_connection *nc, int ev, void *p) {

	if (ev == MG_EV_HTTP_REQUEST) {
		mg_serve_http(nc, (struct http_message *) p, s_http_server_opts);
	}
}



void func_init(void * ctx, int status)
{
	DIGGI_ASSERT(ctx);
	
	DiggiAPI *a_cont = (DiggiAPI*)ctx;

	a_cont->GetLogObject()->Log("func_init:mongoose server\n");

	s_http_port = a_cont->GetFuncConfig()["https_port"].value.tostring().c_str();
	s_http_server_opts.document_root = a_cont->GetFuncConfig()["document_root"].value.tostring().c_str();  // Serve current directory, although this is not the final solution. 
	s_http_server_opts.enable_directory_listing = "no";

    bind_opts.ssl_cert = "cert.pem";
    bind_opts.ssl_key = "key.pem";
	
}

void func_start(void *ctx, int status)
{
	
	DIGGI_ASSERT(ctx);

	auto a_cont = (DiggiAPI*)ctx;
	struct mg_mgr mgr;
	struct mg_connection *nc;
	const char *err;

	mg_mgr_init(&mgr, NULL);
	a_cont->GetLogObject()->Log("Starting SSL server on port %s\n", s_http_port);


	//bind_opts.ssl_cert = s_ssl_cert;
	//bind_opts.ssl_key = s_ssl_key;
	bind_opts.error_string = &err;
	printf("Starting SSL server on port %s\n",
         s_http_port);
 	 nc = mg_bind_opt(&mgr, s_http_port, ev_handler, bind_opts);
	if (nc == NULL) {
		a_cont->GetLogObject()->Log("Failed to create listener: %s\n", err);
		return;
	}

	// Set up HTTP server parameters
	mg_set_protocol_http_websocket(nc);
	
	time_t now = 0;
	for (;;) {
		now = mg_mgr_poll(&mgr, 1000);
	}
	mg_mgr_free(&mgr);

}

void func_stop(void *ctx, int status) 
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	a_cont->GetLogObject()->Log("func_stop:mongoose server\n");
}