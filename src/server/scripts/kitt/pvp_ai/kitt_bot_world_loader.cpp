// ----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "kitt_bot_world_loader.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "World.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ArenaTeamMgr.h"
#include "ArenaTeam.h"
#include "BattlegroundMgr.h"
#include "Battleground.h"
#include "Map.h"
#include "MapManager.h"
#include "DatabaseEnv.h"
#include "QueryHolder.h"
#include <chrono>
#include <set>
#include <vector>
#include <memory>
#include <future>
#include "GameTime.h"
#include "Log.h"
#include "CharacterDatabase.h" 
#include "CharacterCache.h"
#include "CharacterPackets.h"
#include "ObjectAccessor.h"
#include "WorldSocket.h"
#include "WorldSession.h"
#include "DBCStores.h"
#include "InstanceSaveMgr.h"
#include "MapInstanced.h"
#include "NetworkThread.h"
#include "Chat.h"
#include "EventProcessor.h"
#include "QueryResult.h"

#include "Config.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include <unordered_map>
#include <boost/asio.hpp>
#include "Guild.h"
#include "GuildMgr.h"
#include "GuildPackets.h"
#include "SocialMgr.h"

#include "WorldPacket.h"
#include "Opcodes.h"

//extern void kitt_start_bot_pvp_AI(Player* botPlayer, uint32 diff);
using namespace Trinity::ChatCommands;

std::vector<BotAsyncTracker> g_MultiBotTracker;

namespace
{
    // config
    static bool KittBotGhostLoader = false;

    std::vector<uint32> GetIdListFromConfig(std::string configPath)
    {
        std::vector<uint32> list;
        std::string configStr = sConfigMgr->GetStringDefault(configPath.c_str(), "");
        if (configStr.empty())
            return list;

        std::stringstream ss(configStr);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item.erase(std::remove(item.begin(), item.end(), ' '), item.end());
            if (!item.empty())
                list.push_back(std::stoul(item));
        }
        return list;
    }

    // ---------

    std::set<ObjectGuid> FictivBotsGuids;

    struct BotConfig
    {
        uint32 accountId;
        uint32 charGuid;
    };

    // Aici adaugi oricati boti vrei (Foloseste Account ID-uri diferite obligatoriu)
    static std::vector<BotConfig> g_BotList;/* = {
        // accID, charGuid
        { 1243, 1576 }, // Bot 1
        { 1244, 1577 }  // Bot 2
    };*/

    /*struct BotAsyncTracker
    {
        uint32 accountId = 0;
        uint32 charGuid = 0;
        WorldSession* realSession = nullptr;
        std::shared_ptr<CharacterDatabaseQueryHolder> holder = nullptr;
        std::future<void> futureResult;
        bool isProcessed = false;
        bool isQueued = false;
        uint32 rejoinTimer = 0;
        bool kickedByPlayer = false;
        uint32 AccRelogDelay = 10000;
        bool AccRealBusy = false;
        bool RemoveFromWorld = false;
        bool AddFromChatCmd = false;
    };
    static std::vector<BotAsyncTracker> g_MultiBotTracker;*/

    // Vector pentru a pastra sesiunile ghost active in memorie fara sa fie sterse
    static std::vector<std::unique_ptr<WorldSession>> g_GhostSessionsStorage;

    static uint32 g_BootSequenceTimer = 10000;

    void ForseazaStergereBotFantomaOFF(BotAsyncTracker& tracker)
    {
        if (!tracker.kickedByPlayer && !tracker.AccRealBusy)
        {
            tracker.RemoveFromWorld = true;
        }
        //tracker.RemoveFromWorld = true;
        tracker.futureResult = {};
        tracker.holder = nullptr;

        ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);
        FictivBotsGuids.erase(playerGuid);

        // Cautam daca jucatorul este inca in lume
        if (Player* botPlayer = ObjectAccessor::FindConnectedPlayer(playerGuid))
        {
            botPlayer->CombatStop();
            botPlayer->RemoveFromWorld();
            ObjectAccessor::RemoveObject(botPlayer);
            botPlayer->SaveToDB(false);

            // CRITIC ANTI-CRASH: Daca sesiunea exista, rupem legatura dintre Player si Sesiune!
            if (WorldSession* session = botPlayer->GetSession())
            {
                // Setam player-ul din sesiune pe nullptr ca destructorul sesiunii sa nu mai apeleze LogoutPlayer pe un obiect mort
                // sa putem da "delete" ....
                session->SetPlayer(nullptr);
            }

            botPlayer->CleanupsBeforeDelete();
            delete botPlayer;
        }

        // Curatam sesiunea din stocarea noastra interna
        auto accountId = tracker.accountId;
        g_GhostSessionsStorage.erase(
            std::remove_if(g_GhostSessionsStorage.begin(), g_GhostSessionsStorage.end(),
                [accountId](const std::unique_ptr<WorldSession>& session) {
                    return session && session->GetAccountId() == accountId;
                }),
            g_GhostSessionsStorage.end()
        );

        // Scoatem sesiunea si din managerul principal de sesiuni al serverului
        if (tracker.realSession)
        {
            sWorld->RemoveSession(tracker.realSession->GetAccountId());
        }
    }
}

bool IsPlayerInBotTracker(uint32 charGuidLow)
{
    for (const auto& tracker : g_MultiBotTracker)
    {
        if (tracker.charGuid == charGuidLow)
        {
            return true;
        }
    }
    return false;
}

void PornesteTotiBotii()
{
    if (!KittBotGhostLoader)
        return;

    if (!g_MultiBotTracker.empty())
        return;

    TC_LOG_INFO("fakPlayer", "LOG CUSTOM: Se porneste incarcarea in masa pentru toti botii...");

    for (const auto& bot : g_BotList)
    {
        std::string accountName = "REAL_BOT_ACC_" + std::to_string(bot.accountId);
        std::shared_ptr<WorldSocket> dummySocket = nullptr;

        WorldSession* fakeSession = new WorldSession(
            bot.accountId, std::move(accountName), dummySocket,
            SEC_PLAYER, 2, 0, std::chrono::minutes(0), LOCALE_enUS, 0, false
        );

        fakeSession->SetRemoteAddress("127.0.0.1");
        fakeSession->m_timeOutTime = GameTime::GetGameTime() + 31536000;
        fakeSession->LoadPermissions();
        //sWorld->AddSession(fakeSession);

        //ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(bot.charGuid);

        //auto loginHolder = std::make_shared<LoginQueryHolder>(bot.accountId, playerGuid);

        //loginHolder->Initialize();

        ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(bot.charGuid);
        ObjectGuid::LowType lowGuid = playerGuid.GetCounter();

        // Folosim clasa oficiala, globala a serverului care nu cere header custom
        auto loginHolder = std::make_shared<CharacterDatabaseQueryHolder>();

        // Alocam manual dimensiunea de 34 de tabele definita in enum
        loginHolder->SetSize(MAX_PLAYER_LOGIN_QUERY);

        // 1. Incarcam tabelul characters (PLAYER_LOGIN_QUERY_LOAD_FROM = 0)
        //CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
        CharacterDatabasePreparedStatement* stmt = nullptr;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_FROM, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GROUP_MEMBER);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GROUP, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INSTANCE);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BOUND_INSTANCES, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURAS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AURAS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELLS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_DAILY);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DAILY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_REPUTATION);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_REPUTATION, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INVENTORY);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_INVENTORY, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACTIONS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACTIONS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAIL);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAILS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SOCIALLIST);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SOCIAL_LIST, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_HOMEBIND);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_HOME_BIND, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELLCOOLDOWNS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELL_COOLDOWNS, stmt);

        if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_DECLINEDNAMES);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DECLINED_NAMES, stmt);
        }

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUILD_MEMBER);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GUILD, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ARENAINFO);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ARENA_INFO, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACHIEVEMENTS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACHIEVEMENTS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_CRITERIAPROGRESS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CRITERIA_PROGRESS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_EQUIPMENTSETS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_EQUIPMENT_SETS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BGDATA);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BG_DATA, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GLYPHS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GLYPHS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_TALENTS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TALENTS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_ACCOUNT_DATA);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACCOUNT_DATA, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SKILLS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SKILLS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_WEEKLY);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_WEEKLY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_RANDOMBG);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_RANDOM_BG, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BANNED);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BANNED, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUSREW);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_REW, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_SEASONAL);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SEASONAL_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_MONTHLY);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MONTHLY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CORPSE_LOCATION);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CORPSE_LOCATION, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PETS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_PET_SLOTS, stmt);

        // Trimitem catre thread-ul MySQL
        SQLQueryHolderTask task(loginHolder);

        BotAsyncTracker tracker;
        tracker.accountId = bot.accountId;
        tracker.charGuid = bot.charGuid;
        tracker.realSession = fakeSession;
        tracker.holder = loginHolder;
        tracker.futureResult = task.GetFuture();
        tracker.isProcessed = false;
        tracker.isQueued = false;

        sWorld->AddSession(tracker.realSession);
        CharacterDatabase.DelayQueryHolder(loginHolder);
        g_MultiBotTracker.push_back(std::move(tracker));
        FictivBotsGuids.insert(playerGuid);
    }

    TC_LOG_INFO("fakPlayer", "LOG CUSTOM: Toate pachetele asincrone ruleaza in fundal pe thread-ul MySQL...");
}

