#include "dbserver.h"
#include "messaging/Pack.h"
static volatile int write_lock = 0;

inline void lock(volatile int *lock)
{
}

inline void unlock(volatile int *lock)
{
    __sync_synchronize(); // Memory barrier.
    *lock = 0;
}

DBServer::DBServer(IDiggiAPI *imngr, std::string dbname) : name(dbname), a_cont(imngr), db_connection(MAX_VIRTUAL_THREADS)
{
    DIGGI_ASSERT(imngr);
    DIGGI_ASSERT(dbname.size());
    /*
        If in memory extra care must be taken to avoid DB locks to avoid rollbacks.
        Since entire db is locked, only single write operation may continue.
    */
    inmem = (name == "file::memory:?cache=shared");

    a_cont->GetMessageManager()->registerTypeCallback(DBServer::executeQuery, SQL_QUERY_MESSAGE_TYPE, this);
    a_cont->GetMessageManager()->registerTypeCallback(DBServer::executeQuery, SQL_QUERY_MESSAGE_BLOB_TYPE, this);
}

void DBServer::executeRetry(void *ptr, int status)
{

    auto ctx = (msg_async_response_t *)ptr;
    DIGGI_ASSERT(ptr);
    DIGGI_ASSERT(ctx->context);
    DIGGI_ASSERT(ctx->msg);
    DBServer::executeQuery(ctx, 1);
    free(ctx->msg);
    free(ctx);
}

