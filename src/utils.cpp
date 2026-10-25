#include "utils.hpp"

#include "log.hpp"

#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <gnu/libc-version.h>
#include <regex>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <dbus/dbus.h>


double Utils::calculateEntropy(const std::vector<uint8_t>& bytes)
{
	auto countMap = std::unordered_map<uint8_t, size_t>();
	for (const auto& byte : bytes)
	{
		countMap[byte]++;
	}

	double val = 0.0;
	for (const auto& count : countMap)
	{
		double freq = static_cast<double>(count.second) / bytes.size();
		val += freq * log2(freq);
	}

	return -val;
}

int Utils::exec(const std::string& executable, const std::vector<std::string>& args, std::string* stdOut)
{
	return exec(std::vector<std::string> { executable }, args, stdOut);
}

int Utils::exec(const std::vector<std::string>& executableList, const std::vector<std::string>& args, std::string* stdOut)
{
	int pipefd[2];

	if (pipe(pipefd) == -1)
	{
		LOG_ERROR("Failed to create pipe!\n");
		return 1;
	}

	LOG_DEBUG("Created pipe %i : %i\n", pipefd[0], pipefd[1]);

	constexpr static const char* env[] =
	{
		"PATH='/usr/bin:/bin'",
		nullptr
	};

	std::vector<const char*> ppChArgs;
	const auto exeName = std::filesystem::path(executableList.at(0)).filename();

	//First argv should be name of executable
	ppChArgs.emplace_back(exeName.c_str());

	for (const auto& arg : args)
	{
		ppChArgs.emplace_back(arg.c_str());
	}
	ppChArgs.emplace_back(nullptr);

	const pid_t pid = fork();
	if (pid == -1)
	{
		close(pipefd[0]);
		close(pipefd[1]);

		LOG_ERROR("Failed to fork!\n");
		return 1;
	}

	if (pid == 0)
	{
		if (dup2(pipefd[1], STDOUT_FILENO) == -1)
		{
			LOG_ERROR("Failed to dup2!\n");
			exit(1);
		}

		//No need for reading
		close(pipefd[0]);
		close(pipefd[1]);

		for (const auto& exePath : executableList)
		{
			execve(exePath.c_str(), const_cast<char**>(ppChArgs.data()), const_cast<char**>(env));
			LOG_DEBUG("Failed to execv %s!\n", exePath.c_str());
		}

		exit(1);
	}

	//No need for writing
	close(pipefd[1]);

	LOG_DEBUG("Child PID %i\n", pid);

	std::ostringstream bufSS;
	char buf[8192];
	int numRead;

	while((numRead = read(pipefd[0], buf, sizeof(buf))) > 0)
	{
		bufSS << std::string(buf, numRead);
	}

	close(pipefd[0]);

	int status;
	if (waitpid(pid, &status, 0) == -1)
	{
		return 1;
	}

	if (!WIFEXITED(status))
	{
		return 1;
	}

	status = WEXITSTATUS(status);

	LOG_DEBUG("Exit Status: %i\n", status);

	if (stdOut)
	{
		*stdOut = bufSS.str();
	}

	return status;
}

bool Utils::isNumber(const char* str)
{
	const unsigned int len = strlen(str);
	if (len < 1)
	{
		return false;
	}

	for (unsigned int i = 0; i < len; i++)
	{
		const char c = str[i];

		if (!std::isdigit(c))
		{
			return false;
		}
	}

	return true;
}

bool Utils::isGLIBCBugged(int* pMajorVer, int* pMinorVer)
{
	static int major = 0;
	static int minor = 0;

	if (!major)
	{
		const char* ver = gnu_get_libc_version();
		const auto split = strsplit(ver, "\\.");

		major = std::stoi(split[0]);
		minor = std::stoi(split[1]);

		LOG_DEBUG("Using glibc %i.%i\n", major, minor);
	}

	if (pMajorVer)
	{
		*pMajorVer = major;
	}
	if (pMinorVer)
	{
		*pMinorVer = minor;
	}

	return major <= 2 && minor <= 42;
}

std::string Utils::getFileSHA256(const std::filesystem::path& path)
{
	//steam's mount namespace includes sha256sum, so we can safely run it
	std::string shaOutput;
	const auto exitCode = exec("/usr/bin/sha256sum", { path }, &shaOutput);

	if (exitCode != 0)
	{
		LOG_ERROR("Failed calculating %s sha256sum!\n", path.filename().c_str());
		return "";
	}

	const auto split = strsplit(shaOutput, " ");
	return split[0];
}

bool Utils::notify(const char* title, const char* text, const int timeOut)
{
	constexpr const char *app_name = "SLSsteam";
	constexpr const char *app_icon = "";

	DBusError error;
	DBusMessage* msg = nullptr;
	dbus_uint32_t replaces_id = 0;
	DBusMessageIter args, array;
	bool success = false;

	dbus_error_init(&error);
	DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &error);

	if (dbus_error_is_set(&error))
	{
		LOG_CUSTOM(k_ELogLevelError | k_ELogLevelOnce, "DBus connection error: %s\n", error.message);
		goto failed;
	}

	msg = dbus_message_new_method_call
	(
		"org.freedesktop.Notifications",
		"/org/freedesktop/Notifications",
		"org.freedesktop.Notifications",
		"Notify"
	);

	if (!msg)
	{
		LOG_ERROR("Failed to create new dbus method call!\n");
		goto failed;
	}

	//TODO: Add support for urgency

	dbus_message_iter_init_append(msg, &args);

	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_name);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &replaces_id);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_icon);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title);
	dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &text);

	dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &array);
	dbus_message_iter_close_container(&args, &array);

	dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &array);
	dbus_message_iter_close_container(&args, &array);

	dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &timeOut);

	if (!dbus_connection_send(conn, msg, nullptr))
	{
		LOG_CUSTOM(k_ELogLevelError | k_ELogLevelOnce, "Failed to send msg to dbus connection!\n");
		goto failed;
	}

	dbus_connection_flush(conn);
	success = true;

failed:
	if (conn)
	{
		dbus_connection_unref(conn);
	}

	if (msg)
	{
		dbus_message_unref(msg);
	}

	dbus_error_free(&error);
	return success;
}

std::vector<std::string> Utils::strsplit(const std::string& str, const std::string& delimeter)
{
	auto splits = std::vector<std::string>();

	std::regex delimeterRe(delimeter);
	auto it = std::sregex_token_iterator(str.begin(), str.end(), delimeterRe, -1);
	std::sregex_token_iterator end;

	while (it != end)
	{
		splits.emplace_back(*it);
		it++;
	}

	return splits;
}
