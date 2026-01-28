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
#include "GameTime.h"

namespace
{
    static std::map<ObjectGuid, time_t> playtimeCooldownMap;
    static uint32 playtimeCooldownTime = 300; // in secunde anti-flood

    struct PlaytimeReward {
        uint32 requiredMinutes;
        uint32 itemId;
        uint32 itemCount;
        std::string message;
    };

    static const std::vector<PlaytimeReward> rewardConfig = {
        {60, 10361, 1, "(played for more than 1 hours)"}, // companion Brown Snake
    };

    struct PeriodicReward {
        uint32 intervalMinutes;
        uint32 itemId;
        uint32 itemCount;
        std::string message;
    };

    static const std::vector<PeriodicReward> periodicConfig = {
        {120,  47241, 1, "Periodic [2h ingame]"}, // embleme of triumph
        {121,  49426, 1, "Periodic [2h ingame]"}, // embleme of frost
        {300,  49908, 1, "Periodic [5h ingame]"}, // primordial saronite
        {300,  43102, 1, "Periodic [5h ingame]"}, // frozen orb
    };
}


class kitt_play_time_reward : public PlayerScript
{
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

    void OnLogout(Player* player) override
    {
        if (!player)
            return;

        playtimeCooldownMap.erase(player->GetGUID());
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
        if (!player)
            return;

        uint32 totalPlaytimeMinutes = player->GetTotalPlayedTime() / 60;
        ObjectGuid playerGuid = player->GetGUID();

        uint32 currentRewardLevel = 0;
        uint32 lastPeriodicTime = 0;

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
            return;
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

        bool periodicAwarded = false;
        for (const auto& pReward : periodicConfig)
        {
            uint32 timesToReward = totalPlaytimeMinutes / pReward.intervalMinutes;
            uint32 timesAlreadyRewarded = lastPeriodicTime / pReward.intervalMinutes;

            if (timesToReward > timesAlreadyRewarded)
            {
                GiveReward(player, pReward.itemId, pReward.itemCount, pReward.message);
                periodicAwarded = true;
            }
        }

        if (periodicAwarded)
        {
            lastPeriodicTime = totalPlaytimeMinutes;
            updated = true;
        }

        if (updated)
        {
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

            trans->PAppend("UPDATE character_kitt_playtime_rewards SET last_reward_level = {}, last_periodic_reward_time = {} WHERE player_guid = {}",
                currentRewardLevel, lastPeriodicTime, playerGuid.GetCounter());

            player->SaveInventoryAndGoldToDB(trans);

            CharacterDatabase.CommitTransaction(trans);

//            CharacterDatabase.PExecute("UPDATE character_kitt_playtime_rewards SET last_reward_level = {}, last_periodic_reward_time = {} WHERE player_guid = {}",
//                currentRewardLevel, lastPeriodicTime, playerGuid.GetCounter());
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

using namespace Trinity::ChatCommands;

class kitt_zplaytime_command_script : public CommandScript
{
public:
    kitt_zplaytime_command_script() : CommandScript("kitt_zplaytime_command_script") {}

    std::vector<ChatCommandBuilder> GetCommands() const override
    {
        return { { "zplaytime", HandlePlaytimeCommand, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No } };
    }

    static bool HandlePlaytimeCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player) return false;

        // --- PROTECTIE FLOOD (Cooldown 10 secunde) ---
        time_t currentTime = GameTime::GetGameTime();

        if (playtimeCooldownMap.count(player->GetGUID()) && currentTime < playtimeCooldownMap[player->GetGUID()])
        {
            uint32 waitTime = playtimeCooldownMap[player->GetGUID()] - currentTime;
            handler->PSendSysMessage("|cffff0000[Anti-Flood]|r Please wait %u seconds before using .zplaytime again.", waitTime);
            return true;
        }

        playtimeCooldownMap[player->GetGUID()] = currentTime + playtimeCooldownTime;
        // ----------------------------------------------

        uint32 totalPlaytimeMinutes = player->GetTotalPlayedTime() / 60;
        uint32 currentRewardLevel = 0;
        uint32 lastPeriodicTime = 0;
        uint32 days = totalPlaytimeMinutes / 1440;            // 1440 minute = 24 ore = 1 zi
        uint32 hours = (totalPlaytimeMinutes % 1440) / 60;   // Restul de minute transformat in ore
        uint32 minutes = totalPlaytimeMinutes % 60;          // Restul de minute

        QueryResult result = CharacterDatabase.PQuery("SELECT last_reward_level, last_periodic_reward_time FROM character_kitt_playtime_rewards WHERE player_guid = {}", player->GetGUID().GetCounter());
        if (result)
        {
            Field* fields = result->Fetch();
            currentRewardLevel = fields[0].GetUInt32();
            lastPeriodicTime = fields[1].GetUInt32();
        }

        std::ostringstream ss;
        ss << "|cff00ff00[Playtime Info]|r Total played time: |cffffffff";

        if (days > 0)
            ss << days << " days, ";
        if (hours > 0 || days > 0)
            ss << hours << " hours and ";

        ss << minutes << " minutes|r.";

        handler->PSendSysMessage("%s", ss.str().c_str());
        //handler->PSendSysMessage("|cff00ff00[Playtime Info]|r Total played time: |cffffffff%u days, %u hours and %u minutes|r.", days, hours, minutes);

        if (currentRewardLevel < rewardConfig.size())
        {
            handler->PSendSysMessage("|cff00ff00Next unique reward:|r");
            const auto& next = rewardConfig[currentRewardLevel];
            ItemTemplate const* it = sObjectMgr->GetItemTemplate(next.itemId);
            std::string itemName = it ? it->Name1 : "Item";

            if (totalPlaytimeMinutes >= next.requiredMinutes)
                handler->PSendSysMessage("|cff00ccff- %s:|r |cffffffffWill be awarded at next save!|r", itemName.c_str());
            else
            {
                uint32 diff = next.requiredMinutes - totalPlaytimeMinutes;
                handler->PSendSysMessage("|cff00ccff- %s:|r left |cffffffff%u hours and %u min|r %s",
                    itemName.c_str(), diff / 60, diff % 60, next.message.c_str());
            }
        }

        handler->PSendSysMessage("|cff00ff00Periodic rewards:|r");
        for (const auto& p : periodicConfig)
        {
            ItemTemplate const* it = sObjectMgr->GetItemTemplate(p.itemId);
            std::string itemName = it ? it->Name1 : "Item";

            if ((totalPlaytimeMinutes / p.intervalMinutes) > (lastPeriodicTime / p.intervalMinutes))
                handler->PSendSysMessage("|cff00ccff- %s:|r |cffffffffWill be awarded at next save!|r", itemName.c_str());
            else
            {
                uint32 nextT = ((lastPeriodicTime / p.intervalMinutes) + 1) * p.intervalMinutes;
                uint32 diff = (nextT > totalPlaytimeMinutes) ? (nextT - totalPlaytimeMinutes) : 0;
                handler->PSendSysMessage("|cff00ccff- %s:|r left |cffffffff%u hours and %u min|r.",
                    itemName.c_str(), diff / 60, diff % 60);
            }
        }
        return true;
    }
};


void AddSC_kitt_play_time_reward()
{
    new kitt_play_time_reward();
    new kitt_zplaytime_command_script();
}
