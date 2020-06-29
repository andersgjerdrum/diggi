
/**
 * Implement a client api for SQL relational database queries and transactions.
 * @file dbclient.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief 
 * @version 0.1
 * @date 2020-01-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "storage/DBClient.h"
#include "messaging/Pack.h"
/**
 * Retrieves columns of table result
 * 
 * @return int count of columns
 */
int DBResult::columnCount()
{
    return intrnl.children.size();
}
/**
 * Unmarshal and return an integer type from a particular column
 * 
 * @param col column number
 * @return int  integer content in cell.
 */
int DBResult::getInt(size_t col)
{
    DIGGI_ASSERT(col < intrnl.children.size());
    auto val = intrnl.children[col].value;
    int result = 0;
    auto raw = val.tostring();
    result = atoi(raw.c_str());
    return result;
}
/**
 * Unmarshal and return doible type from a particular column
 * 
 * @param col column number
 * @return double content in cell
 */
double DBResult::getDouble(size_t col)
{
    DIGGI_ASSERT(col < intrnl.children.size());
    auto val = intrnl.children[col].value;
    double result = 0.0;
    auto raw = val.tostring();
    result = atof(raw.c_str());
    return result;
}
/**
 * Unmarshal and return unsigned 64-bit int from a particular column.
 * 
 * @param col column number
 * @return uint64_t content in cell
 */
uint64_t DBResult::getUnsigned(size_t col)
{
    DIGGI_ASSERT(col < intrnl.children.size());
    auto val = intrnl.children[col].value;
    uint64_t result = 0;
    auto raw = val.tostring();
    result = (uint64_t)atoi(raw.c_str());
    return result;
}
/**
 * Unmarshal and return string type from a particular column
 * @warning incurs a copy operation, costly for large strings.
 * @param col column number
 * @return std::string content in cell
 */
std::string DBResult::getString(size_t col)
{
    DIGGI_ASSERT(col < intrnl.children.size());
    auto val = intrnl.children[col].value;
    auto raw = val.copy();
    auto ret = std::string(raw, val.size());
    free(raw);
    return ret;
}
/**
 * To translate database client sql api into a synchronous blocking one, this method, invoked on succesfull completion of a request.
 * pushes response onto incomming request queue.
 * 
 * @param ptr incomming database operation response in form of a msg_async_response_t(context field is DBClient)
 * @param status status flag (unused) future work
 */
void DBClient::set_callback_msg(void *ptr, int status)
{
    DIGGI_ASSERT(ptr);
    auto rsp = (msg_async_response_t *)ptr;
    auto _this = (DBClient *)rsp->context;
    DIGGI_ASSERT(_this);
    DIGGI_ASSERT(rsp->msg);
    auto msg_cpy = COPY(msg_t, rsp->msg, rsp->msg->size);
    _this->free_msgs.push_back(msg_cpy);
    _this->incomming_msgs.push_back(msg_cpy);
}
/**
 * After a number of Database client query operation are marhaled and sent, 
 * this method may be invoked to wait for a given number of responses.
 * 
 * @param wait_for count of response objects to wait for.
 */
void DBClient::get_callback_msg(size_t wait_for)
{
    DIGGI_ASSERT(wait_for);
    size_t count = this->incomming_msgs.size();
    while (this->incomming_msgs.size() < count + wait_for)
    {
        if (!this->threadPool->Alive())
        {
            return;
        }
        this->threadPool->Yield();
    }
}
/**
 * Connect to a given diggi instance by specifying the human readable address for the target relational DB.
 * returns immediately as connections are esetablished via the trusted root prior to invocation. 
 * If the client instance does not match the correct SGX quote, the server will deny access.
 * @param inf human readable address to DB.
 */
void DBClient::connect(std::string inf)
{
    connection_info = inf;
}

/**
 * Begin a multirequest DB transaction.
 * 
 */
void DBClient::beginTransaction()
{
    DIGGI_ASSERT(!this->transaction_active);
    DIGGI_ASSERT(!this->appended_begin_statement);
    this->transaction_active = true;
    this->appended_begin_statement = false;
}
/**
 * execute a query request against a db server, and await the response.
 * if no transaction is in progress, a new will be started.
 * @param templ template database request string
 * @param ... parameter to template string
 * @return int successful operation flag
 */