void PornesteBotIndividual(uint32 accountId, uint32 charGuid)
{
    std::string accountName = "REAL_BOT_ACC_" + std::to_string(accountId);
    std::shared_ptr<WorldSocket> dummySocket = nullptr;

    WorldSession* fakeSession = new WorldSession(
        accountId, std::move(accountName), dummySocket,
        SEC_PLAYER, 2, 0, std::chrono::minutes(0), LOCALE_enUS, 0, false
    );

    fakeSession->SetRemoteAddress("127.0.0.1");
    fakeSession->m_timeOutTime = GameTime::GetGameTime() + 31536000;
    fakeSession->LoadPermissions();
    //sWorld->AddSession(fakeSession);

    //ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(charGuid);

    //auto loginHolder = std::make_shared<LoginQueryHolder>(accountId, playerGuid);
    //loginHolder->Initialize();

    ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(charGuid);
    ObjectGuid::LowType lowGuid = playerGuid.GetCounter();

    // Folosim clasa oficiala, globala a serverului care nu cere header custom
    auto loginHolder = std::make_shared<CharacterDatabaseQueryHolder>();

    // Alocam manual dimensiunea de 34 de tabele definita in enum
    loginHolder->SetSize(MAX_PLAYER_LOGIN_QUERY);

    // 1. Incarcam tabelul characters (PLAYER_LOGIN_QUERY_LOAD_FROM = 0)
    //CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
    CharacterDatabasePreparedStatement* stmt = nullptr;

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_FROM, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GROUP_MEMBER);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GROUP, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INSTANCE);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BOUND_INSTANCES, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURAS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AURAS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELLS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_DAILY);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DAILY_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_REPUTATION);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_REPUTATION, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INVENTORY);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_INVENTORY, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACTIONS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACTIONS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAIL);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAILS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SOCIALLIST);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SOCIAL_LIST, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_HOMEBIND);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_HOME_BIND, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELLCOOLDOWNS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELL_COOLDOWNS, stmt);

    if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_DECLINEDNAMES);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DECLINED_NAMES, stmt);
    }

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUILD_MEMBER);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GUILD, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ARENAINFO);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ARENA_INFO, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACHIEVEMENTS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACHIEVEMENTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_CRITERIAPROGRESS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CRITERIA_PROGRESS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_EQUIPMENTSETS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_EQUIPMENT_SETS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BGDATA);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BG_DATA, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GLYPHS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GLYPHS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_TALENTS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TALENTS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_ACCOUNT_DATA);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACCOUNT_DATA, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SKILLS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SKILLS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_WEEKLY);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_WEEKLY_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_RANDOMBG);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_RANDOM_BG, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BANNED);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BANNED, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUSREW);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_REW, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_SEASONAL);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SEASONAL_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_MONTHLY);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MONTHLY_QUEST_STATUS, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CORPSE_LOCATION);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CORPSE_LOCATION, stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PETS);
    stmt->setUInt32(0, lowGuid);
    loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_PET_SLOTS, stmt);

    // Trimitem catre thread-ul MySQL
    SQLQueryHolderTask task(loginHolder);

    CharacterDatabase.DelayQueryHolder(loginHolder);
    FictivBotsGuids.insert(playerGuid);
    

    bool gasitInTracker = false;
    for (auto& tracker : g_MultiBotTracker)
    {
        if (tracker.accountId == accountId)
        {
            // resetam
            tracker.realSession = nullptr;
            tracker.holder = nullptr;
            tracker.futureResult = {};

            // adaugam
            tracker.realSession = fakeSession;
            tracker.holder = loginHolder;
            tracker.futureResult = task.GetFuture();

            // resetare flags
            tracker.isProcessed = false;
            tracker.kickedByPlayer = false;
            tracker.AccRelogDelay = 5000; // Sincronizam resetarea de siguranta
            tracker.AccRealBusy = false;
            tracker.RemoveFromWorld = false;

            sWorld->AddSession(tracker.realSession);

            gasitInTracker = true;
            break;
        }
    }

    if (!gasitInTracker)
    {
        BotAsyncTracker tracker;
        tracker.accountId = accountId;
        tracker.charGuid = charGuid;
        tracker.realSession = fakeSession;
        tracker.holder = loginHolder;
        tracker.futureResult = task.GetFuture();
        tracker.isProcessed = false;
        tracker.isQueued = false;
        tracker.rejoinTimer = 0;              // Completat
        tracker.kickedByPlayer = false;
        tracker.AccRelogDelay = 10000;        // Completat conform structurii
        tracker.AccRealBusy = false;          // Completat conform structurii
        tracker.RemoveFromWorld = false;

        sWorld->AddSession(tracker.realSession);
        g_MultiBotTracker.push_back(std::move(tracker));
    }

    

    TC_LOG_INFO("fakPlayer", "LOG REBOOT: Secventa asincrona a fost relansata curat pentru Cont ID: {}!", accountId);
}

void ForseazaStergereBotFantoma(BotAsyncTracker& tracker)
{
    // test ....
    if (!tracker.isProcessed)
    {
        TC_LOG_INFO("fakPlayer", "LOG CUSTOM AVERTISMENT: Botul cu GUID {} se afla in loading screen! Stergerea a fost blocata pentru siguranta.", tracker.charGuid);
        return;
    }

    if (!tracker.kickedByPlayer && !tracker.AccRealBusy)
    {
        tracker.RemoveFromWorld = true;
    }
    //tracker.RemoveFromWorld = true;
    tracker.futureResult = {};
    tracker.holder = nullptr;

    ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);
    FictivBotsGuids.erase(playerGuid);

    // Cautam daca jucatorul este inca in lume
    if (Player* botPlayer = ObjectAccessor::FindConnectedPlayer(playerGuid))
    {
        botPlayer->CombatStop();
        botPlayer->RemoveFromWorld();
        ObjectAccessor::RemoveObject(botPlayer);
        botPlayer->SaveToDB(false);

        // CRITIC ANTI-CRASH: Daca sesiunea exista, rupem legatura dintre Player si Sesiune!
        if (WorldSession* session = botPlayer->GetSession())
        {
            // Setam player-ul din sesiune pe nullptr ca destructorul sesiunii sa nu mai apeleze LogoutPlayer pe un obiect mort
            // sa putem da "delete" ....
            if (session)
                session->SetPlayer(nullptr);
        }

        botPlayer->CleanupsBeforeDelete();
        delete botPlayer;
    }

    // Curatam sesiunea din stocarea noastra interna
    /*auto accountId = tracker.accountId;
    g_GhostSessionsStorage.erase(
        std::remove_if(g_GhostSessionsStorage.begin(), g_GhostSessionsStorage.end(),
            [accountId](const std::unique_ptr<WorldSession>& session) {
                return session && session->GetAccountId() == accountId;
            }),
        g_GhostSessionsStorage.end()
    );*/

    // Scoatem sesiunea si din managerul principal de sesiuni al serverului
    if (tracker.realSession)
    {
        uint32 accId = tracker.realSession->GetAccountId();
        sWorld->RemoveSession(accId);

        SessionMap& writableSessions = const_cast<SessionMap&>(sWorld->GetAllSessions());
        auto itr = writableSessions.find(accId);
        if (itr != writableSessions.end())
        {
            // IMPORTANT: NU MAI DAM "delete" direct aici (ca sa evitam crash-ul de double-free)!
            // Doar o stergem din map-ul global pentru a debloca contul pe loc!
            writableSessions.erase(itr);
        }

        //sWorld->RemoveSession(tracker.realSession->GetAccountId());

        //delete tracker.realSession;
        tracker.realSession = nullptr;
    }
}


class kitt_bot_account_login_interceptor : public AccountScript
{
public:
    kitt_bot_account_login_interceptor() : AccountScript("kitt_bot_account_login_interceptor") {}

    void OnAccountLogin(uint32 accountId) override
    {
        for (auto& tracker : g_MultiBotTracker)
        {
            if (tracker.accountId == accountId && tracker.isProcessed && !tracker.kickedByPlayer)
            {
                // 1. Marcam imediat trackerul ca fiind preluat de un jucator real
                tracker.AccRelogDelay = 5000;
                tracker.kickedByPlayer = true;
                tracker.AccRealBusy = true;
                tracker.isProcessed = false;

                TC_LOG_INFO("fakPlayer", "LOG ACCOUNT KICK: Jucatorul real s-a logat pe contul {}. Se porneste curatarea controlata a botului...", accountId);

                ForseazaStergereBotFantoma(tracker);

                TC_LOG_INFO("fakPlayer", "LOG ACCOUNT KICK: Curatare finalizata cu succes pentru contul {}.", accountId);
                break;
            }
        }
    }
};

class kitt_bot_world_loader : public WorldScript
{
public:
    kitt_bot_world_loader() : WorldScript("kitt_bot_world_loader") {}

