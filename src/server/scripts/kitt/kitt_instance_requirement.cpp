#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Config.h"


namespace
{
    static uint32 sKittIccInstanceRequirement = 0;
    static uint32 sKittIccMinItemLevel = 0;
    static uint32 sKittRSInstanceRequirement = 0;
    static uint8 IccReqEquippedCount = 11;
}



class kitt_instance_requirement : public PlayerScript
{
public:
    kitt_instance_requirement() : PlayerScript("kitt_instance_requirement") {}

    uint32 GetAverageItemLevel(Player* player)
    {
        uint32 totalILvl = 0;
        uint8 count = 0;

        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (i == EQUIPMENT_SLOT_TABARD || i == EQUIPMENT_SLOT_BODY)
                continue;

            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                totalILvl += item->GetTemplate()->ItemLevel;
                count++;
            }
        }

        if (count < IccReqEquippedCount)
            return 0;

        return count > 0 ? (totalILvl / count) : 0;
    }

    void OnMapChanged(Player* player) override
    {
        Map* map = player->GetMap();

        // Icecrown Citadel map
        if (map->GetId() == 631)
        {
            if (sKittIccInstanceRequirement == 0)
                return;

            if (player->IsGameMaster())
                return;

            uint8 diff = map->GetDifficulty();
            bool hasAccess = true;
            std::string msg = "";

            switch (diff)
            {
            case RAID_DIFFICULTY_10MAN_NORMAL:
              {
                if (!player->HasAchieved(3917) && !player->HasAchieved(3916))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must complete BOTH Trial of the Crusader 10 and 25-player to enter Icecrown Citadel!";
                }
                else if (!player->HasAchieved(3917))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You completed ToC 25, but you are still missing Trial of the Crusader 10-player!";
                }
                else if (!player->HasAchieved(3916))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You completed ToC 10, but you are still missing Trial of the Crusader 25-player!";
                }
                break;
              }

            case RAID_DIFFICULTY_10MAN_HEROIC:
                if (!player->HasAchieved(4530))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat The Lich King on 10-player Normal to enter Heroic mode!";
                }
                break;

            case RAID_DIFFICULTY_25MAN_NORMAL:
                if (!player->HasAchieved(4530))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat The Lich King on 10-player Normal to enter 25-player Normal!";
                }
                break;

            case RAID_DIFFICULTY_25MAN_HEROIC:
              {
                if (!player->HasAchieved(4597) && !player->HasAchieved(4583))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat BOTH The Lich King 25 Normal and 10 Heroic player to enter 25-player Heroic mode!";
                }
                else if (!player->HasAchieved(4597))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat The Lich King on 25-player Normal to enter Heroic mode!";
                }
                else if (!player->HasAchieved(4583))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat The Lich King on 10-player Heroic to enter Heroic mode!";

                }
                break;
              }

            default:
                break;
            }

            if (hasAccess && sKittIccMinItemLevel > 0)
            {
                uint32 avgILvl = GetAverageItemLevel(player);
                if (avgILvl < sKittIccMinItemLevel)
                {
                    hasAccess = false;
                    if (avgILvl == 0)
                        msg = "|cffff0000Access Denied:|r You must have at least " + std::to_string(IccReqEquippedCount) + " items equipped to enter!";
                    else
                        msg = "|cffff0000Access Denied:|r Your Average Item Level (" + std::to_string(avgILvl) + ") is below the required " + std::to_string(sKittIccMinItemLevel) + "!";
                }
            }


            if (!hasAccess)
            {
                ObjectGuid playerGuid = player->GetGUID();
                std::string messageToSend = msg;

                player->m_Events.AddEventAtOffset([playerGuid]()
                    {
                        Player* p = ObjectAccessor::FindPlayer(playerGuid);
                        if (p && p->GetSession())
                        {
                            p->TeleportTo(571, 5865.62f, 2107.66f, 635.98f, 3.55f);
                        }
                    }, 5s);

                player->m_Events.AddEventAtOffset([playerGuid, messageToSend]()
                    {
                        Player* p = ObjectAccessor::FindPlayer(playerGuid);
                        if (p && p->GetSession())
                        {
                            ChatHandler(p->GetSession()).PSendSysMessage(messageToSend.c_str());
                        }
                    }, 10s);
            }
        }

        // Ruby Sanctum map
        if (map->GetId() == 724)
        {
            if (sKittRSInstanceRequirement == 0)
                return;

            if (player->IsGameMaster())
                return;

            uint8 diff = map->GetDifficulty();
            bool hasAccess = true;
            std::string msg = "";

            switch (diff)
            {
            case RAID_DIFFICULTY_10MAN_NORMAL:
                if (!player->HasAchieved(4530))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat The Lich King on 10-player Normal to enter Ruby Sanctum!";
                }
                break;

            case RAID_DIFFICULTY_25MAN_NORMAL:
                if (!player->HasAchieved(4597))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat The Lich King on 25-player Normal to enter Ruby Sanctum 25!";
                }
                break;

            case RAID_DIFFICULTY_10MAN_HEROIC:
                if (!player->HasAchieved(4817))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat Halion on 10-player Normal to enter Heroic mode!";
                }
                break;

            case RAID_DIFFICULTY_25MAN_HEROIC:
                if (!player->HasAchieved(4815))
                {
                    hasAccess = false;
                    msg = "|cffff0000Access Denied:|r You must defeat Halion on 25-player Normal to enter Heroic mode!";
                }
                break;
            }

            if (!hasAccess)
            {
                ObjectGuid playerGuid = player->GetGUID();
                std::string messageToSend = msg;

                player->m_Events.AddEventAtOffset([playerGuid]()
                    {
                        Player* p = ObjectAccessor::FindPlayer(playerGuid);
                        if (p && p->GetSession())
                        {
                            p->TeleportTo(571, 3585.71f, 218.125f, -120.054f, 5.28f);
                        }
                    }, 5s);

                player->m_Events.AddEventAtOffset([playerGuid, messageToSend]()
                    {
                        Player* p = ObjectAccessor::FindPlayer(playerGuid);
                        if (p && p->GetSession())
                        {
                            ChatHandler(p->GetSession()).PSendSysMessage(messageToSend.c_str());
                        }
                    }, 10s);
            }
        }
    }
};

class kitt_instance_requirement_config : public WorldScript
{
public:
    kitt_instance_requirement_config() : WorldScript("kitt_instance_requirement_config") {}

    void OnConfigLoad(bool /*reload*/) override
    {
        sKittIccInstanceRequirement = sConfigMgr->GetIntDefault("Kitt.ICC.Instance.Requirement", 0);
        sKittIccMinItemLevel = sConfigMgr->GetIntDefault("Kitt.ICC.Instance.MinItemLevel", 0);
        sKittRSInstanceRequirement = sConfigMgr->GetIntDefault("Kitt.RS.Instance.Requirement", 0);
    }
};


void AddSC_kitt_instance_requirement()
{
    new kitt_instance_requirement_config();
    new kitt_instance_requirement();
}
