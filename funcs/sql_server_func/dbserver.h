#ifndef DBSERVER_H
#define DBSERVER_H

#include "messaging/IMessageManager.h"
#include "runtime/DiggiAPI.h"
#include "DiggiAssert.h"
#include "sqlite3.h"
#include "ZeroCopyString.h"
#include <inttypes.h>
#include "JSONParser.h"
#include "misc.h"

class IDBServer {
public:
	IDBServer() {}
	virtual	~IDBServer() {}
};

class DBServer : public IDBServer {
	std::string name;
	IDiggiAPI *a_cont;
	std::vector<sqlite3 *> db_connection;
    bool inmem;
public:
	static void register_callback_per_thread(void * ptr, int status);
	DBServer(IDiggiAPI *imngr, std::string dbname);
	static void executeQuery(void * ctx, int status);
	static void executeRetry(void * ptr, int status);

	// Inherited via IDBServer
};
#endif