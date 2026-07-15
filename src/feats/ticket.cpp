#include "ticket.hpp"

#include "fakeappid.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include "../sdk/CNetPacket.hpp"
#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/IClientUser.hpp"
#include "../sdk/IClientUtils.hpp"
#include "../sdk/steam.hpp"

#include "base64/base64.hpp"
#include "yaml-cpp/emitter.h"
#include "yaml-cpp/emittermanip.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <vector>

uint32_t Ticket::oneTimeSteamIdSpoof = 0;
std::map<AppId_t, Ticket::SavedTicket> Ticket::ticketMap = std::map<AppId_t, SavedTicket>();
std::map<AppId_t, Ticket::SavedTicket> Ticket::encryptedTicketMap = std::map<AppId_t, SavedTicket>();

std::string Ticket::getTicketDir()
{
	std::stringstream ss;
	ss << g_config.getDir().c_str() << "/cache";

	const auto dir = ss.str();
	if (!std::filesystem::exists(dir.c_str()))
	{
		std::filesystem::create_directory(dir.c_str());
	}

	return ss.str();
}

std::string Ticket::getTicketPath(const AppId_t appId)
{
	std::stringstream ss;
	ss << getTicketDir().c_str() << "/ticket_" << appId << ".yaml";

	return ss.str();
}

Ticket::SavedTicket Ticket::getCachedTicket(const AppId_t appId)
{
	if (ticketMap.contains(appId))
	{
		return ticketMap[appId];
	}

	SavedTicket ticket {};

	const auto path = getTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return ticket;
	}

	std::ifstream ifs(path, std::ios::in);

	g_pLog->debug("Reading ticket for %u\n", appId);

	auto node = YAML::LoadFile(path);
	ticket.steamId = node["steamId"].as<uint32_t>();
	ticket.ticket = std::string
	(
		base64::from_base64(node["ticket"].as<std::string>())
	);
	//g_pLog->debug("Ticket: %u, %s\n", ticket.steamId, ticket.ticket.c_str());

	ticketMap[appId] = ticket;

	return ticket;
}

bool Ticket::saveTicketToCache(const CMsgClientGetAppOwnershipTicketResponse& resp)
{
	const AppId_t appId = resp.app_id();

	g_pLog->debug("Saving ticket for %u...\n", appId);

	const auto bytes = resp.ticket();

	YAML::Emitter node;
	node << YAML::BeginMap;
	node << YAML::Key << "steamId";
	node << YAML::Value << g_currentSteamId;
	node << YAML::Key << "ticket";
	node << YAML::Value << base64::to_base64(bytes);
	node << YAML::EndMap;

	const auto path = Ticket::getTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(node.c_str(), node.size());

	g_pLog->once("Saved ticket for %u\n", appId);

	//TODO: Skip copy
	SavedTicket ticket {};
	ticket.ticket = bytes;
	ticketMap[appId] = ticket;
	
	return true;
}

void Ticket::launchApp(const AppId_t appId)
{
	auto ticket = getCachedTicket(appId);
	if (!ticket.ticket.size())
	{
		return;
	}

	g_pSteamEngine->getUser(0)->updateAppOwnershipTicket(appId, reinterpret_cast<void*>(ticket.ticket.data()), ticket.ticket.size());
	g_pLog->once("Force loaded AppOwnershipTicket for %i\n", appId);
}

