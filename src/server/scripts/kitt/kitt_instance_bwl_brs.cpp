#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Group.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Map.h"
#include "ChatCommand.h"
#include "WorldSession.h"
#include <set>
#include <map>
#include "DBCStores.h"
#include "SpellAuraEffects.h"
#include "GameTime.h"
#include "Log.h"

#include "Containers.h"
#include "CreatureTextMgr.h"
#include "GossipDef.h"
#include "ScriptedGossip.h"
#include "World.h"
#include "LootMgr.h"


namespace
{
    const uint32 MAP_BWL_BRS = 229;
}

class kitt_instance_bwl_brs : public PlayerScript
{
public:
    kitt_instance_bwl_brs() : PlayerScript("kitt_instance_bwl_brs") {}

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

    void KittAddLootElite(Player* /*player*/, Loot* loot) /*override*/
    {
        //ObjectGuid sourceGuid = loot->sourceGuid;

        // loot comun
        //loot->items.clear(); // sterge loot vechi
        //AddKittCustomItem(loot, 43102, 1, 1.0f, false); // frozen orb

        loot->gold += 100000; // adauga 50g la loot
        //player->SendLoot(sourceGuid, LOOT_CORPSE);
    }

    void OnAfterLootFill(Player* player, Loot* loot) override
    {
        if (!player || !player->GetSession() || !loot)
            return;

        Creature* creature = player->GetMap()->GetCreature(loot->sourceGuid);
        if (!creature)
            return;

        Map* map = creature->GetMap();
        if (map->GetId() != MAP_BWL_BRS)
            return;

        if (creature->isElite())
        {
            KittAddLootElite(player, loot);
        }
    }
};




// scalare
// --- SECTIUNEA 1: MODIFICARE NIVEL PRIN HOOK-UL TAU GLOBAL ---
class kitt_instance_bwl_brs_CreatureScaler : public AllCreatureScript
{
public:
    kitt_instance_bwl_brs_CreatureScaler() : AllCreatureScript("kitt_instance_bwl_brs_CreatureScaler") {}

    void OnAllCreatureUpdate(Creature* creature, uint32 /*diff*/) override
    {
        if (!creature || !creature->IsAlive())
            return;

        Map* map = creature->GetMap();
        if (!map || map->GetId() != MAP_BWL_BRS)
            return;

        if (creature->IsNPCBotOrPet() || creature->IsPet() || creature->IsTotem() || creature->IsVehicle())
            return;

        uint8 nivelCurent = creature->GetLevel();

        if (nivelCurent >= 50 && nivelCurent <= 63)
        {
            ScaleazaCreaturaLaNivel80(creature, nivelCurent);
        }
    }

    void Creature_SelectLevel(const CreatureTemplate* /*cinfo*/, Creature* creature) override
    {
        if (!creature || !creature->IsAlive())
            return;

        Map* map = creature->GetMap();
        if (!map || map->GetId() != MAP_BWL_BRS)
            return;

        if (creature->IsNPCBotOrPet() || creature->IsPet() || creature->IsTotem() || creature->IsVehicle())
            return;

        uint8 nivelCurent = creature->GetLevel();

        if (nivelCurent >= 50 && nivelCurent <= 63)
        {
            ScaleazaCreaturaLaNivel80(creature, nivelCurent);
        }
    }

private:
    void ScaleazaCreaturaLaNivel80(Creature* creature, uint8 nivelCurent)
    {
        if (!creature || !creature->IsAlive())
            return;

        Map* map = creature->GetMap();
        if (!map || map->GetId() != MAP_BWL_BRS)
            return;

        if (creature->IsNPCBotOrPet() || creature->IsPet() || creature->IsTotem() || creature->IsVehicle())
            return;

        //uint8 nivelCurent = creature->GetLevel();

        // Evitam loop-ul infinit verificand daca monstrul este in intervalul de Vanilla
        if (nivelCurent >= 50 && nivelCurent <= 63)
        {
            uint8 noulNivel = nivelCurent + 20; // Transformam 60->80, 63->83

            creature->SetLevel(noulNivel);
            creature->UpdateLevelDependantStats();

            // Multiplicator bonus pentru HP-ul monstrilor din instanta
            float multiplicatorHP = 10.0f;
            uint32 hpNou = creature->GetMaxHealth() * multiplicatorHP;
            creature->SetMaxHealth(hpNou);
            creature->SetHealth(hpNou);
            creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, (float)hpNou);
            creature->SetCustomAggroDistances(20.0f, 20.0f);

            float multiplicatorArmor = 3.5f; // 4.5
            uint32 armorNoua = creature->GetArmor() * multiplicatorArmor;
            creature->SetStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, (float)armorNoua);

            float multiplicatorAP = 3.5f; // 4.5

            uint32 apMeleeNou = creature->GetFlatModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE) * multiplicatorAP;
            uint32 apRangedNou = creature->GetFlatModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE) * multiplicatorAP;

            // Aplicam noile valori in mod corect pentru TrinityCore
            creature->SetStatFlatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, (float)apMeleeNou);
            creature->SetStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, (float)apRangedNou);
        }
    }
};

// --- SECTIUNEA 2: HOOKS DE DAMAGE SI HEAL (UnitScript) ---
class kitt_instance_bwl_brs_DamageScaler : public UnitScript
{
public:
    kitt_instance_bwl_brs_DamageScaler() : UnitScript("kitt_instance_bwl_brs_DamageScaler") {}

    /*void OnHeal(Unit* healer, Unit* reciever, uint32& gain) override
    {
        gain = AplicaMultiplicatorDamage(reciever, healer, gain);
    }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        damage = AplicaMultiplicatorDamage(victim, attacker, damage);
    }*/

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage) override
    {
        damage = AplicaMultiplicatorDamage(target, attacker, damage);
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        damage = AplicaMultiplicatorDamage(target, attacker, damage);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage) override
    {
        uint32 damageConversie = damage > 0 ? (uint32)damage : 0;
        damage = (int32)AplicaMultiplicatorDamage(target, attacker, damageConversie);
    }

private:
    uint32 AplicaMultiplicatorDamage(Unit* /*target*/, Unit* attacker, uint32 damage)
    {
        if (!attacker || attacker->GetTypeId() == TYPEID_PLAYER || !attacker->IsInWorld())
            return damage;

        Map* map = attacker->GetMap();
        if (!map || map->GetId() != MAP_BWL_BRS)
            return damage;

        // Daca atacatorul este un NPCBot sau un pet de bot, damage-ul lui NU trebuie inmultit,
        // deoarece boti au deja stats si spell-uri native de nivel 80.
        if (attacker && attacker->IsNPCBotOrPet())
            return damage;

        if ((attacker->IsHunterPet() || attacker->IsPet() || attacker->IsSummon()) && attacker->IsControlledByPlayer())
            return damage;

        // Multiplicator pentru damage-ul monstrilor din instanta
        float damageMultiplier = 4.5f;

        return uint32(damage * damageMultiplier);
    }
};

/*
void AddSC_kitt_instance_bwl_brs()
{
    new kitt_instance_bwl_brs();
    new kitt_instance_bwl_brs_CreatureScaler();
    new kitt_instance_bwl_brs_DamageScaler();
}
*/