    void PornesteBotIndividualOFF(uint32 accountId, uint32 charGuid)
    {
        std::string accountName = "REAL_BOT_ACC_" + std::to_string(accountId);
        std::shared_ptr<WorldSocket> dummySocket = nullptr;

        WorldSession* fakeSession = new WorldSession(
            accountId, std::move(accountName), dummySocket,
            SEC_PLAYER, 2, 0, std::chrono::minutes(0), LOCALE_enUS, 0, false
        );

        fakeSession->SetRemoteAddress("127.0.0.1");
        fakeSession->m_timeOutTime = GameTime::GetGameTime() + 31536000;
        fakeSession->LoadPermissions();
        sWorld->AddSession(fakeSession);

        //ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(charGuid);

        //auto loginHolder = std::make_shared<LoginQueryHolder>(accountId, playerGuid);
        //loginHolder->Initialize();

        ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(charGuid);
        ObjectGuid::LowType lowGuid = playerGuid.GetCounter();

        // Folosim clasa oficiala, globala a serverului care nu cere header custom
        auto loginHolder = std::make_shared<CharacterDatabaseQueryHolder>();

        // Alocam manual dimensiunea de 34 de tabele definita in enum
        loginHolder->SetSize(MAX_PLAYER_LOGIN_QUERY);

        // 1. Incarcam tabelul characters (PLAYER_LOGIN_QUERY_LOAD_FROM = 0)
        //CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
        CharacterDatabasePreparedStatement* stmt = nullptr;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_FROM, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GROUP_MEMBER);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GROUP, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INSTANCE);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BOUND_INSTANCES, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURAS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AURAS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELLS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_DAILY);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DAILY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_REPUTATION);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_REPUTATION, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INVENTORY);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_INVENTORY, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACTIONS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACTIONS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAIL);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAILS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SOCIALLIST);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SOCIAL_LIST, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_HOMEBIND);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_HOME_BIND, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELLCOOLDOWNS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELL_COOLDOWNS, stmt);

        if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_DECLINEDNAMES);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DECLINED_NAMES, stmt);
        }

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUILD_MEMBER);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GUILD, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ARENAINFO);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ARENA_INFO, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACHIEVEMENTS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACHIEVEMENTS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_CRITERIAPROGRESS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CRITERIA_PROGRESS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_EQUIPMENTSETS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_EQUIPMENT_SETS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BGDATA);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BG_DATA, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GLYPHS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GLYPHS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_TALENTS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TALENTS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_ACCOUNT_DATA);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACCOUNT_DATA, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SKILLS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SKILLS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_WEEKLY);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_WEEKLY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_RANDOMBG);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_RANDOM_BG, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BANNED);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BANNED, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUSREW);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_REW, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_SEASONAL);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SEASONAL_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_MONTHLY);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MONTHLY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CORPSE_LOCATION);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CORPSE_LOCATION, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PETS);
        stmt->setUInt32(0, lowGuid);
        loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_PET_SLOTS, stmt);

        // Trimitem catre thread-ul MySQL
        SQLQueryHolderTask task(loginHolder);

        bool gasitInTracker = false;
        for (auto& tracker : g_MultiBotTracker)
        {
            if (tracker.accountId == accountId)
            {
                // resetam
                tracker.realSession = nullptr;
                tracker.holder = nullptr;
                tracker.futureResult = {};

                // adaugam
                tracker.realSession = fakeSession;
                tracker.holder = loginHolder;
                tracker.futureResult = task.GetFuture();

                // resetare flags
                tracker.isProcessed = false;
                tracker.kickedByPlayer = false;
                tracker.AccRelogDelay = 5000; // Sincronizam resetarea de siguranta
                tracker.AccRealBusy = false;
                tracker.RemoveFromWorld = false;

                gasitInTracker = true;
                break;
            }
        }

        if (!gasitInTracker)
        {
            BotAsyncTracker tracker;
            tracker.accountId = accountId;
            tracker.charGuid = charGuid;
            tracker.realSession = fakeSession;
            tracker.holder = loginHolder;
            tracker.futureResult = task.GetFuture();
            tracker.isProcessed = false;
            tracker.isQueued = false;
            tracker.rejoinTimer = 0;              // Completat
            tracker.kickedByPlayer = false;
            tracker.AccRelogDelay = 10000;        // Completat conform structurii
            tracker.AccRealBusy = false;          // Completat conform structurii
            tracker.RemoveFromWorld = false;

            g_MultiBotTracker.push_back(std::move(tracker));
        }

        CharacterDatabase.DelayQueryHolder(loginHolder);
        FictivBotsGuids.insert(playerGuid);

        TC_LOG_INFO("fakPlayer", "LOG REBOOT: Secventa asincrona a fost relansata curat pentru Cont ID: {}!", accountId);
    }

    void OnStartup() override
    {
        if (KittBotGhostLoader)
        {
            TC_LOG_INFO("server.loading", ">> KITT [GHOST Loader] ACTIVAT.");
            //PornesteTotiBotii(); // il ruleaza din config
        }
        else
        {
            TC_LOG_INFO("server.loading", ">> KITT [GHOST Loader] DEZACTIVAT.");
            return;
        }
    }

    void PornesteTotiBotiiOFF()
    {
        if (!KittBotGhostLoader)
            return;

        if (!g_MultiBotTracker.empty())
            return;

        TC_LOG_INFO("fakPlayer", "LOG CUSTOM: Se porneste incarcarea in masa pentru toti botii...");

        for (const auto& bot : g_BotList)
        {
            std::string accountName = "REAL_BOT_ACC_" + std::to_string(bot.accountId);
            std::shared_ptr<WorldSocket> dummySocket = nullptr;

            WorldSession* fakeSession = new WorldSession(
                bot.accountId, std::move(accountName), dummySocket,
                SEC_PLAYER, 2, 0, std::chrono::minutes(0), LOCALE_enUS, 0, false
            );

            fakeSession->SetRemoteAddress("127.0.0.1");
            fakeSession->m_timeOutTime = GameTime::GetGameTime() + 31536000;
            fakeSession->LoadPermissions();
            sWorld->AddSession(fakeSession);

            //ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(bot.charGuid);

            //auto loginHolder = std::make_shared<LoginQueryHolder>(bot.accountId, playerGuid);

            //loginHolder->Initialize();

            ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(bot.charGuid);
            ObjectGuid::LowType lowGuid = playerGuid.GetCounter();

            // Folosim clasa oficiala, globala a serverului care nu cere header custom
            auto loginHolder = std::make_shared<CharacterDatabaseQueryHolder>();

            // Alocam manual dimensiunea de 34 de tabele definita in enum
            loginHolder->SetSize(MAX_PLAYER_LOGIN_QUERY);

            // 1. Incarcam tabelul characters (PLAYER_LOGIN_QUERY_LOAD_FROM = 0)
            //CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
            CharacterDatabasePreparedStatement* stmt = nullptr;

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_FROM, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GROUP_MEMBER);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GROUP, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INSTANCE);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BOUND_INSTANCES, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURAS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AURAS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELLS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_DAILY);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DAILY_QUEST_STATUS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_REPUTATION);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_REPUTATION, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INVENTORY);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_INVENTORY, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACTIONS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACTIONS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAIL);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAILS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SOCIALLIST);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SOCIAL_LIST, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_HOMEBIND);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_HOME_BIND, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELLCOOLDOWNS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELL_COOLDOWNS, stmt);

            if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
            {
                stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_DECLINEDNAMES);
                stmt->setUInt32(0, lowGuid);
                loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DECLINED_NAMES, stmt);
            }

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_GUILD_MEMBER);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GUILD, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ARENAINFO);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ARENA_INFO, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACHIEVEMENTS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACHIEVEMENTS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_CRITERIAPROGRESS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CRITERIA_PROGRESS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_EQUIPMENTSETS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_EQUIPMENT_SETS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BGDATA);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BG_DATA, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GLYPHS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GLYPHS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_TALENTS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TALENTS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_ACCOUNT_DATA);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACCOUNT_DATA, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SKILLS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SKILLS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_WEEKLY);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_WEEKLY_QUEST_STATUS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_RANDOMBG);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_RANDOM_BG, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BANNED);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BANNED, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUSREW);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_REW, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_SEASONAL);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SEASONAL_QUEST_STATUS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS_MONTHLY);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MONTHLY_QUEST_STATUS, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CORPSE_LOCATION);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CORPSE_LOCATION, stmt);

            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PETS);
            stmt->setUInt32(0, lowGuid);
            loginHolder->SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_PET_SLOTS, stmt);

            // Trimitem catre thread-ul MySQL
            //SQLQueryHolderTask task(loginHolder);
            std::future<void> tempFuture;
            {
                SQLQueryHolderTask task(loginHolder);
                tempFuture = task.GetFuture();
            }

            BotAsyncTracker tracker;
            tracker.accountId = bot.accountId;
            tracker.charGuid = bot.charGuid;
            tracker.realSession = fakeSession;
            tracker.holder = loginHolder;
            //tracker.futureResult = task.GetFuture();
            tracker.futureResult = std::move(tempFuture);
            tracker.isProcessed = false;
            tracker.isQueued = false;

            CharacterDatabase.DelayQueryHolder(loginHolder);

            g_MultiBotTracker.push_back(std::move(tracker));

            //CharacterDatabase.DelayQueryHolder(loginHolder);
            FictivBotsGuids.insert(playerGuid);
        }

        TC_LOG_INFO("fakPlayer", "LOG CUSTOM: Toate pachetele asincrone ruleaza in fundal pe thread-ul MySQL...");
    }


    void OnConfigLoad(bool /*reload*/) override
    {
        KittBotGhostLoader = sConfigMgr->GetBoolDefault("Kitt.Bot.Ghost.Loader", false);

        if (KittBotGhostLoader)
        {
            TC_LOG_INFO("server.loading", ">> KITT [GHOST Loader] config load. Option ACTIVAT.");
            g_BotList.clear();

            // Folosim functia ta pentru a incarca ambele liste separate
            std::vector<uint32> accounts = GetIdListFromConfig("Kitt.Bot.Ghost.List.Acc"); // acc uniq per guid char
            std::vector<uint32> guids = GetIdListFromConfig("Kitt.Bot.Ghost.List.Guid"); // guid uniq per acc (not multipli)

            if (!accounts.empty() && !guids.empty() && accounts.size() == guids.size())
            {
                for (size_t i = 0; i < accounts.size(); ++i)
                {
                    BotConfig bot{};
                    bot.accountId = accounts[i];
                    bot.charGuid = guids[i];
                    g_BotList.push_back(bot);
                }
                TC_LOG_INFO("server.loading", ">> KITT [GHOST Loader] S-au incarcat {} boti cu functia custom.", (uint32)g_BotList.size());
            }
            else if (accounts.size() != guids.size())
            {
                TC_LOG_ERROR("server.loading", ">> KITT [GHOST Loader] Eroare! Ai {} conturi si {} guid-uri in config. Numarul lor trebuie sa fie egal!", (uint32)accounts.size(), (uint32)guids.size());
            }

            if (g_MultiBotTracker.empty())
            {
                PornesteTotiBotii();
            }
        }
        else
        {
            TC_LOG_INFO("server.loading", ">> KITT [GHOST Loader] config load. Option DEZACTIVAT.");

            // 1. Golim lista de configuratie pentru a opri orice incercare de re-logare automata
            if (!g_MultiBotTracker.empty())
            {
                //g_BotList.clear();

                // 2. Parcurgem toti botii care sunt procesati sau in curs de procesare si ii stergem din lume
                for (auto& tracker : g_MultiBotTracker)
                {
                    // Apelam functia ta existenta pentru fiecare bot activ in tracker
                    ForseazaStergereBotFantoma(tracker);
                }

                // 3. Golim complet si vectorul de trackere pentru a reseta starea sistemului
                g_MultiBotTracker.clear();
                g_GhostSessionsStorage.clear();

                TC_LOG_INFO("server.loading", ">> KITT [GHOST Loader] Toti botii fantoma au fost scosi din lume cu succes.");
            }
        }
    }

};