int DBClient::executeBlob(const char *query, char *blob, size_t blob_size)
{
    incomming_msgs.clear();
    DIGGI_ASSERT(mngr);
    std::string transactionstmnt = "";
    if (this->transaction_active)
    {
        DIGGI_ASSERT(false);
    }

    auto querylength = strlen(query) + 1;
    auto expectechars = sizeof(size_t) + querylength + (int)transactionstmnt.size() + blob_size;

    DIGGI_ASSERT(connection_info.size());
    auto preparedmsg = this->mngr->allocateMessage(connection_info, expectechars, CALLBACK, deliveryopt);
    uint8_t *preparedquery = preparedmsg->data;
    Pack::pack<size_t>(&preparedquery, querylength);

    Pack::packBuffer(&preparedquery, (uint8_t *)query, querylength);
    Pack::packBuffer(&preparedquery, (uint8_t *)blob, blob_size);

    preparedmsg->type = SQL_QUERY_MESSAGE_BLOB_TYPE;
    this->mngr->Send(preparedmsg, set_callback_msg, this);
    get_callback_msg(1);

    return 0;
}

int DBClient::execute(const char *templ, ...)
{
    incomming_msgs.clear();
    DIGGI_ASSERT(sizeof(templ) > 0);
    DIGGI_ASSERT(mngr);
    std::string transactionstmnt = "";
    if (this->transaction_active && !this->appended_begin_statement)
    {

        transactionstmnt = "BEGIN TRANSACTION;";

        this->appended_begin_statement = true;
    }
    else if (!this->transaction_active)
    {
        DIGGI_ASSERT(!this->appended_begin_statement);
    }

    va_list args;
    va_start(args, templ);
    auto vnspritnfresult = vsnprintf(NULL, 0, templ, args) + 1; // safe byte for \0
    DIGGI_ASSERT(vnspritnfresult > 0);
    auto expectechars = vnspritnfresult + (int)transactionstmnt.size();
    va_end(args);

    DIGGI_ASSERT(connection_info.size());
    auto preparedmsg = this->mngr->allocateMessage(connection_info, expectechars, CALLBACK, deliveryopt);
    char *preparedquery = (char *)preparedmsg->data + transactionstmnt.size();

    if (transactionstmnt.size())
    {
        memcpy(preparedmsg->data, transactionstmnt.c_str(), transactionstmnt.size());
    }
    preparedmsg->size = expectechars + sizeof(msg_t);

    va_start(args, templ);
    auto chars = vsnprintf(preparedquery, vnspritnfresult, templ, args);
    va_end(args);

    DIGGI_ASSERT(chars > 0);
    DIGGI_ASSERT(chars <= expectechars);
    //printf("clientsend:%s\n", preparedquery);
    preparedmsg->type = SQL_QUERY_MESSAGE_TYPE;
    this->mngr->Send(preparedmsg, set_callback_msg, this);
    get_callback_msg(1);

    return 0;
}
/**
 * Execute a list of queries as a single transactions and await their response
 * Executed in batches as specified in BATCH_SIZE, to reduces memory pressure on target DB.
 * @see BATCH_SIZE
 * @param statements list of queries
 * @return int success flag
 */
int DBClient::executemany(std::vector<std::string> statements)
{
    if (!this->threadPool->Alive())
    {
        return -1;
    }
    incomming_msgs.clear();
    DIGGI_ASSERT(statements.size());
    DIGGI_ASSERT(mngr);
    DIGGI_ASSERT(connection_info.size());

    int batches = 0;
    for (auto statement : statements)
    {
        batches++;
        DIGGI_ASSERT(statement.size());

        auto preparedmsg = this->mngr->allocateMessage(connection_info, statement.size() + 1, CALLBACK, deliveryopt);
        memcpy(preparedmsg->data, statement.c_str(), statement.size());
        preparedmsg->type = SQL_QUERY_MESSAGE_TYPE;
        preparedmsg->size = statement.size() + 1 + sizeof(msg_t);
        preparedmsg->data[statement.size() - 1] = '\0';
        this->mngr->Send(preparedmsg, set_callback_msg, this);
        /*
			TODO:bundle multiple executions together.
			We must wait for callback to achieve synchrony
		*/
        if (batches % BATCH_SIZE == 0)
        {
            get_callback_msg(BATCH_SIZE);
            incomming_msgs.clear();
            batches = 0;
        }
    }
    if (batches)
    {
        get_callback_msg(batches);
    }
    return 0;
}
/**
 * Commit transactions and free incomming results cache. 
 */
