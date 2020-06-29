#include "runtime/func.h"
#include "messaging/IMessageManager.h"
#include "posix/stdio_stubs.h"
#include <stdio.h>
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "sqlite3.h"
#include "posix/io_stubs.h"
#include "messaging/Util.h"
#include "telemetry.h"
#include "posix/intercept.h"


/*
	Test Sql func
*/


static DiggiAPI* a_cont = nullptr;

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
	a_cont->GetLogObject()->Log("Test SQL func callback invoked\n");
	int i;
	for (i = 0; i<argc; i++) {
		a_cont->GetLogObject()->Log("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	a_cont->GetLogObject()->Log("\n");
	return 0;
}


void func_init(void * ctx, int status)
{
	DIGGI_ASSERT(ctx);

	a_cont = (DiggiAPI*)ctx;
	a_cont->GetLogObject()->Log(LRELEASE, "Initializing SQL func\n");

	/*
		Set encrypted to true
	*/

	iostub_setcontext(a_cont, true);
	pthread_stubs_set_thread_manager(a_cont->GetThreadPool());
	set_syscall_interposition(1);
}

void func_start(void *ctx, int status)
{
	DIGGI_ASSERT(ctx);

	auto a_cont = (DiggiAPI*)ctx;

	a_cont->GetLogObject()->Log("Starting SQL func\n");

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	std::string sql;
	a_cont->GetLogObject()->Log(LRELEASE, "Opening db\n");
	rc = sqlite3_open("test.db", &db);

	if (rc) {
		a_cont->GetLogObject()->Log(LRELEASE, "Can't open database: %s\n", sqlite3_errmsg(db));
		return;
	}
	else {
		a_cont->GetLogObject()->Log(LRELEASE, "Opened database successfully\n");
	}

	/* Create SQL statement */
	sql = "CREATE TABLE COMPANY("  \
		"ID INT PRIMARY KEY     NOT NULL," \
		"NAME           TEXT    NOT NULL," \
		"AGE            INT     NOT NULL," \
		"ADDRESS        CHAR(50)," \
		"SALARY         REAL );";

	/* Execute SQL statement */
	rc = sqlite3_exec(db, sql.c_str(), callback, 0, &zErrMsg);

	DIGGI_ASSERT(rc == SQLITE_OK);
	//telemetry_capture("initialization");

	for (int i = 0; i < 1500; i++) {

		rc = sqlite3_exec(db, "BEGIN TRANSACTION; SELECT * FROM COMPANY; ", callback, 0, &zErrMsg);
		DIGGI_ASSERT(rc == SQLITE_OK);

		auto string = std::string("INSERT INTO COMPANY VALUES(") + Utils::itoa(i) + std::string(",'Anders', 9000, '123 party avenue', 123);COMMIT;");
		rc = sqlite3_exec(db, string.c_str(), 0, 0, &zErrMsg);
		
		//telemetry_capture("Insert cost");
		DIGGI_ASSERT(rc == SQLITE_OK);

		a_cont->GetLogObject()->Log(LRELEASE, "done with insert\n");
	}
	sqlite3_close(db);

	a_cont->GetLogObject()->Log(LRELEASE, "Exit SQL func\n");

}

void func_stop(void *ctx, int status)
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	a_cont->GetLogObject()->Log("SQL func Stopping\n");
	pthread_stubs_unset_thread_manager();
	set_syscall_interposition(0);
}