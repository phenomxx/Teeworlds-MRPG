/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "player.h"

#include "gamecontext.h"
#include "gamemodes/dungeon.h"

#include "mmocore/Components/Accounts/AccountCore.h"
#include "mmocore/Components/Bots/BotCore.h"
#include "mmocore/Components/Dungeons/DungeonData.h"
#include "mmocore/Components/Guilds/GuildCore.h"
#include "mmocore/Components/Quests/QuestCore.h"
#include "mmocore/Components/Worlds/WorldSwapCore.h"

#include "mmocore/Components/Inventory/ItemData.h"
#include "mmocore/Components/Skills/SkillData.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS * ENGINE_MAX_WORLDS + MAX_CLIENTS)

IServer* CPlayer::Server() const { return m_pGS->Server(); };
CPlayer::CPlayer(CGS *pGS, int ClientID) : m_pGS(pGS), m_ClientID(ClientID)
{
	for(short& SortTab : m_aSortTabs)
		SortTab = 0;

	m_Spawned = true;
	m_aPlayerTick[TickState::Respawn] = Server()->Tick() + Server()->TickSpeed();
	m_aPlayerTick[TickState::Die] = Server()->Tick();
	m_PrevTuningParams = *pGS->Tuning();
	m_NextTuningParams = m_PrevTuningParams;

	// constructor only for players
	if(m_ClientID < MAX_PLAYERS)
	{
		m_LastVoteMenu = NOPE;
		m_OpenVoteMenu = MenuList::MAIN_MENU;
		m_MoodState = MOOD_NORMAL;
		Acc().m_Team = GetStartTeam();
		GS()->SendTuningParams(ClientID);
		ClearTalking();
	}
}

CPlayer::~CPlayer()
{
	m_aHiddenMenu.clear();
	delete m_pCharacter;
	m_pCharacter = nullptr;
}

