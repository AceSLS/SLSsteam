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
#include <sstream>


std::unordered_map<AppId_t, CSteamId> Ticket::oneTimeSteamIdSpoof = std::unordered_map<AppId_t, CSteamId>();
std::unordered_map<AppId_t, Ticket::SavedTicket> Ticket::ticketMap = std::unordered_map<AppId_t, SavedTicket>();
std::unordered_map<AppId_t, Ticket::SavedTicket> Ticket::encryptedTicketMap = std::unordered_map<AppId_t, SavedTicket>();

std::filesystem::path Ticket::getTicketDir()
{
	auto dir = g_config.getDir();
	dir.append("cache");

	if (!std::filesystem::exists(dir.c_str()))
	{
		std::filesystem::create_directory(dir.c_str());
	}

	return dir;
}

std::filesystem::path Ticket::getTicketPath(const AppId_t appId)
{
	std::ostringstream ss;
	ss << "ticket_" << appId << ".yaml";

	auto dir = getTicketDir();
	dir.append(ss.str());

	return dir;
}

Ticket::SavedTicket* Ticket::getCachedTicket(const AppId_t appId)
{
	if (ticketMap.contains(appId))
	{
		return &ticketMap.at(appId);
	}

	const auto path = getTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return nullptr;
	}

	std::ifstream ifs(path, std::ios::in);

	LOG_DEBUG("Reading ticket for %u\n", appId);

	SavedTicket& ticket = ticketMap[appId];

	auto node = YAML::LoadFile(path);
	ticket.steamId = CSteamId(node["steamId"].as<uint64_t>());
	ticket.ticket = std::string
	(
		base64::from_base64(node["ticket"].as<std::string>())
	);

	return &ticket;
}

bool Ticket::saveTicketToCache(const CMsgClientGetAppOwnershipTicketResponse& resp)
{
	const AppId_t appId = resp.app_id();

	LOG_DEBUG("Saving ticket for %u...\n", appId);

	const auto bytes = resp.ticket();

	YAML::Emitter node;
	node << YAML::BeginMap;
	node << YAML::Key << "steamId";
	node << YAML::Value << g_currentSteamId.steamId64;
	node << YAML::Key << "ticket";
	node << YAML::Value << base64::to_base64(bytes);
	node << YAML::EndMap;

	const auto path = Ticket::getTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(node.c_str(), node.size());

	LOG_ONCE("Saved ticket for %u\n", appId);

	SavedTicket& ticket = ticketMap[appId];
	ticket.steamId = g_currentSteamId;
	ticket.ticket = bytes;
	
	return true;
}

void Ticket::connectPipe(const HSteamPipe pipe)
{
	const auto& proc = g_processMap.at(pipe);

	if (!proc.steamDRM)
	{
		return;
	}

	const SavedTicket* ticket = getCachedTicket(proc.appId);
	if (!ticket)
	{
		return;
	}

	oneTimeSteamIdSpoof[proc.appId] = ticket->steamId;
}

void Ticket::launchApp(const AppId_t appId)
{
	auto ticket = getCachedTicket(appId);
	if (!ticket)
	{
		return;
	}

	//Replacing this call with an injected GetAppOwnershipTicketResponse is possible, but breaks in offline mode so we don't do that
	g_pSteamEngine->getUser(0)->updateAppOwnershipTicket(appId, reinterpret_cast<void*>(ticket->ticket.data()), ticket->ticket.size());
	LOG_ONCE("Force loaded AppOwnershipTicket for %i\n", appId);
}

void Ticket::getEncryptedAppTicket(const AppId_t appId)
{
	if (!(g_config.smartTickets.copy() & CConfig::k_ESmartTicketsDenuvo))
	{
		return;
	}

	const SavedTicket* cached = Ticket::getCachedEncryptedTicket(appId);
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

	const SavedTicket* cached = Ticket::getCachedTicket(appId);
	if (!cached)
	{
		return;
	}

	oneTimeSteamIdSpoof[appId] = cached->steamId;
}