class kitt_bot_world_update : public WorldScript
{

public:
    kitt_bot_world_update() : WorldScript("kitt_bot_world_update") {}

    void OnUpdate(uint32 diff) override
    {
        if (!KittBotGhostLoader)
            return;

        if (g_BootSequenceTimer > 0)
        {
            if (g_BootSequenceTimer <= diff)
                g_BootSequenceTimer = 0;
            else
                g_BootSequenceTimer -= diff;
        }

        for (auto& tracker : g_MultiBotTracker)
        {
            if (tracker.RemoveFromWorld)
                continue;

            if (tracker.AccRelogDelay > diff)
            {
                tracker.AccRelogDelay -= diff;
                continue;
            }

            // --- RE-INTRARE ASINCRONA DUPA LOGOUT JUCATOR REAL ---
            if (!tracker.isProcessed && tracker.kickedByPlayer)
            {
                if (g_BootSequenceTimer > 0)
                    return;

                tracker.AccRealBusy = false;

                //ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);

                SessionMap const& sesiuniGlobale = sWorld->GetAllSessions();

                for (auto const& [sessId, sessionPointer] : sesiuniGlobale)
                {
                    if (sessionPointer && sessionPointer->GetAccountId() == tracker.accountId && sessionPointer != tracker.realSession)
                    {
                        TC_LOG_INFO("fakPlayer", "Verificare Sesiune: Contul {} egal cu: {}", sessionPointer->GetAccountId(), tracker.accountId);

                        tracker.AccRealBusy = true;
                        break;
                    }
                }

                if (tracker.AccRealBusy)
                {
                    tracker.AccRelogDelay = 10000;
                    continue;
                }

                // Daca omul a dat logout si contul e complet liber:
                if (!tracker.AccRealBusy)
                {
                    tracker.AccRelogDelay = 5000;
                    tracker.kickedByPlayer = false;
                    PornesteBotIndividual(tracker.accountId, tracker.charGuid);

                    TC_LOG_INFO("fakPlayer", "LOG REJOIN: Contul {} a fost eliberat. Execut direct secventa de boot...", tracker.accountId);
                }
                continue;
            }

            if (!tracker.realSession)
                continue;

            // Intrare in lume
            if (!tracker.isProcessed && !tracker.kickedByPlayer)
            {
                if (g_BootSequenceTimer > 0)
                    return;

                if (tracker.futureResult.valid() && tracker.futureResult.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    tracker.isProcessed = true;
                    g_BootSequenceTimer = 5000;

                    TC_LOG_INFO("fakPlayer", "LOG CUSTOM: Baza de date a returnat datele pentru GUID {}! Se forteaza intrarea...", tracker.charGuid);
                    /*
                    std::string tempName = "GHOST_SESSION_" + std::to_string(tracker.charGuid);
                    std::shared_ptr<WorldSocket> nullSocket = nullptr;

                    auto ghostSession = std::make_unique<WorldSession>(tracker.accountId, std::move(tempName), nullSocket, SEC_PLAYER, 2, 0, std::chrono::minutes(0), LOCALE_enUS, 0, false);
                    //ghostSession->LoadPermissions();
                    
                    Player* botPlayer = new Player(ghostSession.get());
                    ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);

                    g_GhostSessionsStorage.push_back(std::move(ghostSession));
                    */
                    WorldSession* realSession = tracker.realSession;
                    if (!realSession)
                    {
                        TC_LOG_INFO("fakPlayer", "LOG CUSTOM EROARE: Sesiunea reala a disparut din tracker pentru GUID {}.", tracker.charGuid);
                        return;
                    }

                    Player* botPlayer = new Player(realSession);
                    ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);

                    if (botPlayer->LoadFromDB(playerGuid, *tracker.holder))
                    {

                        // ==================== ACTIVARE GUILDA REPARATA COMPLET ====================
                        QueryResult dbGuildResult = CharacterDatabase.PQuery("SELECT `guildid`, `rank` FROM `guild_member` WHERE `guid` = {}", tracker.charGuid);

                        if (dbGuildResult)
                        {
                            Field* fields = dbGuildResult->Fetch();
                            uint32 dbGuildId = fields[0].GetUInt32(); // Coloana 0 = guildId
                            uint8 dbRankId = fields[1].GetUInt8();   // Coloana 1 = rank

                            if (dbGuildId > 0)
                            {
                                // 1. Setam ID-ul si rank-ul direct pe obiectul botului (reparam ce a omis LoadFromDB)
                                botPlayer->SetInGuild(dbGuildId);
                                botPlayer->SetGuildRank(dbRankId);

                                // 2. Luam obiectul guildei din managerul global folosind ID-ul aflat
                                if (Guild* guild = sGuildMgr->GetGuildById(dbGuildId))
                                {
                                    // 3. Apelam functia din header-ul tau pentru a-l trece online
                                    // Parametrii: player-ul, tipul de flag (1 = online status), starea (true = online)
                                    guild->OnPlayerStatusChange(botPlayer, 1, true);

                                    // 4. Sincronizam zona si nivelul in lista interna a breslei
                                    guild->UpdateMemberData(botPlayer, GUILD_MEMBER_DATA_ZONEID, botPlayer->GetZoneId());
                                    guild->UpdateMemberData(botPlayer, GUILD_MEMBER_DATA_LEVEL, botPlayer->GetLevel());

                                    TC_LOG_INFO("fakPlayer", "LOG GUILDA REUSIT: Botul {} (GUID: {}) este acum online in Guilda ID {}.", botPlayer->GetName(), tracker.charGuid, dbGuildId);
                                }
                            }
                        }
                        else
                        {
                            // Daca nu are guilda in baza de date
                            botPlayer->SetInGuild(0);
                            botPlayer->SetGuildRank(0);
                        }

                        botPlayer->ForceValuesUpdateAtIndex(PLAYER_GUILDID);
                        // =====================================================================

                        // protectie map instance
                        // nu se poate crea instata daca nu e jucator deja
                        uint32 botMapId = botPlayer->GetMapId();
                        MapEntry const* mapEntry = sMapStore.LookupEntry(botMapId);

                        if (!mapEntry || mapEntry->Instanceable())
                        {
                            ForseazaStergereBotFantoma(tracker);

                            uint32 homeMapId = botPlayer->m_homebindMapId;
                            float homeX = botPlayer->m_homebindX;
                            float homeY = botPlayer->m_homebindY;
                            float homeZ = botPlayer->m_homebindZ;
                            float homeO = botPlayer->GetOrientation();
                            //botPlayer->TeleportTo(homeMapId, homeX, homeY, homeZ, homeO);
                            CharacterDatabase.PExecute("UPDATE characters SET position_x = {}, position_y = {}, position_z = {}, orientation = {}, map = {}, instance_id = 0 WHERE guid = {};",
                                homeX, homeY, homeZ, homeO, homeMapId, tracker.charGuid);


                            TC_LOG_INFO("fakPlayer", "LOG GHOST PROTECTIE: Botul {} a fost mutat in memorie catre Homebind (Map: {}).", botPlayer->GetName().c_str(), homeMapId);

                            PornesteBotIndividual(tracker.accountId, tracker.charGuid);

                            continue;
                        }
                        // -------------------

                        botPlayer->GetMotionMaster()->Initialize();
                        botPlayer->SendDungeonDifficulty(false);

                        //tracker.realSession->LoadPermissions();
                        tracker.realSession->SetPlayer(botPlayer);

                        //sCharacterCache->AddCharacterCacheEntry(botPlayer->GetGUID(), tracker.accountId, botPlayer->GetName(), botPlayer->GetGender(), botPlayer->GetRace(), botPlayer->GetClass(), botPlayer->GetLevel());

                        Map* map = sMapMgr->CreateBaseMap(botPlayer->GetMapId());

                        if (map)
                        {
                            botPlayer->SetMap(map);

                            // 1. Calculam coordonatele celulei si grid-ului unde se afla botul in Shattrath/lume
                            //GridCoord p = Trinity::ComputeGridCoord(botPlayer->GetPositionX(), botPlayer->GetPositionY());

                            // 2. Fortam serverul sa incarce in memorie bucata de harta (Grid-ul) pentru acele coordonate
                            // Fara asta, in Shattrath botul va cadea in gol sau va genera crash la tick-ul de update
                            map->LoadGrid(botPlayer->GetPositionX(), botPlayer->GetPositionY());


                            botPlayer->GetMap()->AddPlayerToMap(botPlayer);

                            botPlayer->AddToWorld();
                            ObjectAccessor::AddObject(botPlayer);

                            // Asta face ca botul sa fie vazut online la comanda /who sau pe panourile web (UCP/Armory)
                            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_ONLINE);
                            stmt->setUInt32(0, botPlayer->GetGUID().GetCounter());
                            CharacterDatabase.Execute(stmt);

                            // Toti jucatorii care il au la Friends vor primi notificarea vizuala "X has come online."
                            sSocialMgr->SendFriendStatus(botPlayer, FRIEND_ONLINE, botPlayer->GetGUID(), true);

                            // 4. COPIAT DIN CORE: Sincronizare si anunt in cadrul Grupului (daca botul era intr-un Party/Raid)
                            if (Group* group = botPlayer->GetGroup())
                            {
                                group->SendUpdate();
                                group->ResetMaxEnchantingLevel();
                                if (group->GetLeaderGUID() == botPlayer->GetGUID())
                                    group->StopLeaderOfflineTimer();
                            }

                            // 5. Sincronizam timpul intern de logare si fortam masca de update vizual pentru guilda sub cap
                            botPlayer->SetInGameTime(GameTime::GetGameTimeMS());

                            // ===========================================================

                            FictivBotsGuids.insert(playerGuid);

                            TC_LOG_INFO("fakPlayer", "LOG CUSTOM REUSIT: {} (GUID: {}) a intrat online permanent pe sesiunea reala!", botPlayer->GetName().c_str(), tracker.charGuid);
                            break;
                        }
                    }
                    else
                    {
                        TC_LOG_INFO("fakPlayer", "LOG CUSTOM EROARE: LoadFromDB a refuzat structura holder-ului pentru GUID {}.", tracker.charGuid);

                        realSession->SetPlayer(nullptr);
                        botPlayer->CleanupsBeforeDelete();
                        delete botPlayer;

                        tracker.realSession = nullptr;
                        tracker.isProcessed = false;

                        /*if (WorldSession* ghostSession = botPlayer->GetSession())
                        {
                            ghostSession->SetPlayer(nullptr);
                        }

                        botPlayer->CleanupsBeforeDelete();
                        delete botPlayer;

                        auto accountId = tracker.accountId;
                        g_GhostSessionsStorage.erase(
                            std::remove_if(g_GhostSessionsStorage.begin(), g_GhostSessionsStorage.end(),
                                [accountId](const std::unique_ptr<WorldSession>& session) {
                                    return session && session->GetAccountId() == accountId;
                                }),
                            g_GhostSessionsStorage.end()
                        );

                        tracker.realSession = nullptr;
                        tracker.isProcessed = false;*/
                    }
                    return;
                }
            }

