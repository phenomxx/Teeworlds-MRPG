/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/mapitems.h>
#include "character_bot_ai.h"
#include <game/server/playerbot.h>

#include <game/server/mmocore/GameEntities/Skills/healthturret/hearth.h>

MACRO_ALLOC_POOL_ID_IMPL(CCharacterBotAI, MAX_CLIENTS*COUNT_WORLD+MAX_CLIENTS)

CCharacterBotAI::CCharacterBotAI(CGameWorld *pWorld) : CCharacter(pWorld) {}

CCharacterBotAI::~CCharacterBotAI() 
{
	RemoveSnapProj(100, GetSnapFullID());
	RemoveSnapProj(100, GetSnapFullID(), true);
}

int CCharacterBotAI::GetSnapFullID() const { return m_pBotPlayer->GetCID() * SNAPBOTS; }

bool CCharacterBotAI::Spawn(class CPlayer *pPlayer, vec2 Pos)
{
	m_pBotPlayer = static_cast<CPlayerBot*>(pPlayer);
	if(!m_pBotPlayer)
		return false;

	if(!CCharacter::Spawn(pPlayer, Pos))
		return false;

	ClearTarget();

	// информация о зарождении моба
	const int SubBotID = m_pBotPlayer->GetBotSub();
	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB && BotJob::MobBot[SubBotID].Boss)
	{
		for(int i = 0; i < 3; i++)
		{
			CreateSnapProj(GetSnapFullID(), 1, PICKUP_HEALTH, true, false);
			CreateSnapProj(GetSnapFullID(), 1, WEAPON_HAMMER, false, true);
		}
		if (!GS()->IsDungeon())
			GS()->ChatWorldID(BotJob::MobBot[SubBotID].WorldID, "", "In your zone emerging {STR}!", BotJob::MobBot[SubBotID].GetName());
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST)
	{
		m_Core.m_LostData = true;
		CreateSnapProj(GetSnapFullID(), 2, PICKUP_HEALTH, true, false);
		CreateSnapProj(GetSnapFullID(), 2, PICKUP_ARMOR, true, false);
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC)
	{
		m_NoAllowDamage = true;
		m_Core.m_ProtectHooked = true;

		const int Function = BotJob::NpcBot[SubBotID].Function;
		if(Function == FunctionsNPC::FUNCTION_NPC_NURSE)
			new CEntityFunctionNurse(&GS()->m_World, m_pBotPlayer->GetCID(), m_Core.m_Pos);
		else if(Function == FunctionsNPC::FUNCTION_NPC_GIVE_QUEST)
			CreateSnapProj(GetSnapFullID(), 3, PICKUP_ARMOR, false, false);
	}
	return true;
}

void CCharacterBotAI::ShowProgress()
{
	for(const auto & ListDmgPlayer : m_ListDmgPlayers)
	{
		CPlayer *pPlayer = GS()->GetPlayer(ListDmgPlayer.first, true);
		if(pPlayer)
		{
			int Health = m_pBotPlayer->GetHealth();
			int StartHealth = m_pBotPlayer->GetStartHealth();
			float gethp = ( Health * 100.0 ) / StartHealth;
			char *Progress = GS()->LevelString(100, (int)gethp, 10, ':', ' ');
			GS()->SBL(ListDmgPlayer.first, BroadcastPriority::BROADCAST_GAME_PRIORITY, 10, "Health {STR}({INT}/{INT})", Progress, &Health, &StartHealth);
			delete Progress;
		}
	}	
}

bool CCharacterBotAI::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	CPlayer* pFrom = GS()->GetPlayer(From, true);
	if (!pFrom)
		return false;

	if(m_pBotPlayer->GetBotType() != BotsTypes::TYPE_BOT_MOB || pFrom->IsBot())
		return false;

	// до урона и после урона здоровье
	int StableDamage = Health();
	bool BotDie = CCharacter::TakeDamage(Force, Dmg, From, Weapon);
	StableDamage -= Health();

	// установить агрессию на того от кого пришел урон
	if (From != m_pBotPlayer->GetCID())
	{
		m_ListDmgPlayers[From] += StableDamage;
		if (m_BotTargetID == m_pBotPlayer->GetCID())
			SetTarget(From);
	}

	// проверка при смерте
	if(BotDie)
	{
		for(const auto& ld : m_ListDmgPlayers)
		{
			const int ParseClientID = ld.first;
			CPlayer* pPlayer = GS()->GetPlayer(ParseClientID, true, true);
			if(!pPlayer || ParseClientID == m_pBotPlayer->GetCID() || distance(pPlayer->m_ViewPos, m_Core.m_Pos) > 1000.0f)
				continue;

			DieRewardPlayer(pPlayer, Force);
		}
		ClearTarget();
		Die(From, Weapon);
		return true;
	}
	return false;
}

