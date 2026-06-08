#include "ScriptMgr.h"
#include "Player.h"
#include "Group.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "WorldSession.h"
#include "Item.h"
#include "Chat.h"
#include "Mail.h"
#include "CharacterDatabase.h"
#include "Map.h"
//#include <unordered_set>

namespace
{
    // leader list items
    const std::unordered_set<uint32> LEAD_ITEMS_LIST = { 900800 };

    // member list items
    const std::unordered_set<uint32> MEMBER_ITEMS_LIST = { 900801 };

    constexpr uint32 MIN_REAL_PLAYERS_REQUIRED = 5;

    bool IsLeaderItem(uint32 itemId)
    {
        return LEAD_ITEMS_LIST.find(itemId) != LEAD_ITEMS_LIST.end();
    }

    bool IsMemberItem(uint32 itemId)
    {
        return MEMBER_ITEMS_LIST.find(itemId) != MEMBER_ITEMS_LIST.end();
    }

    bool ValidateLeaderConditions(Player* player)
    {
        if (!player)
            return false;

        Map* map = player->GetMap();
        if (!map || !map->IsRaid())
            return false;

        Group* group = player->GetGroup();
        if (!group || !group->isRaidGroup())
            return false;

        if (group->GetLeaderGUID() != player->GetGUID())
            return false;

        uint32 realPlayersCount = 0;
        Group::MemberSlotList const& memberList = group->GetMemberSlots();

        for (auto const& slot : memberList)
        {
            Player* member = ObjectAccessor::FindPlayer(slot.guid);
            if (member && member->GetSession() && !member->IsNPCBot())
            {
                realPlayersCount++;
            }
        }

        return (realPlayersCount >= MIN_REAL_PLAYERS_REQUIRED);
    }

    bool ValidateMemberConditions(Player* member)
    {
        if (!member)
            return false;

        Map* map = member->GetMap();
        if (!map || !map->IsRaid())
            return false;

        Group* group = member->GetGroup();
        if (!group || !group->isRaidGroup())
            return false;

        Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
        if (!leader)
            return false;

        return ValidateLeaderConditions(leader);
    }

    void MoveItemToMail(Player* player, Item* item)
    {
        if (!player || !item)
            return;

        uint8 bag = item->GetBagSlot();
        uint8 slot = item->GetSlot();
        uint32 itemGuidLow = item->GetGUID().GetCounter();

        player->MoveItemFromInventory(bag, slot, true);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_ITEM);
        stmt->setUInt32(0, itemGuidLow);
        trans->Append(stmt);

        item->SetOwnerGUID(ObjectGuid::Empty);
        item->SetState(ITEM_NEW, player);

        MailDraft draft("Raid System: Item Moved", "The item has been moved to your mailbox.\nYou entered a restricted zone while it was equipped.");
        draft.AddItem(item);

        draft.SendMailTo(trans, player, MailSender(MAIL_CREATURE, 34337));
        CharacterDatabase.CommitTransaction(trans);

        item->SetOwnerGUID(player->GetGUID());
        item->SetState(ITEM_UNCHANGED, player);

        ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Raid System]:|r The item |cffffffff[%s]|r has been moved to your mailbox (inventory full).", item->GetTemplate()->Name1.c_str());
    }

    void ProcessUnequipForItem(Player* player, Item* item)
    {
        if (!player || !item)
            return;

        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);

        if (msg == EQUIP_ERR_OK)
        {
            uint8 srcBag = item->GetBagSlot();
            uint8 srcSlot = item->GetSlot();

            player->RemoveItem(srcBag, srcSlot, true);
            player->StoreItem(dest, item, true);

            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Raid System]:|r The item has been moved to your inventory (requirements not met)!");
        }
        else
        {
            MoveItemToMail(player, item);
        }
    }

    void CheckAndProcessUnequip(Player* player)
    {
        if (!player)
            return;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item)
            {
                if (IsLeaderItem(item->GetEntry()))
                {
                    ProcessUnequipForItem(player, item);
                }
                else if (IsMemberItem(item->GetEntry()))
                {
                    ProcessUnequipForItem(player, item);
                }
            }
        }
    }
}

class UnequipCheckEvent : public BasicEvent
{
public:
    UnequipCheckEvent(ObjectGuid leaderGuid) : _leaderGuid(leaderGuid) {}

    bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
    {
        Player* player = ObjectAccessor::FindPlayer(_leaderGuid);
        if (!player)
            return true;

        Group* group = player->GetGroup();

        if (!group)
        {
            CheckAndProcessUnequip(player);
            return true;
        }

        ObjectGuid currentLeaderGuid = group->GetLeaderGUID();
        Player* currentLeader = ObjectAccessor::FindPlayer(currentLeaderGuid);

        if (player->GetGUID() != currentLeaderGuid)
        {
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (item && IsLeaderItem(item->GetEntry()))
                {
                    ProcessUnequipForItem(player, item);
                }
            }
        }

        if (currentLeader && ValidateLeaderConditions(currentLeader))
        {
            //TC_LOG_ERROR("kitt", "Raidul este valid, membrii isi pastreaza armele de membru.");
        }
        else
        {
            CheckAndProcessUnequip(player);
        }

        return true;
    }



private:
    ObjectGuid _leaderGuid;
};

class AddSC_kitt_raid_leader_items_player : public PlayerScript
{
public:
    AddSC_kitt_raid_leader_items_player() : PlayerScript("AddSC_kitt_raid_leader_items_player") {}

    InventoryResult OnCanEquipItem(Player* player, uint8 /*slot*/, uint16& /*dest*/, Item* item, bool /*swap*/, bool /*not_loading*/) override
    {
        if (!player || !item)
            return EQUIP_ERR_OK;

        if (IsLeaderItem(item->GetEntry()))
        {
            if (!ValidateLeaderConditions(player))
            {
                if (player->GetSession())
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Raid System]:|r You cannot equip this item! You must be inside an instance, as a Raid Leader, with at least 5 real players inRaid.");

                return EQUIP_ERR_ITEM_LOCKED;
            }
        }
        else if (IsMemberItem(item->GetEntry()))
        {
            if (!ValidateMemberConditions(player))
            {
                if (player->GetSession())
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Raid System]:|r You cannot equip this item! You must be inside an instance and your raid leader must fulfill the conditions.");

                return EQUIP_ERR_ITEM_LOCKED;
            }
        }

        return EQUIP_ERR_OK;
    }

    void OnEquip(Player* player, Item* item, uint16 /*slot*/, bool /*update*/) override
    {
        if (player && item && IsLeaderItem(item->GetEntry()) && player->GetSession())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Raid System]:|r A special item has been equipped! Lead your raid to glory.");
        }
    }

    void OnMapChanged(Player* player) override
    {
        if (player && !ValidateLeaderConditions(player))
        {
            CheckAndProcessUnequip(player);
        }
    }
};

class AddSC_kitt_raid_leader_items_group : public GroupScript
{
public:
    AddSC_kitt_raid_leader_items_group() : GroupScript("AddSC_kitt_raid_leader_items_group") {}

    void OnRemoveMember(Group* group, ObjectGuid /*guid*/, RemoveMethod /*method*/, ObjectGuid /*kicker*/, char const* /*reason*/) override
    {
        if (!group) return;

        Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
        if (leader)
        {
            leader->m_Events.AddEvent(new UnequipCheckEvent(leader->GetGUID()), leader->m_Events.CalculateTime(2s));
        }
    }

    void OnChangeLeader(Group* /*group*/, ObjectGuid newLeaderGuid, ObjectGuid oldLeaderGuid) override
    {
        if (Player* oldLeader = ObjectAccessor::FindPlayer(oldLeaderGuid))
        {
            oldLeader->m_Events.AddEvent(new UnequipCheckEvent(oldLeaderGuid), oldLeader->m_Events.CalculateTime(400ms));
        }

        if (Player* newLeader = ObjectAccessor::FindPlayer(newLeaderGuid))
        {
            newLeader->m_Events.AddEvent(new UnequipCheckEvent(newLeaderGuid), newLeader->m_Events.CalculateTime(600ms));
        }
    }

    void OnDisband(Group* group) override
    {
        if (!group) return;

        if (Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID()))
        {
            leader->m_Events.AddEvent(new UnequipCheckEvent(leader->GetGUID()), leader->m_Events.CalculateTime(2s));
        }
    }
};

void AddSC_kitt_raid_leader_items()
{
    new AddSC_kitt_raid_leader_items_player();
    new AddSC_kitt_raid_leader_items_group();
}
