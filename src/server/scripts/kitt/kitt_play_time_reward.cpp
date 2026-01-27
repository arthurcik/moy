#include "ScriptMgr.h"
#include "Player.h"
#include "GuildMgr.h"
#include "Config.h"
#include "Guild.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Chat.h"
#include "Item.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"




class kitt_play_time_reward : public PlayerScript
{
private:
    struct PlaytimeReward {
        uint32 requiredMinutes;
        uint32 itemId;
        uint32 itemCount;
        std::string message;
    };

    const std::vector<PlaytimeReward> rewardConfig = {
        {1440, 49426, 20, "(played for more than 24 hours)"}
    };

    struct PeriodicReward {
        uint32 intervalMinutes;
        uint32 itemId;
        uint32 itemCount;
        std::string message;
    };

    const std::vector<PeriodicReward> periodicConfig = {
        {60, 49426, 1, "Periodic [under development]"}
    };

public:
    kitt_play_time_reward() : PlayerScript("kitt_play_time_reward") {}

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (!player || !player->IsInWorld() || !player->GetSession())
            return;

        ObjectGuid playerGuid = player->GetGUID();

        player->m_Events.AddEventAtOffset([this, playerGuid]()
            {
                Player* player = ObjectAccessor::FindPlayer(playerGuid);

                if (!player || !player->IsInWorld() || !player->GetSession())
                    return;

                CheckRewards(player);

            }, 10s);
    }

    void OnSave(Player* player) override
    {
        if (!player || !player->IsInWorld() || !player->GetSession() || !player->IsAlive() || player->GetSession()->isLogingOut())
            return;

        CheckRewards(player);
    }

private:

    void CheckRewards(Player* player)
    {
        uint32 totalPlaytimeMinutes = player->GetTotalPlayedTime() / 60;
        uint32 currentRewardLevel = 0;
        uint32 lastPeriodicTime = 0;
        ObjectGuid playerGuid = player->GetGUID();

        QueryResult result = CharacterDatabase.PQuery("SELECT last_reward_level, last_periodic_reward_time FROM character_kitt_playtime_rewards WHERE player_guid = {}", playerGuid.GetCounter());

        if (result)
        {
            Field* fields = result->Fetch();
            currentRewardLevel = fields[0].GetUInt32();
            lastPeriodicTime = fields[1].GetUInt32();
        }
        else
        {
            CharacterDatabase.PExecute("INSERT INTO character_kitt_playtime_rewards (player_guid, player_name, last_reward_level, last_periodic_reward_time) VALUES ({}, '{}', 0, 0)",
                playerGuid.GetCounter(), player->GetName().c_str());
        }

        bool updated = false;

        for (uint32 i = currentRewardLevel; i < rewardConfig.size(); ++i)
        {
            if (totalPlaytimeMinutes >= rewardConfig[i].requiredMinutes)
            {
                GiveReward(player, rewardConfig[i].itemId, rewardConfig[i].itemCount, rewardConfig[i].message);
                currentRewardLevel = i + 1;
                updated = true;
            }
            else break;
        }

        for (const auto& pReward : periodicConfig)
        {
            uint32 timesToReward = totalPlaytimeMinutes / pReward.intervalMinutes;
            uint32 timesAlreadyRewarded = lastPeriodicTime / pReward.intervalMinutes;

            if (timesToReward > timesAlreadyRewarded)
            {
                GiveReward(player, pReward.itemId, pReward.itemCount, pReward.message);
                lastPeriodicTime = totalPlaytimeMinutes;
                updated = true;
            }
        }

        if (updated)
        {
            CharacterDatabase.PExecute("UPDATE character_kitt_playtime_rewards SET last_reward_level = {}, last_periodic_reward_time = {} WHERE player_guid = {}",
                currentRewardLevel, lastPeriodicTime, playerGuid.GetCounter());
        }
    }

    void GiveReward(Player* player, uint32 itemId, uint32 itemCount, std::string customMsg)
    {
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, itemCount);

        if (msg == EQUIP_ERR_OK)
        {
            player->StoreNewItem(dest, itemId, itemCount, true);

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            std::string itemName = itemTemplate ? itemTemplate->Name1 : "Obiect";

            ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[Playtime Reward]|r %s: |cffffffff%s|r x%u.",
                customMsg.c_str(), itemName.c_str(), itemCount);
        }
        else
        {
            SendRewardToMail(player, itemId, itemCount);
        }

    }

    void SendRewardToMail(Player* player, uint32 itemId, uint32 itemCount)
    {
        if (!player)
            return;

        Item* item = Item::CreateItem(itemId, itemCount, player);
        if (!item)
            return;

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        item->SaveToDB(trans);
        MailDraft draft("Playtime Reward", "Your inventory was full. You have received your reward via mail.");
        draft.AddItem(item);
        draft.SendMailTo(trans, player, MailSender(MAIL_CREATURE, 34337));
        CharacterDatabase.CommitTransaction(trans);

        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[Playtime Reward]:|r Inventory full! Your reward |cffffffff[%s]|r has been sent to your mailbox.",
            item->GetTemplate()->Name1.c_str());
    }

};



void AddSC_kitt_play_time_reward()
{
    new kitt_play_time_reward();
}