void CCharacterBotAI::Die(int Killer, int Weapon)
{
	if(m_pBotPlayer->GetBotType() != BotsTypes::TYPE_BOT_MOB)
		return;

	m_ListDmgPlayers.clear();
	CCharacter::Die(Killer, Weapon);
}

void CCharacterBotAI::CreateRandomDropItem(int DropCID, int Random, int ItemID, int Count, vec2 Force)
{
	if (DropCID < 0 || DropCID >= MAX_PLAYERS || !GS()->m_apPlayers[DropCID] || !GS()->m_apPlayers[DropCID]->GetCharacter() || !IsAlive())
		return;

	const int RandomDrop = (Random == 0 ? 0 : random_int() % Random);
	if (RandomDrop == 0)
		GS()->CreateDropItem(m_Core.m_Pos, DropCID, ItemID, Count, 0, Force);
	return;
}

void CCharacterBotAI::DieRewardPlayer(CPlayer* pPlayer, vec2 ForceDies)
{
	const int ClientID = pPlayer->GetCID();
	const int BotID = m_pBotPlayer->GetBotID();
	const int SubID = m_pBotPlayer->GetBotSub();

	if (m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB)
		GS()->Mmo()->Quest()->AddMobProgress(pPlayer, BotID);

	for (int i = 0; i < 6; i++)
	{
		const int DropItem = BotJob::MobBot[SubID].DropItem[i];
		const int CountItem = BotJob::MobBot[SubID].CountItem[i];
		if (DropItem <= 0 || CountItem <= 0)
			continue;

		const int RandomDrop = BotJob::MobBot[SubID].RandomItem[i];
		CreateRandomDropItem(ClientID, RandomDrop, DropItem, CountItem, ForceDies);
	}

	const int MultiplierExperience = kurosio::computeExperience(BotJob::MobBot[SubID].Level) / g_Config.m_SvKillmobsIncreaseLevel;
	const int MultiplierRaid = clamp(GS()->IncreaseCountRaid(MultiplierExperience), 1, GS()->IncreaseCountRaid(MultiplierExperience));
	pPlayer->AddExp(MultiplierRaid);

	const int MultiplierDrops = clamp(MultiplierRaid / 2, 1, MultiplierRaid);
	GS()->CreateDropBonuses(m_Core.m_Pos, 1, MultiplierDrops, (1+random_int() % 2), ForceDies);

	const int MultiplierGolds = BotJob::MobBot[SubID].Power / g_Config.m_SvStrongGold;
	pPlayer->AddMoney(MultiplierGolds);
	if (random_int() % 80 == 0)
	{
		pPlayer->GetItem(itSkillPoint).Add(1);
		GS()->Chat(ClientID, "Skill points increased. Now ({INT}SP)", &pPlayer->GetItem(itSkillPoint).Count);
	}
}

void CCharacterBotAI::ClearTarget()
{
	const int FromID = m_pBotPlayer->GetCID();
	if (m_BotTargetID != FromID)
	{
		m_pBotPlayer->m_TargetPos = vec2(0, 0);
		m_EmotionsStyle = 1 + random_int() % 5;
		m_BotTargetID = FromID;
		m_BotTargetLife = 0;
		m_BotTargetCollised = 0;
	}
}

void CCharacterBotAI::SetTarget(int ClientID)
{
	m_EmotionsStyle = EMOTE_ANGRY;
	m_BotTargetID = ClientID;
}

void CCharacterBotAI::ChangeWeapons()
{
	const int randtime = 1+random_int()%3;
	if(Server()->Tick() % (Server()->TickSpeed()*randtime) == 0)
	{
		const int randomweapon = random_int()%5;	
		m_ActiveWeapon = clamp(randomweapon, (int)WEAPON_HAMMER, (int)WEAPON_LASER);
	}
}

void CCharacterBotAI::Tick()
{
	if(!GS()->CheckPlayersDistance(m_Core.m_Pos, 1000.0f) || !IsAlive())
		return;

	EngineBots();
	CCharacter::Tick();
}