/* #########################################################################
	FUNCTIONS PLAYER ENGINE
######################################################################### */
void CPlayer::Tick()
{
	if (!Server()->ClientIngame(m_ClientID))
		return;

	if(!m_pCharacter && GetTeam() == TEAM_SPECTATORS)
		m_ViewPos -= vec2(clamp(m_ViewPos.x - m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y - m_LatestActivity.m_TargetY, -400.0f, 400.0f));

	if(!IsAuthed())
		return;

	Server()->SetClientScore(m_ClientID, Acc().m_Level);
	{
		IServer::CClientInfo Info;
		if (Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}

		if (Server()->Tick() % Server()->TickSpeed() == 0)
		{
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if (m_pCharacter)
	{
		if(m_pCharacter->IsAlive())
			m_ViewPos = m_pCharacter->GetPos();
		else
		{
			delete m_pCharacter;
			m_pCharacter = nullptr;
		}
	}
	else if (m_Spawned && m_aPlayerTick[TickState::Respawn] + Server()->TickSpeed() * 3 <= Server()->Tick())
		TryRespawn();

	// update player tick
	TickSystemTalk();
	HandleTuningParams();
	EffectsTick();
}

void CPlayer::PostTick()
{
	// update latency value
	if (Server()->ClientIngame(m_ClientID) && GS()->IsPlayerEqualWorldID(m_ClientID) && IsAuthed())
		GetTempData().m_TempPing = m_Latency.m_Min;
}

void CPlayer::EffectsTick()
{
	if(Server()->Tick() % Server()->TickSpeed() != 0 || CGS::ms_aEffects[m_ClientID].empty())
		return;

	for(auto pEffect = CGS::ms_aEffects[m_ClientID].begin(); pEffect != CGS::ms_aEffects[m_ClientID].end();)
	{
		pEffect->second--;
		if(pEffect->second <= 0)
		{
			if(m_pCharacter && m_pCharacter->IsAlive())
				GS()->CreateTextEffect(m_pCharacter->m_Core.m_Pos, pEffect->first.c_str(), TEXTEFFECT_FLAG_POTION|TEXTEFFECT_FLAG_REMOVING);
			GS()->Chat(m_ClientID, "You lost the effect {STR}.", pEffect->first.c_str());
			pEffect = CGS::ms_aEffects[m_ClientID].erase(pEffect);
			continue;
		}
		++pEffect;
	}
}

void CPlayer::TickSystemTalk()
{
	if(m_DialogNPC.m_TalkedID == -1 || m_DialogNPC.m_TalkedID == m_ClientID)
		return;

	const int TalkedID = m_DialogNPC.m_TalkedID;
	if(!m_pCharacter || TalkedID < MAX_PLAYERS || !GS()->m_apPlayers[TalkedID] || distance(m_ViewPos, GS()->m_apPlayers[TalkedID]->m_ViewPos) > 180.0f)
		ClearTalking();
}

void CPlayer::HandleTuningParams()
{
	if(!(m_PrevTuningParams == m_NextTuningParams))
	{
		if(GetCharacter())
		{
			CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
			int *pParams = (int *)&m_NextTuningParams;
			for(unsigned i = 0; i < sizeof(m_NextTuningParams)/sizeof(int); i++)
				Msg.AddInt(pParams[i]);
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_ClientID);
		}
		m_PrevTuningParams = m_NextTuningParams;
	}
	m_NextTuningParams = *GS()->Tuning();
}

void CPlayer::Snap(int SnappingClient)
{
	if(!Server()->ClientIngame(m_ClientID))
		return;

	CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_PlayerFlags = m_PlayerFlags&PLAYERFLAG_CHATTING;
	pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_READY;
	if(Server()->IsAuthed(m_ClientID))
		pPlayerInfo->m_PlayerFlags |= PLAYERFLAG_ADMIN;

	pPlayerInfo->m_Latency = (SnappingClient == -1 ? m_Latency.m_Min : GetTempData().m_TempPing);
	pPlayerInfo->m_Score = Acc().m_Level;

	// --------------------- CUSTOM ----------------------
	if(!GS()->IsMmoClient(SnappingClient) || GetTeam() == TEAM_SPECTATORS || !IsAuthed())
		return;

	CNetObj_Mmo_ClientInfo *pClientInfo = static_cast<CNetObj_Mmo_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_MMO_CLIENTINFO, m_ClientID, sizeof(CNetObj_Mmo_ClientInfo)));
	if(!pClientInfo)
		return;

	const bool localClient = (bool)(m_ClientID == SnappingClient);
	pClientInfo->m_Local = localClient;
	pClientInfo->m_WorldType = GS()->Mmo()->WorldSwap()->GetWorldType();
	pClientInfo->m_MoodType = m_MoodState;
	pClientInfo->m_Level = Acc().m_Level;
	pClientInfo->m_Exp = Acc().m_Exp;
	pClientInfo->m_Health = GetHealth();
	pClientInfo->m_HealthStart = GetStartHealth();
	pClientInfo->m_Armor = GetMana();

	dynamic_string Buffer;
	for (auto& eff : CGS::ms_aEffects[m_ClientID])
	{
		char aBuf[32];
		const bool Minutes = eff.second >= 60;
		str_format(aBuf, sizeof(aBuf), "%s %d%s ", eff.first.c_str(), Minutes ? eff.second / 60 : eff.second, Minutes ? "m" : "");
		Buffer.append_at(Buffer.length(), aBuf);
	}
	StrToInts(pClientInfo->m_Potions, 12, Buffer.buffer());
	Buffer.clear();

	Server()->Localization()->Format(Buffer, GetLanguage(), "{INT}", GetItem(itGold).m_Value);
	StrToInts(pClientInfo->m_Gold, 6, Buffer.buffer());
	Buffer.clear();

	if(Acc().IsGuild())
	{
		char aBuf[24];
		const int GuildID = Acc().m_GuildID;
		str_format(aBuf, sizeof(aBuf), "%s %s", GS()->Mmo()->Member()->GetGuildRank(GuildID, Acc().m_GuildRank), GS()->Mmo()->Member()->GuildName(GuildID));
		StrToInts(pClientInfo->m_StateName, 6, aBuf);
	}
	else
		StrToInts(pClientInfo->m_StateName, 6, "\0");
}