            if (tracker.AddFromChatCmd)
                continue;

            // 2. LOGICA DINAMICA DE COADA SI PORT IN ARENA
            if (tracker.isProcessed)
            {
                ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);
                Player* botPlayer = ObjectAccessor::FindPlayer(playerGuid);


                if (botPlayer && botPlayer->IsInWorld() && !botPlayer->IsLoading())
                {
                    bool areCoadaActiva = botPlayer->InBattlegroundQueue();
                    bool esteInMeciAcum = botPlayer->GetMap()->IsBattleArena();

                    // --- PASUL A: PORTARE IN ARENA PRIN EVENIMENT (ANTI-CRASH TOTAL) ---
                    if (areCoadaActiva && !esteInMeciAcum)
                    {
                        for (uint32 slot = 0; slot < 3; ++slot)
                        {
                            BattlegroundQueueTypeId queueTypeId = botPlayer->GetBattlegroundQueueTypeId(slot);

                            if (queueTypeId.BattlemasterListId != 0)
                            {
                                BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(queueTypeId);
                                GroupQueueInfo ginfoData;

                                if (bgQueue.GetPlayerGroupInfoData(botPlayer->GetGUID(), &ginfoData))
                                {
                                    if (ginfoData.IsInvitedToBGInstanceGUID > 0)
                                    {
                                        BattlegroundTypeId bgTypeId = BattlegroundTypeId(queueTypeId.BattlemasterListId);
                                        Battleground* bg = sBattlegroundMgr->GetBattleground(ginfoData.IsInvitedToBGInstanceGUID, bgTypeId);

                                        if (bg)
                                        {
                                            Map* actualArenaMap = sMapMgr->FindMap(bg->GetMapId(), bg->GetInstanceID());
                                            if (!actualArenaMap)
                                            {
                                                // === reinvie si tele home ===
                                                if (!botPlayer->IsAlive())
                                                {
                                                    TC_LOG_INFO("fakPlayer", " !isalive");
                                                    botPlayer->ResurrectPlayer(1.0f);
                                                    botPlayer->SpawnCorpseBones();

                                                    botPlayer->TeleportTo(botPlayer->m_homebindMapId, botPlayer->m_homebindX, botPlayer->m_homebindY, botPlayer->m_homebindZ, botPlayer->GetOrientation());

                                                    if (botPlayer->GetSession())
                                                    {
                                                        WorldPacket pachetGol;
                                                        botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                                                    }

                                                    botPlayer->DurabilityRepairAll(false, 0, false);
                                                    botPlayer->RemoveAllAuras();
                                                }

                                                if (botPlayer->IsInCombat())
                                                {
                                                    botPlayer->AttackStop();
                                                    botPlayer->ClearInCombat();
                                                }

                                                float groundHeight = botPlayer->GetMap()->GetHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY(), botPlayer->GetPositionZ());

                                                if (botPlayer->GetPositionZ() > (groundHeight + 5.0f) ||
                                                    botPlayer->IsFalling() ||
                                                    botPlayer->IsUnderWater() ||
                                                    botPlayer->IsInWater() ||
                                                    botPlayer->GetPositionZ() < botPlayer->GetMap()->GetMinHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY()))
                                                {
                                                    TC_LOG_INFO("fakPlayer", " in apa sau... cade... Falling....");
                                                    // Jucatorul pica in gol sub harta
                                                    botPlayer->TeleportTo(botPlayer->m_homebindMapId, botPlayer->m_homebindX, botPlayer->m_homebindY, botPlayer->m_homebindZ, botPlayer->GetOrientation());

                                                    if (botPlayer->GetSession())
                                                    {
                                                        WorldPacket pachetGol;
                                                        botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                                                    }
                                                }
                                                // ================

                                                continue;
                                            }

                                            // protectie preventiva
                                            botPlayer->GetMotionMaster()->Clear();
                                            botPlayer->GetMotionMaster()->MoveIdle();
                                            botPlayer->StopMoving();
                                            // -------------

                                            Team botTeamFromQueue = ginfoData.Team;
                                            TeamId bgTeamId = TEAM_ALLIANCE;

                                            if (botTeamFromQueue == HORDE)
                                            {
                                                bgTeamId = TEAM_HORDE;
                                            }
                                            else if (botTeamFromQueue != ALLIANCE)
                                            {
                                                if (Group* botGroup = botPlayer->GetGroup())
                                                    bgTeamId = (botGroup->GetGUID().GetCounter() % 2 == 0) ? TEAM_ALLIANCE : TEAM_HORDE;
                                            }

                                            // Sincronizam tabara botului cu echipa stabilita
                                            botPlayer->SetBGTeam(bgTeamId == TEAM_ALLIANCE ? ALLIANCE : HORDE);

                                            Position const* startPos = bg->GetTeamStartPosition(bgTeamId);

                                            if (startPos)
                                            {
                                                tracker.isQueued = true;

                                                //TC_LOG_INFO("fakPlayer", "LOG ARENA: Botul {} accepta invitatia nativ...", botPlayer->GetName().c_str());
                                                botPlayer->SetBattlegroundEntryPoint();
                                                botPlayer->SetBattlegroundId(bg->GetInstanceID(), bg->GetTypeID());

                                                //bg->AddPlayer(botPlayer);
                                                bgQueue.RemovePlayer(botPlayer->GetGUID(), false);

                                                botPlayer->TeleportTo(bg->GetMapId(), startPos->GetPositionX(), startPos->GetPositionY(), startPos->GetPositionZ(), startPos->GetOrientation());

                                                if (botPlayer->GetSession())
                                                {
                                                    WorldPacket pachetGol;
                                                    botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                                                }

                                                if (!botPlayer->IsInWorld() && !botPlayer->IsLoading() && !botPlayer->IsBeingTeleported())
                                                {
                                                    botPlayer->AddToWorld();
                                                }

                                                TC_LOG_INFO("fakPlayer", "LOG ARENA REUSIT: {} este online, activ si listat in arena!", botPlayer->GetName());

                                                break;
                                            }
                                        }
                                        continue;
                                    }
                                }
                            }
                        }
                    }

                    // --- PASUL A2: DETECTARE SFARSIT DE MECI SI EVACUARE AUTOMATA ---
                    if (esteInMeciAcum)
                    {
                        if (tracker.AddFromChatCmd)
                            return;

                        Battleground* bg = botPlayer->GetBattleground();

                        // pentru AI cand este in meci
                        if (bg && bg->GetStatus() == STATUS_IN_PROGRESS)
                        {
                            if (FictivBotsGuids.find(botPlayer->GetGUID()) != FictivBotsGuids.end())
                            {
                                kitt_start_bot_pvp_AI(botPlayer, diff);
                            }
                        }

                        // STATUS_WAIT_LEAVE are valoarea nativa 4. O verificam direct in siguranta:
                        if (bg && bg->GetStatus() == STATUS_WAIT_LEAVE)
                        {
                            botPlayer->AttackStop();
                            botPlayer->CombatStop();
                            botPlayer->GetMotionMaster()->Clear();
                            botPlayer->GetMotionMaster()->MoveIdle();
                            botPlayer->StopMoving();

                            TC_LOG_INFO("fakPlayer", "LOG ARENA: Meciul s-a terminat pentru {}. Se forteaza parasirea instantei...", botPlayer->GetName().c_str());

                            botPlayer->LeaveBattleground(true, true);

                            // opcode
                            if (botPlayer->GetSession())
                            {
                                WorldPacket pachetGol;
                                botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                            }

                            // --- LOGICA VERIFICARE SI ADAUGARE RATING ---
                            CheckAndRewardArenaBotRating(botPlayer);
                            CheckAndRewardArenaBotPersonalRating(botPlayer);

                            //tracker.isQueued = false;

                            //break;
                            continue;
                        }
                    }

                    // --- PASUL B: INSCRIERE SI RE-INSCRIERE AUTOMATA (AIci punem JoinGroupArena2v2Rated) ---
                    if (!areCoadaActiva && !esteInMeciAcum)
                    {
                        if (tracker.isQueued)
                        {
                            tracker.isQueued = false;
                            tracker.rejoinTimer = urand(15000, 35000);
                            TC_LOG_INFO("fakPlayer", "LOG STATUS: Botul {} a iesit din query sau meci. Pornesc cronometrul de re-inscriere...", botPlayer->GetName().c_str());

                            // verificare si la iesire din bg
                            float groundHeight = botPlayer->GetMap()->GetHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY(), botPlayer->GetPositionZ());
                            if (botPlayer->GetPositionZ() > (groundHeight + 5.0f) ||
                                botPlayer->IsFalling() ||
                                botPlayer->IsUnderWater() ||
                                botPlayer->IsInWater() ||
                                botPlayer->GetPositionZ() < botPlayer->GetMap()->GetMinHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY()))
                            {
                                TC_LOG_INFO("fakPlayer", "22222 in apa sau... cade... Falling....");
                                // Jucatorul pica in gol sub harta
                                botPlayer->TeleportTo(botPlayer->m_homebindMapId, botPlayer->m_homebindX, botPlayer->m_homebindY, botPlayer->m_homebindZ, botPlayer->GetOrientation());

                                if (botPlayer->GetSession())
                                {
                                    WorldPacket pachetGol;
                                    botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                                }
                            }
                        }

                        if (tracker.rejoinTimer <= diff)
                        {
                            bool existaJucatoriLaCoada = false;

                            if (Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA))
                            {
                                if (PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), botPlayer->GetLevel()))
                                {
                                    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bg->GetTypeID(), bracketEntry->GetBracketId(), 2); // 2 = 2v2
                                    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

                                    // join daca lista nu e goala pt toti botii
                                    /*for (uint32 j = 0; j < 2; ++j)
                                    {
                                        if (!bgQueue.m_QueuedGroups[j].empty())
                                        {
                                            existaJucatoriLaCoada = true;
                                            break;
                                        }
                                    }*/

                                    // join doar daca in asteptare este cineva fara pereche
                                    uint32 totalEchipeInCoada = 0;
                                    for (uint32 j = 0; j < 2; ++j)
                                    {
                                        totalEchipeInCoada += bgQueue.m_QueuedGroups[j].size();
                                    }

                                    // Daca numarul de echipe din coada este IMPAR (1, 3, 5...), inseamna ca cineva nu are pereche!
                                    // Doar in acest caz botul are voie sa intre ca sa completeze perechea.
                                    if (totalEchipeInCoada % 2 != 0)
                                    {
                                        existaJucatoriLaCoada = true;
                                    }
                                }
                            }

                            if (!existaJucatoriLaCoada)
                            {
                                tracker.rejoinTimer = urand(10000, 20000);
                            }
                            else
                            {

                                if (botPlayer->HasAura(26013))
                                {
                                    botPlayer->RemoveAura(26013);
                                }

                                //JoinGroupArena2v2Rated(botPlayer);

                                Group* checkGroup = botPlayer->GetGroup();

                                if (checkGroup && checkGroup->IsLeader(botPlayer->GetGUID()))
                                {
                                    tracker.isQueued = true;
                                    tracker.rejoinTimer = 0;
                                    JoinGroupArena2v2Rated(botPlayer);
                                }
                                else
                                {
                                    // Daca este un simplu membru (ca Judy), nu ii setam isQueued = true acum.
                                    // El isi va lua starea de true automat din ramura "else" a conditiei principale, 
                                    // doar dupa ce liderul lui va fi apucat sa il bage efectiv in coada (areCoadaActiva va deveni true).
                                    tracker.isQueued = false;
                                }
                            }
                        }
                        else
                        {
                            tracker.rejoinTimer -= diff;
                        }
                    }
                    else
                    {
                        tracker.isQueued = true;
                    }
                }
            }
        }

        g_MultiBotTracker.erase(
            std::remove_if(g_MultiBotTracker.begin(), g_MultiBotTracker.end(),
                [](const BotAsyncTracker& tracker) { return tracker.RemoveFromWorld; }),
            g_MultiBotTracker.end()
        );
    }