std::filesystem::path Ticket::getEncryptedTicketPath(const AppId_t appId)
{
	std::ostringstream ss;
	ss << "encryptedTicket_" << appId << ".yaml";

	auto dir = getTicketDir();
	dir.append(ss.str());

	return dir;
}

Ticket::SavedTicket* Ticket::getCachedEncryptedTicket(const AppId_t appId)
{
	const AppId_t fakeAppId = FakeAppIds::getFakeAppId(appId);
	const auto smartTickets = g_config.smartTickets.copy();

	if (!(smartTickets & CConfig::k_ESmartTicketsDenuvo) && appId && fakeAppId && fakeAppId != appId)
	{
		LOG_ONCE("Returning empty cached encrypted Ticket for %u because it's running as %u\n", appId, fakeAppId);
		return nullptr;
	}

	if (encryptedTicketMap.contains(appId))
	{
		return &encryptedTicketMap.at(appId);
	}

	const auto path = getEncryptedTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return nullptr;
	}

	std::ifstream ifs(path, std::ios::in);

	LOG_DEBUG("Reading encrypted ticket for %u\n", appId);

	SavedTicket& ticket = encryptedTicketMap[appId];

	auto node = YAML::LoadFile(path);
	ticket.steamId = CSteamId(node["steamId"].as<uint64_t>());
	ticket.ticket = std::string
	(
		//Can not get yaml-cpp to properly decode
		//TODO: Investigate
		//reinterpret_cast<const char*>
		//(
		//	&YAML::DecodeBase64(node["encryptedTicket"].as<std::string>()).at(0)
		//)
		base64::from_base64(node["encryptedTicket"].as<std::string>())
	);

	return &ticket;
}

bool Ticket::saveEncryptedTicketToCache(const CMsgClientRequestEncryptedAppTicketResponse& resp)
{
	const AppId_t appId = resp.app_id();

	LOG_DEBUG("Saving encrypted ticket for %u...\n", appId);

	auto bytes = resp.SerializeAsString();

	YAML::Emitter node;
	node << YAML::BeginMap;
	node << YAML::Key << "steamId";
	node << YAML::Value << g_currentSteamId.steamId64;
	node << YAML::Key << "encryptedTicket";
	//node << YAML::Value << YAML::EncodeBase64(reinterpret_cast<const unsigned char*>(bytes.c_str()), bytes.size());
	node << YAML::Value << base64::to_base64(bytes);
	node << YAML::EndMap;

	const auto path = getEncryptedTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(node.c_str(), node.size());

	LOG_ONCE("Saved encrypted ticket for %u\n", appId);

	SavedTicket& ticket = encryptedTicketMap[appId];
	ticket.steamId = g_currentSteamId;
	ticket.ticket = bytes;
	
	return true;
}

void Ticket::recvEncryptedAppTicket(CNetPacket* pkt)
{
	auto msg = pkt->deserializeBody<CMsgClientRequestEncryptedAppTicketResponse>();

	if (msg.eresult() == k_EResultOK)
	{
		saveEncryptedTicketToCache(msg);
		return;
	}

	const SavedTicket* ticket = getCachedEncryptedTicket(msg.app_id());
	if (!ticket)
	{
		return;
	}

	msg.ParseFromString(ticket->ticket);
	pkt->serialize(msg);

	LOG_DEBUG("Using encryptedTicket_%u from disk\n", msg.app_id());
}

void Ticket::recvAppTicket(const CNetPacket* pkt)
{
	const auto msg = pkt->deserializeBody<CMsgClientGetAppOwnershipTicketResponse>();
	if (msg.eresult() == k_EResultOK)
	{
		saveTicketToCache(msg);
		return;
	}

	//We do not load tickets from disk in the network layer, otherwise they won't be loaded in offline mode
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