CCharacter *CPlayer::GetCharacter() const
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return nullptr;
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;
	int SpawnType = SPAWN_HUMAN;
	if(GetTempData().m_TempSafeSpawn)
	{
		const int SafezoneWorldID = GS()->GetRespawnWorld();
		if(SafezoneWorldID >= 0 && !GS()->IsPlayerEqualWorldID(m_ClientID, SafezoneWorldID))
		{
			ChangeWorld(SafezoneWorldID);
			return;
		}
		SpawnType = SPAWN_HUMAN_SAFE;
	}

	if(!GS()->m_pController->CanSpawn(SpawnType, &SpawnPos, vec2(-1, -1)))
		return;

	if(!GS()->IsDungeon() && length(GetTempData().m_TempTeleportPos) > 0.0f)
	{
		SpawnPos = GetTempData().m_TempTeleportPos;
		GetTempData().m_TempTeleportPos = vec2(-1, -1);
	}

	const int AllocMemoryCell = MAX_CLIENTS*GS()->GetWorldID()+m_ClientID;
	m_pCharacter = new(AllocMemoryCell) CCharacter(&GS()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GS()->CreatePlayerSpawn(SpawnPos);
	m_Spawned = false;
}

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);
		delete m_pCharacter;
		m_pCharacter = nullptr;
	}
}

void CPlayer::OnDisconnect()
{
	KillCharacter();
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
		m_pCharacter->OnDirectInput(NewInput);

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput) const
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter)
		m_pCharacter->OnPredictedInput(NewInput);
}

int CPlayer::GetTeam()
{
	if(GS()->Mmo()->Account()->IsActive(m_ClientID))
		return Acc().m_Team;
	return TEAM_SPECTATORS;
}

/* #########################################################################
	FUNCTIONS PLAYER HELPER
######################################################################### */
void CPlayer::ProgressBar(const char *Name, int MyLevel, int MyExp, int ExpNeed, int GivedExp) const
{
	if (GS()->IsMmoClient(m_ClientID))
	{
		GS()->SendProgressBar(m_ClientID, MyExp, ExpNeed, Name);
		return;
	}

	char aBufBroadcast[128];
	const float GetLevelProgress = (float)(MyExp * 100.0) / (float)ExpNeed;
	const float GetExpProgress = (float)(GivedExp * 100.0) / (float)ExpNeed;
	const std::unique_ptr<char[]> Level = std::move(GS()->LevelString(100, (int)GetLevelProgress, 10, ':', ' '));
	str_format(aBufBroadcast, sizeof(aBufBroadcast), "^235Lv%d %s%s %0.2f%%+%0.3f%%(%d)XP\n", MyLevel, Name, Level.get(), GetLevelProgress, GetExpProgress, GivedExp);
	GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_INFORMATION, 100, aBufBroadcast);
}

bool CPlayer::Upgrade(int Value, int *Upgrade, int *Useless, int Price, int MaximalUpgrade) const
{
	const int UpgradeNeed = Price*Value;
	if((*Upgrade + Value) > MaximalUpgrade)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_WARNING, 100, "Upgrade has a maximum level.");
		return false;
	}

	if(*Useless < UpgradeNeed)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_WARNING, 100, "Not upgrade points for +{INT}. Required {INT}.", Value, UpgradeNeed);
		return false;
	}

	*Useless -= UpgradeNeed;
	*Upgrade += Value;
	return true;
}

/* #########################################################################
	FUNCTIONS PLAYER ACCOUNT
######################################################################### */
bool CPlayer::SpendCurrency(int Price, int ItemID)
{
	if (Price <= 0)
		return true;

	CItemData& pItemPlayer = GetItem(ItemID);
	if(pItemPlayer.m_Value < Price)
	{
		GS()->Chat(m_ClientID,"Required {INT}, but you have only {INT} {STR}!", Price, pItemPlayer.m_Value, pItemPlayer.Info().GetName());
		return false;
	}
	return pItemPlayer.Remove(Price);
}

