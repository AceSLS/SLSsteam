-- Define a global table to store everything into that you do not
-- want to get garbage collected
Example = Example or {
	setup = false,

	-- Hooks
	postCallbackHook = nil, postCallbackTramp = nil,

	-- Functions
	clientUserLoggedOn = nil
}

-- Do not run multiple times. While Luas do support hot reloading it's
-- not a good idea to do so. Linux is lacking an API to suspend threads
-- so things can and most likely will go wrong. Only hot reload plugins
-- while developing
if Example.setup then
	return
end
Example.setup = true

local ffi = require("ffi")

ffi.cdef[[
	void* place_lua_hook(const int, const void*);
	void* unpack_user_data(const void*);

	typedef void(*PostCallback_t)(void*, uint32_t, void*, uint32_t, uint32_t);
	typedef bool(*IClientUser_BLoggedOn_t)(void*);
]]

log.debug("Luas loading :)")

local modSteamClient = memhlp.getModule("steamclient.so")
local postCallbackPtr = memhlp.getJmpTarget(memhlp.patternScan("E8 ? ? ? ? 8B 75 ? 89 D8", modSteamClient))

Example.hkPostCallback = ffi.cast("PostCallback_t", function(user, type, pCallback, callbackSize, a4)
	-- Lua is not thread safe, so we use a recursive_mutex to prevent
	-- multiple threads using the same lua_State simultaneously
	-- LuaMutex() locks automatically, ~LuaMutex() unlocks automatically
	-- but since Lua doesn't run the garbage collector as soon as the function ends we call unlock manually
	-- SLSsteam will automatically lock the shared LuaMutex before firing a callback/rerunning Luas
	local mutex = LuaMutex()

	log.debug("PostCallback " .. type)
	Example.postCallbackTramp(user, type, pCallback, callbackSize, a4)

	mutex:unlock()
end)

Example.postCallbackHook = LuaHook("PostCalback", postCallbackPtr)
-- We use a c function to place the hook since you can not cast a cdata to a userdata object
Example.postCallbackTramp = ffi.cast("PostCallback_t", ffi.C.place_lua_hook(Example.postCallbackHook.index, Example.hkPostCallback))
log.debug("Postcallback hooked!")

-- We cast to intptr_t since lua messes up the conversion to unsigned integer otherwise
local clientUserMapLoggedOn = VFTableInfo_t("14IClientUserMap", "BLoggedOn")
if not clientUserMapLoggedOn:init() then
	log.notify("Failed to parse IClientUserMap!")
end

-- IClientUser is subclass 1 of CUser
local clientUserLoggedOn = VFTableInfo_t("5CUser", "BLoggedOn", clientUserMapLoggedOn.index, 0)
clientUserLoggedOn:init()

Example.clientUserLoggedOn = ffi.cast("IClientUser_BLoggedOn_t", clientUserLoggedOn.ptr)

Example.initialized = function()
	local config = SLS.config
	local engine = SLS.steamEngine
	local user = engine:getUser(0)
	local clientUser = user:getClientUser()
	local apps = user:getClientApps()

	local pClientUser = ffi.C.unpack_user_data(clientUser)
	log.debug("pClientUser: " .. tostring(pClientUser))

	log.debug("IClientUser::BLoggedOn -> " .. tostring(clientUser:loggedOn())) -- SLS wrapped function call
	-- This is just an example how to call arbitrary functions
	log.debug("IClientUser::BLoggedOn -> " .. tostring(Example.clientUserLoggedOn(pClientUser))) -- Raw function call

	local function addappid(appId)
		if user:isSubscribed(appId) then
			log.debug(appId .. " is already subscribed! Not adding to additionalApps...")
		end

		local appList = config:getAdditionalApps()
		table.insert(appList, appId)
		log.debug("Added app " .. appId)
		config:setAdditionalApps(appList)
	end

	addappid(236430) --  #DARK SOULS™ II
	addappid(271940) -- # Dark Souls II - Pre-Order DLC ROW
	addappid(271941) -- # Dark Souls II - Digital Extras
	addappid(271942) -- # Dark Souls™ II Crown of the Sunken King
	addappid(271943) -- # DARK SOULS™ II Crown of the Old Iron King
	addappid(271944) -- # DARK SOULS™ II Crown of the Ivory King
	addappid(284450) -- # DARK SOULS™ II - Season Pass
	addappid(287360) -- # Dark Souls II - Pre-Order DLC JP
	addappid(287770) -- # Darks Souls II JP Retail Pre-Order DLC 1
	addappid(287771) -- # Darks Souls II JP Retail Pre-Order DLC 2
	addappid(289120) -- # Darks Souls II JP Retail Pre-Order DLC 3
	addappid(289121) -- # Darks Souls II JP Retail Pre-Order DLC 4
	addappid(289122) -- # Darks Souls II JP Retail Pre-Order DLC 5
	addappid(355700) -- # Dark Souls II Upgrade to DX11 (no content)

	log.custom(log.LogLevelDebug | log.LogLevelOnce, "Custom deduplicated log")

	log.info("Lua apps added!")
end

Example.configLoaded = function()
	local config = SLS.config
	local someList = config:getIntList("AppIds")

	for _, v in ipairs(someList) do
		log.debug("AppIds " .. v)
	end

	log.info("Lua config loaded")
end

SLS.registerCallback("SLSsteam::initialized", Example.initialized)
SLS.registerCallback("SLSsteam::configLoaded", Example.configLoaded)

log.notify("example.lua loaded!")
