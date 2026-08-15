#pragma once

constexpr static const char* defaultConfig = R"(#Example AppIds Config for those not familiar with YAML:
#AppIds:
#  - 440
#  - 730
#Take care of not messing up your spaces! Otherwise it won't work

#Example of DlcData:
#DlcData:
#  AppId:
#    FirstDlcAppId: "Dlc Name"
#    SecondDlcAppId: "Dlc Name"

#Example of DenuvoGames:
#DenuvoGames:
#  SteamId:
#    -  AppId1
#    -  AppId2

#Example of FakeAppIds:
#FakeAppIds:
#  AppId1: FakeAppId1
#  AppId2: FakeAppId2

#Disables Family Share license locking for self and others
DisableFamilyShareLock: yes

#Switches to whitelist instead of the default blacklist
UseWhitelist: no

#List of AppIds to ex-/include. Either specify each DLC you want to in-/exclude individually
#or add the DLC's parent AppId (the Game's one) to allow unlocking all of their child AppIds
AppIds:

#Additional AppIds to inject (Overrides OwnerIds for apps you got shared! This breaks downloads)
#Best to use this only on games NOT in your library.
#AppIds on this list will automatically get added to your AppIds setting aswell, but only for the initial check.
#It will get ignored in exclusion checks for the parent AppId
AdditionalApps:

#Sync AdditionalApps to connected Steam Remote Play clients.
#Remote apps stay in memory on the receiving client.
SyncRemoteAdditionalApps: no

#Extra Data for Dlcs belonging to a specific AppId. Only needed
#when the App you're playing is hit by Steams 64 DLC limit
DlcData:

#Used to retrieve ProductInfo from Steam servers for some games
AppTokens:

#Legacy CD keys required by some games
#Unless explicitly specified SLSsteam will generate a random 4 * 4 key using A-Z + 0-9 seeded by your accountId + appId
#If you want to delete injected CD keys delete cdk_[number] from your localconfig.vdf
CDKeys:

#Fake Steam being offline for specified AppIds. Same format as AppIds
FakeOffline:

#Change AppIds of games to enable networking features
#Use 0 as a key to set for all unowned Apps
#Do not run multiple apps under the same AppId simultaneously! It's possible but
#will most likely cause undefined behaviour
#Requires access to /proc to read processes' real AppId from their environment (most distros allow this by default)
FakeAppIds:

#Override Depot manifest IDs
#Use this to download older game versions or to lock a game to a specific version
ManifestIds:

#Never download these depots
DepotBlacklist:

#Custom ingame statuses. Set AppId to 0 to disable
IdleStatus:
  AppId: 0
  Title: ""

#Override game titles. Only works with owned appIds! For injected appIds use either UnownedStatus or combine them with FakeAppIds
GameTitles:

#Override purchase time stamps
SubscriptionTimestamps:

#Blocks games from unlocking on wrong accounts
DenuvoGames:

#Overrides your SteamId an app sees. Only needed when the automatic SteamId spoofing
#fails (some games do call GetSteamId before they request your ticket)
#Also can be used to workaround locked saves
#Either set to SteamId or to 0 to use the SteamId in the cached AppOwnershipTicket
SteamIdOverride:

#Automatically grab Achievement schemas from Steams CDN which makes them always up to date
#Slows down game's loading page the first time you click them
#If you don't want this set it to 0 and use tools/schema-grabber instead to grab them outside of steam
#to get the achievement schemas. Doing so will revert to the previous behaviour of falling back to
#the offline cache (appcache/stats) when a GetUserStats request fails
MaxSchemaTries: 10

#Automatically disable SLSsteam when steamclient.so does not match a predefined file hash that is known to work
#You should enable this if you're planing to use SLSsteam with Steam Deck's gamemode
SafeMode: no

#Warn user via notification when steamclient.so hash differs from known safe hash
#Mostly useful for development so I don't accidentally miss an update
WarnHashMissmatch: no

#Notify when SLSsteam is done initializing
NotifyInit: yes

#Enable sending commands to SLSsteam via /tmp/SLSsteam.API
API: no

#Disable cloud saves for unlocked games. Set to "no" if using CloudRedirect or similar.
DisableCloud: yes

#Disable updates for AppIds on AdditionalApps
#Only works for unowned games, since those do not get any depots from CUserAppManager::BuildDepotDependency.
#For owned games use ManifestIds
DisableUpdates: yes

#Changes your Persona's Name clientsidedly
FakeName: ""

#Changes your account's E-Mail clientsided. Leave blank to disable
FakeEmail: ""

#Changes your wallet's balance clientsidedly. 0 to turn off
FakeWalletBalance: 0

#Log levels:
#Trace = 0x1 #Tracing
#Once = 0x2 #Only log once
#Debug = 0x4 #Debugging statements
#Warn = 0x8 #Something went wrong but it's not terrible
#Error = 0x10 #Something went wrong and it's terrible/can't be recovered from. Function failed
#Info = 0x20 #Log for users/external tools
#NotifyShort = 0x40
#NotifyLong = 0x80
#LogLevels below Warn are stripped from release versions
#If you want to use them use a debug version
#LogLevels are bitwise flags, so for example to only
#show Long Notifications & Errors use 0x90 (calculated via 0x80 or 0x10)
#Default enables everything
LogLevels: 0xff

#Dump all used IClientInterfaceMaps
DumpClientInterfaces: no

#Logs all calls to Steamworks (this makes the logfile huge! Only useful for debugging/analyzing
ExtendedLogging: no)";
