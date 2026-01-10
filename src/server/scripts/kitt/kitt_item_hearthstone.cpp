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
#include "ScriptedGossip.h"
#include "GossipDef.h"
#include "Vehicle.h"
#include "VehicleDefines.h"
#include "TemporarySummon.h"




class kitt_item_hearthstone : public ItemScript
{
public:
    kitt_item_hearthstone() : ItemScript("kitt_item_hearthstone") {}

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        uint32 KittItemID = 6948;
        if (item->GetEntry() == KittItemID)
        {
            uint32 spellUse = 8690; // hearthstone spell


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
                player->GetSession()->SendNotification("%s", msg.c_str());

                //player->GetSession()->SendNotification("Acest obiect nu este gata inca!");
                return true;
            }

            TempSummon* ItemTriggerCreature = player->SummonCreature(90015, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 120s);

            if (ItemTriggerCreature)
            {
                // Seteaz? flag-urile pentru a fi invizibil/neinteractiv vizual
                //ItemTriggerCreature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1 | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_PACIFIED);
                ItemTriggerCreature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                // se vede umbra cu ascundere corp
                // ItemTriggerCreature->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_HIDE_BODY);
                // model invizibil
                //ItemTriggerCreature->SetDisplayId(11686);
                //ItemTriggerCreature->GetMotionMaster()->MoveFollow(player, 0.0f, player->GetOrientation());
                //ItemTriggerCreature->GetMotionMaster()->MoveFollow(player, -0.8f, player->GetOrientation() -0.8f);
                //ItemTriggerCreature->GetVehicleKit()->Install();

                player->EnterVehicle(ItemTriggerCreature, 0);

                //ItemTriggerCreature->SetCanFly(player->CanFly());
                //ItemTriggerCreature->SetDisableGravity(true); // Previne r?m?nerea ?n urm? ?n aer

                //ClearGossipMenuFor(player);

                // Folosim un MenuID (ex: 999) ?i ActionID (1, 2)
                //AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "Vindecare", 999, 1);
                //AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Teleport Mall", 999, 2);

                // FOARTE IMPORTANT: Trimitem GUID-ul NPC-ului spawnat, NU al juc?torului
                // Astfel, c?nd dai click pe op?iune, serverul va g?si NPC-ul l?ng? tine ?i va permite execu?ia
                //SendGossipMenuFor(player, 1, ItemTriggerCreature->GetGUID());
                //SendGossipMenuFor(player, 1, item->GetGUID());
            }
            return true;
        }
        return true;
    }
};


class kitt_item_hearthstone_npc : public CreatureScript
{
public:
    kitt_item_hearthstone_npc() : CreatureScript("kitt_item_hearthstone_npc") {}

    struct kitt_item_hearthstone_npcAI : public ScriptedAI
    {
        kitt_item_hearthstone_npcAI(Creature* creature) : ScriptedAI(creature) {}

        void IsSummonedBy(WorldObject* /*summoner*/) override
        {
            // Verific?m dac? vehiculul are kit-ul necesar
            if (me->GetVehicleKit())
            {
                // Instal?m NPC-ul 90014 pe scaunul 1
                // Argumente: entry, seatId, minion, summonType, summonTime
                me->GetVehicleKit()->InstallAccessory(90014, 1, true, TEMPSUMMON_MANUAL_DESPAWN, 0);
            }
        }
        void PassengerBoarded(Unit* passenger, int8 seatId, bool apply) override
        {
            // C?nd juc?torul urc? pe scaunul 0
            if (apply && passenger->ToPlayer() && seatId == 0)
            {
                // C?ut?m pasagerul de pe scaunul 1 (NPC-ul de gossip)
                if (Unit* gossipNPC = me->GetVehicleKit()->GetPassenger(1))
                {
                    // Deschidem automat meniul de gossip al pasagerului pentru juc?tor
                    // Acesta este "trick-ul" care face butoanele s? func?ioneze
                    passenger->ToPlayer()->PrepareGossipMenu(gossipNPC, 1, true);
                    passenger->ToPlayer()->SendPreparedGossip(gossipNPC);
                }
            }
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {

            if (player->GetVehicle()) {
                player->ExitVehicle();
            }


            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            uint32 const sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);


            if (sender == 999)
            {
                switch (action)
                {
                case 1: // Vindecare
                    player->SetHealth(player->GetMaxHealth());
                    player->GetSession()->SendNotification("Vindecat!");
                    break;
                case 2: // Teleport
                    player->TeleportTo(571, 5804.15f, 624.77f, 647.76f, 1.64f);
                    break;
                }
            }


            CloseGossipMenuFor(player);
            me->DespawnOrUnsummon(); // ?l ?tergem imediat
            return true;
        }
    };
    CreatureAI* GetAI(Creature* creature) const override
    {
        return new kitt_item_hearthstone_npcAI(creature);
    }
};



void AddSC_kitt_item_hearthstone()
{
    //new kitt_item_hearthstone();
    //new kitt_item_hearthstone_npc();
}
