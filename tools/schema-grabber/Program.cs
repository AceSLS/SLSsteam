using System.Net;
using System.Text.RegularExpressions;

using ProtoBuf;
using SteamKit2;
using SteamKit2.Internal;
using SteamKit2.Authentication;

namespace SchemaGrabber
{
	public sealed class GetUserStatsCallback : CallbackMsg
	{
		public CMsgClientGetUserStatsResponse body;

		internal GetUserStatsCallback(IPacketMsg packetMsg)
		{
			var statsResponse = new ClientMsgProtobuf<CMsgClientGetUserStatsResponse>(packetMsg);
			this.JobID = statsResponse.TargetJobID;
			body = statsResponse.Body;
		}
	}

	public sealed partial class GetUserStatsCallbackHandler : ClientMsgHandler
	{
		public override void HandleMsg(IPacketMsg msg)
		{
			CallbackMsg callback = null;

			switch (msg.MsgType)
			{
				case EMsg.ClientGetUserStatsResponse:
					callback = new GetUserStatsCallback(msg);
					break;
			}

			if (callback != null)
			{
				this.Client.PostCallback(callback);
			}
		}
	}

	class SchemaGrabber
	{
		//Thanks Parasitic-Hollow for the idea
		public const string ReviewUrl = "https://api.steampowered.com/IUserReviewsService/GetAppReviews/v1/?appid={appId}&num_per_page=10&filter=1&languages[0]=all&day_range=0&review_type=0&purchase_type=1&filter_offtopic_activity=false&use_review_quality=false";

		public readonly string GuardDataDir = Path.Combine(Environment.CurrentDirectory, "Sentries");

		public string GetSteamDir()
		{
			var home = Environment.GetEnvironmentVariable("HOME");
			if (home == null)
			{
				Console.WriteLine("Failed to get home directory!");
				Environment.Exit(1);
			}

			var steamPaths = new[]
			{
				".steam/steam",
				".var/app/com.valvesoftware.Steam/.steam/steam"
			};

			var steamPath = "";

			foreach (var path in steamPaths)
			{
				var combined = Path.Combine(home, path);
				if (Directory.Exists(combined))
				{
					steamPath = combined;
					break;
				}
			}

			if (steamPath.Length < 1)
			{
				Console.WriteLine("Failed to find steam directory!");
				Environment.Exit(1);
			}

			return steamPath;
		}

		public string GetStatsDir()
		{
			var steamPath = GetSteamDir();
			var statsDir = Path.Combine(steamPath, "appcache/stats");

			if (!Directory.Exists(statsDir))
			{
				if (!Directory.CreateDirectory(statsDir).Exists)
				{
					Console.WriteLine($"Failed to create {statsDir}");
					Environment.Exit(1);
				}
			}

			return statsDir;
		}

		public List<ulong> GetUsers()
		{
			var users = new List<ulong>();

			var loginVdf = Path.Combine(GetSteamDir(), "config", "loginusers.vdf");
			if (!File.Exists(loginVdf))
			{
				Console.WriteLine($"{loginVdf} does not exist! Will generate schemas only for the logged in account");
				return users;
			}

			var text = File.ReadAllText(loginVdf);

			//var re = new Regex("(?<=^\t\")[0-9]+(?=\")"); //Ain't work
			var re = new Regex("(?<=\t\")[0-9]{17}(?=\")");
			var matches = re.Matches(text);

			foreach (var match in matches)
			{
				var steamIdStr = match.ToString();
				if (steamIdStr == null)
				{
					continue;
				}

				var accountId = ulong.Parse(steamIdStr) & 0xFFFFFFFF;
				users.Add(accountId);
				//Console.WriteLine($"Found user {accountId}");
			}

			return users;
		}

