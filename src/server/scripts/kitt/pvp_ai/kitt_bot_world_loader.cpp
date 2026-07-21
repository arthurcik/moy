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
std::mutex g_BotTrackerMutex;

std::unordered_map<uint32, KittBotArenaTracker> g_KittBotArenaRegistru; // arena join muti-task

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
    //static std::vector<std::unique_ptr<WorldSession>> g_GhostSessionsStorage;

    static uint32 g_BootSequenceTimer = 10000;

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
        //tracker.futureResult = task.GetFuture();
        tracker.isProcessed = false;
        tracker.isQueued = false;
        tracker.isReady = false;



        tracker.realSession->SetIsKittBot(true);
        sWorld->AddSession(tracker.realSession);
        //CharacterDatabase.DelayQueryHolder(loginHolder);
        uint32 currentAccountId = bot.accountId;

        // 3. Inseram trackerul in vectorul global PROTEJAT DE MUTEX
        {
            std::lock_guard<std::mutex> lock(g_BotTrackerMutex);
            g_MultiBotTracker.push_back(std::move(tracker));
        }

        // 4. FIX PENTRU E2634: Utilizam AddQueryHolderCallback din interiorul sesiunii create pentru bot
        // Aceasta functie stie nativ sa trimita holderul catre CharacterDatabase fara erori de compilare.
        fakeSession->AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(loginHolder))
            .AfterComplete([currentAccountId](SQLQueryHolderBase const& /*holder*/)
                {
                    // Aceasta bucata se executa AUTOMAT pe thread-ul principal cand SQL-ul s-a terminat!
                    std::lock_guard<std::mutex> lock(g_BotTrackerMutex);

                    // Cautam botul in vector dupa accountId
                    for (auto& t : g_MultiBotTracker)
                    {
                        if (t.accountId == currentAccountId)
                        {
                            t.isReady = true; // Activam flag-ul ca datele sunt complet incarcate in memorie
                            break;
                        }
                    }
                });
        //g_MultiBotTracker.push_back(std::move(tracker));
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

    //CharacterDatabase.DelayQueryHolder(loginHolder);
    //FictivBotsGuids.insert(playerGuid);

    fakeSession->SetIsKittBot(true);
    sWorld->AddSession(fakeSession);
    uint32 currentAccountId = accountId;

    {
        std::lock_guard<std::mutex> lock(g_BotTrackerMutex);
        bool gasitInTracker = false;
        for (auto& tracker : g_MultiBotTracker)
        {
            if (tracker.accountId == accountId)
            {
                // resetam
                //tracker.realSession = nullptr;
                //tracker.holder = nullptr;
                //tracker.futureResult = {};
                tracker.isReady = false;

                // adaugam
                tracker.realSession = fakeSession;
                tracker.holder = loginHolder;
                //tracker.futureResult = task.GetFuture();

                // resetare flags
                tracker.isProcessed = false;
                tracker.kickedByPlayer = false;
                tracker.AccRelogDelay = 5000; // Sincronizam resetarea de siguranta
                tracker.AccRealBusy = false;
                tracker.RemoveFromWorld = false;

                //sWorld->AddSession(tracker.realSession);

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
            //tracker.futureResult = task.GetFuture();
            tracker.isReady = false;
            tracker.isProcessed = false;
            tracker.isQueued = false;
            tracker.rejoinTimer = 0;              // Completat
            tracker.kickedByPlayer = false;
            tracker.AccRelogDelay = 10000;        // Completat conform structurii
            tracker.AccRealBusy = false;          // Completat conform structurii
            tracker.RemoveFromWorld = false;

            //sWorld->AddSession(tracker.realSession);
            g_MultiBotTracker.push_back(std::move(tracker));
        }
    }

    fakeSession->AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(loginHolder))
        .AfterComplete([currentAccountId](SQLQueryHolderBase const& /*holder*/)
            {
                // Aceasta bucata se executa AUTOMAT cand thread-ul MySQL a terminat reincarcarea datelor botului!
                std::lock_guard<std::mutex> lock(g_BotTrackerMutex);

                // Cautam botul in vector dupa accountId pentru a-l trece in starea pregatita
                for (auto& t : g_MultiBotTracker)
                {
                    if (t.accountId == currentAccountId)
                    {
                        t.isReady = true;
                        break;
                    }
                }
            });

    FictivBotsGuids.insert(playerGuid);

    TC_LOG_INFO("fakPlayer", "LOG REBOOT: Secventa asincrona a fost relansata curat pentru Cont ID: {}!", accountId);
}

void ForseazaStergereBotFantoma(BotAsyncTracker& tracker)
{
    // test ....
    if (!tracker.kickedByPlayer && !tracker.isProcessed)
    {
        TC_LOG_INFO("fakPlayer", "LOG CUSTOM AVERTISMENT: Botul cu GUID {} se afla in loading screen! Stergerea a fost blocata pentru siguranta.", tracker.charGuid);
        return;
    }

    if (!tracker.kickedByPlayer && !tracker.AccRealBusy)
    {
        tracker.RemoveFromWorld = true;
    }
    //tracker.RemoveFromWorld = true;
    //tracker.futureResult = {};
    tracker.holder = nullptr;

    ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);
    FictivBotsGuids.erase(playerGuid);

    // Cautam daca jucatorul este inca in lume
    if (Player* botPlayer = ObjectAccessor::FindConnectedPlayer(playerGuid))
    {
        //TC_LOG_INFO("login erase", "se activeaza la PEMATUR?");
        botPlayer->GetSession()->LogoutPlayer(true);
        botPlayer->GetSession()->PlayerLogout();
        //botPlayer->CombatStop();
        //botPlayer->RemoveFromWorld();
        //ObjectAccessor::RemoveObject(botPlayer);
        //botPlayer->SaveToDB(false);

        // CRITIC ANTI-CRASH: Daca sesiunea exista, rupem legatura dintre Player si Sesiune!
        /*if (WorldSession* session = botPlayer->GetSession())
        {
            // Setam player-ul din sesiune pe nullptr ca destructorul sesiunii sa nu mai apeleze LogoutPlayer pe un obiect mort
            // sa putem da "delete" ....
            if (session)
                session->SetPlayer(nullptr);
        }*/

        //botPlayer->CleanupsBeforeDelete();
        //delete botPlayer;
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
        //uint32 accId = tracker.realSession->GetAccountId();
        //sWorld->RemoveSession(accId);

        /*SessionMap& writableSessions = const_cast<SessionMap&>(sWorld->GetAllSessions());
        auto itr = writableSessions.find(accId);
        if (itr != writableSessions.end())
        {
            // IMPORTANT: NU MAI DAM "delete" direct aici (ca sa evitam crash-ul de double-free)!
            // Doar o stergem din map-ul global pentru a debloca contul pe loc!
            writableSessions.erase(itr);
        }*/

        //sWorld->RemoveSession(tracker.realSession->GetAccountId());

        //delete tracker.realSession;
        if (!tracker.isProcessed)
        {
            tracker.realSession->GetQueryProcessor().CancelAll();
        }
        tracker.realSession->SetIsKittBot(false);
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
            // CAZUL 2: Botul NU este inca procesat (este in cele 5 secunde de delay sau in incarcare SQL)
            else if (tracker.accountId == accountId && !tracker.kickedByPlayer)
            {
                tracker.AccRelogDelay = 5000;
                tracker.kickedByPlayer = true;
                tracker.AccRealBusy = true;
                tracker.isProcessed = false;
                tracker.isReady = false; // Oprim asincronul SQL din a mai valida datele ca fiind gata

                TC_LOG_INFO("fakPlayer", "LOG ACCOUNT KICK [PREMATUR]: Jucatorul real s-a logat inainte ca botul sa intre in lume (Cont ID: {}). Se avorteaza secventa...", accountId);

                ForseazaStergereBotFantoma(tracker);

                TC_LOG_INFO("fakPlayer", "LOG ACCOUNT KICK [PREMATUR]: Sesiunea fantoma si flag-urile au fost curatate pentru contul {}.", accountId);
                break;
            }
        }
    }
};

class kitt_bot_world_loader : public WorldScript
{
public:
    kitt_bot_world_loader() : WorldScript("kitt_bot_world_loader") {}

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
                //g_GhostSessionsStorage.clear();

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
            ReintrareAsyncDupaLogOut(tracker, diff);

            if (!tracker.realSession)
                continue;

            // Intrare in lume
            IntrareaInLume(tracker, diff);

            if (tracker.AddFromChatCmd)
                continue;

