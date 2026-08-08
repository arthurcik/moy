// ----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----



#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Battleground.h"
#include "Chat.h"
#include "Log.h"
#include <unordered_map>
#include <mutex>

namespace
{
    static uint32 kittBattlegroundRewardEnable = 0;
    static uint32 KillCount = 10;
    static const uint32 AVitemReward = 20560; // AV mark
    uint32 static const itemMarkOfHonor = 43589; // Wintergrasp Mark
    uint32 static const itemStoneKeeperShard = 43228; // Stone Keeper's Shard

    std::unordered_map<ObjectGuid, uint32> avKillsMap;
    std::mutex avKillsMutex;
}

class kitt_battleground_reward_av : public PlayerScript
{
public:
    kitt_battleground_reward_av() : PlayerScript("kitt_battleground_reward_av") {}

    void OnPVPKill(Player* killer, Player* killed) override
    {
        if (kittBattlegroundRewardEnable == 0)
            return;

        if (!killer || !killed)
            return;
        if (!killer->GetMap()->IsBattleground())
            return;
        if (killer->GetMapId() != 30) // bg >> AV
            return;

        std::lock_guard<std::mutex> lock(avKillsMutex);
        avKillsMap[killer->GetGUID()]++;

        if (avKillsMap[killer->GetGUID()] >= KillCount)
        {
            killer->AddItem(AVitemReward, 1);

            avKillsMap[killer->GetGUID()] = 0;
        }
    }

    void OnCreatureKill(Player* killer, Creature* killed) override
    {
        if (kittBattlegroundRewardEnable == 0)
            return;

        if (!killer || !killed)
            return;
        if (!killer->GetMap()->IsBattleground())
            return;
        if (killer->GetMapId() != 30) // bg >> AV
            return;
        if (!killed->IsNPCBot())
            return;

        std::lock_guard<std::mutex> lock(avKillsMutex);
        avKillsMap[killer->GetGUID()]++;

        if (avKillsMap[killer->GetGUID()] >= KillCount)
        {
            killer->AddItem(AVitemReward, 1);

            avKillsMap[killer->GetGUID()] = 0;
        }
    }
};

class kitt_battleground_reward_config : public WorldScript
{
public:
    kitt_battleground_reward_config() : WorldScript("kitt_battleground_reward_config") {}

    void OnStartup() override
    {
        if (kittBattlegroundRewardEnable == 1)
        {
            TC_LOG_INFO("server.loading", ">> KITT [BG Reward] ACTIVAT. Kill Count: {}", KillCount);
        }
        else
        {
            TC_LOG_INFO("server.loading", ">> KITT [BG Reward] DEZACTIVAT.");
        }
    }

    void OnConfigLoad(bool /*reload*/) override
    {
        kittBattlegroundRewardEnable = sConfigMgr->GetIntDefault("kitt.Battleground.Reward.Enable", 0);
        KillCount = sConfigMgr->GetIntDefault("kitt.Battleground.Reward.Kill.Count", 10);

        if (kittBattlegroundRewardEnable == 1)
            TC_LOG_INFO("server.loading", ">> KITT [BG Reward] config load. Option ACTIVAT. Kill Count: {}", KillCount);
        else
            TC_LOG_INFO("server.loading", ">> KITT [BG Reward] config load. Option DEZACTIVAT.");
    }
};

class kitt_battleground_rewards_player : public PlayerScript
{
public:
    kitt_battleground_rewards_player() : PlayerScript("kitt_battleground_rewards_player") {}

    void OnBattlegroundEnd(Player* player, Battleground* bg, uint32 winner) override
    {
        if (!player || !bg)
            return;

        if (player->GetBattlegroundTypeId() != BATTLEGROUND_AV)
            return;

        if (bg->isArena())
            return;

        if (winner != ALLIANCE && winner != HORDE)
            return;

        // Ambele functii returneaza acum acelasi tip de valori: ALLIANCE (469) sau HORDE (67)
        uint32 playerBgTeam = player->GetBGTeam();

        // Comparam direct valorile reale de factiuni
        if (playerBgTeam == winner)
        {
            player->AddItem(itemMarkOfHonor, 1);
            player->AddItem(itemStoneKeeperShard, 3);

            ChatHandler(player->GetSession()).SendSysMessage("Ai castigat meciul! Ai primit 1x Wintergrasp Mark si 3x Stone Keeper's Shard.");
        }
    }
};

void AddSC_kitt_battleground_reward()
{
    new kitt_battleground_reward_config();
    new kitt_battleground_reward_av();
    new kitt_battleground_rewards_player();
}