		public List<uint> GetInstalledGames()
		{
			var games = new List<uint>();

			var libraryVdf = Path.Combine(GetSteamDir(), "config", "libraryfolders.vdf");
			if (!File.Exists(libraryVdf))
			{
				Console.WriteLine($"{libraryVdf} does not exist!");
				return games;
			}

			var text = File.ReadAllText(libraryVdf);

			var re = new Regex("(?<=\t\t\t\")[0-9]+(?=\")");
			var matches = re.Matches(text);

			foreach (var match in matches)
			{
				var appIdStr = match.ToString();
				if (appIdStr == null)
				{
					continue;
				}

				games.Add(uint.Parse(appIdStr));
				Console.WriteLine($"Found {appIdStr} in library");
			}

			return games;
		}

		protected bool finished = false;
		protected CallbackManager callbackManager;

		protected string guardDataFile => Path.Combine(GuardDataDir, Username + ".sentry");

		protected string getStoredGuardData()
		{
			if (!Directory.Exists(GuardDataDir))
			{
				Directory.CreateDirectory(GuardDataDir);
			}

			if (!File.Exists(guardDataFile))
			{
				return null;
			}

			return File.ReadAllText(guardDataFile);
		}

		protected void storeGuardData(string guardData)
		{
			if (!File.Exists(guardDataFile))
			{
				File.Create(guardDataFile).Close();
			}

			File.WriteAllText(guardDataFile, guardData);
		}

		public readonly string Username;
		public readonly string Password;
		public readonly uint TargetAppId;

		public SteamClient Client;
		public SteamUser User;
		public SteamFriends Friends;
		public SteamApps Apps;

		public SchemaGrabber(string username, string password, uint targetAppId)
		{
			Username = username;
			Password = password;
			TargetAppId = targetAppId;

			Client = new SteamClient();
			callbackManager = new CallbackManager(Client);

			User = Client.GetHandler<SteamUser>();
			Friends = Client.GetHandler<SteamFriends>();
			Apps = Client.GetHandler<SteamApps>();

			if
			(
				User == null
				|| Friends == null
				|| Apps == null
			)
			{
				Console.WriteLine("Failed to get Handlers!");
				Environment.Exit(1);
			}

			Client.AddHandler(new GetUserStatsCallbackHandler());

			callbackManager.Subscribe<SteamClient.ConnectedCallback>(OnConnected);
			callbackManager.Subscribe<SteamClient.DisconnectedCallback>(OnDisconnected);

			callbackManager.Subscribe<SteamUser.LoggedOnCallback>(OnLoggedOn);
			callbackManager.Subscribe<SteamUser.LoggedOffCallback>(OnLoggedOff);
			callbackManager.Subscribe<SteamUser.AccountInfoCallback>(OnAccountInfo);
		}

		async protected void OnConnected(SteamClient.ConnectedCallback cb)
		{
			Console.WriteLine("Connected to Steam! Logging in...");

			var details = new SteamKit2.Authentication.AuthSessionDetails
			{
				Username = Username,
				Password = Password,
				IsPersistentSession = true,
				Authenticator = new UserConsoleAuthenticator()
			};

			var guardData = getStoredGuardData();
			if (guardData != null)
			{
				//TODO: Add to details directly?
				details.GuardData = guardData;
			}

			var authSess = await Client.Authentication.BeginAuthSessionViaCredentialsAsync(details);
			var resp = await authSess.PollingWaitForResultAsync();

			if (resp.NewGuardData != null)
			{
				storeGuardData(resp.NewGuardData);
			}

			User.LogOn(new SteamUser.LogOnDetails
			{
				Username = resp.AccountName,
				AccessToken = resp.RefreshToken,
				ShouldRememberPassword = true
			});
		}

		protected void OnDisconnected(SteamClient.DisconnectedCallback cb)
		{
			Console.WriteLine("Disconnected from Steam! Exiting...");
			finished = true;
		}

		protected void OnLoggedOn(SteamUser.LoggedOnCallback cb)
		{
			Console.WriteLine($"Logged in as {Username}");
		}

		protected void OnLoggedOff(SteamUser.LoggedOffCallback cb)
		{
			Console.WriteLine($"Logged out from {Username}");
			finished = true;
		}

