#include "api.hpp"

#include "sdk/IClientAppManager.hpp"
#include "sdk/IClientApps.hpp"

#include "config.hpp"
#include "filewatcher.hpp"
#include "utils.hpp"

#include <atomic>
#include <ios>
#include <mutex>
#include <vector>


namespace SLSAPI
{
	struct InstallRequest
	{
		uint32_t appId;
		int32_t library;
		bool requestedAppInfo;
		bool loggedWaitingForAppInfo;
	};

	const char* path = "/tmp/SLSsteam.API";
	std::fstream fstream;
	CFileWatcher* watcher;

	std::atomic_bool hasPendingRequests = false;
	std::mutex pendingRequestMutex;
	std::vector<InstallRequest> pendingInstallRequests;
	thread_local uint32_t currentInstallApp = 0;

	bool appInfoSectionsReady(uint32_t appId, bool logMissing)
	{
		if (!g_pClientApps)
		{
			if (logMissing)
			{
				g_pLog->info("API g_pClientApps is nullptr! Can't check appinfo for %u.\n", appId);
			}
			return false;
		}

		char sectionData[1] = {};
		int32_t commonSize = g_pClientApps->getAppDataSection(appId, APPINFOSECTION_COMMON, sectionData, sizeof(sectionData));
		bool ready = commonSize >= 0;

		if (!ready && logMissing)
		{
			g_pLog->info("API Waiting for appinfo %u common=%i\n", appId, commonSize);
		}

		return ready;
	}
}

uint32_t SLSAPI::currentInstallAppId()
{
	return currentInstallApp;
}

bool SLSAPI::isEnabled()
{
	return g_config.api.get() && fstream.is_open();
}

void SLSAPI::onFileChange()
{
	//Hot reload support :)
	if (!isEnabled())
	{
		return;
	}

	//Shitty way to reopen the stream. We have to do this, otherwise the fstream gets invalidated when running echo >
	fstream.close();
	fstream.open(path);

	char cmd[128];
	fstream.getline(cmd, sizeof(cmd));

	g_pLog->debug("API Running %s\n", cmd);

	auto split = Utils::strsplit(cmd, "|");
	if (split.size() > 2 && strcmp(split[0].c_str(), "install") == 0)
	{
		try
		{
			uint32_t appId = std::strtoul(split[1].c_str(), nullptr, 10);
			int32_t library = std::strtol(split[2].c_str(), nullptr, 10);

			{
				std::lock_guard<std::mutex> lock(pendingRequestMutex);
				pendingInstallRequests.push_back({appId, library, false, false});
			}
			hasPendingRequests.store(true);

			g_pLog->info("API Queued install %s to %s\n", split[1].c_str(), split[2].c_str());
		}
		catch(...)
		{
			g_pLog->info("API Failed to parse %s or %s!\n", split[1].c_str(), split[2].c_str());
		}
	}
}

void SLSAPI::runPendingRequests()
{
	if (!hasPendingRequests.exchange(false))
	{
		return;
	}

	std::vector<InstallRequest> requests;
	{
		std::lock_guard<std::mutex> lock(pendingRequestMutex);
		requests.swap(pendingInstallRequests);
	}

	std::vector<InstallRequest> requeueRequests;
	for (InstallRequest& request : requests)
	{
		if (!g_pClientAppManager)
		{
			g_pLog->info("API g_pClientAppManager is nullptr! Aborting queued install...\n");
			continue;
		}

		if (request.requestedAppInfo && !appInfoSectionsReady(request.appId, !request.loggedWaitingForAppInfo))
		{
			request.loggedWaitingForAppInfo = true;
			requeueRequests.push_back(request);
			continue;
		}

		g_pLog->info("API Installing %u to %i on IClientAppManager::RunIPCFrame\n", request.appId, request.library);
		currentInstallApp = request.appId;
		EAppUpdateError installResult = g_pClientAppManager->installApp(request.appId, request.library);
		currentInstallApp = 0;
		g_pLog->info("API InstallApp(%u, %i) -> %i\n", request.appId, request.library, installResult);

		if (installResult == APP_UPDATE_ERROR_MISSING_CONFIG)
		{
			if (request.requestedAppInfo)
			{
				g_pLog->info("API InstallApp(%u, %i) still missing configuration after appinfo retry.\n", request.appId, request.library);
				continue;
			}

			if (!g_pClientApps)
			{
				g_pLog->info("API g_pClientApps is nullptr! Can't request appinfo for %u.\n", request.appId);
				continue;
			}

			bool requestAppInfoResult = g_pClientApps->requestAppInfoUpdate(request.appId);
			g_pLog->info("API RequestAppInfoUpdate(%u) -> %i after missing configuration\n", request.appId, requestAppInfoResult);

			if (requestAppInfoResult)
			{
				request.requestedAppInfo = true;
				request.loggedWaitingForAppInfo = false;
				requeueRequests.push_back(request);
			}
		}
	}

	if (!requeueRequests.empty())
	{
		std::lock_guard<std::mutex> lock(pendingRequestMutex);
		pendingInstallRequests.insert(pendingInstallRequests.end(), requeueRequests.begin(), requeueRequests.end());
		hasPendingRequests.store(true);
	}
}

void SLSAPI::init()
{
	fstream = std::fstream(path, std::ios::in | std::ios::out);

	watcher = new CFileWatcher(onFileChange);
	watcher->addFile(path);
	watcher->start();

	g_pLog->debug("SLSsteam API initialized!\n");
}
