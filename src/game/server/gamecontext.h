/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <base/another/kurhelper.h>

#include <engine/server.h>
#include <engine/storage.h>
#include <engine/console.h>
#include <engine/shared/memheap.h>
#include <teeother/components/localization.h>
#include <game/server/enum_context.h>
#include <game/commands.h>
#include <game/layers.h>
#include <game/voting.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"

#include "playerbot.h"
#include "player.h"

#include <game/server/mmocore/MmoController.h>


#ifdef _MSC_VER
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

class CGS : public IGameServer
{
	/* #########################################################################
		VAR AND OBJECT GAMECONTEX DATA 
	######################################################################### */
	int m_WorldID;
	class MmoController *pMmoController;

	IServer *m_pServer;
	class IConsole *m_pConsole;
	
	CLayers m_Layers;
	CCollision m_Collision;

	CNetObjHandler m_NetObjHandler;
	CTuningParams m_Tuning;

	CGS(int Resetting);
	void Construct(int Resetting);
	bool m_Resetting;

	// данжы
	int m_DungeonID;

public:
	IServer *Server() const { return m_pServer; }
	class IConsole *Console() { return m_pConsole; }

	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }
	
	CGS();
	~CGS();
	void Clear();

	MmoController *Mmo() { return pMmoController; }

	CEventHandler m_Events;
	CPlayer *m_apPlayers[MAX_CLIENTS];
	class IGameController *m_pController;
	CGameWorld m_World;
	CCommandManager m_CommandManager;
	CCommandManager* CommandManager() { return &m_CommandManager; }

	/* #########################################################################
		SWAP GAMECONTEX DATA 
	######################################################################### */


	// - - - - - - - - - - - -
	struct StructParsing
	{
		int ParsingLifeTick;
		int ParsingClientID;
		int ParsingSaveInt;
		char ParsingType[32];
	};
	static std::map < int, StructParsing > Interactive;
	// - - - - - - - - - - - -
	struct StructInteractiveSub
	{
		// обычные что не можно раскидать на все
		int TempID;
		int TempID2;
		int TempID3;

		// остальное все
		ShopJob::AuctionItem AuctionItem;

		char RankName[32];
		char GuildName[32];
	};
	static std::map < int, StructInteractiveSub > InteractiveSub;

	// - - - - - - - - - - - -
	static std::map < int, std::map < std::string, int > > Effects;
	// - - - - - - - - - - - -
	struct StructAttribut
	{
		char Name[32];
		char FieldName[32];
		int UpgradePrice;
		int AtType;
	};
	static std::map < int, StructAttribut > AttributInfo;

	/* #########################################################################
		HELPER PLAYER FUNCTION 
	######################################################################### */
	class CCharacter *GetPlayerChar(int ClientID);
	CPlayer *GetPlayer(int ClientID, bool CheckAuthed = false, bool CheckCharacter = false);
	char* LevelString(int max, int value, int step, char ch1, char ch2);
	ItemJob::ItemInformation &GetItemInfo(int ItemID) const;

	/* #########################################################################
		EVENTS 
	######################################################################### */
	void CreateDamage(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self);
	void CreateDamageTranslate(vec2 Pos, int Id, vec2 Source, int HealthAmount, int ArmorAmount, bool Self);
	void CreateHammerHit(vec2 Pos);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage);
	void CreatePlayerSpawn(vec2 Pos);
	void CreateDeath(vec2 Pos, int Who);
	void CreateSound(vec2 Pos, int Sound, int64 Mask=-1);
	void CreatePlayerSound(int ClientID, int Sound);
	void SendMmoEffect(vec2 Pos, int EffectID);
	void SendMmoPotion(vec2 Pos, const char *Potion, bool Added);

	/* #########################################################################
		CHAT FUNCTIONS 
	######################################################################### */
private:
	void SendChat(int ChatterClientID, int Mode, int To, const char *pText);

