#include "ScriptMgr.h"
#include "Chat.h"
#include "LootMgr.h"

namespace
{
    const uint32 MAP_BWL = 469;
    const uint32 MAP_GM_ISLE = 1;
}


class kitt_custom_loot_mapid : public PlayerScript
{
public:
    kitt_custom_loot_mapid() : PlayerScript("kitt_custom_loot_mapid") {}

    void AddKittCustomItem(Loot* loot, uint32 itemID, uint8 count = 1, float dropChance = 100.0f, bool ffa = false)
    {
        uint32 reference = 0;
        bool needsQuest = false;
        uint16 lootMode = 1;
        uint8 groupId = 0;

        LootStoreItem newItem(itemID, reference, dropChance, needsQuest, lootMode, groupId, count, count);
        loot->AddItem(newItem);

        if (ffa && !loot->items.empty())
        {
            LootItem& lastItem = loot->items.back();
            lastItem.freeforall = true;
        }
    }


    void OnAfterLootFill(Player* player, Loot* loot) override
    {
        if (!player || !player->GetSession())
            return;

        uint32 KittMapID = player->GetMapId();

        switch (KittMapID)
        {
        case MAP_BWL:
        {
            uint32 entry = loot->sourceGuid.GetEntry();

            switch (entry)
            {
            case 90014:
                //loot->items.clear(); // sterge loot vechi
                AddKittCustomItem(loot, 49426, 10, 100.0f, true); // Embleme FFA
                AddKittCustomItem(loot, 31193, 1, 100.0f, false);
                AddKittCustomItem(loot, 45085, 1, 50.0f, false); // 1 bucata, 50% sansa
                loot->gold += 500000; // adauga 50g la loot
                break;

            case 90015: // Alt mob (ex: alt lup)
                //AddKittCustomItem(loot, 49426, 2, 50.0f, true); // ?ans? 50% la 2 embleme
                break;
            }
            break;
        }

        case MAP_GM_ISLE:
        {
            uint32 entry = loot->sourceGuid.GetEntry();

            switch (entry)
            {
            case 90014:
                //loot->items.clear(); // sterge loot vechi
                AddKittCustomItem(loot, 49426, 10, 100.0f, true); // Embleme FFA
                AddKittCustomItem(loot, 31193, 1, 100.0f, false);
                AddKittCustomItem(loot, 45085, 1, 50.0f, false); // 1 bucata, 50% sansa
                loot->gold += 500000; // adauga 50g la loot

                ChatHandler(player->GetSession()).PSendSysMessage("Test Modul loot generat");
                break;

            case 90015: // Alt mob (ex: alt lup)
                //AddKittCustomItem(loot, 49426, 2, 50.0f, true); // ?ans? 50% la 2 embleme
                break;
            }
            break;
        }
        }
    }
};


/*void AddSC_kitt_custom_loot_mapid()
{
    new kitt_custom_loot_mapid();
}
*/
