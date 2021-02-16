/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include <engine/server.h>
#include <teeother/tl/singletion.h>
#include <game/server/enum_global.h>

STRINGABLE_ENUM_IMPL(MINER)
STRINGABLE_ENUM_IMPL(PLANT)
STRINGABLE_ENUM_IMPL(EMEMBERUPGRADE)

// multiworlds
#define WorldsInstance CSingleton<CWorldGameServerArray>::Get()

class CWorldGameServerArray
{
	struct CWorldGameServer
	{
		char m_aName[64];
		char m_aPath[512];
		class IGameServer* m_pGameServer;
		IEngineMap* m_pLoadedMap;
	};
public:
	std::map < int /*ID*/, CWorldGameServer /*Game world*/ > ms_aWorlds;
	~CWorldGameServerArray() { Clear(); }

	bool IsValid(int WorldID) { return ms_aWorlds.find(WorldID) != ms_aWorlds.end(); }
	bool Add(int WorldID, IKernel* pKernel);
	void Clear();
};

// 
class CSnapIDPool
{
	enum
	{
		MAX_IDS = 16*1024,
	};

	class CID
	{
	public:
		short m_Next;
		short m_State; // 0 = free, 1 = alloced, 2 = timed
		int m_Timeout;
	};

	CID m_aIDs[MAX_IDS];

	int m_FirstFree;
	int m_FirstTimed;
	int m_LastTimed;
	int m_Usage;
	int m_InUsage;

public:

	CSnapIDPool();

	void Reset();
	void RemoveFirstTimeout();
	int NewID();
	void TimeoutIDs();
	void FreeID(int ID);
};


class CServerBan : public CNetBan
{
	class CServer *m_pServer;

	template<class T> int BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason);

public:
	class CServer *Server() const { return m_pServer; }

	void InitServerBan(class IConsole *pConsole, class IStorageEngine *pStorage, class CServer* pServer);

	virtual int BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason);
	virtual int BanRange(const CNetRange *pRange, int Seconds, const char *pReason);

	static void ConBanExt(class IConsole::IResult *pResult, void *pUser);
};

class CServer : public IServer
{
	class IConsole *m_pConsole;
	class IStorageEngine *m_pStorage;

public:
	virtual class IGameServer* GameServer(int WorldID = 0)
	{
		if(WorldsInstance->ms_aWorlds.find(WorldID) == WorldsInstance->ms_aWorlds.end())
			return nullptr;
		return WorldsInstance->ms_aWorlds[WorldID].m_pGameServer;
	}
	class IConsole *Console() { return m_pConsole; }
	class IStorageEngine*Storage() { return m_pStorage; }
	class DiscordJob *m_pDiscord;

	enum
	{
		AUTHED_NO=0,
		AUTHED_MOD,
		AUTHED_ADMIN,

		MAX_RCONCMD_RATIO = 8,
		MAX_RCONCMD_SEND=16,
	};

	class CClient
	{
	public:

		enum
		{
			STATE_EMPTY = 0,
			STATE_AUTH,
			STATE_CONNECTING,
			STATE_READY,
			STATE_INGAME,

			SNAPRATE_INIT=0,
			SNAPRATE_FULL,
			SNAPRATE_RECOVER
		};

		class CInput
		{
		public:
			int m_aData[MAX_INPUT_SIZE];
			int m_GameTick; // the tick that was chosen for the input
		};

		// connection state info
		int m_State;
		int m_Latency;
		int m_SnapRate;

		int m_LastAckedSnapshot;
		int m_LastInputTick;
		CSnapshotStorage m_Snapshots;

		CInput m_LatestInput;
		CInput m_aInputs[200]; // TODO: handle input better
		int m_CurrentInput;

		// names update
		char m_aName[MAX_NAME_ARRAY_SIZE];
		char m_aClan[MAX_CLAN_ARRAY_SIZE];
		char m_aLanguage[MAX_LANGUAGE_LENGTH];

		int m_Version;
		int m_Country;
		int m_Score;
		int m_Authed;
		int m_AuthTries;

		int m_WorldID;
		int m_OldWorldID;
		bool m_ChangeMap;

		int m_MapChunk;
		bool m_NoRconNote;
		bool m_Quitting;
		const IConsole::CCommandInfo *m_pRconCmdToSend;

		int m_ClientVersion;
		void Reset();
	};

	CClient m_aClients[MAX_CLIENTS];

	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIDPool m_IDPool;
	CNetServer m_NetServer;
	CEcon m_Econ;
	CServerBan m_ServerBan;