void CPlayer::GiveEffect(const char* Potion, int Sec, float Chance)
{
	if(!m_pCharacter || !m_pCharacter->IsAlive())
		return;

	const float RandomChance = frandom() * 100.0f;
	if(RandomChance < Chance)
	{
		GS()->Chat(m_ClientID, "You got the effect {STR} time {INT}sec.", Potion, Sec);
		CGS::ms_aEffects[m_ClientID][Potion] = Sec;
		GS()->CreateTextEffect(m_pCharacter->m_Core.m_Pos, Potion, TEXTEFFECT_FLAG_POTION|TEXTEFFECT_FLAG_ADDING);
	}
}

bool CPlayer::IsActiveEffect(const char* Potion) const
{
	return CGS::ms_aEffects[m_ClientID].find(Potion) != CGS::ms_aEffects[m_ClientID].end();
}

void CPlayer::ClearEffects()
{
	CGS::ms_aEffects[m_ClientID].clear();
}

const char *CPlayer::GetLanguage() const
{
	return Server()->GetClientLanguage(m_ClientID);
}

void CPlayer::UpdateTempData(int Health, int Mana)
{
	GetTempData().m_TempHealth = Health;
	GetTempData().m_TempMana = Mana;
}

void CPlayer::AddExp(int Exp)
{
	Acc().m_Exp += Exp;
	for( ; Acc().m_Exp >= ExpNeed(Acc().m_Level); )
	{
		Acc().m_Exp -= ExpNeed(Acc().m_Level), Acc().m_Level++;
		Acc().m_Upgrade += 10;

		GS()->CreateDeath(m_pCharacter->m_Core.m_Pos, m_ClientID);
		GS()->CreateSound(m_pCharacter->m_Core.m_Pos, 4);
		GS()->CreateText(m_pCharacter, false, vec2(0, -40), vec2(0, -1), 30, "level");
		GS()->ChatFollow(m_ClientID, "Level UP. Now Level {INT}!", Acc().m_Level);
		if(Acc().m_Exp < ExpNeed(Acc().m_Level))
		{
			GS()->StrongUpdateVotes(m_ClientID, MenuList::MAIN_MENU);
			GS()->Mmo()->SaveAccount(this, SaveType::SAVE_STATS);
			GS()->Mmo()->SaveAccount(this, SaveType::SAVE_UPGRADES);
		}
	}
	ProgressBar("Account", Acc().m_Level, Acc().m_Exp, ExpNeed(Acc().m_Level), Exp);

	if (rand() % 5 == 0)
		GS()->Mmo()->SaveAccount(this, SaveType::SAVE_STATS);

	if (Acc().IsGuild())
		GS()->Mmo()->Member()->AddExperience(Acc().m_GuildID);
}

void CPlayer::AddMoney(int Money)
{
	GetItem(itGold).Add(Money);
}

bool CPlayer::GetHidenMenu(int HideID) const
{
	if(m_aHiddenMenu.find(HideID) != m_aHiddenMenu.end())
		return m_aHiddenMenu.at(HideID);

	return false;
}

bool CPlayer::IsAuthed() const
{
	if(GS()->Mmo()->Account()->IsActive(m_ClientID))
		return (bool)(Acc().m_UserID > 0);
	return false;
}

int CPlayer::GetStartTeam() const
{
	if(IsAuthed())
		return TEAM_RED;
	return TEAM_SPECTATORS;
}

int CPlayer::ExpNeed(int Level)
{
	return computeExperience(Level);
}

int CPlayer::GetStartHealth()
{
	return 10 + GetAttributeCount(Stats::StHardness, true);
}

int CPlayer::GetStartMana()
{
	const int EnchantBonus = GetAttributeCount(Stats::StPiety, true);
	return 10 + EnchantBonus;
}

void CPlayer::ShowInformationStats(BroadcastPriority Priority)
{
	if (!m_pCharacter)
		return;

	const int Health = GetHealth();
	const int StartHealth = GetStartHealth();
	const int Mana = GetMana();
	const int StartMana = GetStartMana();
	GS()->Broadcast(m_ClientID, Priority, 100, "H: {INT}/{INT} M: {INT}/{INT}", Health, StartHealth, Mana, StartMana);
}