private:
    void JoinGroupArena2v2Rated(Player* botPlayer)
    {
        if (!botPlayer)
            return;

        // Pasul 1: Inscrierea se face DOAR daca botul este liderul grupului sau daca este solo
        Group* group = botPlayer->GetGroup();
        if (group && !group->IsLeader(botPlayer->GetGUID()))
        {
            // Daca nu este liderul, sarim peste el. Liderul grupului va inscrie automat tot party-ul!
            return;
        }

        // Pasul 2: Preluam datele de baza pentru arene
        Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
        if (!bg)
            return;

        PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), botPlayer->GetLevel());
        if (!bracketEntry)
            return;

        // Pasul 3: ID-ul cozii pentru Arena 2v2 (Slot index 0 in echipa, dimensiune echipa 2)
        uint8 teamSizeIndex = 0; // 0 = 2v2, 1 = 3v3, 2 = 5v5
        BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bg->GetTypeID(), bracketEntry->GetBracketId(), 2);
        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

        // Pasul 4: Preluam ID-ul echipei de Arena 2v2 si rating-urile (Exact ca in functia ta nativa)
        uint32 arenaTeamId = botPlayer->GetArenaTeamId(teamSizeIndex);
        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(arenaTeamId);

        uint32 arenaRating = 1;
        uint32 matchmakerRating = 0;
        uint32 previousOpponents = 0;

        if (at)
        {
            arenaRating = at->GetRating();
            if (arenaRating <= 0) arenaRating = 1;

            matchmakerRating = at->GetAverageMMR(group);
            previousOpponents = at->GetPreviousOpponents();
        }
        else
        {
            TC_LOG_ERROR("fakPlayer", "Eroare Coada: Liderul bot {} nu are o echipa de arena 2v2 valida!", botPlayer->GetName().c_str());
            return;
        }

        // Fortam viata la toti membrii din grup inainte de inscriere pentru a trece de verificari
        if (group)
        {
            for (GroupReference const* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                if (Player* member = itr->GetSource())
                    member->SetHealth(member->GetMaxHealth());
            }
        }
        else
        {
            botPlayer->SetHealth(botPlayer->GetMaxHealth());
        }

        // Pasul 5: Apelam AddGroup trimitand pointer-ul de grup ('group') si setand isRated = true, isPremade = false
        // Semnatura ta din core: AddGroup(leader, group, bracketEntry, isRated, isPremade, ArenaRating, MatchmakerRating, arenateamid, PreviousOpponentsArenaTeamId)
        // Ocolim complet "WorldPackets" si "SendPacket" pentru a evita prabusirea retelei (nullptr socket)
        GroupQueueInfo* ginfo = bgQueue.AddGroup(
            botPlayer,          // leader
            group,              // group (Transmitem grupul lor real)
            bracketEntry,       // bracketEntry
            true,               // isRated = true (Meci cu rating)
            false,              // isPremade = false
            arenaRating,        // ArenaRating
            matchmakerRating,   // MatchmakerRating
            arenaTeamId,        // arenateamid
            previousOpponents   // PreviousOpponentsArenaTeamId
        );

        if (ginfo)
        {
            // Pasul 6: Adaugam ID-ul de coada in structura fiecarui membru din grup (exact cum face serverul nativ)
            if (group)
            {
                for (GroupReference const* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                {
                    if (Player* member = itr->GetSource())
                    {
                        if (!member->InBattlegroundQueue())
                        {
                            member->AddBattlegroundQueueId(bgQueueTypeId);
                        }
                    }
                }
            }
/*            else
            {
                if (!botPlayer->InBattlegroundQueue())
                {
                    botPlayer->AddBattlegroundQueueId(bgQueueTypeId);
                }
            }*/

            // Pasul 7: Programeaza serverul sa caute meci pentru acest MMR
            sBattlegroundMgr->ScheduleQueueUpdate(matchmakerRating, bgQueueTypeId);

            TC_LOG_INFO("fakPlayer", "Succes Coada: Echipa de boti condusa de {} a intrat oficial in query-ul de Matchmaking 2v2 Rated!", botPlayer->GetName().c_str());
        }
        else
        {
            TC_LOG_ERROR("fakPlayer", "Eroare Coada: AddGroup a returnat NULL pentru grupul botului {}.", botPlayer->GetName().c_str());
        }
    }

    void CheckAndRewardArenaBotRating(Player* botPlayer)
    {
        if (!botPlayer)
            return;

        uint8 teamSizeIndex = 0; // 0 = 2v2
        uint32 arenaTeamId = botPlayer->GetArenaTeamId(teamSizeIndex);

        if (arenaTeamId == 0)
            return;

        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(arenaTeamId);
        if (!at)
            return;

        uint32 currentRating = at->GetRating();

        // Daca rating-ul a scazut sub pragul dorit (1350)
        if (currentRating < 1150) // 1350
        {
            uint32 bonusPoints = urand(150, 250);
            uint32 newRating = currentRating + bonusPoints;

            ArenaTeamStats& stats = const_cast<ArenaTeamStats&>(at->GetStats());
            stats.Rating = newRating;

            TC_LOG_INFO("fakPlayer", "LOG ARENA RATING ASINCRON: Echipa botului {} (ID: {}) avea {} rating. S-au adaugat {} puncte bonus. Noul rating fortat in RAM si salvat in DB: {}",
                botPlayer->GetName(), arenaTeamId, currentRating, bonusPoints, at->GetRating());
        }
    }

    void CheckAndRewardArenaBotPersonalRating(Player* botPlayer)
    {
        if (!botPlayer)
            return;

        uint8 teamSizeIndex = 0; // 0 = 2v2
        uint32 arenaTeamId = botPlayer->GetArenaTeamId(teamSizeIndex);

        if (arenaTeamId == 0)
            return;

        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(arenaTeamId);
        if (!at)
            return;

        uint32 currentPersonalRating = botPlayer->GetUInt32Value(PLAYER_FIELD_ARENA_TEAM_INFO_1_1 + (teamSizeIndex * ARENA_TEAM_END) + ARENA_TEAM_PERSONAL_RATING);

        if (currentPersonalRating < 1150) // 1350
        {
            uint32 bonusPoints = urand(150, 250);
            uint32 newPersonalRating = currentPersonalRating + bonusPoints;

            uint32 newMMR = newPersonalRating;
            /*if (newMMR < 1500)
            {
                newMMR = 1500;
            }*/

            botPlayer->SetArenaTeamInfoField(teamSizeIndex, ARENA_TEAM_PERSONAL_RATING, newPersonalRating);

            for (ArenaTeam::MemberList::iterator itr = at->m_membersBegin(); itr != at->m_membersEnd(); ++itr)
            {
                if (itr->Guid == botPlayer->GetGUID())
                {
                    itr->PersonalRating = newPersonalRating;
                    itr->MatchMakerRating = newMMR;
                    break;
                }
            }

            TC_LOG_INFO("fakPlayer", "LOG CACHE RAM FIXED: S-au salvat in siguranta {} PR si {} MMR in cache-ul RAM al echipei {} pentru urmatorul meci!",
                newPersonalRating, newMMR, arenaTeamId);
        }
    }

};

