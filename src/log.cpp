#include "log.hpp"

#include "config.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <sstream>


std::string ELogLevel_ToString(const LogLevelFlags_t lvlFlags)
{
	constexpr static auto flagToString = [](const unsigned int flag)
	{
		switch(flag)
		{
			case k_ELogLevelTrace:
				return "Trace";
			case k_ELogLevelOnce:
				return "Once";
			case k_ELogLevelDebug:
				return "Debug";
			case k_ELogLevelWarn:
				return "Warn";
			case k_ELogLevelError:
				return "Error";
			case k_ELogLevelInfo:
				return "Info";
			case k_ELogLevelNotifyShort:
				return "Notify";
			case k_ELogLevelNotifyLong:
				return "Notify Long";
			case k_ELogLevelAPI:
				return "API";

			default:
				return "Unknown";
		}
	};

	std::ostringstream lvlStr;

	for (int i = ELogLevelCount - 1; i >= 0; i--)
	{
		const unsigned int flag = 1 << i;

		if (lvlFlags & flag)
		{
			if (lvlStr.str().size() > 0)
			{
				lvlStr << "|";
			}

			lvlStr << flagToString(flag);
		}
	}

	return lvlStr.str();
}

bool CLog::shouldNotify(const LogLevelFlags_t flags)
{
	const unsigned int configLevels = g_config.logLevels.copy();
	if ((flags & k_ELogLevelNotifyLong) && (configLevels & k_ELogLevelNotifyLong))
	{
		return true;
	}

	if ((flags & k_ELogLevelNotifyShort) && (configLevels & k_ELogLevelNotifyShort))
	{
		return true;
	}

	return false;
}

std::string CLog::buildNotification(const LogLevelFlags_t flags, const char* msg)
{
	const bool notifyShort = flags & k_ELogLevelNotifyShort;
	const bool notifyLong = flags & k_ELogLevelNotifyLong;

	if (!notifyShort && !notifyLong)
	{
		return "";
	}

	const bool warn = flags & k_ELogLevelWarn;
	const bool error = flags & k_ELogLevelError;
	std::ostringstream notifySS;

	notifySS << "notify-send ";

	if (flags & k_ELogLevelNotifyLong)
	{
		notifySS << "-t 30000";
	}
	else if (flags & k_ELogLevelNotifyShort)
	{
		notifySS << "-t 10000";
	}

	notifySS << " -u ";

	if (error)
	{
		notifySS << "\"critical\" \"SLSsteam\" \"Error:\n";
	}
	else if (warn)
	{
		notifySS << "\"normal\" \"SLSsteam\" \"Warning:\n";
	}
	else
	{
		notifySS << "\"normal\" \"SLSsteam\" \"";
	}

	//Leading quote is added by error/warn/none if statement above
	notifySS << msg << "\"";

	return notifySS.str();
}

