#ifndef ICONNECTION_H
#define ICONNECTION_H
#include <stddef.h>
#include <string>
#include "ZeroCopyString.h"
#include "datatypes.h"
using namespace std;


class IConnection
{
public:
	IConnection(){}
	virtual ~IConnection(){}
	virtual int initialize(string hostname, string servername, string serverport) = 0;
	virtual void write(zcstring buf, async_cb_t cb, void *ptr) = 0;
	virtual	void read(size_t length, async_cb_t cb, void * context) = 0;
	virtual	void read_line(async_cb_t cb, void* ptr) = 0;
	virtual string get_hostname() = 0;
	virtual string get_port() = 0;
	virtual void close() = 0;
};

#endif