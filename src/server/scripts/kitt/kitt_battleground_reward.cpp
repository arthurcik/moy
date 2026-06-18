// ----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----



#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Log.h"
#include <unordered_map>
#include <mutex>

namespace
{
    static uint32 kittBattlegroundRewardEnable = 0;
    static uint32 KillCount = 10;
    static const uint32 AVitemReward = 20560; // AV mark

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


void AddSC_kitt_battleground_reward()
{
    new kitt_battleground_reward_config();
    new kitt_battleground_reward_av();
}