public:
	void Chat(int ClientID, const char* pText, ...);
	void ChatFollow(int ClientID, const char* pText, ...);
	void ChatAccountID(int AccountID, const char* pText, ...);
	void ChatDiscord(const char *Color, const char *Title, const char* pText, ...);
	void ChatDiscordChannel(const char* pChanel, const char* Color, const char* Title, const char* pText, ...);
	void ChatGuild(int GuildID, const char* pText, ...);
	void ChatWorldID(int WorldID, const char *Suffix, const char* pText, ...);
	void Motd(int ClientID, const char* Text, ...);

	/* #########################################################################
		BROADCAST FUNCTIONS 
	######################################################################### */
private:
	struct CBroadcastState
	{
		int m_NoChangeTick;
		char m_PrevMessage[1024];
		
		int m_Priority;
		char m_NextMessage[1024];
		
		int m_LifeSpanTick;
		int m_TimedPriority;
		char m_TimedMessage[1024];
	};
	CBroadcastState m_BroadcastStates[MAX_PLAYERS];

public:
	void AddBroadcast(int ClientID, const char* pText, int Priority, int LifeSpan);
	void SendBroadcast(const char *pText, int ClientID, int Priority, int LifeSpan);
	void SBL(int ClientID, int Priority, int LifeSpan, const char *pText, ...);
	void BroadcastWorldID(int WorldID, int Priority, int LifeSpan, const char *pText, ...);
	void BroadcastTick(int ClientID);

	/* #########################################################################
		PACKET MESSAGE FUNCTIONS 
	######################################################################### */
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendMotd(int ClientID);
	void SendSettings(int ClientID);
	void SendSkinChange(int ClientID, int TargetID);
	void SendEquipItem(int ClientID, int TargetID);
	void SendRangeEquipItem(int TargetID, int StartSenderClientID, int EndSenderClientID33);
	void SendTeam(int ClientID, int Team, bool DoChatMsg, int TargetID);
	void SendGameMsg(int GameMsgID, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ClientID);
	void SendGameMsg(int GameMsgID, int ParaI1, int ParaI2, int ParaI3, int ClientID);

	void SendChatCommand(const CCommandManager::CCommand* pCommand, int ClientID);
	void SendChatCommands(int ClientID);
	void SendRemoveChatCommand(const CCommandManager::CCommand* pCommand, int ClientID);

	void SendTuningParams(int ClientID);
	void SendTalkText(int ClientID, int TalkingID, bool PlayerTalked, const char *Message, int Style = -1, int TalkingEmote = -1);
	void SendProgressBar(int ClientID, int Count, int Request, const char *Message);
	void ClearTalkText(int ClientID);
	int CheckPlayerMessageWorldID(int ClientID);
	int64 MaskWorldID();

	/* #########################################################################
		ENGINE GAMECONTEXT 
	######################################################################### */	
	void OnInit(int WorldID) override;
	void OnConsoleInit() override;
	void OnShutdown() override;

	void OnTick() override;
	void OnTickLocalWorld();
	void OnPreSnap() override;
	void OnSnap(int ClientID) override;
	void OnPostSnap() override;
	
	void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) override;
	void OnClientConnected(int ClientID) override;
	void ChangeWorld(int ClientID) override;
	void UpdateQuestsBot(int QuestID, int Step) override;

	void OnClientEnter(int ClientID) override;
	void OnClientDrop(int ClientID, const char *pReason, bool ChangeWorld = false) override;
	void OnClientDirectInput(int ClientID, void *pInput) override;
	void OnClientPredictedInput(int ClientID, void *pInput) override;
	bool IsClientReady(int ClientID) const override;
	bool IsClientPlayer(int ClientID) const override;

	const char *GameType() const override;
	const char *Version() const override;
	const char *NetVersion() const override;

	void ClearClientData(int ClientID) override;
	int GetRank(int AuthID) override;

	/* #########################################################################
		CONSOLE GAMECONTEXT 
	######################################################################### */