uint32_t Ticket::getTicketOwnershipExtendedData
(
	const AppId_t appId,
	void* ticket,
	const uint32_t ticketSize,
	uint32_t* offAppId,
	uint32_t* offSteamId,
	uint32_t* offSig,
	uint32_t* sigSize
)
{
	const SavedTicket cached = Ticket::getCachedTicket(appId);
	const uint32_t steamId = cached.steamId;
	if (steamId)
	{
		oneTimeSteamIdSpoof = steamId;
		return 0;
	}

	//Exploit from OpenSteam001
	std::vector<char> splicedTicket;
	splicedTicket.resize(ticketSize);

	uint32_t actualSize = g_pClientUser->getAppOwnershipTicketExtendeData
	(
		7,
		splicedTicket.data(),
		splicedTicket.size(),
		offAppId,
		offSteamId,
		offSig,
		sigSize
	);

	g_pLog->debug("Ticket 7 size %u\n", actualSize);

	const uint8_t* appIdBytes = reinterpret_cast<const uint8_t*>(&appId);
	splicedTicket.insert(std::next(splicedTicket.begin(), *offSig), appIdBytes, appIdBytes + sizeof(appId));

	*offAppId = *offSig;
	*offSig = *offAppId + sizeof(appId);

	memcpy(ticket, splicedTicket.data(), splicedTicket.size());
	
	g_pLog->debug("Spliced ticket for %u\n", appId);
	
	return actualSize;
}

std::string Ticket::getEncryptedTicketPath(const AppId_t appId)
{
	std::stringstream ss;
	ss << getTicketDir().c_str() << "/encryptedTicket_" << appId << ".yaml";

	return ss.str();
}

Ticket::SavedTicket Ticket::getCachedEncryptedTicket(const AppId_t appId)
{
	const AppId_t realAppId = FakeAppIds::getRealAppIdForCurrentPipe();
	const AppId_t fakeAppId = FakeAppIds::getFakeAppId(realAppId);

	SavedTicket ticket {};

	if (realAppId && fakeAppId && appId != realAppId)
	{
		g_pLog->once("Returning empty cached encrypted ticket for %u because it's set to %u\n", realAppId, fakeAppId);
		return ticket;
	}

	if (encryptedTicketMap.contains(appId))
	{
		return encryptedTicketMap[appId];
	}

	const auto path = getEncryptedTicketPath(appId);
	if (!std::filesystem::exists(path.c_str()))
	{
		return ticket;
	}

	std::ifstream ifs(path, std::ios::in);

	g_pLog->debug("Reading encrypted ticket for %u\n", appId);

	auto node = YAML::LoadFile(path);
	ticket.steamId = node["steamId"].as<uint32_t>();
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
	//g_pLog->debug("Ticket: %u, %s\n", ticket.steamId, ticket.ticket.c_str());

	encryptedTicketMap[appId] = ticket;

	return ticket;
}

bool Ticket::saveEncryptedTicketToCache(const CMsgClientRequestEncryptedAppTicketResponse& resp)
{
	const AppId_t appId = resp.app_id();

	g_pLog->debug("Saving encrypted ticket for %u...\n", appId);

	auto bytes = resp.SerializeAsString();

	YAML::Emitter node;
	node << YAML::BeginMap;
	node << YAML::Key << "steamId";
	node << YAML::Value << g_currentSteamId;
	node << YAML::Key << "encryptedTicket";
	//node << YAML::Value << YAML::EncodeBase64(reinterpret_cast<const unsigned char*>(bytes.c_str()), bytes.size());
	node << YAML::Value << base64::to_base64(bytes);
	node << YAML::EndMap;

	const auto path = getEncryptedTicketPath(appId);
	std::ofstream ofs(path.c_str(), std::ios::out);

	ofs.write(node.c_str(), node.size());

	g_pLog->once("Saved encrypted ticket for %u\n", appId);

	//TODO: Skip copy
	SavedTicket ticket {};
	ticket.steamId = g_currentSteamId;
	ticket.ticket = bytes;
	encryptedTicketMap[appId] = ticket;
	
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

	const SavedTicket ticket = getCachedEncryptedTicket(msg.app_id());
	if(!ticket.steamId)
	{
		return;
	}

	msg.ParseFromString(ticket.ticket);
	pkt->serialize(msg);

	g_pLog->debug("Using encryptedTicket_%u from disk\n", msg.app_id());
}

void Ticket::recvAppTicket(const CNetPacket* pkt)
{
	const auto msg = pkt->deserializeBody<CMsgClientGetAppOwnershipTicketResponse>();
	if(msg.eresult() == k_EResultOK)
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
