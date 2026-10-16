#pragma once

#include "types.hpp"


//CSteamClient contains it's own appId, it's own pipeHandle, the userHandle it's connected to
//Also has virtual functions responsible for creating the InterfaceMapBase<T> classess,
//ConnectToGlobalUser, etc.
//If we want we can hook this up to export the whole SteamAPI via lua, connect our own pipes and
//some more. Maybe we can even go for multiple pipes for the same game but I doubt that would be useful

//Function 71 -> 37IClientNetworkingSocketsSerializedMap
//Function 8 -> 14IClientUserMap
//Function 13 -> 17IClientFriendsMap
//Function 15 -> 17IClientBillingMap
//Function 14 -> 15IClientUtilsMap
//Function 36 -> 30IClientNetworkDeviceManagerMap
//Function 37 -> 27IClientSystemPerfManagerMap
//Function 38 -> 23IClientSystemManagerMap
//Function 39 -> 27IClientSystemDockManagerMap
//Function 40 -> 28IClientSystemAudioManagerMap
//Function 41 -> 30IClientSystemDisplayManagerMap
//Function 16 -> 21IClientMatchmakingMap
//Function 17 -> 14IClientAppsMap
//Function 77 -> N25SteamNetworkingSocketsLib32CSteamNetworkingUtilsSteamClientE
//Function 77 -> N25SteamNetworkingSocketsLib32CSteamNetworkingUtilsSteamClientE
//Function 18 -> 24CSteamMatchMakingServers
//Function 21 -> 19IClientUserStatsMap
//Function 24 -> 23IClientRemoteStorageMap
//Function 25 -> 21IClientScreenshotsMap
//Function 27 -> 25IClientGameCoordinatorMap
//Function 43 -> 20IClientAppManagerMap
//Function 44 -> 21IClientConfigStoreMap
//Function 47 -> 14IClientHTTPMap
//Function 50 -> 15IClientAudioMap
//Function 51 -> 15IClientMusicMap
//Function 46 -> 19IClientGameStatsMap
//Function 52 -> 25IClientUnifiedMessagesMap
//Function 53 -> 16CSteamController
//Function 54 -> 26IClientParentalSettingsMap
//Function 56 -> 29IClientRemoteClientManagerMap
//Function 59 -> 13IClientUGCMap
//Function 57 -> 22IClientStreamClientMap
//Function 58 -> 19IClientShortcutsMap
//Function 61 -> 12IClientVRMap
//Function 63 -> 17CSteamHTMLSurface
//Function 65 -> 15IClientVideoMap
//Function 62 -> 27IClientGameNotificationsMap
//Function 66 -> 30IClientControllerSerializedMap
//Function 69 -> 26IClientSharedConnectionMap
//Function 70 -> 16IClientShaderMap
//Function 72 -> 16IClientCompatMap
//Function 64 -> 18IClientTimelineMap
//Function 79 -> 20IClientRemotePlayMap
//Function 14 -> 15IClientUtilsMap
//SDK_Class CSteamClient;

//template<typename T>
//SDK_Class InterfaceMapBase
//{
//	void* vft; 					//0x0
//	HSteamUser userHandle;		//0x4
//	HSteamPipe pipeHandle;		//0x8
//	uint8_t __pad0xC[0x4];		//0xC
//	CSteamClient* steamClient;	//0x10
//};
