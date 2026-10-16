#include "ticket.hpp"

#include "../config.hpp"
#include "../globals.hpp"
#include "../process.hpp"

#include "fakeappid.hpp"

#include "base64/base64.hpp"
#include "yaml-cpp/emitter.h"
#include "yaml-cpp/emittermanip.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <regex>
#include <sstream>


std::unique_ptr<CFileWatcher> Ticket::fileWatcher = nullptr;

void Ticket::onFileChange(const std::filesystem::path& path, __attribute__((unused)) int eventMask)
{
	const auto fileName = path.filename().string();

	if (!fileName.starts_with(getTicketField(ETicketType::AppOwnershipTicket)) && !fileName.starts_with(getTicketField(ETicketType::EncryptedAppOwnershipTicket)))
	{
		return;
	}

	const auto re = std::regex("[0-9]+");
	std::smatch smatch;

	if (!std::regex_search(fileName, smatch, re))
	{
		LOG_ERROR("File %s reloaded, yet no matching appId was found!\n", path.c_str());
		return;
	}

	const AppId_t appId = std::stoull(smatch.str());
	const auto guard = std::lock_guard(ticketMapMutex);

	LOG_DEBUG("%s reloaded, appId %u\n", path.c_str(), appId);

	if (ticketMap.contains(appId))
	{
		ticketMap.erase(appId);
		LOG_DEBUG("Erased %u from ticketMap!\n", appId);
	}

	if (encryptedTicketMap.contains(appId))
	{
		encryptedTicketMap.erase(appId);
		LOG_DEBUG("Erased %u from encryptedTicketMap!\n", appId);
	}
}

std::unordered_map<AppId_t, CSteamId> Ticket::oneTimeSteamIdSpoof = std::unordered_map<AppId_t, CSteamId>();

std::mutex Ticket::ticketMapMutex;
std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>> Ticket::ticketMap = std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>>();
std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>> Ticket::encryptedTicketMap = std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>>();

std::filesystem::path Ticket::getTicketDir()
{
	auto dir = g_config.getDir();
	dir.append("cache");

	if (!std::filesystem::exists(dir.c_str()))
	{
		std::filesystem::create_directory(dir.c_str());
	}

	if (!fileWatcher)
	{
		fileWatcher = std::make_unique<CFileWatcher>(onFileChange, WATCH_MASK);
		
		if (fileWatcher->addFile(dir) == -1)
		{
			LOG_NOTIFYERROR("Failed to watch ticket directory! Hot reloading will be disabled\n");
		}
		else
		{
			fileWatcher->start();
		}
	}

	return dir;
}

std::filesystem::path Ticket::getTicketPath(const AppId_t appId, const ETicketType type)
{
	std::ostringstream ss;

	const char* ticketField = getTicketField(type);
	ss << ticketField << "_" << appId << ".yaml";

	auto dir = getTicketDir();
	dir.append(ss.str());

	return dir;
}

std::shared_ptr<Ticket::SavedTicket> Ticket::getCachedTicket(const AppId_t appId, const ETicketType type)
{
	auto& cached = getTicketMap(type);
	const char* ticketField = getTicketField(type);

	//TODO: Use shared_mutex, lock shared for read, lock unique for write
	const auto guard = std::lock_guard(ticketMapMutex);

	if (cached.contains(appId))
	{
		return cached.at(appId);
	}

	const auto path = getTicketPath(appId, type);
	if (!std::filesystem::exists(path.c_str()))
	{
		return nullptr;
	}

	std::ifstream ifs(path, std::ios::in);

	LOG_DEBUG("Reading %s for %u\n", ticketField, appId);

	auto node = YAML::LoadFile(path);

	auto ticket = std::make_shared<SavedTicket>();
	ticket->steamId = CSteamId(node["steamId"].as<uint64_t>());
	ticket->ticket = std::string
	(
		base64::from_base64(node[ticketField].as<std::string>())
	);

	cached[appId] = ticket;

	return ticket;
}

