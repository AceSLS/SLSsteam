#pragma once

#include <cstdarg>
#include <fstream>
#include <memory>
#include <shared_mutex>
#include <unordered_set>

#define LOG_IF_EXISTS(fn) if (g_pLog) { fn; }

#define LOG_TRACE(fmt, ...) LOG_IF_EXISTS(g_pLog->trace(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_TRACE_ONCE(fmt, ...) LOG_IF_EXISTS(g_pLog->traceOnce(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

#define LOG_ONCE(fmt, ...) LOG_IF_EXISTS(g_pLog->once(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

#define LOG_DEBUG(fmt, ...) LOG_IF_EXISTS(g_pLog->debug(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_DEBUG_ONCE(fmt, ...) LOG_IF_EXISTS(g_pLog->debugOnce(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

#define LOG_WARN(fmt, ...) LOG_IF_EXISTS(g_pLog->warn(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_ERROR(fmt, ...) LOG_IF_EXISTS(g_pLog->error(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_INFO(fmt, ...) LOG_IF_EXISTS(g_pLog->info(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_NOTIFY(fmt, ...) LOG_IF_EXISTS(g_pLog->notify(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_NOTIFYLONG(fmt, ...) LOG_IF_EXISTS(g_pLog->notifyLong(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_NOTIFYWARN(fmt, ...) LOG_IF_EXISTS(g_pLog->notifyWarn(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_NOTIFYERROR(fmt, ...) LOG_IF_EXISTS(g_pLog->notifyError(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))
#define LOG_API(fmt, ...) LOG_IF_EXISTS(g_pLog->api(__FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__))

#define LOG_CUSTOM(lvl, fmt, ...) g_pLog->custom(lvl, __FILE__, __FUNCTION__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)


typedef unsigned int LogLevelFlags_t;

enum ELogLevel : LogLevelFlags_t
{
	k_ELogLevelTrace = 1 << 0, //Tracing for debug
	k_ELogLevelOnce = 1 << 1, //Only log once
	k_ELogLevelDebug = 1 << 2, //Debugging statements
	k_ELogLevelWarn = 1 << 3, //Something went wrong but it's not terrible
	k_ELogLevelError = 1 << 4, //Something went wrong and it's terrible/can't be recovered from. Function failed
	k_ELogLevelInfo = 1 << 5, //Log for users/external tools
	k_ELogLevelNotifyShort = 1 << 6,
	k_ELogLevelNotifyLong = 1 << 7,

	k_ELogLevelAPI = 1 << 8 //Used by API. Not togglable via config, gets set by API yes/no
};

constexpr unsigned int ELogLevelCount = 9;

std::string ELogLevel_ToString(LogLevelFlags_t lvlFlags);

class CLog
{
	struct Notification_t
	{
		std::string msg;
		int timeOut;
	};

	std::ofstream ofstream;
	std::unordered_set<std::string> msgHist {};
	std::shared_mutex mutex;

	bool shouldNotify(const LogLevelFlags_t flags);
	Notification_t buildNotification(const LogLevelFlags_t flags, const char* msg);
	void __log(const LogLevelFlags_t flags, const char* file, const char* function, const int line, const char* msg, va_list& vArgs);

	//Calls to these will fully disappear from the release build
	void __trace(const char* file, const char* function, const int line, const char* msg, va_list& vArgs);
	void __traceOnce(const char* file, const char* function, const int line, const char* msg, va_list& vArgs);
	void __once(const char* file, const char* function, const int line, const char* msg, va_list& vArgs);
	void __debug(const char* file, const char* function, const int line, const char* msg, va_list& vArgs);
	void __debugOnce(const char* file, const char* function, const int line, const char* msg, va_list& vArgs);

public:
	std::string path;

	CLog(const char* path);
	~CLog();

	//Calls to these will fully disappear from the release build
	__attribute__((__format__(__printf__, 5, 6)))
	void trace(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void traceOnce(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void once(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void debug(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void debugOnce(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void warn(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void error(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void info(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void notify(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void notifyLong(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void notifyWarn(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void notifyError(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 5, 6)))
	void api(const char* file, const char* function, const int line, const char* msg, ...);
	__attribute__((__format__(__printf__, 6, 7)))
	void custom(const unsigned int flags, const char* file, const char* function, const int line, const char* msg, ...);

	static CLog* createDefaultLog();
};

extern std::unique_ptr<CLog> g_pLog;