	int64 m_GameStartTime;
	int m_RunServer;
	int m_RconClientID;
	int m_RconAuthLevel;
	int m_PrintCBIndex;

	// map
	enum
	{
		MAP_CHUNK_SIZE=NET_MAX_PAYLOAD-NET_MAX_CHUNKHEADERSIZE-4, // msg type
	};
	int m_MapChunksPerRequest;

	int m_RconPasswordSet;
	int m_GeneratedRconPassword;

	CRegister m_Register;
	CMapChecker m_MapChecker;

	CServer();
	~CServer();

	// world time
	int m_ShiftTime;
	int m_LastShiftTick;
	int m_TimeWorldMinute;
	int m_TimeWorldHour;
	bool m_TimeWorldAlarm;

	virtual int GetMinutesWorldTime() const;
	virtual int GetHourWorldTime() const;
	virtual int GetOffsetWorldTime() const;
	virtual void SetOffsetWorldTime(int Hour);
	virtual bool CheckWorldTime(int Hour, int Minute);
	virtual const char* GetStringTypeDay() const;
	virtual int GetEnumTypeDay() const;

	// basic
	virtual void SetClientName(int ClientID, const char *pName);
	virtual void SetClientClan(int ClientID, char const *pClan);
	virtual void SetClientCountry(int ClientID, int Country);
	virtual void SetClientScore(int ClientID, int Score);

	virtual void ChangeWorld(int ClientID, int NewWorldID);
	virtual int GetClientWorldID(int ClientID);
	virtual void BackInformationFakeClient(int FakeClientID);

	virtual void SetClientProtocolVersion(int ClientID, int Version);
	virtual int GetClientProtocolVersion(int ClientID);

	virtual void SetClientLanguage(int ClientID, const char* pLanguage);
	virtual const char* GetClientLanguage(int ClientID) const;
	virtual const char* GetWorldName(int WorldID);

	virtual void SendDiscordMessage(const char *pChannel, const char* pColor, const char* pTitle, const char* pText);
	virtual void SendDiscordGenerateMessage(const char *pTitle, int AccountID, const char* pColor = "\0");
	virtual void UpdateDiscordStatus(const char *pStatus);

	void Kick(int ClientID, const char *pReason);

	int64 TickStartTime(int Tick);
	int Init();

	void InitRconPasswordIfUnset();

	void SetRconCID(int ClientID);
	bool IsAuthed(int ClientID) const;
	bool IsBanned(int ClientID);
	int GetClientInfo(int ClientID, CClientInfo *pInfo) const;
	void GetClientAddr(int ClientID, char *pAddrStr, int Size) const;
	const char *ClientName(int ClientID) const;
	const char *ClientClan(int ClientID) const;
	int ClientCountry(int ClientID) const;
	bool ClientIngame(int ClientID) const;

	virtual int SendMsg(CMsgPacker* pMsg, int Flags, int ClientID, int64 Mask = -1, int WorldID = -1);

	void DoSnapshot(int WorldID);

	static int NewClientCallback(int ClientID, void *pUser);
	static int DelClientCallback(int ClientID, const char *pReason, void *pUser);

	void SendMap(int ClientID);
	void SendConnectionReady(int ClientID);
	void SendRconLine(int ClientID, const char *pLine);
	static void SendRconLineAuthed(const char *pLine, void *pUser, bool Highlighted);

	void SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void UpdateClientRconCommands();

	void ProcessClientPacket(CNetChunk *pPacket);

	void SendServerInfo(int ClientID);
	void GenerateServerInfo(CPacker *pPacker, int Token);

	void PumpNetwork();

	const char *GetMapName() const;
	bool LoadMap(int ID);

	void InitRegister(CNetServer *pNetServer, IEngineMasterServer *pMasterServer, IConsole *pConsole);
	int Run();

	static void ConKick(IConsole::IResult *pResult, void *pUser);
	static void ConStatus(IConsole::IResult *pResult, void *pUser);
	static void ConShutdown(IConsole::IResult *pResult, void *pUser);
	static void ConLogout(IConsole::IResult *pResult, void *pUser);

	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainModCommandUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconPasswordSet(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	void RegisterCommands();

	// Bots
	virtual void InitClientBot(int ClientID);

	virtual int SnapNewID();
	virtual void SnapFreeID(int ID);
	virtual void *SnapNewItem(int Type, int ID, int Size);
	void SnapSetStaticsize(int ItemType, int Size);
};

#endif