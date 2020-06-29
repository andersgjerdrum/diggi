/**
 * @file main.cpp
 * @author Anders Gjerdrum (anders.t.gjerdrum@uit.no)
 * @brief main process entry point for diggi host setup.
 * @version 0.1
 * @date 2020-01-30
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "runtime/main.h"

#ifdef UNTRUSTED_APP


#define CONFIG_FILE "configuration.json"
#define MAX_func_MEMORY_CONSUMPTION 1024U * 1024U * 1024U
#define DEFAULT_PORT (char*)"6000"
#define DEFAULT_IDENTIFIER (char*)"1"
static DiggiAPI* acon = nullptr;

/**
 * @brief operating system signal handler for SIGHUP
 * Supported for graceful shutdown
 * 
 * @param signum 
 */
void hup(int signum)
{
	DIGGI_ASSERT(acon);
	DIGGI_ASSERT(acon->GetLogObject());
	DIGGI_ASSERT(acon->GetThreadPool());
	acon->GetLogObject()->Log(LRELEASE,"Process recieved SIGHUP.\n");
	exit(-1);
}
/**
 * @brief operating system signal handler for SIGTERM
 * Supported for graceful shutdown
 * @param signum 
 */
void term(int signum)
{
	DIGGI_ASSERT(acon);
	DIGGI_ASSERT(acon->GetLogObject());
	DIGGI_ASSERT(acon->GetThreadPool());
	acon->GetLogObject()->Log(LRELEASE,"Process recieved SIGTERM.\n");
	exit(-1);
}
/**
 * @brief operating system signal handler for SIGINT
 * Supported for graceful shutdown.
 * 
 * @param signum 
 */
void inter(int signum)
{
	DIGGI_ASSERT(acon);
	DIGGI_ASSERT(acon->GetLogObject());
	DIGGI_ASSERT(acon->GetThreadPool());
	acon->GetLogObject()->Log(LRELEASE,"Process recieved SIGINT.\n");
	exit(-1);
}
/**
 * @brief Get the configuration object from disk.
 * 
 * @param in filename
 * @return string configuration string
 */
string get_config(string in)
{

	ifstream input(in);

	return string((std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>());
}

/**
 * @brief Process entry function
 * Loads diggi configuration from disk, set up signalhandlers and bootstraps the untrusted runtime.
 * Initializes control plane http request server for configuration updates.
 * Setup inbound data plane listenin socket for inbound diggi messages.
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char *argv[])
{
	/* 
		Register correct sigterm and sigint
	*/
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = term;
	sigaction(SIGTERM, &action, NULL);

	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = inter;
	sigaction(SIGINT, &action, NULL);
	
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = hup;
	sigaction(SIGHUP, &action, NULL);


	char * default_config = NULL;
	char * default_port = NULL;
	char * identifier = NULL;
    bool EXIT_WHEN_DONE = false;
	if (argc > 1) {
		for (int i = 1; i < argc; i++)
		{
			if (strcmp(argv[i], "--config") == 0)
			{
				if (i + 1 < argc)
				{
					default_config = argv[i + 1];
				}
				else
				{
					printf("%s, specify config filepath expicitly\n", __PRETTY_FUNCTION__);
					return -1;
				}
			}
            if (strcmp(argv[i], "--exit_when_done") == 0)
			{
				EXIT_WHEN_DONE = true;
			}
			else if (strcmp(argv[i], "--port") == 0) {
				if (i + 1 < argc)
				{
					default_port = argv[i + 1];
				}
				else
				{
					printf("%s, specify port expicitly\n", __PRETTY_FUNCTION__);
					return -1;
				}
			}
			else if (strcmp(argv[i], "--id") == 0) {
				if (i + 1 < argc)
				{
					identifier = argv[i + 1];
				}
				else
				{
					printf("%s, specify identity expicitly\n", __PRETTY_FUNCTION__);
					return -1;
				}
			}

		}
	}
	if(default_port == NULL){
		default_port = DEFAULT_PORT;
	}
	if(identifier == NULL){
		identifier = DEFAULT_IDENTIFIER;
	}

	aid_t pro;
	pro.raw = 0;
	pro.fields.type = PROC;
	pro.fields.proc = (uint8_t)Utils::str_to_id(std::string(identifier));
    
    /*
        Set thread name and pin to core 0
    */
    auto tself = pthread_self();
	pthread_setname_np(tself, "main_thread");  
    // cpu_set_t cpuset;
	// CPU_ZERO(&cpuset);
	// CPU_SET(0, &cpuset);
	// int rc = pthread_setaffinity_np(tself, sizeof(cpu_set_t), &cpuset);
    // DIGGI_ASSERT(rc == 0);

	auto thrd_pool = new ThreadPool(DIGGI_RUNTIME_THREAD_COUNT, ENCLAVE_MODE, "runtime_thread");
	auto logs = new StdLogger(thrd_pool);

	logs->SetFuncId(pro, "untrusted_runtime");

	string config;
	if (default_config == NULL)
	{
		config = get_config(CONFIG_FILE);
	}
	else
	{
		config = get_config(string(reinterpret_cast<const char*>(default_config), strlen(default_config)));
	}

	

	/*
		TODO: allow for threadsafe execution of harness code in separate thread
	*/
	//harness_threadpool = new ThreadPool(1, 1, logs);
	auto mbedtcp = new SimpleTCP();
	auto controll_server = new Connection(thrd_pool, http_incomming, mbedtcp, logs);
	controll_server->InitializeServer(NULL, default_port);
	auto data_server = new Connection(thrd_pool, tcp_incomming, mbedtcp, logs);
	/*
		Data plane served on separate port
	*/
	
	auto dataport = atoi(default_port) + 1;
	data_server->InitializeServer(NULL, Utils::itoa(dataport).c_str());

	/*pass config string into func_init*/
	
	acon = new DiggiAPI(thrd_pool, nullptr, nullptr, nullptr, nullptr, logs, pro, nullptr);
    SET_DIGGI_GLOBAL_CONTEXT(acon);
	runtime_init(acon, 1);
	runtime_start(new zcstring(config), 1, EXIT_WHEN_DONE);
	
	logs->Log(LRELEASE,"Waiting for exit ...\n");
	/*
		Steals thread until exit is called
	*/
	thrd_pool->InitializeThread();

	/*
		Dump telemetry values
	*/

	/*
		Stop threadpool before calling func_stop on separate thread.
	*/
	controll_server->close();
	data_server->close();
	thrd_pool->Stop();
    logs->Log(LRELEASE,"Exiting Process\n");

	runtime_stop(acon, 1);

	delete thrd_pool;
	delete controll_server;
	delete  data_server;
	delete mbedtcp;
	delete logs;
	delete  acon;
	return 0;
}

#endif