/* #########################################################################
	FUNCTIONS PLAYER PARSING
######################################################################### */
bool CPlayer::ParseItemsF3F4(int Vote)
{
	if (!m_pCharacter)
	{
		GS()->Chat(m_ClientID, "Use it when you're not dead!");
		return true;
	}

	// - - - - - F3- - - - - - -
	if (Vote == 1)
	{
		if(GS()->IsDungeon())
		{
			const int DungeonID = GS()->GetDungeonID();
			if(!CDungeonData::ms_aDungeon[DungeonID].IsDungeonPlaying())
			{
				GetTempData().m_TempDungeonReady ^= true;
				GS()->Chat(m_ClientID, "You change the ready mode to {STR}!", GetTempData().m_TempDungeonReady ? "ready" : "not ready");
			}
			return true;
		}
	}
	// - - - - - F4- - - - - - -
	else
	{
		// conversations for vanilla clients
		if(GetTalkedID() > 0 && !GS()->IsMmoClient(m_ClientID))
		{
			if(m_aPlayerTick[TickState::LastDialog] && m_aPlayerTick[TickState::LastDialog] > GS()->Server()->Tick())
				return true;

			m_aPlayerTick[TickState::LastDialog] = GS()->Server()->Tick() + (GS()->Server()->TickSpeed() / 4);
			SetTalking(GetTalkedID(), false);
			return true;
		}

		if(m_PlayerFlags & PLAYERFLAG_SCOREBOARD && GetEquippedItemID(EQUIP_WINGS) > 0)
		{
			m_Flymode ^= true;
			GS()->Chat(m_ClientID, "You {STR} fly mode, your hook changes!", m_Flymode ? "Enable" : "Disable");
			return true;
		}
	}
	return false;
}
// vote parsing and improving statistics
bool CPlayer::ParseVoteUpgrades(const char *CMD, const int VoteID, const int VoteID2, int Get)
{
	if(PPSTR(CMD, "UPGRADE") == 0)
	{
		if(Upgrade(Get, &Acc().m_aStats[VoteID], &Acc().m_Upgrade, VoteID2, 1000))
		{
			GS()->Mmo()->SaveAccount(this, SaveType::SAVE_UPGRADES);
			GS()->ResetVotes(m_ClientID, MenuList::MENU_UPGRADE);
		}
		return true;
	}

	if(PPSTR(CMD, "HIDEN") == 0)
	{
		if(VoteID < TAB_STAT)
			return true;

		for(auto& x : m_aHiddenMenu)
		{
			if((x.first > NUM_TAB_MENU && x.first != VoteID))
				x.second = false;
		}

		m_aHiddenMenu[VoteID] ^= true;
		if(m_aHiddenMenu[VoteID] == false)
			m_aHiddenMenu.erase(VoteID);

		if(m_ActiveMenuRegisteredCallback)
			m_ActiveMenuRegisteredCallback(m_ActiveMenuOptionCallback);
		else
			GS()->StrongUpdateVotes(m_ClientID, m_OpenVoteMenu);
		return true;
	}
	return false;
}

CItemData& CPlayer::GetItem(int ItemID)
{
	CItemData::ms_aItems[m_ClientID][ItemID].m_ItemID = ItemID;
	CItemData::ms_aItems[m_ClientID][ItemID].SetItemOwner(this);
	return CItemData::ms_aItems[m_ClientID][ItemID];
}

CSkillData& CPlayer::GetSkill(int SkillID)
{
	CSkillData::ms_aSkills[m_ClientID][SkillID].m_SkillID = SkillID;
	CSkillData::ms_aSkills[m_ClientID][SkillID].SetSkillOwner(this);
	return CSkillData::ms_aSkills[m_ClientID][SkillID];
}

CQuestData& CPlayer::GetQuest(int QuestID)
{
	CQuestData::ms_aPlayerQuests[m_ClientID][QuestID].m_QuestID = QuestID;
	CQuestData::ms_aPlayerQuests[m_ClientID][QuestID].m_pPlayer = this;
	return CQuestData::ms_aPlayerQuests[m_ClientID][QuestID];
}