void DBClient::commit()
{
    if (!this->transaction_active)
    {
        DIGGI_ASSERT(!this->appended_begin_statement);
        return;
    }
    this->freeDBResults();
    this->transaction_active = false;
    this->appended_begin_statement = false;
    std::string statement = "COMMIT;";
    DIGGI_ASSERT(statement.size());
    DIGGI_ASSERT(mngr);
    DIGGI_ASSERT(connection_info.size());

    auto preparedmsg = this->mngr->allocateMessage(connection_info, statement.size(), CALLBACK, deliveryopt);
    memcpy(preparedmsg->data, statement.c_str(), statement.size());

    preparedmsg->type = SQL_QUERY_MESSAGE_TYPE;
    preparedmsg->size = statement.size() + sizeof(msg_t);
    this->mngr->Send(preparedmsg, set_callback_msg, this);
    get_callback_msg(1);

    return;
}
/**
 * Free all incomming query responses cached by api.
 * 
 */
void DBClient::freeDBResults()
{
    for (auto msgx : free_msgs)
    {
        free(msgx);
    }
    free_msgs.clear();
    incomming_msgs.clear();
}
/**
 * Fetch a single row of the requst response table.
 * Asumes only a single response in table, will be purged from api after this call.
 * If more than one is expected, use fetchall.
 * 
 * @return DBResult row containing results
 */
DBResult DBClient::fetchone()
{
    DIGGI_ASSERT(incomming_msgs.size());

    auto incomming_msg = incomming_msgs.front();
    incomming_msgs.erase(incomming_msgs.begin());
    auto zstr = zcstring((char *)incomming_msg->data, incomming_msg->size - sizeof(msg_t));
    auto results = json_node(zstr);
    if (results.children.size() != 0)
    {
        return DBResult(results.children[0]);
    }
    else
    {
        return DBResult();
    }
}
/**
 * Fetcha all result rows cached by the api since last execute invocation.
 * 
 * @return std::vector<DBResult> list of row results.
 */
std::vector<DBResult> DBClient::fetchall()
{
    std::vector<DBResult> returnVector = {};
    DIGGI_ASSERT(incomming_msgs.size());
    for (msg_t *incomming_msg : incomming_msgs)
    {
        if (incomming_msg->size > sizeof(msg_t))
        {
            auto zstr = zcstring((char *)incomming_msg->data, incomming_msg->size - sizeof(msg_t));
            auto results = json_node(zstr);
            DIGGI_ASSERT(results.children.size());
            for (auto result : results.children)
            {
                returnVector.push_back(DBResult(result));
            }
        }
    }
    incomming_msgs.clear();
    return returnVector;
}
/**
 * request an explicit rollback for transaction.
 * Frees all cached results.
 */
void DBClient::rollback()
{
    this->transaction_active = false;
    this->appended_begin_statement = false;
    std::string statement = "ROLLBACK;";
    DIGGI_ASSERT(statement.size());
    DIGGI_ASSERT(mngr);

    this->freeDBResults();
    DIGGI_ASSERT(connection_info.size());

    auto preparedmsg = this->mngr->allocateMessage(connection_info, statement.size(), CALLBACK, deliveryopt);
    memcpy(preparedmsg->data, statement.c_str(), statement.size());

    preparedmsg->type = SQL_QUERY_MESSAGE_TYPE;

    this->mngr->Send(preparedmsg, set_callback_msg, this);
    get_callback_msg(1);

    return;
}
