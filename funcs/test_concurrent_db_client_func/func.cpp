#include "runtime/func.h"

#include "messaging/IMessageManager.h"
#include "messaging/Util.h"
#include "storage/DBClient.h"
#include "DiggiAssert.h"
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include <chrono>
#include <ctime>
#include <numeric>                                                              


/*
	DB client func
*/

#define DB_BENCHMARK_MESSAGE_TYPE (msg_type_t)678
static std::string server_name = "sql_server_func";
static volatile size_t threads_done = 0;

void func_init(void * ctx, int status)
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	a_cont->GetLogObject()->Log("Initializing DB client func\n");
}



void execute_tpcc_thread(void *ctx, int status) {

	DIGGI_ASSERT(ctx);
    std::string sel_q  = "SELECT * FROM COMPANY;";
	auto dbclient = (DBClient*)ctx;
	auto a_cont = GET_DIGGI_GLOBAL_CONTEXT();
    
	a_cont->GetLogObject()->Log(LRELEASE,"db Executing benchmark tpcc=%p\n",dbclient);
    int val = (size_t)atoi(a_cont->GetFuncConfig()["Duration"].value.tostring().c_str());

    auto latency  = new std::vector<double>();

    auto duration =  std::chrono::duration<double> (val);
    auto start =  std::chrono::system_clock::now();
    int select_count = 0;
	while ((std::chrono::system_clock::now() - start) < duration){
        auto l_start =  std::chrono::system_clock::now();
        dbclient->execute(sel_q.c_str());
        auto l_stop =  std::chrono::system_clock::now();
        latency->push_back((double)std::chrono::duration_cast<std::chrono::milliseconds>(l_stop - l_start).count());
        select_count++;
    }
    auto stop =  std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    double select_per_sec = (double)select_count/((double)diff.count() / 1000);
    a_cont->GetLogObject()->Log(LRELEASE,"Selects total, duration, avg latency, throughput");
    
    auto fname = std::to_string(a_cont->GetId().raw) + ".telemetry.log";
    FILE * pFile = fopen(fname.c_str(), "w+");
    fprintf(pFile, "%d, %d, %f, %f\n", 
                                select_count, 
                                diff.count(), 
                                std::accumulate(latency->begin(), latency->end(), 0.0)/latency->size(),
                                select_per_sec);
    a_cont->GetLogObject()->Log(LRELEASE,"%d, %d, %f, %f\n", 
                                select_count, 
                                diff.count(), 
                                std::accumulate(latency->begin(), latency->end(), 0.0)/latency->size(),
                                select_per_sec);
	fflush(pFile);
	fclose(pFile);
	__sync_fetch_and_add(&threads_done, 1);
    if(threads_done == a_cont->GetThreadPool()->physicalThreadCount()){
            a_cont->GetSignalHandler()->Stopfunc();
    }
    delete latency;
	delete dbclient;
}

void execute_tpcc_thread_unpack(void * ptr, int status){
	auto resp = (msg_async_response_t*)ptr;
    execute_tpcc_thread(resp->context, 1);
}

void send_from_thread(void* ptr, int status){
    auto a_cont = GET_DIGGI_GLOBAL_CONTEXT();
    auto func_map = a_cont->GetMessageManager()->getfuncNames();
    for (std::map<std::string, aid_t>::const_iterator it = func_map.begin(); 
                                                it != func_map.end(); it++) {
                                                    
            if((it->first.find("test_concurrent_db_client_func") != std::string::npos) 
                                && (it->second.raw != a_cont->GetId().raw)){

                auto msg = a_cont->GetMessageManager()->allocateMessage(it->first, 8, REGULAR);
                msg->type = DB_BENCHMARK_MESSAGE_TYPE;
                a_cont->GetLogObject()->Log(
                    LRELEASE,"Notifying %s of db loading complete\n",
                    it->first.c_str());
                a_cont->GetMessageManager()->Send(msg, nullptr, nullptr);
            }
    }
}

void register_and_wait(void *ptr, int status)
{
	DIGGI_ASSERT(ptr);
	auto a_cont = GET_DIGGI_GLOBAL_CONTEXT();
	a_cont->GetLogObject()->Log(LRELEASE,"Listening for db load complete event\n");

	a_cont->GetMessageManager()->registerTypeCallback(
				execute_tpcc_thread_unpack,
				DB_BENCHMARK_MESSAGE_TYPE, 
				ptr);
}

void func_start(void * ctx, int status)
{
    /*create table*/
   std::string create_table = "CREATE TABLE COMPANY("  \
		"ID INT PRIMARY KEY     NOT NULL," \
		"NAME           TEXT    NOT NULL," \
		"AGE            INT     NOT NULL," \
		"ADDRESS        CHAR(50)," \
		"SALARY         REAL );";

	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	auto thread_p = a_cont->GetThreadPool();
		
	a_cont->GetLogObject()->Log(LRELEASE,"Starting db client func\n");
	for (size_t i = 0; i < thread_p->physicalThreadCount(); i++) {

		auto dbclient = new DBClient(a_cont->GetMessageManager(), thread_p);
        dbclient->connect(a_cont->GetFuncConfig()["connected-to"].value.tostring());
		auto should_load =  (a_cont->GetFuncConfig()["load"].value == "1") && (i == 0);
		
		if (should_load){

			a_cont->GetLogObject()->Log(LRELEASE,"db Loading tables...\n");
            dbclient->execute(create_table.c_str());

        	for (int i = 0; i < 100; i++) {
                auto string = std::string("INSERT INTO COMPANY VALUES(") + Utils::itoa(i) + std::string(",'Anders', 9000, '123 party avenue', 123);");
                dbclient->execute(string.c_str());
            }
			a_cont->GetLogObject()->Log(LRELEASE,"db done loading\n");
			
		}

        if((a_cont->GetFuncConfig()["master"].value == "1")){
            /*
				Send message to all other 
			*/
            thread_p->ScheduleOn(i, send_from_thread, nullptr, __PRETTY_FUNCTION__);
        }

		if((a_cont->GetFuncConfig()["wait-for-load"].value == "1")){
			a_cont->GetLogObject()->Log(LRELEASE,"Registering wait db=%p thread on thread=%lu\n", dbclient, i);
			thread_p->ScheduleOn(i, register_and_wait, dbclient, __PRETTY_FUNCTION__);
		}
		else {
			a_cont->GetLogObject()->Log(LRELEASE,"Registering execute db=%p thread on thread=%lu\n",dbclient, i);

			thread_p->ScheduleOn(i, execute_tpcc_thread, dbclient, __PRETTY_FUNCTION__);
		}
	}
}

void func_stop(void *ctx, int status) 
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	a_cont->GetLogObject()->Log("Stopping db client func\n");
}