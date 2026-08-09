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

    uint32 static const itemMarkOfHonor = 43589; // Wintergrasp Mark
    uint32 static const itemStoneKeeperShard = 43228; // Stone Keeper's Shard
    // AV cont
    uint32 static const AVitemReward = 20560; // AV mark
    uint32 static const AV_WinsKillCountMarkOfHonor = 100;
    uint32 static const AV_WinsKillCountStoneKeeperShard = 20;
    uint32 static const AV_LoseKillCountMarkOfHonor = 200;
    uint32 static const AV_LoseKillCountStoneKeeperShard = 40;
    // WS
    uint32 static const WSitemReward = 20558; // WS mark
}

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
        if (kittBattlegroundRewardEnable == 0)
            return;

        if (!player || !bg)
            return;

        if (bg->isArena())
            return;

        uint32 BonusItemBG = 0;
        uint32 BonusItemBGcount = 0;

        uint32 WinskillMarkCount = 0;
        uint32 WinskillShardCount = 0;
        uint32 LosekillMarkCount = 0;
        uint32 LosekillShardCount = 0;

        uint32 WinsMarkCount = 0;
        uint32 WinsShardCount = 0;
        uint32 LoseMarkCount = 0;
        uint32 LoseShardCount = 0;

        uint32 configKillCount = 0;
        std::string bgNameShort = "";

        switch (player->GetBattlegroundTypeId())
        {
        case BATTLEGROUND_AV:
            WinsMarkCount = 1;
            WinsShardCount = 3;
            LoseMarkCount = 1;
            LoseShardCount = 3;

            WinskillMarkCount = AV_WinsKillCountMarkOfHonor;
            WinskillShardCount = AV_WinsKillCountStoneKeeperShard;
            LosekillMarkCount = AV_LoseKillCountMarkOfHonor;
            LosekillShardCount = AV_LoseKillCountStoneKeeperShard;

            BonusItemBG = AVitemReward;
            BonusItemBGcount = 5;
            configKillCount = KillCount;            // Pragul tau de kill-uri pentru AV
            bgNameShort = "AV";
            break;

        case BATTLEGROUND_WS:
            BonusItemBG = WSitemReward;
            BonusItemBGcount = 5;
            configKillCount = KillCount;            // Pragul tau de kill-uri pentru AV
            bgNameShort = "WS";
            break;

        default:
            // Daca BG-ul curent nu este configurat in case-uri, oprim executia
            return;
        }

        if (winner != ALLIANCE && winner != HORDE)
            return;

        uint32 playerBgTeam = player->GetBGTeam();

        bool winnerFlag = false;
        if (playerBgTeam == winner)
        {
            winnerFlag = true;
        }
        else
        {
            winnerFlag = false;
        }

        if (BattlegroundScore* score = bg->GetPlayerScore(player->GetGUID()))
        {
            uint32 totalKills = score->GetHonorableKills();

            /*uint32 AVrewardMultiplier = totalKills / configKillCount;
            uint32 WinsMarkRewardMultiplier = totalKills / WinskillMarkCount;
            uint32 WinsShardRewardMultiplier = totalKills / WinskillShardCount;
            uint32 LoseMarkRewardMultiplier = totalKills / LosekillMarkCount;
            uint32 LoseShardRewardMultiplier = totalKills / LosekillShardCount;*/

            uint32 BonusRewardMultiplier = (configKillCount > 0) ? (totalKills / configKillCount) : 0;
            uint32 WinsMarkRewardMultiplier = (WinskillMarkCount > 0) ? (totalKills / WinskillMarkCount) : 0;
            uint32 WinsShardRewardMultiplier = (WinskillShardCount > 0) ? (totalKills / WinskillShardCount) : 0;
            uint32 LoseMarkRewardMultiplier = (LosekillMarkCount > 0) ? (totalKills / LosekillMarkCount) : 0;
            uint32 LoseShardRewardMultiplier = (LosekillShardCount > 0) ? (totalKills / LosekillShardCount) : 0;

            bool WinsMsgSend = false;
            bool LoseMsgSend = false;

            if (winnerFlag)
            {
                if (BonusItemBGcount >= 1)
                {
                    player->AddItem(BonusItemBG, BonusItemBGcount + BonusRewardMultiplier);
                    WinsMsgSend = true;
                }

                if (WinsMarkCount >= 1)
                {
                    player->AddItem(itemMarkOfHonor, WinsMarkCount + WinsMarkRewardMultiplier);
                    WinsMsgSend = true;
                }

                if (WinsShardCount >= 1)
                {
                    player->AddItem(itemStoneKeeperShard, WinsShardCount + WinsShardRewardMultiplier);
                    WinsMsgSend = true;
                }

                if (WinsMsgSend)
                {
                    //std::string winMsg = "|cff00ff00[Reward]|r You won the match! - " + bgNameShort;
                    std::string winMsg = "|cff00ff00[Reward]|r You won the match |cffffffff" + bgNameShort +
                        "|r with |cffffffff" + std::to_string(totalKills) + "|r Honorable Kills!";
                    ChatHandler(player->GetSession()).SendSysMessage(winMsg.c_str());
                }
            }
            else
            {
                if (BonusRewardMultiplier >= 1)
                {
                    player->AddItem(BonusItemBG, BonusRewardMultiplier);
                    LoseMsgSend = true;
                }

                if (LoseMarkCount >= 1)
                {
                    player->AddItem(itemMarkOfHonor, LoseMarkCount + LoseMarkRewardMultiplier);
                    LoseMsgSend = true;
                }

                if (LoseShardCount >= 1)
                {
                    player->AddItem(itemStoneKeeperShard, LoseShardCount + LoseShardRewardMultiplier);
                    LoseMsgSend = true;
                }

                if (LoseMsgSend)
                {
                    //std::string loseMsg = "|cff00ff00[Reward]|r You lose the match! - " + bgNameShort;
                    std::string loseMsg = "|cff00ff00[Reward]|r You lose the match |cffffffff" + bgNameShort +
                        "|r with |cffffffff" + std::to_string(totalKills) + "|r Honorable Kills!";
                    ChatHandler(player->GetSession()).SendSysMessage(loseMsg.c_str());
                }
            }

            /*std::string msg = "|cff00ff00[Reward]|r For the " + std::to_string(totalKills) +
                " honorable kills gathered, you received " + std::to_string(AVrewardMultiplier) +
                "x " + bgNameShort + " rewards!";
            ChatHandler(player->GetSession()).SendSysMessage(msg.c_str());*/
        }
    }
};

void AddSC_kitt_battleground_reward()
{
    new kitt_battleground_reward_config();
    new kitt_battleground_rewards_player();
}