class kitt_bot_chat_handler : public PlayerScript
{
public:
    kitt_bot_chat_handler() : PlayerScript("kitt_bot_chat_handler") {}

    void OnChat(Player* player, uint32 type, uint32 /*lang*/, std::string& msg, Player* receiver) override
    {
        if (!player || !receiver)
            return;

        if (type != 6 && type != 7)
            return;

        uint32 AccSecurity = player->GetSession()->GetSecurity();
        if (AccSecurity < 5)
            return;

        if (FictivBotsGuids.find(receiver->GetGUID()) == FictivBotsGuids.end())
            return;

        std::transform(msg.begin(), msg.end(), msg.begin(), ::tolower);

        std::string raspunsText = "";

        if (msg == "vino" || msg == "port")
        {
            uint32 targetMapId = player->GetMapId();
            MapEntry const* mapEntry = sMapStore.LookupEntry(targetMapId);

            if (!mapEntry || mapEntry->Instanceable())
            {
                raspunsText = "Nu vreau";
            }
            else
            {

                raspunsText = "Pornesc spre tine acum!";

                // Aici vom pune mai tarziu secventa de teleportare safely
                //uint32 targetMapId = player->GetMapId();
                float posX = player->GetPositionX();
                float posY = player->GetPositionY();
                float posZ = player->GetPositionZ();
                float orientation = player->GetOrientation();

                receiver->TeleportTo(targetMapId, posX, posY, posZ, orientation);

                if (receiver->GetSession())
                {
                    WorldPacket pachetGol;
                    receiver->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                }

                if (!receiver->IsInWorld())
                {
                    receiver->AddToWorld();
                }
            }
        }
        else if (msg == "status")
        {
            raspunsText = "Sunt online si pregatit de arena!";
        }
        else
        {
            raspunsText = "Nu inteleg aceasta comanda. Scrie 'vino' sau 'status'.";
        }

        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_UNIVERSAL, receiver, receiver, raspunsText);
        player->SendDirectMessage(&data);
    }
};

class kitt_ghost_player_command : public CommandScript
{
public:
    kitt_ghost_player_command() : CommandScript("kitt_ghost_player_command") {}

    std::vector<ChatCommandBuilder> GetCommands() const override
    {
        static std::vector<ChatCommandBuilder> kittGhostPlayerCommandSubcommandTable1 =
        {
            //{ "list",   HandleListAllGhostAccess,      rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            //{ "set",    HandleSetGhostAccess,    rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            //{ "del",    HandleDelGhostAccess,    rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
        };

        static std::vector<ChatCommandBuilder> kittGhostPlayerCommandSubcommandTable =
        {
            { "access",     kittGhostPlayerCommandSubcommandTable1 },
            { "list",  HandleShowGhostList,          rbac::RBAC_PERM_COMMAND_LEARN, Console::No },
            { "add",      HandleStartGhostInWorld,      rbac::RBAC_PERM_COMMAND_LEARN, Console::No },
            { "addmass",  HandleStartBulkGhostsInWorld, rbac::RBAC_PERM_COMMAND_LEARN, Console::No },
            { "remove",     HandleRemoveGhostFromWorld,   rbac::RBAC_PERM_COMMAND_LEARN, Console::No },
        };

        static std::vector<ChatCommandBuilder> kittGhostPlayerCommandTable =
        {
            { "zGhost", kittGhostPlayerCommandSubcommandTable },
        };

        return kittGhostPlayerCommandTable;
    }

    static bool HandleStartGhostInWorld(ChatHandler* handler, Optional<std::string_view> args)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return true;

        // 1. Verificam daca s-a introdus numele caracterului
        if (!args)
        {
            handler->SendSysMessage("Usage: |cffffffff.ztfcbot ghost|r |cff00ff00CharacterName|r");
            return true;
        }

        // 2. Normalizam numele primit (Prima litera mare, restul mici)
        std::string targetName(args.value());
        if (!targetName.empty())
        {
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            targetName[0] = std::toupper(targetName[0]);
        }

        // 3. Cautam caracterul in Cache-ul global pentru a-i afla GUID-ul si Account ID-ul
        CharacterCacheEntry const* targetCache = sCharacterCache->GetCharacterCacheByName(targetName);
        if (!targetCache)
        {
            handler->PSendSysMessage("Character |cffffffff'%s'|r does not exist in database.", targetName.c_str());
            return true;
        }

        // 4. Verificam daca caracterul solicitat este deja online (sa nu ii furam sesiunea daca se joaca cineva real)
        if (ObjectAccessor::FindConnectedPlayer(targetCache->Guid))
        {
            handler->PSendSysMessage("Character |cffffffff'%s'|r is already online!", targetName.c_str());
            return true;
        }

        uint32 targetAccId = targetCache->AccountId;
        uint32 targetGuidLow = targetCache->Guid.GetCounter();

        // 5. Verificam daca acest caracter este deja in trackerul nostru (sa nu il incarcam de doua ori)
        for (const auto& tracker : g_MultiBotTracker)
        {
            if (tracker.charGuid == targetGuidLow)
            {
                handler->PSendSysMessage("Character |cffffffff'%s'|r is already registered in the bot system.", targetName.c_str());

                return true;
            }
        }

        if (sWorld->FindSession(targetAccId))
        {
            handler->PSendSysMessage("Cannot load |cffffffff'%s'|r. A real player or another session is already active on Account ID: %u.",
                targetName.c_str(), targetAccId);
            return true;
        }

        // 5. Verificam trackerul global pentru boti
        for (const auto& tracker : g_MultiBotTracker)
        {
            // Verificam daca caracterul exact este deja incarcat
            if (tracker.charGuid == targetGuidLow)
            {
                handler->PSendSysMessage("Character |cffffffff'%s'|r is already registered in the bot system.", targetName.c_str());
                return true;
            }

            // Daca botul gasit este activ (isProcessed) sau in curs de incarcare (!RemoveFromWorld)
            if (tracker.accountId == targetAccId && !tracker.RemoveFromWorld)
            {
                handler->PSendSysMessage("Cannot load |cffffffff'%s'|r. Account ID: %u already has another bot registered in the system.",
                    targetName.c_str(), targetAccId);
                return true;
            }
        }

        handler->PSendSysMessage("Starting async ghost loading for |cff00ff00%s|r (Account: %u, GUID: %u)...",
            targetName.c_str(), targetAccId, targetGuidLow);

        // =================================================================
        // --- GHOST START IN WORLD ---
        // =================================================================
        g_BootSequenceTimer = 5000;
        PornesteBotIndividual(targetAccId, targetGuidLow);

        for (auto& tracker : g_MultiBotTracker)
        {
            if (tracker.accountId == targetAccId)
            {
                tracker.AddFromChatCmd = true;
                break;
            }
        }