		protected async void OnAccountInfo(SteamUser.AccountInfoCallback cb)
		{
			Console.WriteLine("Account Info received! Grabbing schema...");

			if (TargetAppId > 0)
			{
				await GetSchema(TargetAppId);
			}
			else
			{
				var games = GetInstalledGames();
				for (var i = 0; i < games.Count; i++)
				{
					Console.Title = $"schema-grabber {i + 1}/{games.Count}";
					var appId = games[i];
					await GetSchema(appId);
				}
			}

			finished = true;
		}

		public AsyncJob<GetUserStatsCallback> GetUserStats(ulong steamId, uint appId)
		{
			var msg = new ClientMsgProtobuf<CMsgClientGetUserStats>(EMsg.ClientGetUserStats);
			msg.SourceJobID = Client.GetNextJobID();

			msg.Body.game_id = appId;
			msg.Body.schema_local_version = -1;
			msg.Body.steam_id_for_user = steamId;

			Client.Send(msg);

			return new AsyncJob<GetUserStatsCallback>(Client, msg.SourceJobID);
		}

		protected async Task GetSchema(uint appId)
		{
			var uri = ReviewUrl.Replace("{appId}", appId.ToString());
			Console.WriteLine($"Downloading {uri}");

			using (var wc = new HttpClient())
			{
				var json = await wc.GetStringAsync(uri);
				var re = new Regex("(?<=\"steamid\":\")[0-9]+(?=\")");
				var matches = re.Matches(json);
				Console.WriteLine($"Got {matches.Count} steamIds");

				foreach (var match in matches)
				{
					var steamIdStr = match.ToString();
					if (steamIdStr == null)
					{
						continue;
					}

					var steamId = ulong.Parse(steamIdStr);
					var resp = await GetUserStats(steamId, appId);
					var body = resp.body;

					Console.WriteLine($"GetUserStatsResponse {body.eresult} for {steamId}");
					if ((EResult)body.eresult != EResult.OK)
					{
						continue;
					}

					if (resp.body.schema.Length < 1)
					{
						Console.WriteLine($"No schema returned for {steamId}");
						continue;
					}

					var statsDir = GetStatsDir();
					var accountId = User.SteamID & 0xffffffff;
					var schemaFile = $"{statsDir}/UserGameStatsSchema_{body.game_id}.bin";

					File.WriteAllBytes(schemaFile, resp.body.schema);
					Console.WriteLine($"Created {schemaFile}");

					var users = GetUsers();
					if (!users.Contains(accountId))
					{
						users.Add(accountId);
					}

					foreach (var user in users)
					{
						var userStatsPath = $"{statsDir}/UserGameStats_{user}_{body.game_id}.bin";
						if (!File.Exists(userStatsPath))
						{
							File.WriteAllBytes(userStatsPath, new byte[]
							{
								0x00, 0x63, 0x61, 0x63, 0x68, 0x65, 0x00, 0x02, 0x63, 0x72, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00,
								0x02, 0x50, 0x65, 0x6e, 0x64, 0x69, 0x6e, 0x67, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x73, 0x00,
								0x00, 0x00, 0x00, 0x00, 0x08, 0x08
							});

							Console.WriteLine($"Created {userStatsPath}");
						}
					}

					break;
				}
			}
		}

		public void CallbackLoop()
		{
			while (!finished)
			{
				callbackManager.RunWaitCallbacks(TimeSpan.FromSeconds(1));
			}
		}

		public void Connect()
		{
			Client.Connect();
		}
	}

	static class Program
	{
		public static void Main(string[] args)
		{
			Console.Title = "schema-grabber";

			if (args.Length < 3 || (args.Length > 0 && args.Any(a => a == "-h" || a == "--help")))
			{
				Console.WriteLine("Usage: ./schema-grabber username password appId");
				Console.WriteLine("Use AppId 0 to generate schemas for all installed games");
				Environment.Exit(1);
			}

			uint appId;
			if (!uint.TryParse(args[2], out appId))
			{
				Console.WriteLine($"{args[2]} is not a number!");
				Environment.Exit(1);
			}

			var grabber = new SchemaGrabber(args[0], args[1], appId);
			grabber.Connect();
			grabber.CallbackLoop();
		}
	}
}
