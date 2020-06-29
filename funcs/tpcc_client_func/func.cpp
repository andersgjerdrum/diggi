#include "runtime/func.h"

#include "messaging/IMessageManager.h"
#include "DiggiAssert.h"
#include <inttypes.h>
#include "runtime/DiggiAPI.h"
#include "tpcc.h"
/*
	TPCC func
*/

#define TPCC_BENCHMARK_MESSAGE_TYPE (msg_type_t)678
static std::string server_name = "sql_server_func";
static volatile size_t threads_done = 0;
static std::map<std::string, const char *> config = {
		{"server.name",	server_name.c_str()},
		{"init.tables",	tpcc_create_tables.c_str()},
		//{"reset",		"no"},
		{"drop.tables",	tpcc_drop_tables.c_str()}
};

void func_init(void * ctx, int status)
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	a_cont->GetLogObject()->Log("Initializing TPCC client func\n");
}



void execute_tpcc_thread(void *ctx, int status) {

	DIGGI_ASSERT(ctx);
	auto tpcc = (Tpcc*)ctx;
	auto a_cont = GET_DIGGI_GLOBAL_CONTEXT();

	a_cont->GetLogObject()->Log(LRELEASE,"TPCC Executing benchmark tpcc=%p\n",tpcc);
    int duration = (size_t)atoi(a_cont->GetFuncConfig()["Duration"].value.tostring().c_str());

	auto result  = tpcc->execute(std::chrono::duration<double>(duration));
	std::chrono::duration<double> diff = result.stop - result.start;
	size_t total_count = result.txn_counters[NEW_ORDER];

	double tpmc = (double)total_count/((double)diff.count()/(double)60);
	a_cont->GetLogObject()->Log(LRELEASE, "TPCC done, %f tpmc , experiment duration %f\n", tpmc, diff.count());
	__sync_fetch_and_add(&threads_done, 1);
    if(threads_done == a_cont->GetThreadPool()->physicalThreadCount()){
            a_cont->GetSignalHandler()->Stopfunc();
    }

	delete tpcc;
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
                                                    
            if((it->first.find("tpcc_client_func") != std::string::npos) 
                                && (it->second.raw != a_cont->GetId().raw)){

                auto msg = a_cont->GetMessageManager()->allocateMessage(it->first, 8, REGULAR, ENCRYPTED);
                msg->type = TPCC_BENCHMARK_MESSAGE_TYPE;
                a_cont->GetLogObject()->Log(
                    LRELEASE,"Notifying %s of TPCC loading complete\n",
                    it->first.c_str());
                a_cont->GetMessageManager()->Send(msg, nullptr, nullptr);
            }
    }
}

void register_and_wait(void *ptr, int status)
{
	DIGGI_ASSERT(ptr);
	auto a_cont = GET_DIGGI_GLOBAL_CONTEXT();
	a_cont->GetLogObject()->Log(LRELEASE,"Listening for TPCC load complete event\n");

	a_cont->GetMessageManager()->registerTypeCallback(
				execute_tpcc_thread_unpack,
				TPCC_BENCHMARK_MESSAGE_TYPE, 
				ptr);
}

void func_start(void * ctx, int status)
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	auto thread_p = a_cont->GetThreadPool();
		
	a_cont->GetLogObject()->Log(LRELEASE,"Starting TPCC client func\n");
	for (size_t i = 0; i < thread_p->physicalThreadCount(); i++) {

		auto dbclient = new DBClient(a_cont->GetMessageManager(), thread_p, CLEARTEXT);
		auto should_load =  (a_cont->GetFuncConfig()["tpcc-load"].value == "1") && (i == 0);
		auto tpcc = new Tpcc(2, 900.0, config, dbclient, should_load);
		
		if (should_load){

			a_cont->GetLogObject()->Log(LRELEASE,"TPCC Loading tables...\n");
			auto duration = tpcc->load();
			a_cont->GetLogObject()->Log(LRELEASE,"TPCC done loading\n");
			
		}

        if((a_cont->GetFuncConfig()["tpcc-load"].value == "1")){
            /*
				Send message to all other 
			*/
            thread_p->ScheduleOn(i, send_from_thread, nullptr, __PRETTY_FUNCTION__);
        }

		if((a_cont->GetFuncConfig()["wait-for-load"].value == "1")){
			a_cont->GetLogObject()->Log(LRELEASE,"Registering wait tpcc=%p thread on thread=%lu\n", tpcc, i);
			thread_p->ScheduleOn(i, register_and_wait, tpcc, __PRETTY_FUNCTION__);
		}
		else {
			a_cont->GetLogObject()->Log(LRELEASE,"Registering execute tpcc=%p thread on thread=%lu\n",tpcc, i);

			thread_p->ScheduleOn(i, execute_tpcc_thread, tpcc, __PRETTY_FUNCTION__);
		}
	}
}

void func_stop(void *ctx, int status) 
{
	DIGGI_ASSERT(ctx);
	auto a_cont = (DiggiAPI*)ctx;
	a_cont->GetLogObject()->Log(LRELEASE, "Stopping TPCC client func\n");
}