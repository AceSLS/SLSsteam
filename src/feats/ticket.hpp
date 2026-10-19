#pragma once

#include "../sdk/sdk.hpp"

#include "../filewatcher.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>


namespace Ticket
{
	constexpr int WATCH_MASK = IN_MODIFY | IN_MOVED_TO | IN_MOVED_FROM | IN_DELETE | IN_CREATE;
	extern std::unique_ptr<CFileWatcher> fileWatcher;
	void onFileChange(const std::filesystem::path& path, int eventMask);

	enum class ETicketType
	{
		AppOwnershipTicket,
		EncryptedAppOwnershipTicket
	};

	class SavedTicket
	{
public:
		CSteamId steamId;
		std::string ticket;

		constexpr bool isValid() const
		{
			return steamId.isSet() && ticket.size() > 0;
		}
	};

	extern std::unordered_map<AppId_t, CSteamId> oneTimeSteamIdSpoof;

	extern std::mutex ticketMapMutex;
	extern std::unordered_map<AppId_t, std::shared_ptr<SavedTicket>> ticketMap;
	extern std::unordered_map<AppId_t, std::shared_ptr<SavedTicket>> encryptedTicketMap;

	constexpr const char* getTicketField(const ETicketType type)
	{
		switch (type)
		{
			case ETicketType::AppOwnershipTicket:
				return "ticket";
			case ETicketType::EncryptedAppOwnershipTicket:
				return "encryptedTicket";
		}

		throw std::runtime_error("Unknown ticket type!");
	}

	constexpr std::unordered_map<AppId_t, std::shared_ptr<Ticket::SavedTicket>>& getTicketMap(const ETicketType type)
	{
		switch (type)
		{
			case ETicketType::AppOwnershipTicket:
				return ticketMap;

			case ETicketType::EncryptedAppOwnershipTicket:
				return encryptedTicketMap;
		}

		throw std::runtime_error("Unknown ticket type!");
	}

	std::filesystem::path getTicketDir();

	//TODO: Fill with error checks
	std::filesystem::path getTicketPath(const AppId_t appId, const ETicketType type);
	std::shared_ptr<SavedTicket> getCachedTicket(const AppId_t appId, const ETicketType type);
	bool saveTicketToCache(const AppId_t appId, const std::string& bytes, const CSteamId& owner, const ETicketType type);

	//Hooks
	void closePipe(const HSteamPipe pipe);
	void launchApp(const GameId_t gameId);
	void getTicketOwnershipExtendedData(const AppId_t appId);

	//Netpacket stuff
	void recvAppTicket(const CNetPacket* pkt);
	void recvEncryptedAppTicket(CNetPacket* pkt);

	void recvMsg(CNetPacket* pkt);
}
