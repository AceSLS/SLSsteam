# Plugin API

SLSsteam now embeds LuaJIT which allows doing basically everything paired with FFI & SLSsteam's exposed functions. \
Plugins get automatically read from SLSsteam's plugin directory (config/plugins).

#### **While hot reloading Luas is possible it's highly advised against doing so. You have been warned.**

If you want to dive right in without reading the docs check out the example.lua in the same directory this README is in.

Some side notes:
- SLSsteam will reload the config each time the luas reload to allow changing it & remove any changes a previous Lua could have done.
- In debug versions SLSsteam will recreate the lua_State on each hot reload. In release versions it does not to
prevent hooks getting placed on code that's potentially being executed.
- When a lua callback gets fired SLSsteam will lock the lua_State mutex


#### typedefs

LuaJIT doesn't have access to all of SLSsteam's internal typedefs. Currently the ones you need to be aware of are:

lm_address_t -> uintptr_t \
LogLevelFlags_t -> unsigned int \
AppId_t -> uint32_t


#### Exports

unpack_user_data(ptr: void*) -> void*: Unpacks a Lua UserData object and returns a pointer to the object it's pointing at. Use this to convert SLS.steamEngine to the real CSteamEngine pointer etc.

If you want to manage memory malloc & free seem to work just fine. I still recommend using Plat_Alloc, Plat_Realloc & Plat_Free from libtier0_s.so instead.


#### curl:

downloadString(url: string, timeout: int) ->: Download url as string with timeoUt \
downloadString(url: string, headers: string[], timeout: int) -> string: Download url as string with headers & timeOut


#### log:

LogLevelTrace -> LogLevelFlags_t \
LogLevelOnce -> LogLevelFlags_t \
LogLevelDebug -> LogLevelFlags_t \
LogLevelWarn -> LogLevelFlags_t \
LogLevelError -> LogLevelFlags_t \
LogLevelInfo -> LogLevelFlags_t \
LogLevelNotify -> LogLevelFlags_t \
LogLevelNotifyLong -> LogLevelFlags_t

log.debug(msg: string): Print debug string to log \
log.warn(msg: string): Print warning string to log \
log.error(msg: string): Print error string to log \
log.info(msg: string): Print info string to log \
log.notify(msg: string): Create notification via notify-send \
log.notifyWarn(msg: string): Create warning notification via notify-send \
log.notifyError(msg: string): Create error notification via notify-send \
log.custom(flags: LogLevelFlags_t, msg: string): Create notification via notify-send


#### lm_module_t:

base -> lm_address_t: Module base \
size -> lm_address_t: Module size \
end -> lm_address_t: Module end


#### memhlp:

getModule(name: string) -> lm_moudle_t*: Get module by name \
getJmpTarget(ptr: void*) -> void*: Get absolute address of relative jmp \
hexdump(ptr: void*, size: size_t) -> string: Get formatted hexdump \
findPrologue(ptr: void*, bytes: int16_t[]) -> void*: Find prologue of function by going backwards until bytes match \
patternScan(pattern: string, module: lm_module_t&) -> void*: Find a pattern in the specified modules .text section \


#### VFTableInfo_t

VFTableInfo_t(typename: string, functionName: string): Create new VFTableInfo_t \
VFTableInfo_t(typename: string, functionName: string, index: unsigned int): Create new VFTableInfo_t \
VFTableInfo_t(typename: string, functionName: string, index: unsigned int, subClassIndex: unsigned int): Create new VFTableInfo_t \
typeName -> string \
functionName -> string \
address -> lm_address_t: The resolved address in current memory \
ptr -> void*: Pointer to the resolved address in current memory \
init() -> bool: Initialize, returns true on success, false otherwise. Check the logs for errors \
getPrintName() -> string: Returns typeName::functionName


#### LuaMutex:

LuaMutex(): Creates a LuaMutex variable & locks SLSsteam's recursive lua_State mutex \
~LuaMutex(): Unlocks the recursive mutex \
lock: Locks the  mutex \
unlock: Unlocks the mutex

#### LuaHook:

