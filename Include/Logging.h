#ifndef LOGGING_H
#define LOGGING_H


#include "datatypes.h"
#include "threading/IThreadPool.h"
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>
#include "DiggiAssert.h"
#include <string>
#include "posix/intercept.h"
#define PRINTF_BUFFERSIZE 16384

#ifdef RELEASE

#define DIGGI_TRACE(logObj, Level, str_fmt, ...) /* noop */
#else
#define DIGGI_TRACE(logObj, Level, str_fmt, ...) logObj->Log(Level,str_fmt, ##__VA_ARGS__)
#endif
typedef enum LogLevel {
	LDEBUG = 1,
	LRELEASE
} LogLevel;

class ILog 
{
public:
	ILog() {}
	virtual ~ILog() {}
	virtual std::string GetfuncName() = 0;
	virtual void SetFuncId(aid_t aid, std::string name = "func") = 0;
	virtual void SetLogLevel(LogLevel lvl) = 0;
	virtual void Log(LogLevel lvl, const char *fmt, ...) = 0;
	virtual void Log(const char *fmt, ...) = 0;
};

class StdLogger : public ILog {
	aid_t id;
	std::string func_name;
	IThreadPool *threadpool;
	LogLevel level;
public:
	StdLogger(IThreadPool * threadpool):threadpool(threadpool){};
	std::string GetfuncName();
	void SetFuncId(aid_t aid, std::string name = "func");
	void SetLogLevel(LogLevel lvl);
	void Log(LogLevel lvl, const char *fmt, ...);
	void Log(const char *fmt, ...);
};

#endif