int CPlayer::GetEquippedItemID(int EquipID, int SkipItemID) const
{
	for(const auto& it : CItemData::ms_aItems[m_ClientID])
	{
		if(!it.second.m_Value || !it.second.m_Settings || it.second.Info().m_Function != EquipID || it.first == SkipItemID)
			continue;
		return it.first;
	}
	return -1;
}

int CPlayer::GetAttributeCount(int BonusID, bool ActiveFinalStats)
{
	int AttributEx = GetItemsAttributeCount(BonusID);

	// if the attribute has the value of player upgrades we sum up
	if (str_comp_nocase(CGS::ms_aAttributsInfo[BonusID].m_aFieldName, "unfield") != 0)
		AttributEx += Acc().m_aStats[BonusID];

	// to the final active attribute stats for the player
	if (ActiveFinalStats && CGS::ms_aAttributsInfo[BonusID].m_Devide > 0)
		AttributEx /= CGS::ms_aAttributsInfo[BonusID].m_Devide;

	// if the best tank class is selected among the players we return the sync dungeon stats
	if(GS()->IsDungeon() && CGS::ms_aAttributsInfo[BonusID].m_UpgradePrice < 10 && CDungeonData::ms_aDungeon[GS()->GetDungeonID()].IsDungeonPlaying())
	{
		CGameControllerDungeon* pDungeon = static_cast<CGameControllerDungeon*>(GS()->m_pController);
		return pDungeon->GetAttributeDungeonSync(this, BonusID);
	}
	return AttributEx;
}

// TODO: not optimized algorithm
int CPlayer::GetItemsAttributeCount(int AttributeID) const
{
	int SummingSize = 0;
	for(const auto& it : CItemData::ms_aItems[m_ClientID])
	{
		if(!it.second.IsEquipped() || !it.second.Info().IsEnchantable() || !it.second.Info().GetInfoEnchantStats(AttributeID))
			continue;

		SummingSize += it.second.GetEnchantStats(AttributeID);
	}
	return SummingSize;
}

int CPlayer::GetLevelTypeAttribute(int Class)
{
	int Attributs = 0;
	for (const auto& at : CGS::ms_aAttributsInfo)
	{
		if (at.second.m_Type == Class)
			Attributs += GetAttributeCount(at.first, true);
	}
	return Attributs;
}

int CPlayer::GetLevelAllAttributes()
{
	int Attributs = 0;
	for(const auto& at : CGS::ms_aAttributsInfo)
		Attributs += GetAttributeCount(at.first, true);

	return Attributs;
}

