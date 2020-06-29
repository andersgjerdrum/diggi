#ifndef DBCLIENT_H
#define DBCLIENT_H
/**
 * Header file for sql relatinal database client interface.
 * @file DBClient.h
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief 
 * @version 0.1
 * @date 2020-01-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include <stdio.h>
#include <stdarg.h>
#include "DiggiAssert.h"
#include <stdlib.h>
#include <string>
#include <vector>
#include "messaging/IMessageManager.h"
#include "threading/IThreadPool.h"
#include "JSONParser.h"
#include "misc.h"

#define BATCH_SIZE 1
/**
 * Database result object for storing the result of a single row.
 * 
 */
class DBResult {
    ///protocol response is in JSON, and each DBResult object carries a reference to the original response object, only unmarshaling will result in copy operation.
	json_node intrnl;
public:
	DBResult(json_node node) :intrnl(node) {
	};
	DBResult() {
	};
	int columnCount();
	int getInt(size_t col);
	double getDouble(size_t col);
	uint64_t getUnsigned(size_t col);
	std::string getString(size_t col);
};

/**
 * Database client interface specification, may be implemented by any class which provide this interface
 */
class IDBClient {
public:
	IDBClient() {}
	virtual	~IDBClient() {}
	virtual void beginTransaction() = 0;
	virtual void connect(std::string host) = 0;
	virtual int execute(const char* tmpl, ...) = 0;
    virtual int executeBlob(const char *query, char *blob, size_t blob_size) = 0;
	virtual int executemany(std::vector<std::string> statements) = 0;
	virtual void commit() = 0;
	virtual void freeDBResults() = 0;
	virtual DBResult fetchone() = 0;
	virtual std::vector<DBResult> fetchall() = 0;
	virtual void rollback() = 0;

};
/**
 * Implementation of IDBClient interface for diggi relational storage.
 *  
 */
class DBClient : public IDBClient {
    ///reference to Messagemanager, carrying the communication api
	IMessageManager *mngr;
    ///option specifying if delivery is encrypted or not, in the event of a client providing additional encryption layers on top.
    msg_delivery_t deliveryopt;
    ///incomming response message queue, cached by the api until requestors are done with them.
	std::vector<msg_t *> incomming_msgs;
    std::vector<msg_t *> free_msgs; 
    ///storing the human readable database name for addressing queries.
	std::string connection_info;
    ///threadpool reference for managing asynchronus execution
	IThreadPool *threadPool;
    ///transaction state object for pending in progress transaction.
	bool transaction_active;
    /// specifying if current series of queries have prepended begin transaction statement.
	bool appended_begin_statement;
    
	void get_callback_msg(size_t wait_for);
public:
/**
 * @brief Construct a new DBClient object
 * Initialize database client, requires messaging api IMessageManager for communicating with database and IThreadPool Api for asynchronus execution.
 * @param mngr 
 * @param threadPool 
 * @param opt 
 */
	DBClient(IMessageManager *mngr, IThreadPool *threadPool, msg_delivery_t opt):
		mngr(mngr), 
        deliveryopt(opt),
		connection_info(""), 
		threadPool(threadPool),
		transaction_active(false),
		appended_begin_statement(false) {
	
	}
	static void set_callback_msg(void * ptr, int status);
	void beginTransaction();
	void connect(std::string inf);
	int execute(const char *statement, ...);
    int executeBlob(const char *query, char *blob, size_t blob_size);
	int executemany(std::vector<std::string> statements);
	void commit();
	void freeDBResults();
	DBResult fetchone();
	std::vector<DBResult> fetchall();
	void rollback();
	~DBClient() {
		freeDBResults();
	}
};


#endif