void DBServer::executeQuery(void *msg, int status)
{

    auto ctx = (msg_async_response_t *)msg;
    DIGGI_ASSERT(ctx);
    DIGGI_ASSERT(ctx->msg);
    DIGGI_ASSERT(ctx->context);
    auto _this = (DBServer *)ctx->context;

    char *sql = (char *)ctx->msg->data;
    size_t querysize = 0;
    if (ctx->msg->type == SQL_QUERY_MESSAGE_BLOB_TYPE)
    {
        memcpy(&querysize, sql, sizeof(size_t));
        sql += sizeof(size_t);
    }
    else
    {
        sql[ctx->msg->size - sizeof(msg_t) - 1] = '\0';
    }
    auto blob_ptr = ctx->msg->data + querysize;
    ALIGNED_CHAR(8)
    bgcmt[] = "BEGIN TRANSACTION;";
    ALIGNED_CHAR(8)
    endtx[] = "COMMIT";
    ALIGNED_CHAR(8)
    rlbck[] = "ROLLBACK";

    if (_this->inmem && zcstring((char *)ctx->msg->data).find(bgcmt) != UINT_MAX)
    {
        while (__sync_lock_test_and_set(&write_lock, 1))
        {
            _this->a_cont->GetThreadPool()->Yield();
        }
        DIGGI_TRACE(_this->a_cont->GetLogObject(),
                    LRELEASE,
                    "Lock taken,%p\n", &write_lock);
    }

    DIGGI_TRACE(_this->a_cont->GetLogObject(),
                LDEBUG,
                "DBServer::executeQuery: vthread=%lu sql query: %s\n",
                _this->a_cont->GetThreadPool()->currentVThreadId(),
                sql);
    std::vector<sqlite3_stmt *> stmts = {};
    const char *leftover;
    if (_this->db_connection[_this->a_cont->GetThreadPool()->currentVThreadId()] == nullptr)
    {
        DIGGI_TRACE(_this->a_cont->GetLogObject(),
                    LRELEASE,
                    "starting db, current vthread=%lu\n",
                    _this->a_cont->GetThreadPool()->currentVThreadId());
        int rc = sqlite3_open(_this->name.c_str(), &_this->db_connection[_this->a_cont->GetThreadPool()->currentVThreadId()]);
        if (rc != SQLITE_OK)
        {
            DIGGI_TRACE(_this->a_cont->GetLogObject(), LRELEASE,
                        "ERROR: opening database = %s, returned errorcode=%d\n",
                        _this->name.c_str(), rc);
        }
        DIGGI_ASSERT(rc == SQLITE_OK);
    }
    /*assume synchrony here */
    auto json_top = json_node();
    json_top.type = ARRAY;
    do
    {
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(_this->db_connection[_this->a_cont->GetThreadPool()->currentVThreadId()], sql, -1, &stmt, &leftover);
        auto errmsg = sqlite3_errmsg(_this->db_connection[_this->a_cont->GetThreadPool()->currentVThreadId()]);
        if (rc == SQLITE_BUSY)
        {
            /*
				Yield execution for other tasks, while waiting for exclusive lock.
			*/
            DIGGI_TRACE(_this->a_cont->GetLogObject(), LDEBUG, "DBServer::executeQuery: Database busy, retrying transaction, vthread=%lu\n",
                        _this->a_cont->GetThreadPool()->currentVThreadId());
            _this->a_cont->GetThreadPool()->Yield();
            continue;
        }
        if (rc != SQLITE_OK)
        {
            DIGGI_TRACE(_this->a_cont->GetLogObject(),
                        LRELEASE,
                        "DBServer::executeQuery: prepare statement vthread=%lu returned %d, errormsg = %s\n",
                        _this->a_cont->GetThreadPool()->currentVThreadId(),
                        rc,
                        errmsg);
            DIGGI_ASSERT(false);
        }
        if (ctx->msg->type == SQL_QUERY_MESSAGE_BLOB_TYPE)
        {
            DIGGI_ASSERT(querysize);
            rc = sqlite3_bind_blob(stmt, 1, blob_ptr, ctx->msg->size - sizeof(msg_t) - querysize, SQLITE_STATIC);
            DIGGI_ASSERT(rc == SQLITE_OK);
        }

        while ((rc = sqlite3_step(stmt)) != SQLITE_DONE)
        {
            auto errmsg = sqlite3_errmsg(_this->db_connection[_this->a_cont->GetThreadPool()->currentVThreadId()]);
            if (rc != SQLITE_DONE && rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_BUSY)
            {
                DIGGI_TRACE(_this->a_cont->GetLogObject(), LRELEASE,
                            "DBServer::executeQuery: ERROR: from step statement vthread=%lu returned %d, errormsg = %s\n", _this->a_cont->GetThreadPool()->currentVThreadId(), rc, errmsg);
                DIGGI_ASSERT(false);
            }
            if (rc == SQLITE_BUSY)
            {
                /*
					Yield execution for other tasks, while waiting for exclusive lock.
				*/
                DIGGI_TRACE(_this->a_cont->GetLogObject(), LDEBUG, "DBServer::executeQuery: Database busy, retrying transaction vthread=%lu\n",
                            _this->a_cont->GetThreadPool()->currentVThreadId());
                _this->a_cont->GetThreadPool()->Yield();
                continue;
            }

            int i;
            int num_cols = sqlite3_column_count(stmt);
            auto json_row = json_node();
            json_row.type = ARRAY;
            for (i = 0; i < num_cols; i++)
            {
                auto zcs = zcstring();
                auto blob_size = sqlite3_column_bytes(stmt, i);
                if (blob_size)
                {

                    auto resdata = zcs.reserve(blob_size);
                    auto blob = sqlite3_column_blob(stmt, i);

                    /*
						Copy data since blob will be deallocated with the next call to sqlite3_step
					*/
                    memcpy(resdata, blob, blob_size);
                    zcs.append((char *)resdata, blob_size);

                    auto blob_node = json_node();
                    /*
						hopefully zcstring will capture reference
					*/
                    blob_node.value = zcs;
                    if (SQLITE_BLOB == sqlite3_column_type(stmt, i))
                    {
                        blob_node.type = BLOB;
                    }
                    else
                    {
                        blob_node.type = STRING;
                    }
                    json_row << blob_node;
                }
            }

            json_top << json_row;
        }
        /*
			reached SQLITE_BUSY and must therefore retry.
		*/
        if (leftover == nullptr)
        {
            continue;
        }
        stmts.push_back(stmt);
        sql = (char *)leftover;
    } while (sql[0] != '\0');

    msg_t *msg_n = nullptr;
    if (json_top.children.size() > 0)
    {

        auto retstring = zcstring();
        json_top.serialize(retstring);
        msg_n = _this->a_cont->GetMessageManager()->allocateMessage(ctx->msg, retstring.size());

        auto stringrepresentation = std::string();
        retstring.tostring(&stringrepresentation);

        DIGGI_TRACE(_this->a_cont->GetLogObject(), LDEBUG, "SQL request results: %s\n", stringrepresentation.c_str());

        memcpy(msg_n->data, stringrepresentation.c_str(), stringrepresentation.size());
        msg_n->size = retstring.size() + sizeof(msg_t);
        DIGGI_ASSERT(stringrepresentation.size() == retstring.size());
    }
    else
    {
        msg_n = _this->a_cont->GetMessageManager()->allocateMessage(ctx->msg, 0);
    }
    /*
		Results have now been copied to a separate buffer
		and we can now free the statement.
	*/
    for (auto stmt : stmts)
    {
        int rc = sqlite3_finalize(stmt);
        DIGGI_ASSERT(rc == SQLITE_OK);
    }

    msg_n->dest = ctx->msg->src;
    msg_n->src = ctx->msg->dest;

    if (_this->inmem && ((zcstring((char *)ctx->msg->data).find(endtx) != UINT_MAX) || (zcstring((char *)ctx->msg->data).find(rlbck) != UINT_MAX)))
    {

        __sync_synchronize(); // Memory barrier.
        write_lock = 0;
        DIGGI_TRACE(_this->a_cont->GetLogObject(),
                    LRELEASE,
                    "Lock released\n");
    }
    _this->a_cont->GetMessageManager()->endAsync(ctx->msg);
    _this->a_cont->GetMessageManager()->Send(msg_n, nullptr, nullptr);
}