// Интерактивы ботов
void CCharacterBotAI::EngineBots()
{
	ResetInput();
	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC)
	{
		const int tx = m_Pos.x + m_Input.m_Direction * 45.0f;
		if(tx < 0)  m_Input.m_Direction = 1;
		else if(tx >= GS()->Collision()->GetWidth() * 32.0f) m_Input.m_Direction = -1;

		m_LatestPrevInput = m_LatestInput;
		m_LatestInput = m_Input;

		EngineNPC();
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB)
		EngineMobs();
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST)
		EngineQuestMob();
}

// Интерактивы NPC
void CCharacterBotAI::EngineNPC()
{
	const int MobID = m_pBotPlayer->GetBotSub();
	const int EmoteBot = BotJob::NpcBot[MobID].Emote;
	EmoteActions(EmoteBot);

	// направление глаз
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int()%4- random_int()%8;
	m_Input.m_TargetX = (m_Input.m_Direction*10+1);

	bool PlayerFinding = false;
	if(BotJob::NpcBot[MobID].Function == FunctionsNPC::FUNCTION_NPC_NURSE)
		PlayerFinding = FunctionNurseNPC();
	else
		PlayerFinding = BaseFunctionNPC();

	const bool StaticBot = BotJob::NpcBot[MobID].Static;
	if (!PlayerFinding && !StaticBot && Server()->Tick() % (Server()->TickSpeed() * (m_Input.m_Direction == 0 ? 5 : 1)) == 0)
		m_Input.m_Direction = -1 + random_int() % 3;
}