        return true;
    }

    static bool HandleStartBulkGhostsInWorld(ChatHandler* handler, Trinity::ChatCommands::Tail args)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return true;

        // 1. Convertim textul ramas in string standard de C++
        std::string argsStr(args);

        // Verificam daca s-au introdus argumente
        if (argsStr.empty())
        {
            handler->SendSysMessage("Usage: |cffffffff.ztfcbot ghostbulk|r |cff00ff00[Count]-[Months]|r (Ex: .ztfcbot ghostbulk 10-3)");
            return true;
        }

        uint32 botCount = 0;
        uint32 monthsInactive = 0;

        // 2. Parsam formatul specific cu cratima (Ex: 10-3)
        if (sscanf(argsStr.c_str(), "%u-%u", &botCount, &monthsInactive) != 2)
        {
            handler->SendSysMessage("Format invalid! Foloseste formatul cu cratima. Usage: |cffffffff.ztfcbot ghostbulk|r |cff00ff00[Count]-[Months]|r (Ex: 10-3)");
            return true;
        }

        // Validare ca numerele sunt mai mari decat 0
        if (botCount == 0 || monthsInactive == 0)
        {
            handler->SendSysMessage("Valorile introduse trebuie sa fie mai mari decat 0!");
            return true;
        }

        // Limitam numarul maxim de boti incarcati la o singura comanda pentru a preveni lag-ul masiv
        if (botCount > 100)
        {
            handler->SendSysMessage("Ce cereai e prea mult! Limita maxima la o singura comanda este de 100 de boti.");
            botCount = 100;
        }

        handler->PSendSysMessage("Cautam %u caractere cu conturi unice, nivel > 60 si inactivitate de peste %u luni...", botCount, monthsInactive);

        // 3. Interogare SQL: Grupam dupa 'account' pentru unicitate deplina
        QueryResult result = CharacterDatabase.PQuery(
            "SELECT c.guid, c.account, c.name FROM characters c "
            "WHERE c.level > 60 AND c.online = 0 "
            "AND c.logout_time < (UNIX_TIMESTAMP() - ({} * 30 * 24 * 60 * 60)) "
            "AND c.logout_time = (SELECT MAX(sub.logout_time) FROM characters sub WHERE sub.account = c.account) "
            "ORDER BY c.logout_time DESC", monthsInactive);

        if (!result)
        {
            handler->SendSysMessage("Nu s-a gasit niciun caracter care sa respecte aceste criterii in baza de date.");
            return true;
        }

        uint32 successfullyStarted = 0;

        // 4. Parcurgem rezultatele gasite
        do
        {
            Field* fields = result->Fetch();

            // EXPLICIT SI FIX: Folosim operatorul [] cu indecsi clari pentru a preveni inversarea datelor
            uint32 targetGuidLow = fields[0].GetUInt32();   // Coloana 0 din SELECT: c.guid
            uint32 targetAccId = fields[1].GetUInt32();   // Coloana 1 din SELECT: c.account
            std::string targetName = fields[2].GetString(); // Coloana 2 din SELECT: c.name

            ObjectGuid targetGuid = ObjectGuid::Create<HighGuid::Player>(targetGuidLow);

            // A. Verificam daca caracterul este deja online in ObjectAccessor
            if (ObjectAccessor::FindConnectedPlayer(targetGuid))
                continue;

            // B. Verificam daca contul are deja o sesiune activa (jucator real online sau alt bot activ)
            if (sWorld->FindSession(targetAccId))
                continue;

            // C. Verificam trackerul global ca sa nu dublam botul
            bool alreadyInTracker = false;
            for (const auto& tracker : g_MultiBotTracker)
            {
                if (tracker.charGuid == targetGuidLow || (tracker.accountId == targetAccId && !tracker.RemoveFromWorld))
                {
                    alreadyInTracker = true;
                    break;
                }
            }

            if (alreadyInTracker)
                continue;

            // D. LOGARE ASINCRONA SECURIZATA
            g_BootSequenceTimer = 5000;

            // Trimitem parametrii mapati corect: Contul (666) si GUID-ul Low (478)
            PornesteBotIndividual(targetAccId, targetGuidLow);

            // Activam flag-ul in tracker exact dupa ID-ul de cont corectat
            for (auto& tracker : g_MultiBotTracker)
            {
                if (tracker.accountId == targetAccId)
                {
                    tracker.AddFromChatCmd = true;
                    break;
                }
            }

            successfullyStarted++;

            // Ne oprim cand am atins numarul cerut de boti solicitati de GM
            if (successfullyStarted >= botCount)
                break;

        } while (result->NextRow());


        // 5. Trimitem raportul final inapoi in chat GM-ului
        if (successfullyStarted > 0)
        {
            handler->PSendSysMessage("|cff00ff00Succes!|r S-a inceput incarcarea pentru |cffffffff%u|r boti pe conturi complet diferite.", successfullyStarted);
        }
        else
        {
            handler->SendSysMessage("Nu s-a putut incarca niciun bot. Conturile gasite sunt deja ocupate sau logate.");
        }

        return true;
    }

    // Comanda auxiliara care iti arata ce boti ai trezit prin comenzi
    static bool HandleShowGhostList(ChatHandler* handler, Optional<std::string_view> /*args*/)
    {
        handler->SendSysMessage("=== |cff00ff00Active Ghost Bots in Tracker|r ===");
        uint32 index = 1;
        for (const auto& tracker : g_MultiBotTracker)
        {
            handler->PSendSysMessage("%u. Account ID: %u | Character GUID: %u | Status: %s",
                index++, tracker.accountId, tracker.charGuid, tracker.isProcessed ? "|cff00ff00ONLINE|r" : "|cffff0000LOADING|r");
        }
        return true;
    }

    static bool HandleRemoveGhostFromWorld(ChatHandler* handler, Optional<std::string_view> args)
    {
        // 1. Verificam daca s-a introdus numele
        if (!args)
        {
            handler->SendSysMessage("Usage: |cffffffff.ztfcbot ghost remove|r |cff00ff00CharacterName|r");
            return true;
        }

        // 2. Normalizam numele primit
        std::string targetName(args.value());
        if (!targetName.empty())
        {
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            targetName[0] = std::toupper(targetName[0]);
        }

        // 3. Cautam in Cache-ul global pentru a afla GUID-ul
        CharacterCacheEntry const* targetCache = sCharacterCache->GetCharacterCacheByName(targetName);
        if (!targetCache)
        {
            handler->PSendSysMessage("Character |cffffffff'%s'|r does not exist in database.", targetName.c_str());
            return true;
        }

        uint32 targetGuidLow = targetCache->Guid.GetCounter();
        bool gasitSiSters = false;

        // 4. Cautam botul in tracker-ul nostru global
        for (auto& tracker : g_MultiBotTracker)
        {
            if (!tracker.isProcessed)
            {
                handler->PSendSysMessage("Ghost character |cff00ff00%s|r is in loading... can't remove.", targetName.c_str());
                break;
            }

            if (tracker.charGuid == targetGuidLow)
            {
                gasitSiSters = true;

                TC_LOG_INFO("fakPlayer", "LOG GHOST REMOVE: Comanda manuala de stergere pentru botul GUID {}.", tracker.charGuid);

                ForseazaStergereBotFantoma(tracker);

                handler->PSendSysMessage("Ghost character |cff00ff00%s|r has been marked for safe removal from the bot system.", targetName.c_str());

                break;
            }
        }

        if (!gasitSiSters)
        {
            handler->PSendSysMessage("Character |cffffffff'%s'|r is not registered in the bot tracker.", targetName.c_str());
        }

        return true;
    }


};



class kitt_ghost_ack_packet : public ServerScript
{
public:
    kitt_ghost_ack_packet() : ServerScript("kitt_ghost_ack_packet") {}

    // 1. CAPTURA PACHETE TRIMISE DE SERVER CATRE BOT (SMSG)
    void OnPacketSend(WorldSession* session, WorldPacket& packet) override
    {
        if (!session || !session->GetPlayer())
            return;

        Player* player = session->GetPlayer();
        uint16 opcode = packet.GetOpcode();
        std::string const& accName = session->GetAccountName();

        if (accName.find("REAL_BOT_ACC_") == std::string::npos)
            return;

        std::string botName = player->GetName();

        // Verificam pachetul nativ de invitatie grup (0x06F)
        if (opcode == 0x06F)  // invite
        {
            TC_LOG_INFO("server.loading", "[BotNetwork] -> HOOK: Botul {} a primit SMSG_GROUP_INVITE! Injectam acceptul...", botName.c_str());

            WorldPacket* acceptInvite = new WorldPacket(CMSG_GROUP_ACCEPT, 4);
            *acceptInvite << uint32(0);
            session->QueuePacket(acceptInvite);
        }
    }

    void OnPacketReceive(WorldSession* session, WorldPacket& packet) override
    {
        if (!session || !session->GetPlayer())
            return;

        Player* player = session->GetPlayer();
        uint16 opcode = packet.GetOpcode();
        std::string const& accName = session->GetAccountName();

        if (accName.find("REAL_BOT_ACC_") == std::string::npos)
            return;

        // Acest log se va aprinde in sfarsit cand motorul extrage pachetul!
        TC_LOG_INFO("server.loading", "[BotNetwork] C->S [PRIMIT] De la Bot: {} | Opcode: 0x{:X} (Size: {})",
            player->GetName().c_str(), opcode, (uint32)packet.size());
    }
};


void AddSC_kitt_bot_world_loader()
{
    new kitt_bot_world_loader();
    new kitt_bot_account_login_interceptor();
    new kitt_bot_world_update();
    new kitt_bot_chat_handler();
    new kitt_ghost_player_command();
    new kitt_ghost_ack_packet();
}
