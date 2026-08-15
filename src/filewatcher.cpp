#include "filewatcher.hpp"

#include "log.hpp"

#include <cstring>
#include <filesystem>
#include <linux/limits.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>


//TODO: Investigate why gcc complains when put into CFileWatcher itself
void* watchLoop(void* args)
{
	constexpr unsigned int BUF_LEN = (10 * (sizeof(struct inotify_event) + NAME_MAX + 1));
	char buf[BUF_LEN] __attribute__ ((aligned(8)));
	char* p;
	inotify_event* event;
	ssize_t size;

	auto watcher = reinterpret_cast<CFileWatcher*>(args);
	LOG_DEBUG("Started FileWatcher %u\n", watcher->notifyFd);

	for (;;)
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
			
			//Editors and tools such as `sed -i` commonly save by renaming a
			//temporary file over the original. Since we watch the parent directory,
			//handle both direct writes and those atomic replacement saves.
			if (!(event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)))
			{
				continue;
			}

			if (strcmp(event->name, path.filename().c_str()) != 0)
			{
				continue;
			}

			LOG_DEBUG("inotify %s(%u) -> %u : %s\n", path.filename().c_str(), event->wd, event->mask, event->len ? event->name : "none");

			watcher->onModify();
		}
	}

	return nullptr;
}

CFileWatcher::CFileWatcher(const FileModifyEvent_t onModify)
{
	this->onModify = onModify;

	notifyFd = inotify_init();
	LOG_DEBUG("Created notify fd %i\n", notifyFd);
}

CFileWatcher::~CFileWatcher()
{
	if (watchThread)
	{
		stop();
	}

	if (notifyFd != -1)
	{
		close(notifyFd);

		for (const auto& fd : fileFdMap)
		{
			if (fd.first == -1)
			{
				continue;
			}

			close(fd.first);
		}
	}
}

int CFileWatcher::addFile(const char* path)
{
	//Watching seperate files does not seem to work very well, since the file descriptor becomes useless
	//on some operations
	const std::filesystem::path p(path);
	int fd = inotify_add_watch
	(
		notifyFd,
		p.parent_path().c_str(),
		IN_CLOSE_WRITE | IN_MOVED_TO
	);
	if (fd == -1)
	{
		return fd;
	}

	fileFdMap[fd] = p;
	LOG_DEBUG("Added %s with file %s to FileWatcher %i\n", p.parent_path().c_str(), p.filename().c_str(), notifyFd);
	return fd;
}

bool CFileWatcher::start()
{
	int code = pthread_create(&watchThread, nullptr, &watchLoop, this);
	return code == 0;
}

void CFileWatcher::stop()
{
	pthread_cancel(watchThread);
}
