#include "filewatcher.hpp"

#include "log.hpp"

#include <cstring>
#include <filesystem>
#include <linux/limits.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>


void* CFileWatcher::watchLoop(void* args)
{
	constexpr unsigned int BUF_LEN = (10 * (sizeof(struct inotify_event) + NAME_MAX + 1));
	char buf[BUF_LEN] __attribute__ ((aligned(8)));
	char* p;
	inotify_event* event;
	ssize_t size;

	auto watcher = reinterpret_cast<CFileWatcher*>(args);
	LOG_DEBUG("Started FileWatcher %u\n", watcher->notifyFd);

	while (watcher->running)
	{
		size = read(watcher->notifyFd, buf, sizeof(buf));

		if (!size)
		{
			LOG_ERROR("Failed to read from FileWatcher %i! (size = 0)\n", watcher->notifyFd);
			break;
		}

		if (size == -1)
		{
			LOG_ERROR("Failed to read from FileWatcher %i (%s)!\n", watcher->notifyFd, strerror(errno));
			break;
		}

		for (p = buf; p < buf + size; )
		{
			event = reinterpret_cast<inotify_event*>(p);
			p += sizeof(inotify_event) + event->len;

			auto path = watcher->fileFdMap[event->wd];
			const bool isDir = std::filesystem::is_directory(path);
			
			if (!(event->mask & watcher->eventMask))
			{
				continue;
			}

			if (!isDir && strcmp(event->name, path.filename().c_str()) != 0)
			{
				continue;
			}

			LOG_DEBUG("inotify %s(%u) -> %u : %s\n", path.filename().c_str(), event->wd, event->mask, event->len ? event->name : "none");

			if (isDir)
			{
				path.append(event->name);
			}

			watcher->onModify(path, event->mask);
		}
	}

	LOG_DEBUG("Watchthread for %i stopped\n", watcher->notifyFd);
	return nullptr;
}

void CFileWatcher::installSigHandler()
{
	//Purely exists to allow us to cancel the blocking read
	struct sigaction sigAction { };
	sigemptyset(&sigAction.sa_mask);

	sigAction.sa_flags = SA_INTERRUPT;
	sigAction.sa_handler = [](int sig)
	{
		LOG_DEBUG("Caught sig %i\n", sig);
	};

	sigaction(INTERRUPT_SIG, &sigAction, nullptr);
	LOG_DEBUG("Installed sighandler\n");
}

CFileWatcher::CFileWatcher(const FileModifyEvent_t onModify, const int eventMask)
{
	this->onModify = onModify;
	this->eventMask = eventMask;

	notifyFd = inotify_init();
	LOG_DEBUG("Created notify fd %i\n", notifyFd);
}

CFileWatcher::~CFileWatcher()
{
	stop();

	if (notifyFd != -1 && close(notifyFd) == -1)
	{
		LOG_ERROR("Failed to close notifyFd %i!\n", notifyFd);
	}
}

int CFileWatcher::addFile(const std::filesystem::path& path)
{
	//Watching seperate files does not seem to work very well, since the file descriptor becomes useless
	//on some operations
	
	for (const auto& fd : fileFdMap)
	{
		if (fd.second == path)
		{
			return 0;
		}
	}

	int fd;

	if (std::filesystem::is_directory(path))
	{
		fd = inotify_add_watch(notifyFd, path.c_str(), eventMask);
		LOG_DEBUG("Adding %s to FileWatcher %i\n", path.filename().c_str(), notifyFd);
	}
	else
	{
		fd = inotify_add_watch(notifyFd, path.parent_path().c_str(), eventMask);
		LOG_DEBUG("Adding %s with file %s to FileWatcher %i\n", path.parent_path().filename().c_str(), path.filename().c_str(), notifyFd);
	}

	if (fd == -1)
	{
		LOG_ERROR("Failed to watch %s!\n", path.filename().c_str());
		return fd;
	}

	fileFdMap[fd] = path;
	return fd;
}

bool CFileWatcher::removeFile(const std::filesystem::path& path)
{
	for (const auto& fd : fileFdMap)
	{
		if (fd.second == path)
		{
			inotify_rm_watch(fd.first, notifyFd);
			return true;
		}
	}

	LOG_WARN("Tried to remove non-existent inotify wathc for %s!\n", path.filename().c_str());
	return false;
}

bool CFileWatcher::start()
{
	try
	{
		running = true;
		watchThread = std::thread(&watchLoop, this);
	}
	catch (...)
	{
		running = false;
		LOG_ERROR("Failed to start watchThread!\n");
	}

	return running;
}

void CFileWatcher::stop()
{
	if (!running)
	{
		return;
	}

	running = false;
	pthread_kill(watchThread.native_handle(), INTERRUPT_SIG);

	try
	{
		watchThread.join();
	}
	catch (...)
	{
	}
}
