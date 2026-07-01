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


extern void kitt_start_bot_pvp_AI(Player* botPlayer);
using namespace Trinity::ChatCommands;

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

    struct BotAsyncTracker
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
    static std::vector<BotAsyncTracker> g_MultiBotTracker;

    // Vector pentru a pastra sesiunile ghost active in memorie fara sa fie sterse
    static std::vector<std::unique_ptr<WorldSession>> g_GhostSessionsStorage;

    static uint32 g_BootSequenceTimer = 10000;

    void ForseazaStergereBotFantoma(BotAsyncTracker& tracker)
    {
        tracker.RemoveFromWorld = true;
        tracker.futureResult = {};
        tracker.holder = nullptr;

        ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);
        FictivBotsGuids.erase(playerGuid);

        // Cautam daca jucatorul este inca in lume
        if (Player* botPlayer = ObjectAccessor::FindConnectedPlayer(playerGuid))
        {
            botPlayer->CombatStop();

            // TrinityCore va salva corect datele si va scoate playerul din harta/lume nativ
            if (botPlayer->GetSession())
            {
                botPlayer->GetSession()->LogoutPlayer(false); // save = true
                botPlayer->GetSession()->KickPlayer("ghost bot");
            }
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
            //sWorld->RemoveSession(tracker.realSession->GetAccountId());
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

class LoginQueryHolder : public CharacterDatabaseQueryHolder
{
private:
    uint32 m_accountId;
    ObjectGuid m_guid;
public:
    LoginQueryHolder(uint32 accountId, ObjectGuid guid)
        : m_accountId(accountId), m_guid(guid) {
    }
    ObjectGuid GetGuid() const { return m_guid; }
    uint32 GetAccountId() const { return m_accountId; }
    bool Initialize();
};

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
                tracker.AccRelogDelay = 5000;

                ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);
                FictivBotsGuids.erase(playerGuid);
                //tracker.isProcessed = false;
                tracker.kickedByPlayer = true;
                tracker.AccRealBusy = true;

                if (tracker.realSession)
                {
                    if (Player* botPlayer = ObjectAccessor::FindConnectedPlayer(playerGuid))
                    {
                        TC_LOG_INFO("fakPlayer", "LOG ACCOUNT KICK: Jucatorul real s-a logat pe contul {}. Se curata imediat botul {} din lume...",
                            accountId, botPlayer->GetName());

                        /*botPlayer->CombatStop();
                        botPlayer->CleanupsBeforeDelete();
                        botPlayer->RemoveAllAuras();

                        if (botPlayer->IsInWorld())
                        {
                            if (Map* map = botPlayer->GetMap())
                                map->RemovePlayerFromMap(botPlayer, true);
                            else
                                botPlayer->RemoveFromWorld();
                        }

                        ObjectAccessor::RemoveObject(botPlayer);*/
                        //sWorld->RemoveSession(tracker.realSession->GetAccountId());

                        //ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);
                        //FictivBotsGuids.erase(playerGuid);

                        botPlayer->CombatStop();
                        if (botPlayer->GetSession())
                        {
                            botPlayer->GetSession()->LogoutPlayer(true); // true = forteaza salvarea imediata in DB
                        }
                        //sWorld->RemoveSession(tracker.realSession->GetAccountId());

                        auto it = std::remove_if(g_GhostSessionsStorage.begin(), g_GhostSessionsStorage.end(),
                            [accountId](const std::unique_ptr<WorldSession>& session) {
                                return session && session->GetAccountId() == accountId;
                            });

                        if (it != g_GhostSessionsStorage.end())
                        {
                            g_GhostSessionsStorage.erase(it, g_GhostSessionsStorage.end());
                            TC_LOG_INFO("fakPlayer", "LOG CLEANUP: Sesiunea artificiala a contului {} a fost stearsa cu succes din stocarea de boti.", accountId);
                        }

                        tracker.isProcessed = false;
                    }
                }

                break;
            }
        }
    }
};

class kitt_bot_world_loader : public WorldScript
{
public:
    kitt_bot_world_loader() : WorldScript("kitt_bot_world_loader") {}

    static void PornesteBotIndividual(uint32 accountId, uint32 charGuid)
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

        ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(charGuid);

        auto loginHolder = std::make_shared<LoginQueryHolder>(accountId, playerGuid);
        loginHolder->Initialize();

        SQLQueryHolderTask task(loginHolder);