// Интерактивы квестовых мобов
void CCharacterBotAI::EngineQuestMob()
{
	const int MobID = m_pBotPlayer->GetBotSub();
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int()%4- random_int()%8;
	m_Input.m_TargetX = (m_Input.m_Direction*10+1);
	EmoteActions(EMOTE_BLINK);

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pFind = GS()->GetPlayer(i, true, true);
		if (pFind && distance(pFind->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) < 128.0f
			&& !GS()->Collision()->IntersectLine(pFind->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0)
			&& m_pBotPlayer->IsActiveSnappingBot(i))
		{
			if (!(BotJob::QuestBot[MobID].m_Talk).empty())
				GS()->SBL(i, BroadcastPriority::BROADCAST_GAME_INFORMATION, 10, "Start dialog with NPC [attack hammer]");

			m_Input.m_TargetX = round_to_int(pFind->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
			m_Input.m_TargetY = round_to_int(pFind->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
			m_Input.m_Direction = 0;
		}
	}
}

// Интерактивы мобов враждебных
void CCharacterBotAI::EngineMobs()
{
	SetAim(m_pBotPlayer->m_TargetPos - m_Pos);
	const int MobID = m_pBotPlayer->GetBotSub();
	bool WeaponedBot = (BotJob::MobBot[MobID].Spread >= 1);
	if(WeaponedBot)
	{
		if(BotJob::MobBot[MobID].Boss)
			ShowProgress();
		ChangeWeapons();
	}

	CPlayer* pPlayer = SearchTenacityPlayer(1000.0f);
	if(pPlayer && pPlayer->GetCharacter())
	{
		m_pBotPlayer->m_TargetPos = pPlayer->GetCharacter()->GetPos();
		Action();
	}
	else if(Server()->Tick() > m_pBotPlayer->m_LastPosTick)
		m_pBotPlayer->m_TargetPos = vec2(0, 0);

	Move();
	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
		m_PrevDirection = m_Input.m_Direction;

	EmoteActions(m_EmotionsStyle);
}

void CCharacterBotAI::Move()
{
	int Index = -1;
	int ActiveWayPoints = 0;
	for(int i = 0; i < m_pBotPlayer->m_PathSize && i < 30 && !GS()->Collision()->IntersectLine(m_pBotPlayer->m_WayPoints[i], m_Pos, 0x0, 0x0); i++)
	{
		Index = i;
		ActiveWayPoints = i;
	}

	vec2 WayDir = vec2(0, 0);
	if(Index > -1)
		WayDir = normalize(m_pBotPlayer->m_WayPoints[Index] - GetPos());

	// установить направление
	if(WayDir.x < 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = -1;
	else if(WayDir.x > 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = 1;
	else
		m_Input.m_Direction = m_PrevDirection;

	// jumping
	const bool IsGround = IsGrounded();
	if(IsGround && WayDir.y < -0.5)
		m_Input.m_Jump = 1;

	// doublejump
	if(!IsGround && WayDir.y < -0.5 && m_Core.m_Vel.y > 0)
		m_Input.m_Jump = 1;

	const bool IsCollide = GS()->Collision()->IntersectLine(m_Pos, m_Pos + vec2(m_Input.m_Direction, 0) * 150, &m_WallPos, 0x0);
	if(IsCollide)
	{
		if(IsGround && GS()->Collision()->IntersectLine(m_WallPos, m_WallPos + vec2(0, -1) * 210, 0x0, 0x0))
			m_Input.m_Jump = 1;

		if(!IsGround && GS()->Collision()->IntersectLine(m_WallPos, m_WallPos + vec2(0, -1) * 125, 0x0, 0x0))
			m_Input.m_Jump = 1;
	}

	// if way points down dont jump
	if(m_Input.m_Jump == 1 && (WayDir.y >= 0 || ActiveWayPoints < 3))
		m_Input.m_Jump = 0;

	// jump over friend
	CCharacter* pChar = GameWorld()->IntersectCharacter(GetPos(), GetPos() + vec2(m_Input.m_Direction, 0) * 150, 16.0f, vec2(0,0), (CCharacter*)this);
	if(pChar)
		m_Input.m_Jump = 1;
	
	// hook
	if(ActiveWayPoints > 2 && !m_Input.m_Hook && (WayDir.x != 0 || WayDir.y != 0) && !pChar)
	{
		if(m_Core.m_HookState == HOOK_GRABBED && m_Core.m_HookedPlayer == -1)
		{
			vec2 HookVel = normalize(m_Core.m_HookPos - GetPos()) * GS()->Tuning()->m_HookDragAccel;
			if(HookVel.y > 0)
				HookVel.y *= 0.3f;
			if((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0))
				HookVel.x *= 0.95f;
			else
				HookVel.x *= 0.75f;

			vec2 Target = vec2(m_Input.m_TargetX, m_Input.m_TargetY);
			float ps = dot(Target, HookVel);
			if(ps > 0 || (Target.y < 0 && m_Core.m_Vel.y > 0.f && m_Core.m_HookTick < SERVER_TICK_SPEED + SERVER_TICK_SPEED / 2))
				m_Input.m_Hook = 1;
			if(m_Core.m_HookTick > 4 * SERVER_TICK_SPEED || length(m_Core.m_HookPos - GetPos()) < 20.0f)
				m_Input.m_Hook = 0;
		}
		else if(m_Core.m_HookState == HOOK_FLYING)
			m_Input.m_Hook = 1;
		else if(m_LatestInput.m_Hook == 0 && m_Core.m_HookState == HOOK_IDLE)
		{
			int NumDir = 32;
			vec2 HookDir(0.0f, 0.0f);
			float MaxForce = 0;
			for(int i = 0; i < NumDir; i++)
			{
				float a = 2 * i * pi / NumDir;
				vec2 dir = direction(a);
				vec2 Pos = GetPos() + dir * GS()->Tuning()->m_HookLength;

				if((GS()->Collision()->IntersectLine(GetPos(), Pos, &Pos, 0) & (CCollision::COLFLAG_SOLID | CCollision::COLFLAG_NOHOOK)) == CCollision::COLFLAG_SOLID)
				{
					vec2 HookVel = dir * GS()->Tuning()->m_HookDragAccel;
					if(HookVel.y > 0)
						HookVel.y *= 0.3f;
					if((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0))
						HookVel.x *= 0.95f;
					else
						HookVel.x *= 0.75f;

					HookVel += vec2(0, 1) * GS()->Tuning()->m_Gravity;

					float ps = dot(WayDir, HookVel);
					if(ps > MaxForce)
					{
						MaxForce = ps;
						HookDir = Pos - GetPos();
					}
				}
			}
			if(length(HookDir) > 32.f)
			{
				SetAim(HookDir);
				m_Input.m_Hook = 1;
			}
		}
	}

	// in case the bot stucks
	if(m_Pos.x != m_PrevPos.x)
		m_MoveTick = Server()->Tick();

	if(m_Pos.x == m_PrevPos.x && Server()->Tick() - m_MoveTick > Server()->TickSpeed() / 2)
	{
		m_Input.m_Direction = -m_Input.m_Direction;
		m_Input.m_Jump = 1;
		m_MoveTick = Server()->Tick();
	}
}

void CCharacterBotAI::Action()
{
	CPlayer* pPlayer = GS()->GetPlayer(m_BotTargetID, true, true);
	if(m_BotTargetID == m_pBotPlayer->GetCID() || !pPlayer)
		return;

	// fire
	if((m_Input.m_Fire & 1) != 0)
	{
		m_Input.m_Fire++;
		m_LatestInput.m_Fire++;
		return;
	}

	if((m_Input.m_Hook && m_Core.m_HookState == HOOK_IDLE) || m_ReloadTimer != 0)
		return;

	if(!(m_Input.m_Fire & 1))
	{
		m_LatestInput.m_Fire++;
		m_Input.m_Fire++;
	}
}

void CCharacterBotAI::SetAim(vec2 Dir)
{
	m_Input.m_TargetX = (int)Dir.x;
	m_Input.m_TargetY = (int)Dir.y;
	m_LatestInput.m_TargetX = (int)Dir.x;
	m_LatestInput.m_TargetY = (int)Dir.y;
}

// Поиск игрока среди людей
CPlayer *CCharacterBotAI::SearchPlayer(int Distance)
{
	for(int i = 0 ; i < MAX_PLAYERS; i ++)
	{
		if(!GS()->m_apPlayers[i] 
			|| !GS()->m_apPlayers[i]->GetCharacter() 
			|| distance(m_Core.m_Pos, GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos) > Distance
			|| GS()->Collision()->IntersectLine(GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos, m_Pos, 0, 0)
			|| Server()->GetWorldID(i) != GS()->GetWorldID())
			continue;
		return GS()->m_apPlayers[i];
	}
	return nullptr;
}

// Поиск игрока среди людей который имеет ярость выше всех
CPlayer *CCharacterBotAI::SearchTenacityPlayer(float Distance)
{
	const bool ActiveTargetID = m_BotTargetID != m_pBotPlayer->GetCID();
	if(!ActiveTargetID && (GS()->IsDungeon() || random_int() % 30 == 0))
	{
		CPlayer *pPlayer = SearchPlayer(Distance);
		if(pPlayer && pPlayer->GetCharacter()) 
			SetTarget(pPlayer->GetCID());
		return pPlayer;
	}

	// сбрасываем агрессию если игрок далеко
	CPlayer* pPlayer = GS()->GetPlayer(m_BotTargetID, true, true);
	if (ActiveTargetID && (!pPlayer 
		|| (pPlayer && (distance(m_Core.m_Pos, pPlayer->GetCharacter()->m_Core.m_Pos) > 600.0f || Server()->GetWorldID(m_BotTargetID) != GS()->GetWorldID()))))
		ClearTarget();

	// не враждебные мобы
	if (!ActiveTargetID || !pPlayer)
		return nullptr; 

	// сбрасываем время жизни таргета
	m_BotTargetCollised = GS()->Collision()->IntersectLine(pPlayer->GetCharacter()->m_Core.m_Pos, m_Pos, 0, 0);
	if (m_BotTargetLife && m_BotTargetCollised)
	{
		m_BotTargetLife--;
		if (!m_BotTargetLife)
			ClearTarget();
	}
	else
	{
		m_BotTargetLife = Server()->TickSpeed() * 2;
	}

	// ищем более сильного
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		// проверяем на дистанцию игрока
		CPlayer* pFinderHard = GS()->GetPlayer(i, true, true);
		if (!pFinderHard || distance(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > 600.0f)
			continue;

		// проверяем есть ли вкуснее игрокв для бота
		const bool FinderCollised = (bool)GS()->Collision()->IntersectLine(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0);
		if (!FinderCollised && ((m_BotTargetLife <= 10 && m_BotTargetCollised)
			|| pFinderHard->GetAttributeCount(Stats::StHardness, true) > pPlayer->GetAttributeCount(Stats::StHardness, true)))
			SetTarget(i);
	}

	return pPlayer;
}

void CCharacterBotAI::EmoteActions(int EmotionStyle)
{
	if (EmotionStyle < EMOTE_PAIN || EmotionStyle > EMOTE_BLINK)
		return;

	if ((Server()->Tick() % (Server()->TickSpeed() * 3 + random_int() % 10)) == 0)
	{
		if (EmotionStyle == EMOTE_BLINK)
		{
			SetEmote(EMOTE_BLINK, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_DOTDOT);

		}
		else if (EmotionStyle == EMOTE_HAPPY)
		{
			SetEmote(EMOTE_HAPPY, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), (random_int() % 2 == 0 ? (int)EMOTICON_HEARTS : (int)EMOTICON_EYES));

		}
		else if (EmotionStyle == EMOTE_ANGRY)
		{
			SetEmote(EMOTE_ANGRY, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), (EMOTICON_SPLATTEE + random_int() % 3));
		}		
		else if (EmotionStyle == EMOTE_PAIN)
		{
			SetEmote(EMOTE_PAIN, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_DROP);
		}
	}
}

// - - - - - - - - - - - - - - - - - - - BASE FUNCTION
bool CCharacterBotAI::BaseFunctionNPC()
{
	bool PlayerFinding = false;
	const int SubBotID = m_pBotPlayer->GetBotSub();
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pFind = GS()->GetPlayer(i, true, true);
		if(pFind && distance(pFind->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) < 128.0f &&
			!GS()->Collision()->IntersectLine(pFind->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0))
		{
			if(!(BotJob::NpcBot[SubBotID].m_Talk).empty())
				GS()->SBL(i, BroadcastPriority::BROADCAST_GAME_INFORMATION, 10, "Start dialog with NPC [attack hammer]");

			m_Input.m_TargetX = static_cast<int>(pFind->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
			m_Input.m_TargetY = static_cast<int>(pFind->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
			m_Input.m_Direction = 0;
			PlayerFinding = true;
		}
	}
	return PlayerFinding;
}

// - - - - - - - - - - - - - - - - - - - FUNCTION NURSE
bool CCharacterBotAI::FunctionNurseNPC()
{
	bool PlayerFinding = false;
	if(Server()->Tick() % Server()->TickSpeed() != 0)
	{
		CPlayer* pFind = SearchPlayer(256.0f);
		if(pFind && pFind->GetCharacter() && pFind->GetHealth() < pFind->GetStartHealth())
		{
			m_Input.m_TargetX = static_cast<int>(pFind->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
			m_Input.m_TargetY = static_cast<int>(pFind->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
		}
		return true;
	}

	char aBuf[16];
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pFind = GS()->GetPlayer(i, true, true);
		if(!pFind || pFind->GetHealth() >= pFind->GetStartHealth() ||
			distance(pFind->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) >= 256.0f ||
			GS()->Collision()->IntersectLine(pFind->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0))
			continue;

		m_Input.m_TargetX = static_cast<int>(pFind->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
		m_Input.m_TargetY = static_cast<int>(pFind->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
		m_LatestInput.m_TargetX = m_Input.m_TargetX;
		m_LatestInput.m_TargetY = m_Input.m_TargetX;

		const int Health = max(pFind->GetStartHealth() / 20, 1);
		vec2 DrawPosition = vec2(pFind->GetCharacter()->m_Core.m_Pos.x, pFind->GetCharacter()->m_Core.m_Pos.y - 90.0f);
		str_format(aBuf, sizeof(aBuf), "%dHP", Health);
		GS()->CreateText(NULL, false, DrawPosition, vec2(0, 0), 40, aBuf, GS()->GetWorldID());
		new CHearth(&GS()->m_World, m_Pos, pFind, Health, pFind->GetCharacter()->m_Core.m_Vel);

		m_Input.m_Direction = 0;
		PlayerFinding = true;
	}
	return PlayerFinding;
}

CEntityFunctionNurse::CEntityFunctionNurse(CGameWorld* pGameWorld, int ClientID, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_EVENTS, Pos)
{
	m_OwnerID = ClientID;
	GameWorld()->InsertEntity(this);
}
void CEntityFunctionNurse::Tick()
{
	if(!GS()->m_apPlayers[m_OwnerID] || !GS()->m_apPlayers[m_OwnerID]->GetCharacter())
	{
		GS()->m_World.DestroyEntity(this);
		return;
	}

	CCharacter* pOwnerChar = GS()->m_apPlayers[m_OwnerID]->GetCharacter();
	vec2 Direction = normalize(vec2(pOwnerChar->m_LatestInput.m_TargetX, pOwnerChar->m_LatestInput.m_TargetY));
	m_Pos = pOwnerChar->m_Core.m_Pos + normalize(Direction) * (28.0f);
}
void CEntityFunctionNurse::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = PICKUP_HEALTH;
}