            // 2. LOGICA DINAMICA DE COADA SI PORT IN ARENA
            PortInArenaDinamic(tracker, diff);
        }

        VerificaSiReseteazaFlaguriBlocate();

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

        Group* group = botPlayer->GetGroup();

        if (!group)
            return;

        if (group && !group->IsLeader(botPlayer->GetGUID()) && group->GetMembersCount() != 2) // 2 trebuie
        {
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
        time_t timpServerCurent = GameTime::GetGameTime();

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

                            // flaguri
                            ResetareFlaguriFormare(arenaTeamId, 2);
                            auto& tArena = g_KittBotArenaRegistru[arenaTeamId];
                            tArena.botLiderDeEchipa = botPlayer->GetGUID();
                            tArena.areGrupIn2v2 = true;
                            tArena.timpIntrareInCoada = timpServerCurent;
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

    void JoinGroupArena3v3Rated(Player* botPlayer)
    {
        if (!botPlayer)
            return;

        Group* group = botPlayer->GetGroup();

        if (!group)
            return;

        if (group && !group->IsLeader(botPlayer->GetGUID()) && group->GetMembersCount() != 3) // 3 trebuie
        {
            return;
        }

        // Pasul 2: Preluam datele de baza pentru arene
        Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
        if (!bg)
            return;

        PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), botPlayer->GetLevel());
        if (!bracketEntry)
            return;

        // Pasul 3: ID-ul cozii pentru Arena 3v3 (Slot index 0 in echipa, dimensiune echipa 2)
        uint8 teamSizeIndex = 1; // 0 = 2v2, 1 = 3v3, 2 = 5v5
        BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bg->GetTypeID(), bracketEntry->GetBracketId(), 3); // 3 = 3v3
        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

        // Pasul 4: Preluam ID-ul echipei de Arena 3v3 si rating-urile (Exact ca in functia ta nativa)
        uint32 arenaTeamId = botPlayer->GetArenaTeamId(teamSizeIndex);
        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(arenaTeamId);

        uint32 arenaRating = 1;
        uint32 matchmakerRating = 0;
        uint32 previousOpponents = 0;
        time_t timpServerCurent = GameTime::GetGameTime();

        if (at)
        {
            arenaRating = at->GetRating();
            if (arenaRating <= 0) arenaRating = 1;

            matchmakerRating = at->GetAverageMMR(group);
            previousOpponents = at->GetPreviousOpponents();
        }
        else
        {
            TC_LOG_ERROR("fakPlayer", "Eroare Coada: Liderul bot {} nu are o echipa de arena 3v3 valida!", botPlayer->GetName().c_str());
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

                            // flaguri
                            ResetareFlaguriFormare(arenaTeamId, 3);
                            auto& tArena = g_KittBotArenaRegistru[arenaTeamId];
                            tArena.botLiderDeEchipa = botPlayer->GetGUID();
                            tArena.areGrupIn3v3 = true;
                            tArena.timpIntrareInCoada = timpServerCurent;

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

            TC_LOG_INFO("fakPlayer", "Succes Coada: Echipa de boti condusa de {} a intrat oficial in query-ul de Matchmaking 3v3 Rated!", botPlayer->GetName().c_str());
        }
        else
        {
            TC_LOG_ERROR("fakPlayer", "Eroare Coada: AddGroup a returnat NULL pentru grupul botului {}.", botPlayer->GetName().c_str());
        }
    }

    void JoinGroupArena5v5Rated(Player* botPlayer)
    {
        if (!botPlayer)
            return;

        Group* group = botPlayer->GetGroup();

        if (!group)
            return;

        if (group && !group->IsLeader(botPlayer->GetGUID()) && group->GetMembersCount() != 5) // 5 trebuie
        {
            return;
        }

        // Pasul 2: Preluam datele de baza pentru arene
        Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
        if (!bg)
            return;

        PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), botPlayer->GetLevel());
        if (!bracketEntry)
            return;

        // Pasul 3: ID-ul cozii pentru Arena 5v5 (Slot index 0 in echipa, dimensiune echipa 2)
        uint8 teamSizeIndex = 2; // 0 = 2v2, 1 = 3v3, 2 = 5v5
        BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bg->GetTypeID(), bracketEntry->GetBracketId(), 5); // 5 = 5v5
        BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

        // Pasul 4: Preluam ID-ul echipei de Arena 5v5 si rating-urile (Exact ca in functia ta nativa)
        uint32 arenaTeamId = botPlayer->GetArenaTeamId(teamSizeIndex);
        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(arenaTeamId);

        uint32 arenaRating = 1;
        uint32 matchmakerRating = 0;
        uint32 previousOpponents = 0;
        time_t timpServerCurent = GameTime::GetGameTime();

        if (at)
        {
            arenaRating = at->GetRating();
            if (arenaRating <= 0) arenaRating = 1;

            matchmakerRating = at->GetAverageMMR(group);
            previousOpponents = at->GetPreviousOpponents();
        }
        else
        {
            TC_LOG_ERROR("fakPlayer", "Eroare Coada: Liderul bot {} nu are o echipa de arena 5v5 valida!", botPlayer->GetName().c_str());
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

                            // flaguri
                            ResetareFlaguriFormare(arenaTeamId, 5);
                            auto& tArena = g_KittBotArenaRegistru[arenaTeamId];
                            tArena.botLiderDeEchipa = botPlayer->GetGUID();
                            tArena.areGrupIn5v5 = true;
                            tArena.timpIntrareInCoada = timpServerCurent;

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

            TC_LOG_INFO("fakPlayer", "Succes Coada: Echipa de boti condusa de {} a intrat oficial in query-ul de Matchmaking 5v5 Rated!", botPlayer->GetName().c_str());
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
        else
        {
            // VERIFICARE SEPARATA PENTRU MMR SUB 1200
            for (ArenaTeam::MemberList::iterator itr = at->m_membersBegin(); itr != at->m_membersEnd(); ++itr)
            {
                if (itr->Guid == botPlayer->GetGUID())
                {
                    uint32 bonusMMR = urand(150, 250);
                    if (itr->MatchMakerRating < 1200)
                    {
                        itr->MatchMakerRating = itr->MatchMakerRating + bonusMMR;

                        TC_LOG_INFO("fakPlayer", "Arena MMR: S-au salvat in siguranta {} MMR in cache-ul RAM al echipei {} pentru urmatorul meci!",
                            itr->MatchMakerRating, arenaTeamId);
                    }
                    break;
                }
            }
        }
    }

    // Intrare in lume
    void IntrareaInLume(BotAsyncTracker& tracker, uint32 /*diff*/)
    {
        if (!tracker.isProcessed && !tracker.kickedByPlayer)
        {
            if (g_BootSequenceTimer > 0)
                return;

            if (tracker.isReady)
            {
                tracker.isProcessed = true;
                g_BootSequenceTimer = 5000;

                auto loginHolder = std::static_pointer_cast<CharacterDatabaseQueryHolder>(tracker.holder);
                if (!loginHolder)
                {
                    tracker.isProcessed = true;
                    //continue;
                    return;
                }

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
                    //continue;
                    return;
                }

                Player* botPlayer = new Player(realSession);
                ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);

                if (botPlayer->LoadFromDB(playerGuid, *tracker.holder))
                {
                    // protectie map instance
                    // nu se poate crea instata daca nu e jucator deja
                    tracker.realSession->SetPlayer(botPlayer); // test sa il gaseasca la stergere

                    uint32 botMapId = botPlayer->GetMapId();
                    MapEntry const* mapEntry = sMapStore.LookupEntry(botMapId);

                    if (!mapEntry || mapEntry->Instanceable())
                    {
                        //ForseazaStergereBotFantoma(tracker);

                        uint32 homeMapId = botPlayer->m_homebindMapId;
                        float homeX = botPlayer->m_homebindX;
                        float homeY = botPlayer->m_homebindY;
                        float homeZ = botPlayer->m_homebindZ;
                        float homeO = botPlayer->GetOrientation();
                        //botPlayer->TeleportTo(homeMapId, homeX, homeY, homeZ, homeO);
                        CharacterDatabase.PExecute("UPDATE characters SET position_x = {}, position_y = {}, position_z = {}, orientation = {}, map = {}, instance_id = 0 WHERE guid = {};",
                            homeX, homeY, homeZ, homeO, homeMapId, tracker.charGuid);


                        TC_LOG_INFO("fakPlayer", "LOG GHOST PROTECTIE: Botul {} a fost mutat in memorie catre Homebind (Map: {}).", botPlayer->GetName().c_str(), homeMapId);

                        //PornesteBotIndividual(tracker.accountId, tracker.charGuid);

                        botPlayer->CleanupsBeforeDelete();
                        delete botPlayer;
                        if (tracker.realSession)
                        {
                            tracker.realSession->SetIsKittBot(false); // Oprim imunitatea sesiunii vechi pentru a fi curatata de nucleu
                            tracker.realSession->SetPlayer(nullptr);
                            tracker.realSession = nullptr;
                        }

                        tracker.RemoveFromWorld = true;
                        tracker.isProcessed = true;
                        PornesteBotIndividual(tracker.accountId, tracker.charGuid);
                        //realSession->SetPlayer(nullptr);

                        //continue;
                        return;
                    }
                    // -------------------

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

                    //botPlayer->GetMotionMaster()->Initialize();
                    //botPlayer->SendDungeonDifficulty(false);

                    //tracker.realSession->LoadPermissions();
                    //tracker.realSession->SetPlayer(botPlayer);

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

                        // 2. CONFIGUR?M SESIUNEA ?I MI?CAREA (Doar dup? ce harta este valid?!)
                        tracker.realSession->SetPlayer(botPlayer);
                        botPlayer->GetMotionMaster()->Initialize();
                        botPlayer->SendDungeonDifficulty(false);

                        // Protectie anti-gravitate la spawn: Seta?i semafoarele pentru a bloca c?derea ?n gol ?n primul tick
                        botPlayer->SetSemaphoreTeleportFar(true);
                        botPlayer->SetSemaphoreTeleportNear(true);

                        // 3. LOGARE ?N LUME ?N ORDINEA OFICIAL? TRINITYCORE
                        // Pasul A: Ad?ug?m ?n Accessorul Global pentru ca thread-urile de h?r?i s? ?l g?seasc? ?n RAM
                        ObjectAccessor::AddObject(botPlayer);

                        // Pasul B: Ad?ug?m playerul ?n registrul fizic al h?r?ii active
                        botPlayer->GetMap()->AddPlayerToMap(botPlayer);

                        // Pasul C: Activ?m prezen?a lui global? ?n lume (Broadcast c?tre cei din jur)
                        botPlayer->AddToWorld();



                        // 4. PLANIFIC?M DEBLOCAREA SEMAFOARELOR (Dup? ce se a?az? ?n Grid)
                        class BotSpawnSafeEvent : public BasicEvent
                        {
                        public:
                            BotSpawnSafeEvent(Player* _player) : player(_player) {}
                            bool Execute(uint64, uint32) override
                            {
                                if (player && player->IsInWorld())
                                {
                                    player->SetSemaphoreTeleportFar(false);
                                    player->SetSemaphoreTeleportNear(false);
                                    player->StopMoving();
                                    TC_LOG_INFO("fakPlayer", "[BotNetwork] -> [SPAWN-SAFE] Semafoarele de siguran?? au fost ridicate pentru {}.", player->GetName().c_str());
                                }
                                return true;
                            }
                        private:
                            Player* player;
                        };
                        botPlayer->m_Events.AddEvent(new BotSpawnSafeEvent(botPlayer), botPlayer->m_Events.CalculateTime(800ms));



                        //botPlayer->GetMap()->AddPlayerToMap(botPlayer);

                        //botPlayer->AddToWorld();
                        //ObjectAccessor::AddObject(botPlayer);

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

                        botPlayer->SendInitialPacketsBeforeAddToMap();
                        botPlayer->SendInitialPacketsAfterAddToMap();

                        // ===========================================================

                        FictivBotsGuids.insert(playerGuid);

                        TC_LOG_INFO("fakPlayer", "LOG CUSTOM REUSIT: {} (GUID: {}) a intrat online permanent pe sesiunea reala!", botPlayer->GetName().c_str(), tracker.charGuid);
                        //break;
                        return;
                    }
                }
                else
                {
                    TC_LOG_INFO("fakPlayer", "LOG CUSTOM EROARE: LoadFromDB a refuzat structura holder-ului pentru GUID {}.", tracker.charGuid);

                    realSession->SetPlayer(nullptr);
                    botPlayer->CleanupsBeforeDelete();
                    delete botPlayer;

                    tracker.realSession->SetIsKittBot(false);
                    tracker.realSession = nullptr;
                    tracker.isProcessed = true;

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
                //return;
                //continue;
                return;
            }
        }

    }

    // --- RE-INTRARE ASINCRONA DUPA LOGOUT JUCATOR REAL ---
    void ReintrareAsyncDupaLogOut(BotAsyncTracker& tracker, uint32 /*diff*/)
    {
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
                //continue;
                return;
            }

            // Daca omul a dat logout si contul e complet liber:
            if (!tracker.AccRealBusy)
            {
                tracker.AccRelogDelay = 5000;
                tracker.kickedByPlayer = false;
                PornesteBotIndividual(tracker.accountId, tracker.charGuid);

                TC_LOG_INFO("fakPlayer", "LOG REJOIN: Contul {} a fost eliberat. Execut direct secventa de boot...", tracker.accountId);
            }
            //continue;
            return;
        }

    }

    // 2. LOGICA DINAMICA DE COADA SI PORT IN ARENA
    void PortInArenaDinamic(BotAsyncTracker& tracker, uint32 diff)
    {
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

                                                /*if (botPlayer->GetSession())
                                                {
                                                    WorldPacket pachetGol;
                                                    botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                                                }*/

                                                botPlayer->DurabilityRepairAll(false, 0, false);
                                                botPlayer->RemoveAllAuras();
                                            }

                                            if (botPlayer->IsInCombat())
                                            {
                                                botPlayer->AttackStop();
                                                botPlayer->ClearInCombat();
                                            }

                                            // 1. Luam inaltimea curenta a botului ca fallback sigur
                                            float groundHeight = botPlayer->GetPositionZ();

                                            // 2. PROTECTIE: Rulam GetHeight DOAR daca coordonatele botului sunt valide matematic in RAM
                                            if (!std::isnan(botPlayer->GetPositionX()) && !std::isinf(botPlayer->GetPositionX()) &&
                                                !std::isnan(botPlayer->GetPositionY()) && !std::isinf(botPlayer->GetPositionY()))
                                            {
                                                groundHeight = botPlayer->GetMap()->GetHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY(), botPlayer->GetPositionZ());
                                            }

                                            // 3. Verificam starea de cadere sau anomalie de pozitie
                                            if (botPlayer->GetPositionZ() > (groundHeight + 5.0f) ||
                                                botPlayer->IsFalling() ||
                                                botPlayer->IsUnderWater() ||
                                                botPlayer->IsInWater() ||
                                                botPlayer->GetPositionZ() < botPlayer->GetMap()->GetMinHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY()))
                                            {
                                                TC_LOG_INFO("fakPlayer", "LOG PROTECTIE: Botul {} este in apa, cade sau e sub harta. Il trimitem acasa.", botPlayer->GetName().c_str());

                                                // Teleportam direct botul. NU mai apelam HandleMoveWorldportAckOpcode cu pachete goale aici!
                                                // Core-ul va cere singur ACK-ul pe canalul corect cand se va executa teleportarea.
                                                botPlayer->TeleportTo(botPlayer->m_homebindMapId, botPlayer->m_homebindX, botPlayer->m_homebindY, botPlayer->m_homebindZ, botPlayer->GetOrientation());

                                                /*if (botPlayer->GetSession())
                                                {
                                                    WorldPacket pachetGol;
                                                    botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                                                }*/
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

                                        // 1. Sincronizam echipa botului
                                        botPlayer->SetBGTeam(bgTeamId == TEAM_ALLIANCE ? ALLIANCE : HORDE);

                                        // 2. Configuram punctele de tranzit si intrarile native conform header-ului tau
                                        botPlayer->SetBattlegroundEntryPoint();
                                        botPlayer->SetBattlegroundId(bg->GetInstanceID(), bg->GetTypeID());

                                        // Corectie functii invitat: Preluam ID-ul de invitatie si il salvam pe player
                                        uint32 inviteTeamId = botPlayer->GetArenaTeamIdInvited();
                                        botPlayer->SetArenaTeamIdInvited(inviteTeamId);

                                        tracker.isQueued = true;

                                        // === LINIA TA DE SIGURANTA ANTI-PUNCTARE DUBLA ===
                                        // sa nu le dea dublu jocuri contorizate
                                        bgQueue.RemovePlayer(botPlayer->GetGUID(), false);

                                        // -- scoate flags arena bots
                                        // ==========================================================
                                        // DETECTIE DINAMICA SI RESETARE FLAGURI REGISTRU TEAM ID
                                        // ==========================================================
                                        // Determinam automat tipul arenei (2, 3 sau 5) pe baza configuratiei cozii
                                        if (queueTypeId.TeamSize > 0)
                                        {
                                            uint8 slotIndexEchipa = (queueTypeId.TeamSize == 3) ? 1 : ((queueTypeId.TeamSize == 5) ? 2 : 0);
                                            uint32 botArenaTeamId = botPlayer->GetArenaTeamId(slotIndexEchipa);

                                            if (botArenaTeamId > 0)
                                            {
                                                auto& tArena = g_KittBotArenaRegistru[botArenaTeamId];

                                                // Dezactivam flag-ul corespunzator arenei din care tocmai a plecat
                                                if (queueTypeId.TeamSize == 2) tArena.areGrupIn2v2 = false;
                                                if (queueTypeId.TeamSize == 3) tArena.areGrupIn3v3 = false;
                                                if (queueTypeId.TeamSize == 5) tArena.areGrupIn5v5 = false;

                                                // Resetam timestamp-ul de coada pentru acest Team ID
                                                tArena.timpIntrareInCoada = 0;

                                                TC_LOG_INFO("fakPlayer", "TRACKER ARENA: Botul {} (TeamID: {}) a acceptat invitatia si intra in meciul de {}v{}. Am deblocat coada locala.",
                                                    botPlayer->GetName().c_str(), botArenaTeamId, queueTypeId.TeamSize, queueTypeId.TeamSize);
                                            }
                                        }

                                        // ================================================

                                        // 3. REPARATIE CRITICA PARAMETRI: Teleportarea nativa prin managerul de Battleground
                                        // Parametrii trimisi: obiectul player, ID-ul real de instanta al arenei, Tipul de BG/Arena
                                        sBattlegroundMgr->SendToBattleground(botPlayer, bg->GetInstanceID(), bg->GetTypeID());

                                        TC_LOG_INFO("fakPlayer", "LOG ARENA REUSIT: {} a intrat nativ in Arena ID: {} (Tip: {}) cu eliminare din coada.",
                                            botPlayer->GetName().c_str(), bg->GetInstanceID(), bg->GetTypeID());

                                        break;


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
                        /*if (botPlayer->GetSession())
                        {
                            WorldPacket pachetGol;
                            botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                        }*/

                        // --- LOGICA VERIFICARE SI ADAUGARE RATING ---
                        CheckAndRewardArenaBotRating(botPlayer);
                        CheckAndRewardArenaBotPersonalRating(botPlayer);

                        // leave group
                        /*if (botPlayer->GetGroup())
                        {
                            TC_LOG_INFO("fakPlayer", "LOG ARENA: Meci end, {} leave GROUP.", botPlayer->GetName().c_str());

                            botPlayer->GetGroup()->RemoveMember(botPlayer->GetGUID());
                        }*/

                        //tracker.isQueued = false;

                        //break;
                        //continue;
                        return;
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
                        if (!botPlayer->IsBeingTeleported() && !botPlayer->IsLoading() &&
                            !std::isnan(botPlayer->GetPositionX()) && !std::isinf(botPlayer->GetPositionX()) &&
                            !std::isnan(botPlayer->GetPositionY()) && !std::isinf(botPlayer->GetPositionY()))
                        {
                            float groundHeight = botPlayer->GetMap()->GetHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY(), botPlayer->GetPositionZ());

                            if (botPlayer->GetPositionZ() > (groundHeight + 5.0f) ||
                                botPlayer->IsFalling() ||
                                botPlayer->IsUnderWater() ||
                                botPlayer->IsInWater() ||
                                botPlayer->GetPositionZ() < botPlayer->GetMap()->GetMinHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY()))
                            {
                                TC_LOG_INFO("fakPlayer", "LOG PROTECTIE: Botul {} cade sau e in apa dupa meci. Il trimitem la Homebind.", botPlayer->GetName().c_str());

                                // Teleportam curat. NU mai trimitem pachetGol in HandleMoveWorldportAckOpcode!
                                // Core-ul nativ isi va gestiona singur tranzitul la Homebind.
                                botPlayer->TeleportTo(botPlayer->m_homebindMapId, botPlayer->m_homebindX, botPlayer->m_homebindY, botPlayer->m_homebindZ, botPlayer->GetOrientation());

                                /*if (botPlayer->GetSession())
                                {
                                    WorldPacket pachetGol;
                                    botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                                }*/
                            }
                        }
                    }

                    if (tracker.rejoinTimer <= diff)
                    {
                        bool existaJucatoriLaCoada = false;

                        uint8 tipArenaAles = 0; // Va salva 2, 3 sau 5 in functie de coada gasita activa

                        if (Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA))
                        {
                            if (PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), botPlayer->GetLevel()))
                            {
                                uint8 coziDeScanat[] = { 5, 3, 2 };

                                for (uint8 tipCoada : coziDeScanat)
                                {
                                    // ==================== OPRITORUL SUPREM PE TEAM ID ====================
                                    // Extragem ID-ul echipei noastre pentru bracket-ul pe care vrem sa il scanam
                                    uint8 slotIndexCoada = (tipCoada == 3) ? 1 : ((tipCoada == 5) ? 2 : 0);
                                    uint32 MyArenaTeamId = botPlayer->GetArenaTeamId(slotIndexCoada);

                                    if (MyArenaTeamId > 0)
                                    {
                                        /*auto& tArena = g_KittBotArenaRegistru[MyArenaTeamId];

                                        // DACA ECHIPA NOASTRA ARE DEJA UN GRUP IN ACEASTA COADA, SARE PESTE EA COMPLET!
                                        // Asta opreste instantaneu grupul lui Roguee din a folosi coada lasata de Jina.
                                        if (tipCoada == 2 && tArena.areGrupIn2v2)
                                        {
                                            continue;
                                        }
                                        if (tipCoada == 3 && tArena.areGrupIn3v3)
                                        {
                                            continue;
                                        }
                                        if (tipCoada == 5 && tArena.areGrupIn5v5)
                                        {
                                            continue;
                                        }*/


                                        // === VERIFICARE GLOBALA CURATA ===
                                        bool coadaOcupataDeCineva = false;

                                        for (auto const& [arenaId, dateArena] : g_KittBotArenaRegistru)
                                        {
                                            // Verifica DOAR variabila specifica tipului de coada curent
                                            if (tipCoada == 2 && dateArena.areGrupIn2v2)
                                            {
                                                coadaOcupataDeCineva = true;
                                                break;
                                            }

                                            if (tipCoada == 3 && dateArena.areGrupIn3v3)
                                            {
                                                coadaOcupataDeCineva = true;
                                                break;
                                            }
                                            if (tipCoada == 5 && dateArena.areGrupIn5v5)
                                            {
                                                coadaOcupataDeCineva = true;
                                                break;
                                            }
                                        }

                                        // Daca e ocupata, sarim peste inregistrare si bucla mare trece la urmatorul tip de coada
                                        if (coadaOcupataDeCineva)
                                        {
                                            continue;
                                        }
                                        // ===============================
                                    }
                                    else
                                    {
                                        // daca nu are echipa pt ce sa gasit in coada
                                        tracker.rejoinTimer = urand(10000, 20000);
                                        continue;
                                    }
                                    // ===================
                                    // Protectie: Botii nu pot da join la o arena mai mare decat numarul lor de membri!
                                    // Daca grupul are 3 boti, sare peste verificarea de 5v5.
                                    /*if (membriGrup < tipCoada)
                                        continue;*/

                                    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bg->GetTypeID(), bracketEntry->GetBracketId(), tipCoada);
                                    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);

                                    uint32 totalEchipeInCoada = 0;
                                    for (uint32 j = 0; j < 2; ++j)
                                    {
                                        totalEchipeInCoada += bgQueue.m_QueuedGroups[j].size();
                                    }

                                    // Daca gasim o coada impara (cineva asteapta pereche), o alegem instant!
                                    if (totalEchipeInCoada % 2 != 0)
                                    {
                                        existaJucatoriLaCoada = true;
                                        tipArenaAles = tipCoada; // Salvam bracket-ul sortat dupa activitate
                                        break; // Oprim bucla pentru ca am gasit meci disponibil!
                                    }
                                }

                                /*BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(bg->GetTypeID(), bracketEntry->GetBracketId(), 2); // 2 = 2v2 | 3 = 3v3 | 5 = 5v5
                                BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);


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
                                }*/
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

                            if (checkGroup && checkGroup->IsLeader(botPlayer->GetGUID()) && checkGroup->GetMembersCount() == tipArenaAles)
                            {
                                tracker.isQueued = true;
                                tracker.rejoinTimer = 0;
                                //JoinGroupArena2v2Rated(botPlayer);

                                // Inscrierea se face strict dupa coada care a declansat activitatea (tipArenaAles)
                                switch (tipArenaAles)
                                {
                                    case 2:
                                    {
                                        JoinGroupArena2v2Rated(botPlayer);
                                        break;
                                    }
                                    case 3:
                                    {
                                        JoinGroupArena3v3Rated(botPlayer);
                                        break;
                                    }
                                    case 5:
                                    {
                                        JoinGroupArena5v5Rated(botPlayer);
                                        break;
                                    }

                                    default:
                                    {
                                        tracker.isQueued = false;
                                        tracker.rejoinTimer = urand(10000, 20000);
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                AjusteazaGrupBotPentruArena(botPlayer, tipArenaAles);

                                tracker.isQueued = false;
                                tracker.rejoinTimer = urand(10000, 20000);
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

    // ajusteaza grup arena
    void AjusteazaGrupBotPentruArena(Player* botPlayer, uint8 tipArenaAles)
    {
        if (!botPlayer || tipArenaAles == 0)
            return;

        uint8 slotIndex = (tipArenaAles == 3) ? 1 : ((tipArenaAles == 5) ? 2 : 0);
        uint32 arenaTeamId = botPlayer->GetArenaTeamId(slotIndex);
        if (arenaTeamId == 0)
            return;

        // Extragem direct tracker-ul echipei din map-ul static
        auto& tArena = g_KittBotArenaRegistru[arenaTeamId];
        tArena.arenaTeamId = arenaTeamId;

        // ==================== PASUL 1: BARIERA DE PROTECTIE (CREARE GRUP) ====================
        // Daca un alt coleg a blocat deja echipa pentru formare in acest frame/tick, ne oprim instant
        if (tArena.inCursDeFormare && tArena.botCareFormeaza != botPlayer->GetGUID())
        {
            return;
        }

        // Daca bracket-ul solicitat este deja marcat ca activ in coada, nu mai incercam sa generam alt grup
        if (tipArenaAles == 2 && tArena.areGrupIn2v2) return;
        if (tipArenaAles == 3 && tArena.areGrupIn3v3) return;
        if (tipArenaAles == 5 && tArena.areGrupIn5v5) return;

        // Daca botul curent face deja parte dintr-un grup, dar el NU este liderul, 
        // inseamna ca este un membru primit primit in vizita. El nu are voie sa asambleze.
        Group* group = botPlayer->GetGroup();
        /*if (group && !group->IsLeader(botPlayer->GetGUID()))
        {
            return;
        }*/

        // Activam bariera: Botul curent isi asuma rolul de constructor al grupului
        if (!tArena.inCursDeFormare)
        {
            tArena.inCursDeFormare = true;
            tArena.botCareFormeaza = botPlayer->GetGUID();
            tArena.timpInceputFormare = GameTime::GetGameTime();
            tArena.botiOcupatiInFormare.clear();
        }

        // Lambda utilitar pentru a anula formarea daca nu gasim conditiile necesare
        auto AbandoneazaFormareaGrupului = [&]() {
            tArena.inCursDeFormare = false;
            tArena.botCareFormeaza = ObjectGuid::Empty;
            tArena.timpInceputFormare = 0;
            tArena.botiOcupatiInFormare.clear();
            };

        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(arenaTeamId);
        if (!at)
        {
            AbandoneazaFormareaGrupului();
            return;
        }

        uint32 membriActuali = group ? group->GetMembersCount() : 1;

        // Daca grupul a atins deja dimensiunea ideala, oprim formarea cu succes
        // (Bariera ramane ridicata o secunda pana cand bucla principala apuca sa ruleze Join)
        if (membriActuali == tipArenaAles)
        {
            return;
        }

        // Daca grupul actual este prea mare, il desfiintam curat pentru a-l reconstrui corect
        if (membriActuali > tipArenaAles)
        {
            if (group)
            {
                group->Disband();
            }
            AbandoneazaFormareaGrupului();
            return;
        }

        // ==================== PASUL 2: RECRUTARE SI MARCARE CA OCUPAT ====================
        uint32 membriNecesariInPlus = tipArenaAles - membriActuali;
        std::vector<Player*> healeriiDisponibili;
        std::vector<Player*> colegiValiziDisponibili;

        bool areHealerInGrup = (DefinesteSiSalveazaRolulBotului(botPlayer) == BOT_ROLE_HEALER);

        // Daca liderul are deja un mini-grup, verificam daca exista deja un healer printre membrii actuali
        if (group && !areHealerInGrup)
        {
            for (auto const& slot : group->GetMemberSlots())
            {
                if (Player* membruExistent = ObjectAccessor::FindConnectedPlayer(slot.guid))
                {
                    if (DefinesteSiSalveazaRolulBotului(membruExistent) == BOT_ROLE_HEALER)
                    {
                        areHealerInGrup = true;
                        break;
                    }
                }
            }
        }

        for (ArenaTeam::MemberList::iterator itr = at->m_membersBegin(); itr != at->m_membersEnd(); ++itr)
        {
            if (itr->Guid == botPlayer->GetGUID())
                continue;

            Player* colegEchipa = ObjectAccessor::FindConnectedPlayer(itr->Guid);
            if (!colegEchipa || !colegEchipa->IsAlive() || colegEchipa->IsLoading())
                continue;

            // Regula stricta: Sa nu fie in pvp activ si sa nu aiba absolut niciun grup activ
            if (colegEchipa->InBattlegroundQueue() || colegEchipa->InBattleground()/* || colegEchipa->GetGroup()*/)
                continue;

            if (Group* grupColeg = colegEchipa->GetGroup())
            {
                // Daca grupul in care se afla colegul nu este chiar grupul liderului nostru curent
                if (!group || grupColeg != group)
                {
                    // Daca el este liderul acelui grup vechi, desfiintam tot grupul
                    if (grupColeg->IsLeader(colegEchipa->GetGUID()))
                    {
                        grupColeg->Disband();
                    }
                    else
                    {
                        // Daca este doar un simplu membru, il scoatem fortat din acel grup
                        grupColeg->RemoveMember(colegEchipa->GetGUID());
                    }
                }
            }

            // IMPARTIRE INTELIGENTA PE ROLURI
            if (DefinesteSiSalveazaRolulBotului(colegEchipa) == BOT_ROLE_HEALER)
            {
                healeriiDisponibili.push_back(colegEchipa);
            }
            else
            {
                colegiValiziDisponibili.push_back(colegEchipa);
            }

            //colegiValiziDisponibili.push_back(colegEchipa);
        }

        size_t totalColegiLiberi = healeriiDisponibili.size() + colegiValiziDisponibili.size();


        // Daca nu avem suficienti parteneri complet liberi pe server, anulam tot fara sa stricam nimic
        if (totalColegiLiberi < membriNecesariInPlus)
        {
            AbandoneazaFormareaGrupului();
            return;
        }

        // AMESTECAM LISTA DE HEALERI pentru diversitate
        if (healeriiDisponibili.size() > 1)
        {
            for (size_t i = healeriiDisponibili.size() - 1; i > 0; --i)
            {
                size_t j = urand(0, i);
                std::swap(healeriiDisponibili[i], healeriiDisponibili[j]);
            }
        }

        // Amestecam partenerii gasiti pentru diversitatea compozitiilor din echipa
        if (colegiValiziDisponibili.size() > 1)
        {
            for (size_t i = colegiValiziDisponibili.size() - 1; i > 0; --i)
            {
                size_t j = urand(0, i);
                std::swap(colegiValiziDisponibili[i], colegiValiziDisponibili[j]);
            }
        }

        // CONSTRUIM LISTA FINALA DE INVITATII (Se pastreaza ordinea: Healerii amestecati au intaietate)
        std::vector<Player*> listaFinalaRecrutare;

        // Daca grupul nu are inca un healer, adaugam intai toti healerii disponibili (gata amestecati)
        if (!areHealerInGrup && !healeriiDisponibili.empty())
        {
            // Adaugam DOAR PRIMUL healer din lista amestecata
            listaFinalaRecrutare.push_back(healeriiDisponibili.front());

            // Inseamna ca am planificat deja un healer, restul locurilor TREBUIE sa fie DPS
            //areHealerInGrup = true;
        }

        // Completam restul listei exclusiv cu DPS-ii amestecati
        listaFinalaRecrutare.insert(listaFinalaRecrutare.end(), colegiValiziDisponibili.begin(), colegiValiziDisponibili.end());

        // PLASA DE SIGURANTA: Daca nu am avut destui DPS online sa umplem echipa, 
        // abia atunci permitem si alti healeri ramasi, ca sa nu anulam meciul degeaba
        if (listaFinalaRecrutare.size() < membriNecesariInPlus && healeriiDisponibili.size() > 1)
        {
            for (size_t i = 1; i < healeriiDisponibili.size(); ++i)
            {
                listaFinalaRecrutare.push_back(healeriiDisponibili[i]);
            }
        }

        // Incepem asamblarea propriu-zisa
        for (Player* coleg : listaFinalaRecrutare)
        {
            if (membriNecesariInPlus == 0)
                break;

            if (!group)
            {
                group = new Group();
                if (!group->Create(botPlayer))
                {
                    delete group;
                    group = nullptr;
                    AbandoneazaFormareaGrupului();
                    break;
                }
                sGroupMgr->AddGroup(group);
            }

            if (group->AddMember(coleg))
            {
                membriNecesariInPlus--;

                // Salvam partenerul recrutat in lista de boti ocupati a trackerului
                tArena.botiOcupatiInFormare.push_back(coleg->GetGUID());

                TC_LOG_INFO("fakPlayer", "TRACKER ARENA: Liderul {} a securizat membrul {} pentru formatul {}v{}.", botPlayer->GetName().c_str(), coleg->GetName().c_str(), tipArenaAles, tipArenaAles);
            }
        }
    }

    // resetare flaguei join arena multi-task

    void ResetareFlaguriFormare(uint32 arenaTeamId, uint8 tipArenaAles)
    {
        if (arenaTeamId == 0)
            return;

        // Gasim direct celula echipei in memorie (Viteza maxima O(1))
        auto& tArena = g_KittBotArenaRegistru[arenaTeamId];

        // Scoaterea tuturor flag-urilor de protectie ale echipei
        tArena.inCursDeFormare = false;
        tArena.botCareFormeaza = ObjectGuid::Empty;
        tArena.timpInceputFormare = 0;

        // Eliberam complet partenerii din rezerva
        tArena.botiOcupatiInFormare.clear();

        TC_LOG_INFO("fakPlayer", "TRACKER ARENA: Resetare flaguri formare pentru echipa {} la categoria {}v{}.", arenaTeamId, tipArenaAles, tipArenaAles);
    }

    void VerificaSiReseteazaFlaguriBlocate()
    {
        static time_t ultimaCuratareGlobala = 0;
        time_t timpServerCurent = GameTime::GetGameTime();

        if (timpServerCurent < ultimaCuratareGlobala + 60)
            return;

        ultimaCuratareGlobala = timpServerCurent;

        for (auto& [teamId, tracker] : g_KittBotArenaRegistru)
        {
            if (teamId == 0)
                continue;

            // 1. VERIFICARE TIMEOUT FORMARE (A ramas blocat la asamblare grup)
            if (tracker.inCursDeFormare && tracker.timpInceputFormare > 0)
            {
                if (timpServerCurent >= tracker.timpInceputFormare + 120)
                {
                    // Daca are grup fizic gata facut pe lider, ii dam disband prin pointerul lui
                    if (Player* botLider = ObjectAccessor::FindConnectedPlayer(tracker.botLiderDeEchipa))
                    {
                        if (Group* gr = botLider->GetGroup())
                            gr->Disband();
                    }

                    // Resetam strict memoria noastra locala
                    tracker.inCursDeFormare = false;
                    tracker.botCareFormeaza = ObjectGuid::Empty;
                    tracker.timpInceputFormare = 0;
                    tracker.botiOcupatiInFormare.clear();

                    TC_LOG_INFO("fakPlayer", "KITT GARBAGE: Timeout formare depasit (120s) pentru echipa {}. Eliberat.", teamId);
                    continue;
                }
            }

            // 2. VERIFICARE TIMEOUT COADA (Asteapta de prea mult timp singur in coada goala)
            bool areOriceCoadaActiva = tracker.areGrupIn2v2 || tracker.areGrupIn3v3 || tracker.areGrupIn5v5;
            if (areOriceCoadaActiva && tracker.timpIntrareInCoada > 0)
            {
                if (timpServerCurent >= tracker.timpIntrareInCoada + 300)
                {
                    // Gasim botul care a dat join (folosim sfeul salvat anterior cat timp era in formare, sau primul online)
                    Player* botLiderCoada = ObjectAccessor::FindConnectedPlayer(tracker.botLiderDeEchipa);

                    if (botLiderCoada)
                    {
                        // Scoatem fortat din toate cozile native ale serverului
                        for (uint8 i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                        {
                            BattlegroundQueueTypeId queueId = botLiderCoada->GetBattlegroundQueueTypeId(i);
                            if (queueId != BATTLEGROUND_QUEUE_NONE)
                            {
                                BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(queueId);
                                bgQueue.RemovePlayer(botLiderCoada->GetGUID(), false);
                            }
                        }

                        // Dizolvam grupul ca sa poata fi amestecati tura urmatoare
                        if (Group* gr = botLiderCoada->GetGroup())
                            gr->Disband();
                    }

                    // Curatam complet toate flag-urile
                    tracker.areGrupIn2v2 = false;
                    tracker.areGrupIn3v3 = false;
                    tracker.areGrupIn5v5 = false;
                    tracker.timpIntrareInCoada = 0;
                    tracker.botCareFormeaza = ObjectGuid::Empty;
                    //tracker.botLiderDeEchipa = ObjectGuid::Empty;

                    TC_LOG_INFO("fakPlayer", "KITT GARBAGE: Timeout coada depasit (300s) pentru echipa {}. Scoatere fortata.", teamId);
                }
            }
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

        std::string raspunsText = "nu ne cunoastem";


        if (msg == "vino" || msg == "port")
        {
            uint32 botMapId = receiver->GetMapId();
            MapEntry const* botMapEntry = sMapStore.LookupEntry(botMapId);

            uint32 targetMapId = player->GetMapId();
            MapEntry const* mapEntry = sMapStore.LookupEntry(targetMapId);

            if (!botMapEntry || botMapEntry->Instanceable() || botMapEntry->IsBattleArena() || botMapEntry->IsBattleground())
            {
                raspunsText = "Nu vreau, busy.";
            }
            else if (!mapEntry || mapEntry->Instanceable() || mapEntry->IsBattleArena() || mapEntry->IsBattleground())
            {
                raspunsText = "Nu vreau acolo";
            }
            else
            {
                raspunsText = "Pornesc spre tine acum!";

                float posX = player->GetPositionX();
                float posY = player->GetPositionY();
                float posZ = player->GetPositionZ();
                float orientation = player->GetOrientation();

                receiver->TeleportTo(targetMapId, posX, posY, posZ, orientation);

                /*if (receiver->GetSession())
                {
                    WorldPacket pachetGol;
                    receiver->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                }*/

                /*if (!receiver->IsInWorld())
                {
                    receiver->AddToWorld();
                }*/
            }
        }

        if (msg == "leave group")
        {
            uint32 botMapId = receiver->GetMapId();
            MapEntry const* mapEntry = sMapStore.LookupEntry(botMapId);

            if (!mapEntry || mapEntry->Instanceable() || mapEntry->IsBattleArena() || mapEntry->IsBattleground())
            {
                raspunsText = "Sunt intr-o instanta acum";
            }
            else
            {
                Group* gr = receiver->GetGroup();
                if (gr)
                {
                    raspunsText = "Ies acum din grupul curent.";
                    gr->RemoveMember(receiver->GetGUID());
                }
                else
                {
                    raspunsText = "Nu sunt in niciun grup.";
                }
            }
        }

        if (msg == "join group")
        {
            Group* group = player->GetGroup();

            if (!group)
            {
                group = new Group();
                if (!group->Create(player))
                {
                    delete group;
                    group = nullptr;
                    return;
                }
                sGroupMgr->AddGroup(group);
            }

            // Adaugam membrul online direct in structura grupului (fara pachete de retea)
            if (!receiver->GetGroup() && group->AddMember(receiver))
            {
                raspunsText = "sigur.";
            }
            else
            {
                raspunsText = "am un grup deja.";
            }
        }

        if (msg == "set home")
        {
            uint32 botMapId = receiver->GetMapId();
            MapEntry const* mapEntry = sMapStore.LookupEntry(botMapId);

            if (!mapEntry || mapEntry->Instanceable() || mapEntry->IsBattleArena() || mapEntry->IsBattleground())
            {
                raspunsText = "Sunt intr-o instanta acum";
            }
            else
            {
                WorldLocation botLocation = receiver->GetWorldLocation();
                uint32 botAreaId = receiver->GetAreaId();
                receiver->SetHomebind(botLocation, botAreaId);

                raspunsText = "Mi-am setat noua casa (Homebind) in aceasta locatie.";
            }
        }

        if (msg == "status")
        {
            raspunsText = "Sunt online si pregatit de arena!";
        }
        /*else
        {
            raspunsText = "Nu inteleg aceasta comanda. Scrie 'vino', 'status'.";
            return;
        }*/

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
            { "addMe",      HandleAddMeInWorld,           rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_5, Console::No },
            { "remMe",      HandleRemoveMeFromWorld,      rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_5, Console::No },
            //{ "list",   HandleListAllGhostAccess,      rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
            //{ "set",    HandleSetGhostAccess,    rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
            //{ "del",    HandleDelGhostAccess,    rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
        };

        static std::vector<ChatCommandBuilder> kittGhostPlayerCommandSubcommandTable =
        {
            { "access",     kittGhostPlayerCommandSubcommandTable1 },
            { "list",       HandleShowGhostList,          rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
            { "add one",    HandleStartGhostInWorld,      rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
            { "add mass",   HandleStartBulkGhostsInWorld, rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
            { "ai on",      HandleAiOnGhostInWorld,       rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
            { "ai off",     HandleAiOffGhostInWorld,      rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
            { "remove",     HandleRemoveGhostFromWorld,   rbac::RBAC_PERM_COMMAND_KITT_GM_RANK_9, Console::No },
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

        if (g_MultiBotTracker.empty())
        {
            handler->SendSysMessage("No ghost bots registered in the system.");
            return true;
        }

        uint32 index = 1;
        for (const auto& tracker : g_MultiBotTracker)
        {
            // 1. Reconstruim ObjectGuid-ul nativ pe baza GUID-ului Low (Counter) salvat in tracker
            ObjectGuid botGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);

            // Cautam in cache numele oficial stocat in baza de date
            std::string charName = "Unknown";
            if (CharacterCacheEntry const* cacheEntry = sCharacterCache->GetCharacterCacheByGuid(botGuid))
            {
                charName = cacheEntry->Name;
            }

            // 2. Determinam starea exacta: ONLINE (daca e in lume) sau LOADING (daca inca se incarca)
            std::string statusText = "|cffff0000LOADING|r";
            if (tracker.isProcessed)
            {
                statusText = "|cff00ff00ONLINE|r";
            }

            // 3. Determinam starea flag-ului AddFromChatCmd (ON cu verde / OFF cu rosu)
            std::string chatCmdText = !tracker.AddFromChatCmd ? "|cff00ff00ON|r" : "|cffff0000OFF|r";

            // 4. Printam linia formatata curat in chat-ul GM-ului
            // Format: 1. [Nume] (AccID: X) | Status: ONLINE | ChatCmd: ON
            handler->PSendSysMessage("%u. |cff00ff00%s|r (AccID: %u) | Status: %s | PvP ai: %s",
                index++, charName.c_str(), tracker.accountId, statusText.c_str(), chatCmdText.c_str());
        }

        handler->SendSysMessage("==========================");
        return true;
    }

    static bool HandleRemoveGhostFromWorld(ChatHandler* handler, Optional<std::string_view> args)
    {
        // 1. Verificam daca s-a introdus numele
        if (!args)
        {
            handler->SendSysMessage("Usage: |cffffffff.zghost remove|r |cff00ff00CharacterName|r");
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
            if (tracker.kickedByPlayer)
            {
                handler->PSendSysMessage("Ghost character |cff00ff00%s|r is online by REAL Player... can't remove.", targetName.c_str());
                break;
            }

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

    static bool HandleAiOnGhostInWorld(ChatHandler* handler, Optional<std::string_view> args)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return true;

        // 1. Verificam daca s-a introdus numele botului
        if (!args)
        {
            handler->SendSysMessage("Usage: |cffffffff.ztfcbot ai on|r |cff00ff00CharacterName|r");
            return true;
        }

        // 2. Normalizam numele primit (Prima litera mare, restul mici)
        std::string targetName(args.value());
        if (!targetName.empty())
        {
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            targetName[0] = std::toupper(targetName[0]);
        }

        // 3. Cautam in Cache-ul global pentru a-i afla GUID-ul Low
        CharacterCacheEntry const* targetCache = sCharacterCache->GetCharacterCacheByName(targetName);
        if (!targetCache)
        {
            handler->PSendSysMessage("Character |cffffffff'%s'|r does not exist in database.", targetName.c_str());
            return true;
        }

        uint32 targetGuidLow = targetCache->Guid.GetCounter();
        bool gasit = false;

        // 4. Parcurgem trackerul pentru a gasi botul si a-i schimba flagul
        for (auto& tracker : g_MultiBotTracker)
        {
            if (tracker.charGuid == targetGuidLow)
            {
                // Schimbam flagul din tracker pe true
                tracker.AddFromChatCmd = false; // Sau tracker.AiActive = true; in functie de cum ai numit variabila
                gasit = true;

                handler->PSendSysMessage("AI Flag set to |cff00ff00ON|r for bot |cff00ff00%s|r.", targetName.c_str());

                // OPTIONAL NATIV: Daca botul e online, ii poti forta reactivarea reactiilor din Core
                if (tracker.isProcessed && tracker.realSession && tracker.realSession->GetPlayer())
                {
                    // Aici poti adauga o linie daca ai o functie nativa de trezire a AI-ului, ex:
                    // tracker.realSession->GetPlayer()->GetAI()->NeedToUpdateProcessor(true);
                }
                break;
            }
        }

        if (!gasit)
        {
            handler->PSendSysMessage("Bot |cffffffff'%s'|r is not active or registered in the tracker.", targetName.c_str());
        }

        return true;
    }

    static bool HandleAiOffGhostInWorld(ChatHandler* handler, Optional<std::string_view> args)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return true;

        // 1. Verificam argumentele
        if (!args)
        {
            handler->SendSysMessage("Usage: |cffffffff.ztfcbot ai off|r |cff00ff00CharacterName|r");
            return true;
        }

        // 2. Normalizam numele
        std::string targetName(args.value());
        if (!targetName.empty())
        {
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            targetName[0] = std::toupper(targetName[0]);
        }

        // 3. Cautam in Cache
        CharacterCacheEntry const* targetCache = sCharacterCache->GetCharacterCacheByName(targetName);
        if (!targetCache)
        {
            handler->PSendSysMessage("Character |cffffffff'%s'|r does not exist.", targetName.c_str());
            return true;
        }

        uint32 targetGuidLow = targetCache->Guid.GetCounter();
        bool gasit = false;

        // 4. Parcurgem trackerul pentru oprire flag
        for (auto& tracker : g_MultiBotTracker)
        {
            if (tracker.charGuid == targetGuidLow)
            {
                // Schimbam flagul din tracker pe false
                tracker.AddFromChatCmd = true; // Sau tracker.AiActive = false;
                gasit = true;

                handler->PSendSysMessage("AI Flag set to |cffff0000OFF|r for bot |cffff0000%s|r.", targetName.c_str());

                // OPTIONAL NATIV: Daca vrei ca botul sa inghete pe loc cand ii dai AI OFF
                if (tracker.isProcessed && tracker.realSession && tracker.realSession->GetPlayer())
                {
                    tracker.realSession->GetPlayer()->StopMoving();
                }
                break;
            }
        }

        if (!gasit)
        {
            handler->PSendSysMessage("Bot |cffffffff'%s'|r is not registered in the tracker.", targetName.c_str());
        }

        return true;
    }

    static bool HandleAddMeInWorld(ChatHandler* handler, Optional<std::string_view> args)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        if (args)
        {
            handler->SendSysMessage("|cff00ff00Ghost|r Foloseste doar comanda fara alte caractere.");
            return true;
        }

        uint32 accountId = me->GetSession()->GetAccountId();
        uint32 charGuid = me->GetGUID().GetCounter();

        // 1. Verificam sub mutex daca esti deja monitorizat (ca sa nu apara dubluri in g_MultiBotTracker)
        {
            std::lock_guard<std::mutex> lock(g_BotTrackerMutex);
            for (auto const& tracker : g_MultiBotTracker)
            {
                if (tracker.accountId == accountId)
                {
                    handler->PSendSysMessage("|cff00ff00Ghost|r Contul tau este deja inregistrat in tracker!");
                    return true;
                }
            }
        }

        // 2. Initializam trackerul cu flag-urile specifice unui jucator real online
        BotAsyncTracker tracker;
        tracker.accountId = accountId;
        tracker.charGuid = charGuid;
        tracker.kickedByPlayer = true;          // Flag pe true: omul are prioritate acum
        tracker.AccRealBusy = true;             // Blocheaza logica de relog din a rula aiurea acum
        tracker.AddFromChatCmd = true;          // Marcam ca a pornit din comanda chat

        // 3. Inseram tracker-ul in siguranta in vectorul global
        {
            std::lock_guard<std::mutex> lock(g_BotTrackerMutex);
            g_MultiBotTracker.push_back(std::move(tracker));
        }

        handler->PSendSysMessage("|cff00ff00Ghost Succes!|r Te-ai adaugat in tracker. Cand vei iesi de pe cont, AI-ul va prelua controlul.");
        return true;
    }

    static bool HandleRemoveMeFromWorld(ChatHandler* handler, Optional<std::string_view> args)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me)
            return false;

        if (args)
        {
            handler->SendSysMessage("|cff00ff00Ghost|r Foloseste doar comanda fara alte caractere.");
            return true;
        }

        uint32 accountId = me->GetSession()->GetAccountId();

        {
            for (auto& tracker : g_MultiBotTracker)
            {
                if (tracker.accountId == accountId)
                {
                    tracker.RemoveFromWorld = true;

                    handler->PSendSysMessage("|cff00ff00Ghost Succes!|r Te-ai sters din tracker.");
                    return true;
                }
            }
        }

        handler->PSendSysMessage("|cff00ff00Ghost|r Contul tau nu este inregistrat in tracker!");
        return true;
    }



};



class kitt_ghost_ack_packet : public ServerScript
{
public:
    kitt_ghost_ack_packet() : ServerScript("kitt_ghost_ack_packet") {}

    void LogheazaOpcodeLipsa(uint32 opcodeId)
    {
        // Deschidem fisierul in modul 'append' (adauga la final, nu sterge ce este deja)
        std::ofstream logFile("zboti_opcodes_lipsa.log", std::ios::app);
        if (!logFile.is_open())
            return;

        std::string opcodeName = "UNKNOWN_OPCODE";

        // REPARATIE NATIVA DEFINITIVA:
        // Apelam functia oficiala gasita in Core-ul tau, facand cast explicit la tipul enum Opcodes
        opcodeName = GetOpcodeNameForLogging(static_cast<Opcodes>(opcodeId));

        // Daca numele returnat este gol, punem o siguranta sa stim ca e un opcode in afara listei
        if (opcodeName.empty())
        {
            opcodeName = "UNKNOWN_OR_CUSTOM_OPCODE";
        }

        // Formatam timpul curent simplu pentru log
        time_t acum = time(nullptr);
        struct tm* tstruct = localtime(&acum);
        char timpBuffer[80];
        strftime(timpBuffer, sizeof(timpBuffer), "%Y-%m-%d %H:%M:%S", tstruct);

        // Scriem in fisier: Data/Ora | ID Hexazecimal | Numele oficial din Core
        logFile << "[" << timpBuffer << "] Opcode lipsa definitie -> ID: 0x"
            << std::hex << std::uppercase << std::setw(3) << std::setfill('0') << opcodeId
            << " | Nume: " << opcodeName << std::endl;

        logFile.close();
    }

    void LogheazaOpcodeCePrimesteBot(WorldSession* session, uint32 opcodeId)
    {
        if (!session)
            return;

        // Deschidem fisierul in modul 'append'
        std::ofstream logFile("zboti_opcodes_tot_ce_primeste.log", std::ios::app);
        if (!logFile.is_open())
            return;

        std::string opcodeName = "UNKNOWN_OPCODE";

        // Preluam numele oficial din Core
        opcodeName = GetOpcodeNameForLogging(static_cast<Opcodes>(opcodeId));
        if (opcodeName.empty())
        {
            opcodeName = "UNKNOWN_OR_CUSTOM_OPCODE";
        }

        // Preluam Account ID-ul sesiunii (este disponibil inclusiv in faza de Login/Incarcare)
        uint32 accId = session->GetAccountId();

        // Verificam daca botul are deja un caracter creat/incarcat in lume pentru a-i scrie si numele
        std::string charName = "Ecran_Caractere";
        if (Player* botPlayer = session->GetPlayer())
        {
            charName = botPlayer->GetName();
        }

        // Formatam timpul curent simplu pentru log
        time_t acum = time(nullptr);
        struct tm* tstruct = localtime(&acum);
        char timpBuffer[80];
        strftime(timpBuffer, sizeof(timpBuffer), "%Y-%m-%d %H:%M:%S", tstruct);

        // Scriem in fisier: Data/Ora | AccID | NumeChar | ID Hex | Nume Opcode
        logFile << "[" << timpBuffer << "] [AccID: " << std::dec << accId
            << " | Char: " << charName << "] -> Opcode: 0x"
            << std::hex << std::uppercase << std::setw(3) << std::setfill('0') << opcodeId
            << " | Nume: " << opcodeName << std::endl;

        logFile.close();
    }


    void InjecteazaOpcodeBot(WorldSession* session, uint32 opcodeId, WorldPacket& data)
    {
        if (!session || !session->IsKittBot())
            return;

        WorldPacket* packet = new WorldPacket(opcodeId, data.size());
        if (!data.empty())
        {
            packet->append(data.contents(), data.size());
        }

        //session->_recvQueue.add(packet);
        //session->QueuePacket(std::move(packet));
        session->QueuePacket(packet);
    }

    bool EstePachetulDestinatBotului(WorldSession* session, WorldPacket& pachetPrimitDeLaServer)
    {
        if (!session || !session->IsKittBot())
            return false;

        Player* botPlayer = session->GetPlayer();
        if (!botPlayer)
            return false;

        // Clonam pachetul pe stiva pentru a-i citi structura fara sa alteram bufferul original
        WorldPacket cititor(pachetPrimitDeLaServer);
        uint64 pachetRawGUID = 0;

        try
        {
            // Apelam functia nativa din ByteBuffer pentru a extrage GUID-ul packed
            cititor.readPackGUID(pachetRawGUID);
        }
        catch (...)
        {
            // Daca pachetul e gol sau nu incepe cu un GUID packed, returnam false din siguranta
            return false;
        }

        // Comparam GUID-ul din pachet cu cel al botului curent
        return (pachetRawGUID == botPlayer->GetGUID().GetRawValue());
    }




    // 1. CAPTURA PACHETE TRIMISE DE SERVER CATRE BOT (SMSG)
    void OnPacketSend(WorldSession* session, WorldPacket& packet) override
    {
        if (!session)
            return;

        if (!session->IsKittBot())
            return;

        //std::string const& accName = session->GetAccountName();

        uint16 opcode = packet.GetOpcode();

        //LogheazaOpcodeCePrimesteBot(session, opcode);


        // Organizam totul sub forma de switch (opcode) conform protocolului nativ
        switch (opcode)
        {
        case SMSG_TRANSFER_PENDING:            // 0x003F
        {
            // Clientul raspunde instant la semnalul de pending cu un prim WORLDPORT_ACK gol
            WorldPacket response(MSG_MOVE_WORLDPORT_ACK, 0);

            // Folosim functia ta curata. Filtrul va lasa pachetul sa treaca automat (exceptie de sistem)
            InjecteazaOpcodeBot(session, MSG_MOVE_WORLDPORT_ACK, response);
            break;
        }

        case SMSG_NEW_WORLD:
        {
            WorldPacket response(MSG_MOVE_WORLDPORT_ACK);
            InjecteazaOpcodeBot(session, MSG_MOVE_WORLDPORT_ACK, response);
            break;
            // Setam o dimensiune minima de 4 sau 8 bytes pentru siguranta bufferului
            /*WorldPacket response(MSG_MOVE_WORLDPORT_ACK, 0);
            InjecteazaOpcodeBot(session, MSG_MOVE_WORLDPORT_ACK, response);

            // OPTIMIZARE NATIVA CONFORM SNIFFER:
            // Imediat dupa al doilea ACK, jocul trimite intotdeauna CMSG_SET_ACTIVE_MOVER (0x026A)
            // pentru a anunta serverul ca entitatea player este cea care controleaza miscarea acum!
            Player* botPlayer = session->GetPlayer();
            if (botPlayer)
            {
                WorldPacket activeMover(CMSG_SET_ACTIVE_MOVER, 8);
                activeMover << botPlayer->GetGUID(); // Trimitem GUID-ul intreg (uint64) cerut de protocol
                InjecteazaOpcodeBot(session, CMSG_SET_ACTIVE_MOVER, activeMover);
            }*/
            break;
        }

        //case MSG_MOVE_TELEPORT:
        case MSG_MOVE_TELEPORT_ACK:
        {
            if (!EstePachetulDestinatBotului(session, packet))
            {
                break;
            }

            // SIGURANTA ANTI-BUCLA: Daca pachetul este gol sau generat de noi, dam break
            if (packet.empty() || packet.size() < 20)
            {
                break;
            }

            Player* botPlayer = session->GetPlayer();
            if (!botPlayer)
            {
                break;
            }

            // 1. SCENARIUL A: TELEPORTARE PE HARTA NOUA (Schimbare fizica de continent Map 0 -> Map 1)
            if (!botPlayer->IsInWorld())
            {
                // Clonam si trimitem ACK-ul binar cerut de server pentru schimbarea hartiilor
                WorldPacket response(MSG_MOVE_TELEPORT_ACK, packet.size());
                if (packet.size() > 0)
                {
                    response.append(packet.contents(), packet.size());
                }
                InjecteazaOpcodeBot(session, MSG_MOVE_TELEPORT_ACK, response);

                // Adaugam si heartbeat-ul de siguranta pe harta noua
                uint32 moveTime = GameTime::GetGameTimeMS();
                WorldPacket heartbeat(MSG_MOVE_HEARTBEAT, 32);
                heartbeat << botPlayer->GetGUID().WriteAsPacked();
                heartbeat << uint32(0); // Flags
                heartbeat << uint16(0); // Flags extra
                heartbeat << moveTime;
                heartbeat << botPlayer->GetPositionX() << botPlayer->GetPositionY() << botPlayer->GetPositionZ() << botPlayer->GetOrientation();
                heartbeat << uint32(0); // Fall time

                InjecteazaOpcodeBot(session, MSG_MOVE_HEARTBEAT, heartbeat);

                TC_LOG_INFO("fakPlayer", "[BotNetwork] -> Teleportare HARTA NOUA confirmata pentru {}.", botPlayer->GetName().c_str());
            }
            else
            {
                // 2. SCENARIUL B: TELEPORTARE PE ACEEASI HARTA (.summon scurt sau aceeasi locatie)
                // REPARATIE FINALA: Fortam stingerea semaforului de miscare scurta direct pe obiectul Player.
                // Aceasta linie face exact ce ar fi facut HandleMoveTeleportAck daca pachetul trecea de socket!
                botPlayer->SetSemaphoreTeleportNear(false);

                // Procesam operatiunile amanate si oprim eventualele miscari vechi
                botPlayer->ProcessDelayedOperations();
                botPlayer->StopMoving();

                TC_LOG_INFO("fakPlayer", "[BotNetwork] -> Teleportare ACEEASI HARTA deblocata cu succes in memorie pentru {}.", botPlayer->GetName().c_str());
            }
            break;
        }

        /*
        case SMSG_FORCE_MOVE_ROOT:
        {
            if (!EstePachetulDestinatBotului(session, packet))
            {
                break;
            }

            Player* botPlayer = session->GetPlayer();
            if (!botPlayer)
                break;

            // Aloc?m spa?iu suficient pentru structura de mi?care cerut? de core
            WorldPacket response(CMSG_FORCE_MOVE_ROOT_ACK, 64);

            // 1. TrinityCore cere obligatoriu GUID-ul packed al entit??ii la ?nceputul pachetului
            response << botPlayer->GetGUID().WriteAsPacked();

            // 2. Cere un contor de mi?care (movement counter / ack count)
            response << uint32(0);

            // 3. Cere flag-urile de mi?care ?i pozi?ia actual?
            response << uint32(0); // MoveFlags
            response << uint16(0); // ExtraMoveFlags
            response << uint32(GameTime::GetGameTimeMS()); // Time
            response << botPlayer->GetPositionX();
            response << botPlayer->GetPositionY();
            response << botPlayer->GetPositionZ();
            response << botPlayer->GetOrientation();
            response << uint32(0); // FallTime

            // Trimitem pachetul complet structurat ?napoi la server
            InjecteazaOpcodeBot(session, CMSG_FORCE_MOVE_ROOT_ACK, response);
            break;
        }
        */

        // 3. REPARATIE PENTRU DEBLOCARE INSTANTA / STRUCTURA DE MAP?
        case SMSG_INSTANCE_DIFFICULTY:
        {
            // In loc de break simplu, cand se schimba dificultatea la iesirea din Arena
            // Procesam instant datele amanate ca sa nu ramana harta agatata in memorie
            Player* botPlayer = session->GetPlayer();
            if (botPlayer)
            {
                botPlayer->ProcessDelayedOperations();
            }
            break;
        }

        case SMSG_TRADE_STATUS: // 0x0120
        {
            TC_LOG_INFO("fakPlayer", "[BotNetwork] -> Cineva a incercat trade cu botul. Se forteaza CMSG_IGNORE_TRADE.");

            WorldPacket response(CMSG_IGNORE_TRADE);
            InjecteazaOpcodeBot(session, CMSG_IGNORE_TRADE, response);
            break;
        }

        case SMSG_GROUP_INVITE:
        {
            TC_LOG_INFO("fakPlayer", "[BotNetwork] -> Botul a primit o invitatie de grup. Se trimite acceptul securizat cu corp de 4 octeti.");

            // Construeam pachetul de raspuns cu spatiu alocat pentru un uint32
            WorldPacket response(CMSG_GROUP_DECLINE, 4); // CMSG_GROUP_DECLINE | CMSG_GROUP_ACCEPT

            // REPARATIE CRASH: Injectam valoarea zero ceruta de handler-ul din Core-ul tau
            response << uint32(0);

            // Injectam pachetul securizat in coada botului
            InjecteazaOpcodeBot(session, CMSG_GROUP_DECLINE, response);
            break;
        }

        case SMSG_CORPSE_RECLAIM_DELAY:        // 0x0269 - Serverul trimite pop-up-ul de Release Spirit
        {
            Player* botPlayer = session->GetPlayer();
            if (!botPlayer)
                break;

            if (botPlayer->IsBeingTeleported() || botPlayer->InBattleground() || botPlayer->InArena())
            {
                TC_LOG_INFO("fakPlayer", "[BotPvP] -> Botul {} a murit in instanta PvP. Ramane spectator pe sol.", botPlayer->GetName().c_str());
                break;
            }

            TC_LOG_INFO("fakPlayer", "[BotNetwork] -> Botul a murit. Interceptat SMSG_CORPSE_RECLAIM_DELAY. Se apasa automat butonul Release Spirit prin CMSG_REPOP_REQUEST.");

            WorldPacket response(CMSG_REPOP_REQUEST, 1);
            response << uint8(0);
            InjecteazaOpcodeBot(session, CMSG_REPOP_REQUEST, response);
            break;
        }

        case SMSG_DEATH_RELEASE_LOC:           // 0x0378
        {
            Player* botPlayer = session->GetPlayer();
            if (!botPlayer)
                break;

            botPlayer->SetSemaphoreTeleportNear(false);
            botPlayer->ProcessDelayedOperations();
            botPlayer->StopMoving();

            break;
        }

        case SMSG_PRE_RESURRECT:               // 0x0494 - Trimis de Core o singura data la cimitir
        {
            Player* botPlayer = session->GetPlayer();
            if (!botPlayer || botPlayer->IsAlive())
                break;

            if (botPlayer->InBattleground() || botPlayer->InArena())
                break;

            WorldSession* currentSession = session;

            TC_LOG_INFO("fakPlayer", "[BotNetwork] -> Interceptat SMSG_PRE_RESURRECT pentru {}. Se programeaza Auto-Revive peste 2 secunde.", botPlayer->GetName().c_str());

            // Construeam evenimentul asincron nativ pentru a astepta asezarea pe sol a fantomei
            class BotDelayedGraveyardReviveEvent : public BasicEvent
            {
            public:
                BotDelayedGraveyardReviveEvent(WorldSession* _session) : session(_session) {}

                bool Execute(uint64, uint32) override
                {
                    if (session && session->IsKittBot() && session->GetPlayer())
                    {
                        Player* bPlayer = session->GetPlayer();

                        // Daca a fost deja inviat intre timp, oprim
                        if (bPlayer->IsAlive())
                            return true;

                        TC_LOG_INFO("fakPlayer", "[BotNetwork] -> Timer-ul de cimitir s-a scurs pentru {}. Se executa ResurrectPlayer nativ.", bPlayer->GetName().c_str());

                        // Inviem nativ botul direct din cod (100% viata/mana, false = fara sickness pentru viteza)
                        bPlayer->ResurrectPlayer(1.0f, false);

                        // Lasam oasele pe sol si oprim fizica de fantoma
                        bPlayer->SpawnCorpseBones();
                        bPlayer->StopMoving();
                    }
                    return true;
                }

            private:
                WorldSession* session;
            };

            // Adaugam evenimentul cu o intarziere de 1500ms (1.5 secunde) pentru stabilitate totala
            botPlayer->m_Events.AddEvent(new BotDelayedGraveyardReviveEvent(currentSession), botPlayer->m_Events.CalculateTime(2s));
            break;
        }

        case SMSG_LOGIN_VERIFY_WORLD:
        {
            WorldPacket response(CMSG_PLAYER_LOGIN);
            InjecteazaOpcodeBot(session, CMSG_PLAYER_LOGIN, response);
            break;
        }

        case SMSG_TIME_SYNC_REQ:               // 0x0390 - Serverul cere verificarea laten?ei
        {
            WorldPacket p(packet);
            uint32 counter = 0;

            try
            {
                if (p.size() >= sizeof(uint32))
                {
                    p >> counter;
                }
            }
            catch (...) { break; }

            static std::map<uint32, uint32> contoareInAsteptare;
            uint32 accId = session->GetAccountId();

            if (contoareInAsteptare[accId] == counter)
            {
                break; // Daca acest contor este deja programat sa raspunda in viitor, ignoram duplicatul!
            }

            contoareInAsteptare[accId] = counter;
            WorldSession* currentSession = session;

            class BotDelayedTimeSyncEvent : public BasicEvent
            {
            public:
                BotDelayedTimeSyncEvent(WorldSession* _session, uint32 _counter)
                    : session(_session), counter(_counter) {
                }

                bool Execute(uint64, uint32) override
                {
                    if (session && session->IsKittBot() && session->GetPlayer())
                    {
                        uint32 timpActualizat = GameTime::GetGameTimeMS();

                        // Alocam pachetul de tip pointer brut pe heap conform regulilor Core-ului tau
                        WorldPacket* response = new WorldPacket(CMSG_TIME_SYNC_RESP, 4 + 4);
                        *response << uint32(counter);        // Returnam contorul original
                        *response << uint32(timpActualizat); // Trimitem timpul real de acum, nu cel de acum o secunda!

                        session->QueuePacket(response);
                    }
                    return true;
                }

            private:
                WorldSession* session;
                uint32 counter;
            };

            if (Player* botPlayer = session->GetPlayer())
            {
                botPlayer->m_Events.AddEvent(new BotDelayedTimeSyncEvent(currentSession, counter), botPlayer->m_Events.CalculateTime(500ms));
            }

            break;
        }


        /*
        case SMSG_MOVE_LAND_WALK:              // 0x00DF - Serverul for?eaz? mersul pe sol
        {
            if (!EstePachetulDestinatBotului(session, packet))
                break;

            Player* botPlayer = session->GetPlayer();
            if (!botPlayer)
                break;

            // 1. Calcul?m ?n?l?imea real? a solului sub picioarele lui
            float realGroundZ = botPlayer->GetPositionZ();
            if (Map* botMap = botPlayer->GetMap())
            {
                realGroundZ = botMap->GetHeight(botPlayer->GetPositionX(), botPlayer->GetPositionY(), botPlayer->GetPositionZ());
                realGroundZ += botPlayer->GetHoverOffset();
            }

            // 2. Construim evenimentul asincron pentru a am?na aterizarea p?n? se golesc vitezele
            class BotDelayedLandingEvent : public BasicEvent
            {
            public:
                BotDelayedLandingEvent(WorldSession* _session, float _z)
                    : session(_session), groundZ(_z) {
                }

                bool Execute(uint64, uint32) override
                {
                    if (session && session->IsKittBot() && session->GetPlayer())
                    {
                        Player* bPlayer = session->GetPlayer();

                        WorldPacket response(MSG_MOVE_FALL_LAND, 64);
                        response << bPlayer->GetGUID().WriteAsPacked();
                        response << uint32(0); // Coada e deja goala, trimitem 0!
                        response << uint16(0);
                        response << uint32(GameTime::GetGameTimeMS());
                        response << float(bPlayer->GetPositionX());
                        response << float(bPlayer->GetPositionY());
                        response << float(groundZ); // Trimitem ?n?l?imea solului calculated anterior
                        response << float(bPlayer->GetOrientation());
                        response << uint32(0); // Fall time = 0

                        // Aloc?m pe heap strict la executie conform regulilor tale din World.cpp
                        WorldPacket* packetToQueue = new WorldPacket(MSG_MOVE_FALL_LAND, response.size());
                        packetToQueue->append(response.contents(), response.size());
                        session->QueuePacket(packetToQueue);
                    }
                    return true;
                }
            private:
                WorldSession* session;
                float groundZ; // P?streaz? valoarea primitiv? float pe stiv? (100% safe, zero leak)
            };

            // O program?m cu 600ms (cu 100ms DUP? ce s-au executat ACK-urile de vitez? de la 500ms!)
            botPlayer->m_Events.AddEvent(new BotDelayedLandingEvent(session, realGroundZ), botPlayer->m_Events.CalculateTime(600ms));
            break;
        }

        case SMSG_FORCE_RUN_SPEED_CHANGE:      // 0x00E2
        {
            if (!EstePachetulDestinatBotului(session, packet))
                break;

            Player* botPlayer = session->GetPlayer();
            if (!botPlayer)
                break;

            WorldPacket cititor(packet);
            cititor.rpos(0);
            uint64 rawGuid = 0;
            uint32 serverMovementCounter = 0;
            uint8 unkByte = 0;
            float serverVitezaTrimisa = 7.0f;

            try
            {
                cititor.readPackGUID(rawGuid);
                cititor >> serverMovementCounter;
                cititor >> unkByte;
                cititor >> serverVitezaTrimisa;
            }
            catch (...) { break; }

            // Eveniment asincron complet izolat - COPIAZA DOAR VALORI PRIMITIVE (100% Scurgere-Safe!)
            class BotDelayedRunSpeedAckEvent : public BasicEvent
            {
            public:
                BotDelayedRunSpeedAckEvent(WorldSession* _session, uint32 _counter, float _speed)
                    : session(_session), counter(_counter), speed(_speed) {
                }

                bool Execute(uint64, uint32) override
                {
                    if (session && session->IsKittBot() && session->GetPlayer())
                    {
                        Player* bPlayer = session->GetPlayer();
                        WorldPacket response(0x00E3, 45);
                        response << bPlayer->GetGUID().WriteAsPacked();
                        response << uint32(counter); // Trimitem counterul citit acum 500ms
                        response << uint32(0); response << uint16(0); response << uint32(GameTime::GetGameTimeMS());
                        response << float(bPlayer->GetPositionX()) << float(bPlayer->GetPositionY()) << float(bPlayer->GetPositionZ()) << float(bPlayer->GetOrientation());
                        response << uint32(0);
                        response << float(speed); // Trimitem viteza citita acum 500ms

                        // ALOCARE PE HEAP STRICT LA EXECUTIE - Va fi stearsa garantat in aceeasi milisecunda!
                        WorldPacket* packetToQueue = new WorldPacket(0x00E3, response.size());
                        packetToQueue->append(response.contents(), response.size());
                        session->QueuePacket(packetToQueue);
                    }
                    return true;
                }
            private:
                WorldSession* session;
                uint32 counter;
                float speed;
            };

            botPlayer->m_Events.AddEvent(new BotDelayedRunSpeedAckEvent(session, serverMovementCounter, serverVitezaTrimisa), botPlayer->m_Events.CalculateTime(500ms));
            break;
        }

        case SMSG_FORCE_SWIM_SPEED_CHANGE:     // 0x00E6
        {
            if (!EstePachetulDestinatBotului(session, packet))
                break;

            Player* botPlayer = session->GetPlayer();
            if (!botPlayer)
                break;

            WorldPacket cititor(packet);
            cititor.rpos(0);
            uint64 rawGuid = 0;
            uint32 serverMovementCounter = 0;
            float serverVitezaSwim = 4.72f;

            try
            {
                cititor.readPackGUID(rawGuid);
                cititor >> serverMovementCounter;
                cititor >> serverVitezaSwim;
            }
            catch (...) { break; }

            class BotDelayedSwimSpeedAckEvent : public BasicEvent
            {
            public:
                BotDelayedSwimSpeedAckEvent(WorldSession* _session, uint32 _counter, float _speed)
                    : session(_session), counter(_counter), speed(_speed) {
                }

                bool Execute(uint64, uint32) override
                {
                    if (session && session->IsKittBot() && session->GetPlayer())
                    {
                        Player* bPlayer = session->GetPlayer();
                        WorldPacket response(0x00E7, 45);
                        response << bPlayer->GetGUID().WriteAsPacked();
                        response << uint32(counter);
                        response << uint32(0); response << uint16(0); response << uint32(GameTime::GetGameTimeMS());
                        response << float(bPlayer->GetPositionX()) << float(bPlayer->GetPositionY()) << float(bPlayer->GetPositionZ()) << float(bPlayer->GetOrientation());
                        response << uint32(0);
                        response << float(speed);

                        WorldPacket* packetToQueue = new WorldPacket(0x00E7, response.size());
                        packetToQueue->append(response.contents(), response.size());
                        session->QueuePacket(packetToQueue);
                    }
                    return true;
                }
            private:
                WorldSession* session;
                uint32 counter;
                float speed;
            };

            botPlayer->m_Events.AddEvent(new BotDelayedSwimSpeedAckEvent(session, serverMovementCounter, serverVitezaSwim), botPlayer->m_Events.CalculateTime(500ms));
            break;
        }

        case SMSG_FORCE_MOVE_UNROOT:           // 0x00EA
        {
            if (!EstePachetulDestinatBotului(session, packet))
                break;

            Player* botPlayer = session->GetPlayer();
            if (!botPlayer)
                break;

            WorldPacket cititor(packet);
            cititor.rpos(0);
            uint64 rawGuid = 0;
            uint32 serverMovementCounter = 0;

            try
            {
                cititor.readPackGUID(rawGuid);
                cititor >> serverMovementCounter;
            }
            catch (...) { break; }

            class BotDelayedUnrootAckEvent : public BasicEvent
            {
            public:
                BotDelayedUnrootAckEvent(WorldSession* _session, uint32 _counter)
                    : session(_session), counter(_counter) {
                }

                bool Execute(uint64, uint32) override
                {
                    if (session && session->IsKittBot() && session->GetPlayer())
                    {
                        Player* bPlayer = session->GetPlayer();
                        WorldPacket response(0x00EB, 41);
                        response << bPlayer->GetGUID().WriteAsPacked();
                        response << uint32(counter);
                        response << uint32(0); response << uint16(0); response << uint32(GameTime::GetGameTimeMS());
                        response << float(bPlayer->GetPositionX()) << float(bPlayer->GetPositionY()) << float(bPlayer->GetPositionZ()) << float(bPlayer->GetOrientation());
                        response << uint32(0);

                        WorldPacket* packetToQueue = new WorldPacket(0x00EB, response.size());
                        packetToQueue->append(response.contents(), response.size());
                        session->QueuePacket(packetToQueue);
                    }
                    return true;
                }
            private:
                WorldSession* session;
                uint32 counter;
            };

            botPlayer->m_Events.AddEvent(new BotDelayedUnrootAckEvent(session, serverMovementCounter), botPlayer->m_Events.CalculateTime(500ms));
            break;
        }
        */



        // =========================================================================
        // 1. LE IGNORAM SILENTION (DEFINITII NATIVE DIN OPCODES.H)
        // =========================================================================
        case MSG_MOVE_TELEPORT:
        case SMSG_RESURRECT_REQUEST:           // 0x015B
        case SMSG_AUTH_RESPONSE:               // 0x1EE - Confirmarea sesiunii in Core
        case SMSG_ADDON_INFO:                  // 0x2EF - Trimiterea listei de addon-uri active
        case SMSG_TUTORIAL_FLAGS:              // 0x0FD - Masca de interfata si tutoriale
        case SMSG_EQUIPMENT_SET_LIST:          // 0x4C0 - Seturile de iteme din inventar salvate
        case MSG_SET_DUNGEON_DIFFICULTY:       // 0x329 - Sincronizarea dificultatii (Normal/Heroic)
        case SMSG_GROUP_LIST:                  // 0x07D - Membrii grupului (daca botul era in party)
        case SMSG_UPDATE_OBJECT:               // 0x0A9 - IMPORTANT: Botul primeste datele 3D din Grid!
        case SMSG_SET_PROFICIENCY:             // 0x127 - Sincronizarea competentelor de arme/armuri
        case SMSG_SET_FLAT_SPELL_MODIFIER:     // 0x266 - Modificatori de spell-uri valorici (talente)
        case SMSG_SET_PCT_SPELL_MODIFIER:      // 0x267 - Modificatori de spell-uri procentuali (talente)
        case SMSG_POWER_UPDATE:                // 0x480 - Resursele botului (Mana, Rage, Energy)
        case SMSG_CLIENTCACHE_VERSION:         // 0x04AB - Verificarea versiunii de cache client
        case SMSG_TALENTS_INFO:                // 0x04C0 - Informatii despre talentele botului
        case SMSG_COMPRESSED_UPDATE_OBJECT:    // 0x01F6 - Update-uri de miscare in masa de la ceilalti jucatori
        case SMSG_AURA_UPDATE_ALL:             // 0x0495 - Incarcarea tuturor buff-urilor initiale
        case SMSG_AURA_UPDATE:                 // 0x0496 - Update periodic de aury/buff-uri
        case SMSG_MESSAGECHAT:                 // 0x0096 - Mesaje de chat receptionate din jur
        case SMSG_MONSTER_MOVE:                // 0x00DD - Miscarea NPC-urilor sau monstrilor din apropiere
        case SMSG_CRITERIA_UPDATE:             // 0x046A - Update de statistici/realizari (Achievements)
        case SMSG_SPELL_GO:                    // 0x0132 - Lansarea fizica a unei vraji in apropierea botului
        case SMSG_PARTY_MEMBER_STATS:          // 0x007E - Update de viata/mana pentru membrii din party
        case SMSG_DESTROY_OBJECT:              // 0x00AA - Disparitia unui obiect/NPC din raza vizuala
        case MSG_MOVE_FALL_LAND:               // 0x00C9 - Confirmarea aterizarii la sol dupa cadere
        case SMSG_SPELL_START:                 // 0x0131 - Inceputul unui cast de spell in jur
        case SMSG_SPELLENERGIZELOG:            // 0x0151 - Log de incarcare energie/mana de la spell-uri
        case SMSG_PERIODICAURALOG:             // 0x024E - Tick-urile periodice de buff-uri/debuff-uri
        case SMSG_CANCEL_COMBAT:               // 0x014E
        case SMSG_UPDATE_INSTANCE_OWNERSHIP:   // 0x032B
        case SMSG_CONTACT_LIST:                // 0x0067
        case SMSG_BIND_POINT_UPDATE:           // 0x0155
        case SMSG_INITIAL_SPELLS:              // 0x012A
        case SMSG_SEND_UNLEARN_SPELLS:         // 0x041E
        case SMSG_ACTION_BUTTONS:              // 0x0129
        case SMSG_INITIALIZE_FACTIONS:         // 0x0122
        case SMSG_ALL_ACHIEVEMENT_DATA:        // 0x047D
        case SMSG_SET_FORCED_REACTIONS:        // 0x02A5
        case SMSG_CHANNEL_NOTIFY:              // 0x0099
        case SMSG_INIT_WORLD_STATES:           // 0x02C2
        case SMSG_UPDATE_WORLD_STATE:          // 0x02C3
        case SMSG_QUESTGIVER_STATUS_MULTIPLE:  // 0x0418
        case SMSG_USERLIST_ADD:                // 0x03F0
        case SMSG_USERLIST_REMOVE:             // 0x03F1
        case MSG_MOVE_START_BACKWARD:          // 0x00B6
        case MSG_MOVE_STOP:                    // 0x00B7
        case MSG_MOVE_START_TURN_LEFT:         // 0x00BC
        case MSG_MOVE_STOP_TURN:               // 0x00BE
        case MSG_MOVE_HEARTBEAT:               // 0x00EE
        case MSG_MOVE_SET_FACING:              // 0x00DA
        case SMSG_WEATHER:                     // 0x02F4 - Sincronizarea ploii/vremei din Stormwind/lume
        case MSG_MOVE_START_FORWARD:           // 0x00B5 - Cineva incepe sa mearga inainte
        case MSG_MOVE_START_STRAFE_LEFT:       // 0x00B8 - Inceput mers lateral stanga
        case MSG_MOVE_START_STRAFE_RIGHT:      // 0x00B9 - Inceput mers lateral dreapta
        case MSG_MOVE_STOP_STRAFE:             // 0x00BA - Oprire mers lateral
        case MSG_MOVE_START_TURN_RIGHT:        // 0x00BD - Inceput rotire dreapta
        case SMSG_BATTLEFIELD_STATUS:          // 0x02D4 - Starea cozii si a meciului de BG
        case SMSG_GROUP_JOINED_BATTLEGROUND:   // 0x02E8 - Anunt ca grupul a intrat in coada
        case SMSG_BATTLEGROUND_PLAYER_JOINED:  // 0x02EC - Jucator nou intrat in instanta de PvP
        case SMSG_GROUP_SET_LEADER:            // 0x0079 - Schimbarea liderului de grup
        case SMSG_SPELLHEALLOG:                // 0x0150 - Log de healing primit de bot sau aliati
        case SMSG_ATTACK_START:                // 0x0143 - Botul a inceput atacul melee pe o tinta
        case SMSG_ATTACK_STOP:                 // 0x0144 - Oprirea atacului melee
        case SMSG_ATTACK_SWING_NOT_IN_RANGE:   // 0x0145 - Tinta melee este prea departe
        case SMSG_CAST_FAILED:                 // 0x0130 - Vraja a esuat (Out of range / Out of sight)
        case SMSG_GAMEOBJECT_DESPAWN_ANIM:     // 0x0215 - Disparitia portilor de start ale arenei/BG-ului
        case SMSG_SPELLNONMELEEDAMAGELOG:      // 0x0250 - Log de damage din magii/spells
        case MSG_MOVE_ROOT:                    // 0x00EC - Serverul imobilizeaza botul (por?ile sunt inchise)
        case MSG_MOVE_SET_RUN_SPEED:           // 0x00CD - Schimbarea vitezei de alergare nativa
        case MSG_MOVE_SET_SWIM_SPEED:          // 0x00D3 - Schimbarea vitezei de inot
        case MSG_MOVE_SET_FLIGHT_SPEED:        // 0x037E - Schimbarea vitezei de zbor
        case MSG_MOVE_SET_RUN_BACK_SPEED:      // 0x00CF - Viteza de mers cu spatele
        case MSG_MOVE_SET_SWIM_BACK_SPEED:     // 0x00D5 - Viteza de inot cu spatele
        case MSG_MOVE_SET_FLIGHT_BACK_SPEED:   // 0x0380 - Viteza de zbor cu spatele
        case MSG_MOVE_TIME_SKIPPED:            // 0x0319 - Corectarea timpului de miscare desincronizat
        case SMSG_MOVE_SET_CAN_FLY:            // 0x0343 - Serverul activeaza/dezactiveaza dreptul de zbor
        case CMSG_MOVE_SET_FLY:                // 0x0346 - Clientul anunta ca a decolat in regim de zbor
        case MSG_MOVE_START_ASCEND:            // 0x0359 - Jucatorul incepe sa zboare in sus (Spacebar)
        case MSG_MOVE_STOP_ASCEND:             // 0x035A - Jucatorul se opreste din zborul in sus
        case SMSG_LOGIN_SET_TIME_SPEED:
        case SMSG_GM_MESSAGECHAT:              // 0x03B3 - Mesaje text primite de la Game Master
        case SMSG_SPELLLOGEXECUTE:             // 0x024C - Jurnalul binar de executie al magiilor
        case SMSG_HIGHEST_THREAT_UPDATE:       // 0x0482 - Schimbarea tintei de aggro de la server
        case SMSG_THREAT_REMOVE:               // 0x0484 - Scoaterea unei tinte din tabelul de threat
        case SMSG_THREAT_CLEAR:                // 0x0485 - Golirea totala a tabelului de threat
        case SMSG_AI_REACTION:                 // 0x013C - Reactia AI-ului nativ din Core
        case SMSG_ATTACKERSTATEUPDATE:         // 0x014A - Datele detaliate de damage/parry/block primite
        case SMSG_SPLINE_SET_RUN_SPEED:        // 0x02FE - Ajustare viteza pe rute fixe
        case SMSG_SPLINE_MOVE_UNROOT:          // 0x0304 - Eliberarea din imobilizare pe spline
        case SMSG_SPLINE_MOVE_SET_WALK_MODE:   // 0x030E - Trecerea fortata la modul de mers la pas
        case SMSG_START_MIRROR_TIMER:          // 0x01D9 - Pornirea barei de oboseala / respiratie sub apa
        case SMSG_STOP_MIRROR_TIMER:           // 0x01DB - Oprirea barei de oboseala la moarte sau iesire la mal
        case SMSG_ENVIRONMENTAL_DAMAGE_LOG:    // 0x01FC - Logul de damage primit din mediu (Fatigue/Lava/Cadere)
        case SMSG_SPLINE_MOVE_ROOT:            // 0x031A - Imobilizarea spline la moartea corpului
        case MSG_MOVE_WATER_WALK:              // 0x02B1 - Permisiunea de a pluti pe apa ca spirit
        case MSG_MOVE_UNROOT:                  // 0x00ED - Deblocarea fizica a miscarii fantomei
        case SMSG_SPLINE_SET_SWIM_SPEED:       // 0x0300 - Ajustarea vitezei de inot/plutire ca spirit
        case SMSG_SPLINE_MOVE_WATER_WALK:      // 0x0309 - Rutarea spline pentru starea de mers pe apa
        case SMSG_ACHIEVEMENT_EARNED:         // 0x0468 - Anunt ca botul sau cineva din jur a luat o realizare
        case SMSG_PARTYKILLLOG:                // 0x01F5 - Log de ucidere a unei tinte in grup/BG
        case SMSG_ARENA_TEAM_STATS:            // 0x035B - Sincronizarea ratingului de Arena la final de meci
        case SMSG_PLAY_SOUND:                  // 0x02D2 - Sunetul de final de meci trimis de server
        case SMSG_CLIENT_CONTROL_UPDATE:       // 0x0159 - Actualizarea controlului fizic la teleportare
        case MSG_PVP_LOG_DATA:                 // 0x02E0 - Tabela finala de scoruri PvP (Kills/Damage)
        case SMSG_LFG_UPDATE_PARTY:            // 0x0368 - Update pe sistemul de cautare grup
        case SMSG_BATTLEGROUND_PLAYER_LEFT:    // 0x02ED - Notificare ca un jucator a parasit instanta PvP
        case SMSG_ARENA_UNIT_DESTROYED:        // 0x04C7 - Stergerea frame-ului special de arena pentru un inamic
        case SMSG_GROUP_DESTROYED:             // 0x007C - Distrugerea grupului de arena cand meciul se inchide
        case MSG_MOVE_JUMP:                    // 0x00BB - Ignoram pachetul cand alti playeri sar in jur
        case SMSG_SPLINE_MOVE_LAND_WALK:       // 0x030A - For?area mersului la sol (frecvent la ie?ire instan?e)
        case SMSG_MOVE_LAND_WALK:
        case SMSG_FORCE_RUN_SPEED_CHANGE:      // 0x00E2
        case SMSG_FORCE_SWIM_SPEED_CHANGE:     // 0x00E6
        case SMSG_FORCE_MOVE_UNROOT:           // 0x00EA
        case SMSG_FORCE_MOVE_ROOT:
        case SMSG_GUILD_EVENT:                 // 0x0092
        case SMSG_SPELLLOGMISS:                // 0x024B
        case SMSG_SPELL_FAILURE:               // 0x0133
        case SMSG_PET_UPDATE_COMBO_POINTS:     // 0x0492
        case SMSG_UPDATE_COMBO_POINTS:         // 0x039D
        case SMSG_COOLDOWN_EVENT:              // 0x0135
        case SMSG_SPELL_FAILED_OTHER:          // 0x02A6
        case SMSG_SPELLDAMAGESHIELD:           // 0x024F
        case SMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE: // 0x0383
        case SMSG_FORCE_SWIM_BACK_SPEED_CHANGE: // 0x02DC
        case SMSG_FORCE_RUN_BACK_SPEED_CHANGE:  // 0x00E4
        case SMSG_FORCE_FLIGHT_SPEED_CHANGE:   // 0x0381
        case SMSG_SPLINE_SET_FLIGHT_SPEED:     // 0x0385
        case SMSG_CLEAR_COOLDOWN:              // 0x01DE
        case SMSG_GUILD_BANK_LIST:             // 0x03E8
        case SMSG_EMOTE:                       // 0x0103
        case SMSG_DISMOUNT:                    // 0x03AC
        case SMSG_STANDSTATE_UPDATE:           // 0x029D
        case SMSG_SPELLDISPELLOG:              // 0x027B
        case SMSG_SPELL_COOLDOWN:              // 0x0134
        case SMSG_SPLINE_SET_RUN_BACK_SPEED:   // 0x02FF
        case SMSG_SPLINE_SET_SWIM_BACK_SPEED:  // 0x0302
        case SMSG_SPLINE_SET_FLIGHT_BACK_SPEED: // 0x0386
        case SMSG_CRITERIA_DELETED:            // 0x049E
        case SMSG_SPELLORDAMAGE_IMMUNE:        // 0x0263
        case SMSG_SET_FACTION_VISIBLE:         // 0x0123
        case SMSG_LOGOUT_COMPLETE:             // 0x004D
        case SMSG_SPELL_DELAYED:               // 0x01E2
        case SMSG_LFG_UPDATE_SEARCH:           // 0x0369
        case SMSG_MOVE_WATER_WALK:             // 0x00DE
        case SMSG_REAL_GROUP_UPDATE:           // 0x0397
        case SMSG_RESYNC_RUNES:                // 0x0487
        case SMSG_ATTACK_SWING_BAD_FACING:     // 0x0146
        case MSG_CHANNEL_START:                // 0x0139
        case MSG_CHANNEL_UPDATE:               // 0x013A
        case SMSG_CHAT_SERVER_MESSAGE:         // 0x0291
        case SMSG_CANCEL_AUTO_REPEAT:          // 0x029C
        case SMSG_LOOT_LIST:                   // 0x03F9
        case SMSG_THREAT_UPDATE:               // 0x0483
        case SMSG_PLAY_SPELL_IMPACT:           // 0x01F7
        case SMSG_PLAY_SPELL_VISUAL:           // 0x01F3
        case CMSG_MOVE_FALL_RESET:             // 0x02CA
        case SMSG_MOVE_KNOCK_BACK:             // 0x00EF
        case SMSG_TEXT_EMOTE:                  // 0x0105
        case SMSG_ENCHANTMENTLOG:              // 0x01D7
        case SMSG_RECEIVED_MAIL:               // 0x0285
        case SMSG_ARENA_TEAM_EVENT:            // 0x0357
        case SMSG_ARENA_TEAM_COMMAND_RESULT:   // 0x0349
        case MSG_SET_RAID_DIFFICULTY:          // 0x04EB
        case SMSG_ZONE_UNDER_ATTACK:           // 0x0254
        case MSG_MOVE_KNOCK_BACK:              // 0x00F1
        case SMSG_PET_SPELLS:                  // 0x0179
        case SMSG_DISPEL_FAILED:               // 0x0262
        case SMSG_ITEM_PUSH_RESULT:            // 0x0166

            break;






        default:
        {
            // Apelam functia de logare automata pentru pachetele nerezolvate
            LogheazaOpcodeLipsa(opcode);
            break;
        }





















        }
    }

    void OnPacketReceive(WorldSession* session, WorldPacket& packet) override
    {
        return; // dezactivat

        if (!session || !session->GetPlayer())
            return;

        if (!session->IsKittBot())
            return;

        Player* player = session->GetPlayer();
        uint16 opcode = packet.GetOpcode();

        // REPARATIE AFISARE NUME: Preluam numele text oficial din Core conform structurii tale
        std::string opcodeName = GetOpcodeNameForLogging(static_cast<Opcodes>(opcode));

        // Daca functia returneaza gol (caz rar), punem o siguranta text
        if (opcodeName.empty())
        {
            opcodeName = "UNKNOWN_CMSG_OPCODE";
        }

        // Log formatat curat: afiseaza Nume Caracter | ID Hexazecimal | Dimensiune | Nume Oficial Pachet
        TC_LOG_INFO("server.loading", "[BotNetwork] Bot->S [PRIMIT] De la Bot: {} | Opcode: 0x{:X} (Size: {}) | Nume: {}",
            player->GetName().c_str(), opcode, (uint32)packet.size(), opcodeName.c_str());
    }

};


class script_sniffer_jucator_real : public ServerScript
{
public:
    script_sniffer_jucator_real() : ServerScript("script_sniffer_jucator_real") {}

    // Prindem pachetele pe care JUCATORUL REAL le trimite catre Server (CMSG)
    void OnPacketReceive(WorldSession* session, WorldPacket& packet) override
    {
        if (!session || !session->GetPlayer())
            return;

        // FILTRU CRITIC: Schimba "Test" cu numele exact al caracterului tau de GM cu care intri in joc!
        if (session->GetPlayer()->GetName() != "Testt")
            return;

        uint16 opcode = packet.GetOpcode();
        std::string opcodeName = GetOpcodeNameForLogging(static_cast<Opcodes>(opcode));

        if (opcodeName.empty())
        {
            opcodeName = "UNKNOWN_CMSG_OPCODE";
        }

        // Acest log se va aprinde in consola DOAR pentru tine!
        TC_LOG_INFO("server.loading", "[SNIFFER-GM] Jucator->Server | Opcode: 0x{:X} (Size: {}) | Nume: {}",
            opcode, (uint32)packet.size(), opcodeName.c_str());
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
    //new script_sniffer_jucator_real();
}
