#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ReputationMgr.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellHistory.h"
#include "DBCStores.h"
#include "Item.h"
#include "Creature.h"
#include "Containers.h"
#include "WorldSession.h"



enum KittItemWinterTransform
{
    SPELL_KITT_ITEM_WINTER_WONDERVOLT_TRANSFORM_1 = 26157,
    SPELL_KITT_ITEM_WINTER_WONDERVOLT_TRANSFORM_2 = 26272,
    SPELL_KITT_ITEM_WINTER_WONDERVOLT_TRANSFORM_3 = 26273,
    SPELL_KITT_ITEM_WINTER_WONDERVOLT_TRANSFORM_4 = 26274
};

std::array<uint32, 4> const KittItemWinterTransformSpells =
{
    SPELL_KITT_ITEM_WINTER_WONDERVOLT_TRANSFORM_1,
    SPELL_KITT_ITEM_WINTER_WONDERVOLT_TRANSFORM_2,
    SPELL_KITT_ITEM_WINTER_WONDERVOLT_TRANSFORM_3,
    SPELL_KITT_ITEM_WINTER_WONDERVOLT_TRANSFORM_4
};

class kitt_item_spawn : public ItemScript
{
public:
    kitt_item_spawn() : ItemScript("kitt_item_spawn") {}



    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        uint32 KittItemID = 900900;
        if (item->GetEntry() == KittItemID)
        {
            // spell 53105 portal dummy
            uint32 spellUse = 53105; // spell for item use
            uint32 spellEfect = 25823; // efect de spawn 3 min cd
            uint32 npcId = 90014; // Inlocuieste cu ID-ul NPC-ului tau
            Milliseconds despawnTime = 180000ms; // despawn time
            float distance = 80.0f;  // distanta de a verifica daca exista deja

            // 1. Verificare daca NPC-ul exista deja pe o raza de ... de yarzi si este viu
            // FindNearestCreature cauta prima instanta a NPC-ului in raza specificata
            if (player->FindNearestCreature(npcId, distance, true))
            {
                player->GetSession()->SendNotification("Exista deja un NPC activ in apropiere (80yd)!");
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

            if (player->GetSpellHistory()->HasCooldown(spellUse, KittItemID, true))
            {
                uint32 remainingMs = player->GetSpellHistory()->GetRemainingCooldown(sSpellMgr->AssertSpellInfo(spellUse));
                uint32 totalSeconds = remainingMs / 1000;

                uint32 oreRamase = totalSeconds / 3600;
                uint32 minutes = (totalSeconds % 3600) / 60;
                uint32 seconds = totalSeconds % 60;

                std::string msg;
                if (oreRamase > 0)
                {
                    // Format: "2 minute si 15 secunde"
                    msg = "Acest item va fi gata in " + std::to_string(oreRamase) + (oreRamase == 1 ? " ora si " : " ore si ") + std::to_string(minutes) + (minutes == 1 ? " minut si " : " minute si ") + std::to_string(seconds) + " secunde.";
                }
                else if (minutes > 0)
                {
                    // Format: "2 minute si 15 secunde"
                    msg = "Acest item va fi gata in " + std::to_string(minutes) + (minutes == 1 ? " minut si " : " minute si ") + std::to_string(seconds) + " secunde.";
                }
                else
                {
                    // Format: "45 secunde"
                    msg = "Acest item va fi gata in " + std::to_string(seconds) + " secunde.";
                }

                //std::string msg = "Acest obiect nu este gata inca! Timp ramas: " + std::to_string(remainingSec) + " secunde.";
                player->GetSession()->SendNotification(msg.c_str());

                //player->GetSession()->SendNotification("Acest obiect nu este gata inca!");
                return true;
            }

            if (Creature* csummon = player->SummonCreature(npcId, *player, TEMPSUMMON_TIMED_DESPAWN, despawnTime))
            {
                csummon->CastSpell(csummon, spellEfect, true); // true = instant cast, false = normal cast
                uint32 randomSpell = Trinity::Containers::SelectRandomContainerElement(KittItemWinterTransformSpells);
                csummon->CastSpell(csummon, randomSpell, true);
                player->GetSession()->SendNotification("NPC invocat pentru 3 minute!");
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
