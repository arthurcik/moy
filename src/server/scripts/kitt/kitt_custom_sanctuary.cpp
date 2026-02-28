#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "DBCStores.h"


class kitt_custom_sanctuary : public PlayerScript
{
public:
    kitt_custom_sanctuary() : PlayerScript("kitt_custom_sanctuary") {}

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        if (!player)
            return;

        if (!player->GetSession())
            return;

        if (player->GetTypeId() != TYPEID_PLAYER)
            return;

        if (!player->IsInWorld())
            return;

        // old methode in spell_area spell 39397, 63726
        bool shouldBeSanctuary = (newZone == 440 && newArea == 2317);

        if (shouldBeSanctuary)
        {
            if (!player->HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_BYTE2_FLAG_SANCTUARY))
            {
                player->SetPvpFlag(UNIT_BYTE2_FLAG_SANCTUARY);
                player->pvpInfo.IsInNoPvPArea = true;

                if (!player->duel && player->GetCombatManager().HasPvPCombat())
                    player->CombatStopWithPets();

                //ChatHandler(player->GetSession()).SendSysMessage("|cff00ff00[Sanctuary]|rAi intrat intr-o zona protejata.");
            }
        }
        else
        {
            AreaTableEntry const* area = sAreaTableStore.LookupEntry(newArea);
            bool isNativeSanctuary = (area && area->IsSanctuary());

            // Daca are flag-ul pus de noi, dar zona noua NU este sanctuary nativ, il scoatem
            if (player->HasPvpFlag(UNIT_BYTE2_FLAG_SANCTUARY) && !isNativeSanctuary)
            {
                player->RemovePvpFlag(UNIT_BYTE2_FLAG_SANCTUARY);
                player->pvpInfo.IsInNoPvPArea = false;

                //ChatHandler(player->GetSession()).SendSysMessage("Test: ai parasit zona custom");
            }
        }
    }
};

void AddSC_kitt_custom_sanctuary()
{
    new kitt_custom_sanctuary();
}