// - - - - - - T A L K I N G - - - - B O T S - - - - - - - - -
void CPlayer::SetTalking(int TalkedID, bool IsStartDialogue)
{
	if (TalkedID < MAX_PLAYERS || !GS()->m_apPlayers[TalkedID] || (!IsStartDialogue && m_DialogNPC.m_TalkedID == -1))
		return;

	m_DialogNPC.m_TalkedID = TalkedID;
	if(IsStartDialogue)
	{
		m_DialogNPC.m_FreezedProgress = false;
		m_DialogNPC.m_Progress = 0;
	}

	GS()->Mmo()->Quest()->QuestTableClear(m_ClientID);
	CPlayerBot* pBotPlayer = static_cast<CPlayerBot*>(GS()->m_apPlayers[TalkedID]);
	const int MobID = pBotPlayer->GetBotSub();
	if (pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC)
	{
		// clearing the end of dialogs or a dialog that was meaningless
		const int sizeTalking = NpcBotInfo::ms_aNpcBot[MobID].m_aDialog.size();
		const bool isTalkingEmpty = NpcBotInfo::ms_aNpcBot[MobID].m_aDialog.empty();
		if ((isTalkingEmpty && m_DialogNPC.m_Progress == IS_TALKING_EMPTY) || (!isTalkingEmpty && m_DialogNPC.m_Progress >= sizeTalking))
		{
			ClearTalking();
			return;
		}

		// you get to know in general if the quest is to give out a random senseless dialog
		int GivingQuestID = GS()->Mmo()->BotsData()->GetQuestNPC(MobID);
		if (isTalkingEmpty || GetQuest(GivingQuestID).GetState() >= QuestState::QUEST_ACCEPT)
		{
			const char* pMeaninglessDialog = GS()->Mmo()->BotsData()->GetMeaninglessDialog();
			GS()->Mmo()->BotsData()->DialogBotStepNPC(this, MobID, -1, TalkedID, pMeaninglessDialog);
			m_DialogNPC.m_Progress = IS_TALKING_EMPTY;
			return;
		}

		// get a quest for the progress of dialogue if it is in this progress we accept the quest
		GivingQuestID = NpcBotInfo::ms_aNpcBot[MobID].m_GivesQuestID;
		if (((m_DialogNPC.m_Progress + 1) >= sizeTalking) && GivingQuestID >= 1)
		{
			if(!m_DialogNPC.m_FreezedProgress)
			{
				GS()->Mmo()->BotsData()->DialogBotStepNPC(this, MobID, m_DialogNPC.m_Progress, TalkedID);
				m_DialogNPC.m_FreezedProgress = true;
				return;
			}

			GetQuest(GivingQuestID).Accept();
			m_DialogNPC.m_Progress++;
		}

		GS()->Mmo()->BotsData()->DialogBotStepNPC(this, MobID, m_DialogNPC.m_Progress, TalkedID);
	}

	else if (pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST)
	{
		const int sizeTalking = QuestBotInfo::ms_aQuestBot[MobID].m_aDialog.size();
		if (m_DialogNPC.m_Progress >= sizeTalking)
		{
			ClearTalking();
			GS()->Mmo()->Quest()->InteractiveQuestNPC(this, QuestBotInfo::ms_aQuestBot[MobID], true);
			return;
		}

		const bool RequiestQuestTask = QuestBotInfo::ms_aQuestBot[MobID].m_aDialog[m_DialogNPC.m_Progress].m_RequestAction;
		if (RequiestQuestTask)
		{
			if (!m_DialogNPC.m_FreezedProgress)
			{
				GS()->Mmo()->Quest()->DoStepDropTakeItems(this, QuestBotInfo::ms_aQuestBot[MobID]);
				GS()->Mmo()->BotsData()->DialogBotStepQuest(this, MobID, m_DialogNPC.m_Progress, TalkedID);
				GS()->Mmo()->BotsData()->ShowBotQuestTaskInfo(this, MobID, m_DialogNPC.m_Progress);
				m_DialogNPC.m_FreezedProgress = true;
				return;
			}

			// skip non complete dialog quest
			if (!GS()->Mmo()->Quest()->InteractiveQuestNPC(this, QuestBotInfo::ms_aQuestBot[MobID], false))
			{
				GS()->Mmo()->BotsData()->DialogBotStepQuest(this, MobID, m_DialogNPC.m_Progress, TalkedID);
				GS()->Mmo()->BotsData()->ShowBotQuestTaskInfo(this, MobID, m_DialogNPC.m_Progress);
				return;
			}
			else
				m_DialogNPC.m_Progress++;
		}

		GS()->Mmo()->BotsData()->DialogBotStepQuest(this, MobID, m_DialogNPC.m_Progress, TalkedID);
	}

	m_DialogNPC.m_Progress++;
}

void CPlayer::ClearTalking()
{
	GS()->ClearDialogText(m_ClientID);
	m_DialogNPC.m_TalkedID = -1;
	m_DialogNPC.m_Progress = 0;
	m_DialogNPC.m_FreezedProgress = false;
}

// - - - - - - F O R M A T - - - - - T E X T - - - - - - - - -
const char *CPlayer::GetDialogText() const
{
	return m_aFormatDialogText;
}