private:
	static void ConParseSkin(IConsole::IResult *pResult, void *pUserData);
	static void ConGiveItem(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConAddCharacter(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainSettingUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainGameinfoUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void NewCommandHook(const CCommandManager::CCommand* pCommand, void* pContext);
	static void RemoveCommandHook(const CCommandManager::CCommand* pCommand, void* pContext);

	/* #########################################################################
		VOTING MMO GAMECONTEXT 
	######################################################################### */
	struct CVoteOptions
	{
		char m_aDescription[VOTE_DESC_LENGTH];
		char m_aCommand[VOTE_CMD_LENGTH];
		int m_TempID;
		int m_TempID2;
	};
	std::list<CVoteOptions> m_PlayerVotes[MAX_PLAYERS];

public:
	void ClearVotes(int ClientID);
	void AV(int To, const char *Cmd, const char *Desc, const int ID = -1, const int ID2 = -1, const char *Icon = "unused");
	void AVL(int To, const char* aCmd, const char* pText, ...);
	void AVH(int To, const int ID, vec3 Color, const char* pText, ...);
	void AVHI(int To, const char *Icon, const int ID, vec3 Color, const char* pText, ...);

	void AVM(int To, const char* Type, const int ID, const int HideID, const char* pText, ...);
	void AVMI(int To, const char *Icon, const char *Type, const int ID, const int HideID, const char *pText, ...);
	void AVD(int To, const char* Type, const int ID, const int ID2, const int HideID, const char* pText, ...);

	void ResetVotes(int ClientID, int MenuList);
	void VResetVotes(int ClientID, int MenuID);
	void AddBack(int ClientID);
	void ShowPlayerStats(CPlayer *pPlayer);
	bool ParseVote(int ClientID, const char *CMD, const int VoteID, const int VoteID2, int Get, const char *Text);

	/* #########################################################################
		MMO GAMECONTEXT 
	######################################################################### */
	void CreateBot(short SpawnPoint, int BotID, int SubID);
	void CreateText(CEntity *pParent, bool Follow, vec2 Pos, vec2 Vel, int Lifespan, const char *pText, int WorldID);
	void CreateDropBonuses(vec2 Pos, int Type, int Count, int NumDrop = 1, vec2 Force = vec2(0.0f, 0.0f));
	void CreateDropItem(vec2 Pos, int ClientID, int ItemID, int Count, int Enchant = 0, vec2 Force = vec2(0.0f, 0.0f));
	void CreateDropItem(vec2 Pos, int ClientID, ItemJob::ItemPlayer &PlayerItem, int Count, vec2 Force = vec2(0.0f, 0.0f));
	bool TakeItemCharacter(int ClientID);
	void SendInbox(int ClientID, const char* Name, const char* Desc, int ItemID = -1, int Count = -1, int Enchant = -1);

private:
	void SendDayInfo(int ClientID);

public:
	void ChangeEquipSkin(int ClientID, int ItemID);
	void ClearInteractiveSub(int ClientID);

	bool CheckClient(int ClientID) const;
	int GetWorldID() const { return m_WorldID; }
	int DungeonID() const { return m_DungeonID; }
	bool IsDungeon() const { return (m_DungeonID > 0); }
	int IncreaseCountRaid(int IncreaseCount) const;
	bool IsClientEqualWorldID(int ClientID, int WorldID = -1) const;
	bool IsAllowedPVP() const { return m_AllowedPVP; }
	const char* AtributeName(int BonusID) const;

private:
	void UpdateZoneDungeon();
	void UpdateZonePVP();

	/* #########################################################################
		FUNCTIONS PLAYER ITEMS
	######################################################################### */
	bool m_AllowedPVP;
	int m_DayEnumType;
	static int m_RaidExp;
};

inline int64 CmaskAll() { return -1; }
inline int64 CmaskOne(int ClientID) { return (int64)1<<ClientID; }
inline int64 CmaskAllExceptOne(int ClientID) { return CmaskAll()^CmaskOne(ClientID); }
inline bool CmaskIsSet(int64 Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }

#endif
