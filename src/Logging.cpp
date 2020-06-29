/**
 * @file logging.cpp
 * @author Anders Gjerdrum (anders.gjerdrum@uit.no)
 * @brief Implementation of diggi logging api.
 * Current implementation only usable for debugging.
 * @warning logging to standard out in production may reveal enclave secrets!!
 * Pipes all logs to standard out file descriptor of host process.
 * Enclave logging explicitly exists enclave through ocall before writing to standard out.
 * TODO: implement an encrypted log server.
 * @see ocalls.cpp
 * @version 0.1
 * @date 2020-02-05
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "Logging.h"

/**
 * @brief get the current diggi instance human readable name. 
 * For attributing logging lines to a particular instance.
 * 
 * @return std::string 
 */
std::string StdLogger::GetfuncName()
{
	return func_name;
}
/**
 * @brief set instance identity
 * Both human readable name and numeric identifier.
 * 
 * @param aid 
 * @param name 
 */
void StdLogger::SetFuncId(aid_t aid, std::string name) {
	id = aid;
	func_name = name;
}

/**
 * @brief set severity of logging level.
 * Lower severity will cause more loggs.
 * @param lvl 
 */
void StdLogger::SetLogLevel(LogLevel lvl) {
	level = lvl;
	/*
		NOOP
	*/
}
/**
 * @brief Log a formatable string with a given log level
 * 
 * @param lvl log as a given severity level
 * @param fmt formattable string
 * @param ... parameters
 */
void StdLogger::Log(LogLevel lvl, const char * fmt, ...) {
	
	if (lvl < level) {
		return;
	}

	char buf[PRINTF_BUFFERSIZE] = { '\0' };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, PRINTF_BUFFERSIZE, fmt, ap);
	va_end(ap);

#if defined(DIGGI_ENCLAVE)

	DIGGI_ASSERT(id.fields.type == ENCLAVE);
	ocall_print_string_diggi(func_name.c_str(), buf, threadpool->currentThreadId(), id.raw);
#else

	DIGGI_ASSERT(id.fields.type != ENCLAVE);
	if (id.fields.type == PROC) {
		printf("[func-R: %s\t (%" PRIu64 "), Thread: %d] -: %s", func_name.c_str() ,id.raw, threadpool->currentThreadId(), buf);
	}
	else if (id.fields.type == LIB) {
		printf("[func-U: %s\t (%" PRIu64 "), Thread: %d] -: %s", func_name.c_str(), id.raw, threadpool->currentThreadId(), buf);

	}
	else {
		DIGGI_ASSERT(false);
	}
	#ifdef TEST_DEBUG
	__real_fflush(stdout);
	#endif

#endif
}
/**
 * @brief log a formatable string as the default set loglevel for Logger class
 * 
 * @param fmt formattable string
 * @param ... parameters
 */
void StdLogger::Log(const char * fmt, ...) {

	if (level > LogLevel::LDEBUG) {
		return;
	}

	char buf[PRINTF_BUFFERSIZE] = { '\0' };
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, PRINTF_BUFFERSIZE, fmt, ap);
	va_end(ap);

#if defined(DIGGI_ENCLAVE)

	DIGGI_ASSERT(id.fields.type == ENCLAVE);
	ocall_print_string_diggi(func_name.c_str(), buf, threadpool->currentThreadId(), id.raw);
#else

	DIGGI_ASSERT(id.fields.type != ENCLAVE);
	if(id.fields.type == PROC){
		printf("[func-R: %s\t (%" PRIu64 "), Thread: %d] -: %s", func_name.c_str(), id.raw, threadpool->currentThreadId(), buf);
	}
	else if (id.fields.type == LIB) {
		printf("[func-U: %s\t (%" PRIu64 "), Thread: %d] -: %s", func_name.c_str(), id.raw, threadpool->currentThreadId(), buf);
	}
	else {
		DIGGI_ASSERT(false);
	}
	#ifdef TEST_DEBUG
	__real_fflush(stdout);
	#endif
#endif

}