void CPlayer::FormatDialogText(int DataBotID, const char *pText) // TODO: perform profiling and debugging to check the performance
{
	if(!DataBotInfo::IsDataBotValid(DataBotID) || m_aFormatDialogText[0] != '\0')
		return;

	str_copy(m_aFormatDialogText, GS()->Server()->Localization()->Localize(GetLanguage(), pText), sizeof(m_aFormatDialogText));

	// arrays replacing dialogs
	const char* pBot = str_find_nocase(m_aFormatDialogText, "[Bot_");
	while(pBot != nullptr)
	{
		int SearchBotID = 0;
		if(sscanf(pBot, "[Bot_%d]", &SearchBotID) && DataBotInfo::IsDataBotValid(SearchBotID))
		{
			char aBufSearch[16];
			str_format(aBufSearch, sizeof(aBufSearch), "[Bot_%d]", SearchBotID);
			str_replace(m_aFormatDialogText, aBufSearch, DataBotInfo::ms_aDataBot[SearchBotID].m_aNameBot);
		}
		pBot = str_find_nocase(m_aFormatDialogText, "[Bot_");
	}

	const char* pWorld = str_find_nocase(m_aFormatDialogText, "[World_");
	while(pWorld != nullptr)
	{
		int WorldID = 0;
		if(sscanf(pWorld, "[World_%d]", &WorldID))
		{
			char aBufSearch[16];
			str_format(aBufSearch, sizeof(aBufSearch), "[World_%d]", WorldID);
			str_replace(m_aFormatDialogText, aBufSearch, Server()->GetWorldName(WorldID));
		}
		pWorld = str_find_nocase(m_aFormatDialogText, "[World_");
	}

	// based replacing dialogs
	str_replace(m_aFormatDialogText, "[Player]", GS()->Server()->ClientName(m_ClientID));
	str_replace(m_aFormatDialogText, "[Talked]", DataBotInfo::ms_aDataBot[DataBotID].m_aNameBot);
	str_replace(m_aFormatDialogText, "[Time]", GS()->Server()->GetStringTypeDay());
	str_replace(m_aFormatDialogText, "[Here]", GS()->Server()->GetWorldName(GS()->GetWorldID()));
}

void CPlayer::ClearDialogText()
{
	mem_zero(m_aFormatDialogText, sizeof(m_aFormatDialogText));
}

void CPlayer::ChangeWorld(int WorldID)
{
	// reset dungeon temp data
	GetTempData().m_TempAlreadyVotedDungeon = false;
	GetTempData().m_TempDungeonReady = false;
	GetTempData().m_TempTankVotingDungeon = 0;
	GetTempData().m_TempTimeDungeon = 0;

	// change worlds
	Acc().m_aHistoryWorld.push_front(WorldID);
	Server()->ChangeWorld(m_ClientID, WorldID);
}

void CPlayer::SendClientInfo(int TargetID)
{
	if(TargetID != -1 && (TargetID < 0 || TargetID >= MAX_PLAYERS || !Server()->ClientIngame(TargetID)))
		return;

	CNetMsg_Sv_ClientInfo ClientInfoMsg;
	ClientInfoMsg.m_ClientID = m_ClientID;
	ClientInfoMsg.m_Local = (bool)(m_ClientID == TargetID);
	ClientInfoMsg.m_Team = GetTeam();
	ClientInfoMsg.m_pName = Server()->ClientName(m_ClientID);
	ClientInfoMsg.m_pClan = Server()->ClientClan(m_ClientID);
	ClientInfoMsg.m_Country = Server()->ClientCountry(m_ClientID);
	ClientInfoMsg.m_Silent = (bool)(IsAuthed());
	for (int p = 0; p < 6; p++)
	{
		ClientInfoMsg.m_apSkinPartNames[p] = Acc().m_aaSkinPartNames[p];
		ClientInfoMsg.m_aUseCustomColors[p] = Acc().m_aUseCustomColors[p];
		ClientInfoMsg.m_aSkinPartColors[p] = Acc().m_aSkinPartColors[p];
	}

	// player data it static have accept it all worlds
	Server()->SendPackMsg(&ClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, TargetID);
}

int CPlayer::GetPlayerWorldID() const
{
	return Server()->GetClientWorldID(m_ClientID);
}