void CLog::__log(const LogLevelFlags_t flags, const char* file, const char* function, const int line, const char* msg, va_list& vArgs)
{
	if (!(g_config.logLevels.copy() & flags))
	{
		return;
	}

	va_list vArgsCpy;
	va_copy(vArgsCpy, vArgs);
	const size_t size = vsnprintf(nullptr, 0, msg, vArgsCpy) + 1; //Allocate one more byte for zero termination
	va_end(vArgsCpy);

	std::string formatted;
	formatted.resize(size);

	va_copy(vArgsCpy, vArgs);
	vsnprintf(formatted.data(), size, msg, vArgsCpy);
	va_end(vArgsCpy);

	//Notifications do not get a newline ending, so we add our own
	//Use built string to check further down
	const auto notification = buildNotification(flags, formatted.c_str());

	if (shouldNotify(flags) && notification.size() > 0)
	{
		//TODO: Fix
		//system(notification.c_str());
		debug(file, function, line, "system(\"%s\")\n", notification.c_str());
	}

	std::ostringstream prefixSS;

	if (file && function)
	{
		prefixSS << "[" << ELogLevel_ToString(flags) << " in " << file << ":" << function << ":" << line << "]";
	}
	else
	{
		prefixSS << "[" << ELogLevel_ToString(flags) << "]";
	}

	const auto prefix = prefixSS.str();

	const auto lock = std::lock_guard(mutex);

	//Prevent crashes from queued operations
	if (!ofstream.is_open())
	{
		return;
	}

	if (flags & k_ELogLevelOnce)
	{
		for (const auto& oldMsg : msgHist)
		{
			if (oldMsg == formatted)
			{
				return;
			}
		}

		msgHist.emplace(formatted);
	}

	//ofstream << prefix << std::setfill(' ') << std::setw(80 - prefix.size()) << " " << formatted.c_str();
	//Padding makes things nicer, but basically unreadable
	ofstream << prefix << " " << formatted.c_str();

	if (notification.size() > 0)
	{
		ofstream << "\n";
	}

	ofstream.flush();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void CLog::__trace(const char* file, const char* function, const int line, const char* msg, va_list& vArgs)
{
#ifdef TRACE
	__log(k_ELogLevelTrace, file, function, line, msg, vArgs);
#endif
}
void CLog::__traceOnce(const char* file, const char* function, const int line, const char* msg, va_list& vArgs)
{
#ifdef TRACE
	__log(k_ELogLevelTrace | k_ELogLevelOnce, file, function, line, msg, vArgs);
#endif
}

void CLog::__once(const char* file, const char* function, const int line, const char* msg, va_list& vArgs)
{
#ifdef DEBUG
	__log(k_ELogLevelOnce, file, function, line, msg, vArgs);
#endif
}
void CLog::__debug(const char* file, const char* function, const int line, const char* msg, va_list& vArgs)
{
#ifdef DEBUG
	__log(k_ELogLevelDebug, file, function, line, msg, vArgs);
#endif
}
void CLog::__debugOnce(const char* file, const char* function, const int line, const char* msg, va_list& vArgs)
{
#ifdef DEBUG
	__log(k_ELogLevelDebug | k_ELogLevelOnce, file, function, line, msg, vArgs);
#endif
}
#pragma GCC diagnostic pop

CLog::CLog(const char* path) : path(path)
{
	ofstream = std::ofstream(path);
	if (!ofstream.is_open())
	{
		//We don't want to boot without a logfile
		throw std::runtime_error("Unable to open logfile!");
	}
}

CLog::~CLog()
{
	if (ofstream.is_open())
	{
		ofstream.close();
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void CLog::trace(const char* file, const char* function, const int line, const char* msg, ...)
{
#ifdef TRACE
	va_list vArgs;
	va_start(vArgs, msg);
	__trace(file, function, line, msg, vArgs);
	va_end(vArgs);
#endif
}
void CLog::traceOnce(const char* file, const char* function, const int line, const char* msg, ...)
{
#ifdef TRACE
	va_list vArgs;
	va_start(vArgs, msg);
	__traceOnce(file, function, line, msg, vArgs);
	va_end(vArgs);
#endif
}

void CLog::once(const char* file, const char* function, const int line, const char* msg, ...)
{
#ifdef DEBUG
	va_list vArgs;
	va_start(vArgs, msg);
	__once(file, function, line, msg, vArgs);
	va_end(vArgs);
#endif
}
void CLog::debug(const char* file, const char* function, const int line, const char* msg, ...)
{
#ifdef DEBUG
	va_list vArgs;
	va_start(vArgs, msg);
	__debug(file, function, line, msg, vArgs);
	va_end(vArgs);
#endif
}
void CLog::debugOnce(const char* file, const char* function, const int line, const char* msg, ...)
{
#ifdef DEBUG
	va_list vArgs;
	va_start(vArgs, msg);
	__debugOnce(file, function, line, msg, vArgs);
	va_end(vArgs);
#endif
}
#pragma GCC diagnostic pop

void CLog::warn(const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);
	__log(k_ELogLevelWarn, file, function, line, msg, vArgs);
	va_end(vArgs);
}
void CLog::error(const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);
	__log(k_ELogLevelError, file, function, line, msg, vArgs);
	va_end(vArgs);
}
void CLog::info(const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);
	__log(k_ELogLevelInfo, file, function, line, msg, vArgs);
	va_end(vArgs);
}
void CLog::notify(const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);
	__log(k_ELogLevelNotifyShort | k_ELogLevelInfo, file, function, line, msg, vArgs);
	va_end(vArgs);
}
void CLog::notifyLong(const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);
	__log(k_ELogLevelNotifyLong | k_ELogLevelInfo, file, function, line, msg, vArgs);
	va_end(vArgs);
}
void CLog::notifyWarn(const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);
	__log(k_ELogLevelNotifyLong | k_ELogLevelWarn, file, function, line, msg, vArgs);
	va_end(vArgs);
}
void CLog::notifyError(const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);
	//Notify type doesn't really matter, since critical stays until clicked
	__log(k_ELogLevelNotifyLong | k_ELogLevelError, file, function, line, msg, vArgs);
	va_end(vArgs);
}
void CLog::api(const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);
	__log(k_ELogLevelAPI, file, function, line, msg, vArgs);
	va_end(vArgs);
}
void CLog::custom(const unsigned int flags, const char* file, const char* function, const int line, const char* msg, ...)
{
	va_list vArgs;
	va_start(vArgs, msg);

	//Obviously these calls will not get fully optimized out
	if (flags == k_ELogLevelTrace)
	{
		__trace(file, function, line, msg, vArgs);
	}
	else if (flags == (k_ELogLevelTrace | k_ELogLevelOnce))
	{
		__traceOnce(file, function, line, msg, vArgs);
	}
	if (flags == k_ELogLevelOnce)
	{
		__once(file, function, line, msg, vArgs);
	}
	else if (flags == k_ELogLevelDebug)
	{
		__debug(file, function, line, msg, vArgs);
	}
	else if (flags == (k_ELogLevelDebug | k_ELogLevelOnce))
	{
		__debugOnce(file, function, line, msg, vArgs);
	}
	else
	{
		__log(flags, file, function, line, msg, vArgs);
	}

	va_end(vArgs);
}

CLog* CLog::createDefaultLog()
{
	const char* home = getenv("HOME");
	if (home)
	{
		std::ostringstream ss;
		ss << home << "/.SLSsteam.log";

		return new CLog(ss.str().c_str());
	}

	return nullptr;
}

std::unique_ptr<CLog> g_pLog;