extern place_lua_hook(const int index, const void* targetFn) -> void*: Used to set a LuaHooks' target function & place it

LuaHook(name: string, targetFunction: void*): Create new LuaHook \
~LuaHook(): Restores the original code the LuaHook overwrote \
name -> string: Hook name, solely used for logging \
fn -> void*: Target function address \
hookFn -> void*: Hook function address \
tramp -> void*: Trampoline address \
size -> size_t: Stolen bytes, taken away for creating the trampoline \
index -> int: Index of the hook used for place_lua_hook \
place(): Place the hook \
remove(): Remove the hook


#### YAMLNode

This API mostly exists for using when the CConfig methods aren't cutting it and you want to parse custom YAML types. It does not have any of the convenience involved in the CConfig, but offers way more control

YAMLNode(): Create new YAMLNode

isDefined -> bool \
isNull -> bool \
isScalar -> bool \
isSequence -> bool \
isMap -> bool \
size -> int

asDouble() -> double \
asInt() -> int \
asString() -> string \
asPairList -> table[table[2]]

addPair(key: YAMLNode, value: YAMLNode)

setDouble(value: double) \
setInt(value: int) \
setString(value: double)


#### CConfig

getAdditionalApps() -> AppId_t[]: Gets all AdditionalApps \
setAdditionalApps(appIds: AppId_t[]): Sets all AdditionalApps

getDouble(name: string, defaultValue: double) -> double: Gets name from config as double \
getInt(name: string, defaultValue: Int64) -> Int64: Gets name from config as Int64 \
getString(name: string, defaultValue: string) -> string: Gets name from config as string

getDoubleList(name: string) -> double[]: Gets name from config as double set (deduplicated) \
getIntList(name: string) -> Int64[]: Gets name from config as Int64 set (deduplicated) \
getStringList(name: string) -> string[]: Gets name from config as string set (deduplicated) \
getNode(name: string) -> YAML::Node: Gets name as YAML::Node


#### CNetPacketBody

type -> uint32_t \
headerSize -> uint32_t


#### CNetPacket

body -> void* \
size -> uint32_t \
refs -> uint32_t


#### CSteamEngine

getUser(index: int) -> CUser: Get the specified CUser instance. 0 is the global user \
getUtils() -> IClientUtils


#### CUser

getClientApps() -> IClientApps \
getClientUser() -> IClientUser \
getAppManager() -> IClientAppManager \
isSubscribed(appId: AppId_t) -> bool \
postCallback(type: uint32_t, pCallback: lm_address_t, callbackSize: size_t): Post a callback to the Steamengine & all open pipes


#### IClientUtils

getAppId() -> AppId_t: Return the appId for the currently active pipe \
getCurrentSteamPipe() -> HSteamPipe: Return the active pipe handle


#### SLS

config -> CConfig*: Gets SLSsteam config, see [CConfig](#cconfig) \
steamEngine -> CSteamEngine*: Gets the Global CSteamEngine instance \
registerCallback(name: string, function): Registers a callback, when it gets fired function will be invoked from SLSsteam


#### Callbacks

"SLSsteam::configLoaing": Fired right after the config created the root YAMLNode. If you want to modify SLSsteam's configuration do so here \
"SLSsteam::configLoaded": Fired right after the config got reloaded and also on each subsequent Lua reload \
"SLSsteam::initialized": Fired when Steam has finished initializing CUser, making it safe to access (gets fired after each subsequent Lua reload aswell) \
"SLSsteam::luaReload": Called right before SLSsteam deletes the current Lua state & recreates it. Use this to clean up your changes to memory (LuaHooks get cleaned up automatically)

"Network::recvPkt": Fired when Steam receives a CNetPacket. Has 1 argument, the CNetPacket UserData object. Use unpack_user_data to get it's pointer. Afterwards either parse it yourself or use something like lua-protobuf \
"Network::sendPkt": Fired when Steam sends a CNetPacket. Has 1 argument, the CNetPacket UserData object. Use unpack_user_data to get it's pointer. Afterwards either parse it yourself or use something like lua-protobuf 