bool Ticket::saveTicketToCache(const AppId_t appId, const std::string& bytes, const CSteamId& owner, const ETicketType type)
{
	const char* ticketField = getTicketField(type);

	LOG_DEBUG("Saving %s for %u...\n", ticketField, appId);

	YAML::Emitter node;
	node << YAML::BeginMap;
	node << YAML::Key << "steamId";
	node << YAML::Value << owner.steamId64;
	node << YAML::Key << ticketField;
	node << YAML::Value << base64::to_base64(bytes);
	node << YAML::EndMap;

	const auto guard = std::lock_guard(ticketMapMutex);

	const auto path = Ticket::getTicketPath(appId, type);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(node.c_str(), node.size());

	LOG_DEBUG("Saved %s for %u\n", ticketField, appId);

	//Do not load ticket into ticketMap
	//Because the filewatcher is slower than we
	//it'll just get deleted right away
	
	return true;
}

void Ticket::connectPipe(const HSteamPipe pipe)
{
	const auto& proc = g_processMap.at(pipe);

	if (!proc.steamDRM)
	{
		return;
	}

	const auto ticket = getCachedTicket(proc.appId, ETicketType::AppOwnershipTicket);
	if (!ticket)
	{
		return;
	}

	oneTimeSteamIdSpoof[proc.appId] = ticket->steamId;
}

void Ticket::getEncryptedAppTicket(const AppId_t appId)
{
	if (!(g_config.smartTickets.copy() & CConfig::k_ESmartTicketsDenuvo))
	{
		return;
	}

	const auto cached = Ticket::getCachedTicket(appId, ETicketType::EncryptedAppOwnershipTicket);
	if (!cached)
	{
		return;
	}

	oneTimeSteamIdSpoof[appId] = cached->steamId;
}

void Ticket::getTicketOwnershipExtendedData(const AppId_t appId)
{
	const auto utils = g_pSteamEngine->getUtils();

	if (g_config.smartTickets.copy() & CConfig::k_ESmartTicketsSteamDRM)
	{
		const auto& proc = g_processMap.at(utils->getCurrentSteamPipe());
		if (proc.steamDRM)
		{
			//Handled in connectPipe
			//For other ticket requests we fall through to spoofing the next GetSteamID call
			return;
		}
	}

	const auto cached = Ticket::getCachedTicket(appId, ETicketType::AppOwnershipTicket);
	if (!cached)
	{
		return;
	}

	oneTimeSteamIdSpoof[appId] = cached->steamId;
}

void Ticket::launchApp(const GameId_t gameId)
{
	if (gameId & GAME_TYPE_SHORTCUT)
	{
		return;
	}

	auto ticket = getCachedTicket(gameId, ETicketType::AppOwnershipTicket);
	if (!ticket)
	{
		return;
	}

	//Replacing this call with an injected GetAppOwnershipTicketResponse is possible, but breaks in offline mode so we don't do that
	g_pSteamEngine->getUser(0)->updateAppOwnershipTicket(gameId, reinterpret_cast<void*>(ticket->ticket.data()), ticket->ticket.size());
	LOG_DEBUG("Force loaded AppOwnershipTicket for %lu\n", gameId);
}

void Ticket::recvAppTicket(const CNetPacket* pkt)
{
	const auto msg = pkt->deserializeBody<CMsgClientGetAppOwnershipTicketResponse>();
	if (msg.eresult() == k_EResultOK)
	{
		saveTicketToCache(msg.app_id(), msg.ticket(), g_currentSteamId, ETicketType::AppOwnershipTicket);
		return;
	}

	//We do not load tickets from disk in the network layer, otherwise they won't be loaded in offline mode
}

void Ticket::recvEncryptedAppTicket(CNetPacket* pkt)
{
	auto msg = pkt->deserializeBody<CMsgClientRequestEncryptedAppTicketResponse>();

	if (msg.eresult() == k_EResultOK)
	{
		saveTicketToCache(msg.app_id(), msg.SerializeAsString(), g_currentSteamId, ETicketType::EncryptedAppOwnershipTicket);
		return;
	}

	const auto ticket = getCachedTicket(msg.app_id(), ETicketType::EncryptedAppOwnershipTicket);
	if (!ticket)
	{
		return;
	}

	msg.ParseFromString(ticket->ticket);
	pkt->serialize(msg);

	LOG_DEBUG("Using encryptedTicket_%u from disk\n", msg.app_id());
}

void Ticket::recvMsg(CNetPacket* pkt)
{
	switch(pkt->getProtoBufType())
	{
		case k_EMsgClientGetAppOwnershipTicketResponse:
			recvAppTicket(pkt);
			break;

		case k_EMsgClientRequestEncryptedAppTicketResponse:
			recvEncryptedAppTicket(pkt);
			break;

		default:
			break;
	}
}
