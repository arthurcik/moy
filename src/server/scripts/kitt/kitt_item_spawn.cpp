#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ReputationMgr.h"
#include "DBCStores.h"
#include "Item.h"
#include "Creature.h"
#include "WorldSession.h"


class kitt_item_spawn : public ItemScript
{
public:
    kitt_item_spawn() : ItemScript("kitt_item_spawn") {}

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (item->GetEntry() == 900900)
        {
            // spell 53105 portal dummy
            uint32 npcId = 90014; // Inlocuieste cu ID-ul NPC-ului tau
            Milliseconds despawnTime = 120000ms; // despawn time
            float distance = 50.0f;  // distanta de a verifica daca exista deja

            // 1. Verificare daca NPC-ul exista deja pe o raza de ... de yarzi si este viu
            // FindNearestCreature cauta prima instanta a NPC-ului in raza specificata
            if (player->FindNearestCreature(npcId, distance, true))
            {
                player->GetSession()->SendNotification("Exista deja un NPC activ in apropiere (50yd)!");
                player->SendEquipError(EQUIP_ERR_CLIENT_LOCKED_OUT, item, nullptr);
                return true;
            }

            // Verificare fantoma
            if (player->isDead())
            {
                player->GetSession()->SendNotification("Nu poti folosi acest item daca esti fantoma");
                // error
                player->SendEquipError(EQUIP_ERR_CLIENT_LOCKED_OUT, item, nullptr);
                return true; // Returnam true pentru a opri executia si a nu consuma item-ul
            }
            // 1. Verificare Combat
            if (player->IsInCombat())
            {
                player->GetSession()->SendNotification("Nu poti folosi acest item in timpul luptei!");
                // error
                player->SendEquipError(EQUIP_ERR_CLIENT_LOCKED_OUT, item, nullptr);
                return true;
            }

            // 2. Verificare Zbor (Flying)
            if (player->IsFlying() || player->IsInFlight())
            {
                player->GetSession()->SendNotification("Nu poti folosi acest item in timp ce zbori!");
                // error
                player->SendEquipError(EQUIP_ERR_CLIENT_LOCKED_OUT, item, nullptr);
                return true;
            }

            // 3. Verificare Mount (Calare)
            if (player->IsMounted())
            {
                player->GetSession()->SendNotification("Trebuie sa descaleci inainte de a folosi acest item!");
                // error
                player->SendEquipError(EQUIP_ERR_CLIENT_LOCKED_OUT, item, nullptr);
                return true;
            }

            // 4. Verificare sub/pe apa (Optional, dar recomandat pentru spawn de NPC)
            if (player->IsInWater() || player->IsUnderWater())
            {
                player->GetSession()->SendNotification("Nu poti folosi acest item in/pe apa!");
                // error
                player->SendEquipError(EQUIP_ERR_CLIENT_LOCKED_OUT, item, nullptr);
                return true;
            }

            if (player->SummonCreature(npcId, *player, TEMPSUMMON_TIMED_DESPAWN, despawnTime))
            {
                player->GetSession()->SendNotification("NPC invocat pentru 2 minute!");
                return false; //Returnam false pentru a permite animatia de cast daca exista
            }
        }
        return false;
    }
};

void AddSC_kitt_item_spawn()
{
    new kitt_item_spawn();
}