        bool gasitInTracker = false;
        for (auto& tracker : g_MultiBotTracker)
        {
            if (tracker.accountId == accountId)
            {
                tracker.realSession = fakeSession;
                tracker.holder = loginHolder;
                tracker.futureResult = task.GetFuture();
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
            sWorld->AddSession(fakeSession);

            ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(bot.charGuid);

            auto loginHolder = std::make_shared<LoginQueryHolder>(bot.accountId, playerGuid);

            loginHolder->Initialize();

            SQLQueryHolderTask task(loginHolder);

            BotAsyncTracker tracker;
            tracker.accountId = bot.accountId;
            tracker.charGuid = bot.charGuid;
            tracker.realSession = fakeSession;
            tracker.holder = loginHolder;
            tracker.futureResult = task.GetFuture();
            tracker.isProcessed = false;
            tracker.isQueued = false;

            g_MultiBotTracker.push_back(std::move(tracker));

            CharacterDatabase.DelayQueryHolder(loginHolder);
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
                    kitt_bot_world_loader::PornesteBotIndividual(tracker.accountId, tracker.charGuid);

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

                    std::string tempName = "GHOST_SESSION_" + std::to_string(tracker.charGuid);
                    std::shared_ptr<WorldSocket> nullSocket = nullptr;

                    auto ghostSession = std::make_unique<WorldSession>(tracker.accountId, std::move(tempName), nullSocket, SEC_PLAYER, 2, 0, std::chrono::minutes(0), LOCALE_enUS, 0, false);
                    //ghostSession->LoadPermissions();

                    Player* botPlayer = new Player(ghostSession.get());
                    ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);

                    g_GhostSessionsStorage.push_back(std::move(ghostSession));

                    if (botPlayer->LoadFromDB(playerGuid, *tracker.holder))
                    {
                        // protectie map instance
                        // nu se poate crea instata daca nu e jucator deja
                        uint32 botMapId = botPlayer->GetMapId();
                        MapEntry const* mapEntry = sMapStore.LookupEntry(botMapId);

                        if (!mapEntry || mapEntry->Instanceable())
                        {
                            ForseazaStergereBotFantoma(tracker);

                            /*botPlayer->CombatStop();
                            if (botPlayer->GetSession())
                            {
                                botPlayer->GetSession()->LogoutPlayer(true); // true = forteaza salvarea imediata in DB
                            }*/

                            uint32 homeMapId = botPlayer->m_homebindMapId;
                            float homeX = botPlayer->m_homebindX;
                            float homeY = botPlayer->m_homebindY;
                            float homeZ = botPlayer->m_homebindZ;
                            float homeO = botPlayer->GetOrientation();
                            //botPlayer->TeleportTo(homeMapId, homeX, homeY, homeZ, homeO);
                            CharacterDatabase.PExecute("UPDATE characters SET position_x = {}, position_y = {}, position_z = {}, orientation = {}, map = {}, instance_id = 0 WHERE guid = {};",
                                homeX, homeY, homeZ, homeO, homeMapId, tracker.charGuid);
                            /*
                            tracker.RemoveFromWorld = true;
                            tracker.futureResult = {};
                            tracker.holder = nullptr;
                            tracker.AccRelogDelay = 5000;


                            // Scoatem sesiunea artificiala din managerul global de sesiuni
                            auto accountId = tracker.accountId;
                            g_GhostSessionsStorage.erase(
                                std::remove_if(g_GhostSessionsStorage.begin(), g_GhostSessionsStorage.end(),
                                    [accountId](const std::unique_ptr<WorldSession>& session) {
                                        return session && session->GetAccountId() == accountId;
                                    }),
                                g_GhostSessionsStorage.end()
                            );
                            sWorld->RemoveSession(tracker.realSession->GetAccountId());

                            // Stergem GUID-ul din set-ul de boti fictivi
                            ObjectGuid playerGuid = ObjectGuid::Create<HighGuid::Player>(tracker.charGuid);
                            FictivBotsGuids.erase(playerGuid);

                            TC_LOG_INFO("fakPlayer", "LOG GHOST PROTECTIE: Botul {} a fost mutat in memorie catre Homebind (Map: {}).", botPlayer->GetName().c_str(), homeMapId);
                            */
                            TC_LOG_INFO("fakPlayer", "LOG GHOST PROTECTIE: Botul {} a fost mutat in memorie catre Homebind (Map: {}).", botPlayer->GetName().c_str(), homeMapId);

                            kitt_bot_world_loader::PornesteBotIndividual(tracker.accountId, tracker.charGuid);

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
                            botPlayer->GetMap()->AddPlayerToMap(botPlayer);

                            botPlayer->AddToWorld();
                            ObjectAccessor::AddObject(botPlayer);

                            //botPlayer->SetPvP(true);
                            //botPlayer->SetGameMaster(true);
                            FictivBotsGuids.insert(playerGuid);

                            TC_LOG_INFO("fakPlayer", "LOG CUSTOM REUSIT: {} (GUID: {}) a intrat online permanent pe sesiunea reala!", botPlayer->GetName().c_str(), tracker.charGuid);
                            break;
                        }
                    }
                    else
                    {
                        TC_LOG_INFO("fakPlayer", "LOG CUSTOM EROARE: LoadFromDB a refuzat structura holder-ului pentru GUID {}.", tracker.charGuid);

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
                        tracker.isProcessed = false;
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
                                                continue;
                                            }

                                            if (!botPlayer->IsAlive())
                                            {
                                                botPlayer->ResurrectPlayer(1.0f);
                                                botPlayer->SpawnCorpseBones();
                                            }

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
                                kitt_start_bot_pvp_AI(botPlayer);
                            }
                        }

                        // STATUS_WAIT_LEAVE are valoarea nativa 4. O verificam direct in siguranta:
                        if (bg && bg->GetStatus() == STATUS_WAIT_LEAVE)
                        {
                            TC_LOG_INFO("fakPlayer", "LOG ARENA: Meciul s-a terminat pentru {}. Se forteaza parasirea instantei...", botPlayer->GetName().c_str());

                            botPlayer->LeaveBattleground();

                            // opcode
                            if (botPlayer->GetSession())
                            {
                                WorldPacket pachetGol;
                                botPlayer->GetSession()->HandleMoveWorldportAckOpcode(pachetGol);
                            }

                            // --- LOGICA VERIFICARE SI ADAUGARE RATING ---
                            //CheckAndRewardArenaBotRating(botPlayer);
                            CheckAndRewardArenaBotPersonalRating(botPlayer);

                            //tracker.isQueued = false;

                            break;
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

                                JoinGroupArena2v2Rated(botPlayer);

                                tracker.isQueued = true;
                                tracker.rejoinTimer = 0;
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
                        member->AddBattlegroundQueueId(bgQueueTypeId);
                    }
                }
            }
            else
            {
                botPlayer->AddBattlegroundQueueId(bgQueueTypeId);
            }

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
        if (currentRating < 1350)
        {
            uint32 bonusPoints = urand(150, 250);
            uint32 newRating = currentRating + bonusPoints;

            // 1. Salvam asincron in baza de date
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            trans->PAppend("UPDATE arena_team SET rating = {} WHERE arenaTeamId = {}", newRating, arenaTeamId);
            CharacterDatabase.AsyncCommitTransaction(trans);

            // 2. Sincronizam in memoria RAM
            // CORECTIE CRITICALA SYNTAX: Am unit tot string-ul pe o singura linie curata
            // Am scos aliasul problematic si am lasat {} simplu, exact pe pozitia coloanei numarul 9
            QueryResult result = CharacterDatabase.PQuery("SELECT arenaTeamId, name, captainGuid, type, backgroundColor, emblemStyle, emblemColor, borderStyle, borderColor, {}, weekGames, weekWins, seasonGames, seasonWins, rank FROM arena_team WHERE arenaTeamId = {}", newRating, arenaTeamId);

            if (result)
            {
                at->LoadArenaTeamFromDB(result);
            }

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

        if (currentPersonalRating < 1350)
        {
            uint32 bonusPoints = urand(150, 250);
            uint32 newPersonalRating = currentPersonalRating + bonusPoints;

            uint32 newMMR = newPersonalRating;
            if (newMMR < 1500)
            {
                newMMR = 1500;
            }

            botPlayer->SetArenaTeamInfoField(teamSizeIndex, ARENA_TEAM_PERSONAL_RATING, newPersonalRating);

            for (ArenaTeam::MemberList::iterator itr = at->m_membersBegin(); itr != at->m_membersEnd(); ++itr)
            {
                itr->PersonalRating = newPersonalRating;
                itr->MatchMakerRating = newMMR;
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
            { "ghostList",  HandleShowGhostList,          rbac::RBAC_PERM_COMMAND_LEARN, Console::No },
            { "ghost",      HandleStartGhostInWorld,      rbac::RBAC_PERM_COMMAND_LEARN, Console::No },
            { "remove",     HandleRemoveGhostFromWorld,   rbac::RBAC_PERM_COMMAND_LEARN, Console::No },
        };

        static std::vector<ChatCommandBuilder> kittGhostPlayerCommandTable =
        {
            { "ztfcGhost", kittGhostPlayerCommandSubcommandTable },
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
        kitt_bot_world_loader::PornesteBotIndividual(targetAccId, targetGuidLow);

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




void AddSC_kitt_bot_world_loader()
{
    new kitt_bot_world_loader();
    new kitt_bot_account_login_interceptor();
    new kitt_bot_world_update();
    new kitt_bot_chat_handler();
    new kitt_ghost_player_command();
}
