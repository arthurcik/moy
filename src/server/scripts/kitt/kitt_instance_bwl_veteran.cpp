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

using namespace Trinity::ChatCommands;

namespace
{
    const uint32 MAP_BWL = 469;
    const uint32 VISUAL_AURA_MARKER = 38164;

    static std::map<ObjectGuid, bool> HeroicSelection;
    static std::set<uint32> ActiveHeroicInstances;
    static std::map<uint32, uint32> InstanceScanTimers;


    static const std::set<uint32> VeteranEligibleItems = {
        16863, 16867, 16861, 16866, // Exemplu: Set Nightslayer (Rogue T1)
        16905, 16906, 16907, 16908, // Exemplu: Set Bloodfang (Rogue T2)
        19019, 19364, 17182, 19337          // Thunderfury, Ashkandi, etc.
    };

    enum VeteranCosts
    {
        ITEM_UPGRADE_TOKEN = 212202,
        GOLD_COST = 500 * 10000  // 500 Gold
    };
}

class kitt_bwl_heroic_core : public PlayerScript
{
public:
    kitt_bwl_heroic_core() : PlayerScript("kitt_bwl_heroic_core") {}

    void OnMapChanged(Player* player) override
    {
        uint32 mapId = player->GetMapId();
        if (mapId != MAP_BWL)
            return;

        ObjectGuid guid = player->GetGUID();
        player->m_Events.AddEventAtOffset([guid, this]()
            {
                Player* player = ObjectAccessor::FindPlayer(guid);

                if (!player || !player->IsInWorld() || player->GetMapId() != MAP_BWL)
                    return;

                InstanceMap* instance = (InstanceMap*)player->GetMap();
                uint32 instanceId = instance->GetInstanceId();

                // 1. Verific?m RAM
                bool isVeteran = (ActiveHeroicInstances.find(instanceId) != ActiveHeroicInstances.end());

                // 3. Activare ini?ial? (doar dac? liderul are comanda ?i instan?a e curat?)
                if (Group* group = player->GetGroup())
                {
                    if (group->IsLeader(player->GetGUID()) && HeroicSelection[player->GetGUID()] && !isVeteran)
                    {
                        // Verific?m dac? instan?a exist? ?n tabelul `instance` (s? nu d?m eroare de FK)
                        // TrinityCore salveaz? de obicei instan?a ?n DB imediat ce juc?torul intr?.
                        isVeteran = true;
                        ActiveHeroicInstances.insert(instanceId);

                        // Folosim REPLACE INTO: dac? exist? deja, ?l actualizeaz?; dac? nu, ?l insereaz?.
                        CharacterDatabase.PExecute("REPLACE INTO instance_veteran_status (id, isVeteran) VALUES ({}, 1)", instanceId);

                        ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[VETERAN]|r Instanta a fost sigilata permanent pe modul Veteran!");
                    }
                }

                if (isVeteran)
                {
                    CheckAndApplyVeteran(player);
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[VETERAN]|r Modul a fost aplicat cu succes!");
                }
            }, 2s);
    }

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

    void KittAddLoot(Player* /*player*/, Loot* loot) /*override*/
    {
        ObjectGuid sourceGuid = loot->sourceGuid;
        if (sourceGuid.GetEntry() == 13020)   // Vaelastrasz the Corrupt
        {
            //loot->items.clear(); // sterge loot vechi
            AddKittCustomItem(loot, 31193, 1, 100.0f, false);
            AddKittCustomItem(loot, 45085, 1, 50.0f, false); // 1 bucata, 50% sansa
            loot->gold += 500000; // adauga 50g la loot
            //ChatHandler(player->GetSession()).PSendSysMessage("|cffffd700[TEST Loot Heroic]|r Ai primit un item Veteran!");
        }
    }

    void KittAddLootElite(Player* /*player*/, Loot* loot) /*override*/
    {
        // loot comun
        //loot->items.clear(); // sterge loot vechi
        loot->gold += 100000; // adauga 50g la loot
    }



    void OnAfterLootFill(Player* player, Loot* loot) override
    {
        if (!player || !player->GetSession() || !loot)
            return;

        Creature* creature = player->GetMap()->GetCreature(loot->sourceGuid);
        if (!creature)
            return;

        Map* map = creature->GetMap();
        if (map->GetId() != MAP_BWL)
            return;

        InstanceMap* instance = (InstanceMap*)map;

        if (ActiveHeroicInstances.find(instance->GetInstanceId()) != ActiveHeroicInstances.end())
        {
            if (creature->IsDungeonBoss())
            {
                if (Group* group = player->GetGroup())
                {
                    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                    {
                        if (Player* member = itr->GetSource())
                        {
                            if (member->IsAtGroupRewardDistance(player))
                            {
                                member->AddItem(49426, 5);
                                //member->ModifyMoney(extraGold);

                                //ChatHandler(member->GetSession()).PSendSysMessage("|cffffd700[Loot Heroic]|r Ai primit un item Veteran!");
                            }
                        }
                    }
                }

                KittAddLoot(player, loot);
            }

            if (creature->isElite())
            {
                KittAddLootElite(player, loot);
            }
        }
    }



    void OnSpellCast(Player* player, Spell* /*spell*/, bool /*skipCheck*/) override
    {
        Map* map = player->GetMap();
        if (map->GetId() != MAP_BWL)
            return;

        InstanceMap* instance = (InstanceMap*)map;
        uint32 instanceId = instance->GetInstanceId();
        if (ActiveHeroicInstances.find(instance->GetInstanceId()) == ActiveHeroicInstances.end())
            return;

        uint32 now = GameTime::GetGameTimeMS();

        if (player->IsInCombat() && (now - InstanceScanTimers[instanceId] > 10000))
        {
            InstanceScanTimers[instanceId] = now;
            CheckAndApplyVeteran(player);
            //ChatHandler(player->GetSession()).PSendSysMessage("|cffffd700[TEST] spell cast mod apply");
        }
    }

    void CheckAndApplyVeteran(Player* player)
    {
        player->m_Events.AddEventAtOffset([player]()
            {
                if (!player || !player->IsInWorld() || player->GetMapId() != MAP_BWL)
                    return;

                InstanceMap* instance = (InstanceMap*)player->GetMap();

                if (ActiveHeroicInstances.find(instance->GetInstanceId()) != ActiveHeroicInstances.end())
                {
                    std::list<Creature*> creatureList;
                    player->GetCreatureListWithEntryInGrid(creatureList, 0, 150.0f);

                    for (Creature* creature : creatureList)
                    {
                        if (!creature || creature->isDead() || creature->IsPet() || creature->IsNPCBot())
                            continue;

                        if (!creature->IsPet() && !creature->IsNPCBot() && creature->GetLevel() < 80 && !creature->HasAura(VISUAL_AURA_MARKER))
                        {
                            creature->SetLevel(creature->GetLevel() + 20);

                            uint32 newHealth = creature->GetMaxHealth() * 10;
                            creature->SetMaxHealth(newHealth);
                            creature->SetHealth(newHealth);

                            float minDmg = creature->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE) * 10.0f;
                            float maxDmg = creature->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE) * 10.0f;
                            creature->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, minDmg);
                            creature->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, maxDmg);

                            creature->SetAttackPower(15000);

                            uint32 newArmor = creature->GetArmor() * 10.0f;
                            creature->SetArmor(newArmor);

                            for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
                                creature->SetResistance(SpellSchools(i), 550);

                            if (creature->isElite() || creature->IsDungeonBoss())
                                creature->SetObjectScale(1.35f);
                            else
                                creature->SetObjectScale(1.10f);

                            // 4. Update for?at
                            creature->UpdateUnitMod(UNIT_MOD_ATTACK_POWER);
                            creature->UpdateUnitMod(UNIT_MOD_ARMOR);
                            creature->UpdateDamagePhysical(BASE_ATTACK);
                            creature->UpdateArmor();
                            creature->UpdateAllResistances();


                            // 5. Aura Vizuala
                            creature->AddAura(VISUAL_AURA_MARKER, creature);
                            //ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[VETERAN]|r Rescan");

                        }

                        if (!creature->HasAura(9344) && !creature->IsPet() && !creature->IsNPCBot())
                        {
                            // 9344 spell cu valoare fixa
                            if (Aura* aura = creature->AddAura(9344, creature))
                                if (AuraEffect* eff = aura->GetEffect(0))
                                    eff->SetAmount(11000);
                        }
                    }
                    // Notific?m liderul/juc?torul
                    //ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[VETERAN]|r Modul Heroic a fost aplicat cu succes!");
                }
            }, 1s); // Delay de 1 secunde
    }

};

class kitt_bwl_commandscript : public CommandScript
{
public:
    kitt_bwl_commandscript() : CommandScript("kitt_bwl_commandscript") {}

    std::vector<ChatCommandBuilder> GetCommands() const override
    {
        return {
            { "zbwlveteran", HandleBWLHeroicCommand, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No }
        };
    }

    static bool HandleBWLHeroicCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player) return false;

        uint32 mapId = player->GetMapId();
        if (mapId == MAP_BWL)
        {
            handler->SendSysMessage("Comanda trebuie data in afara BWL!");
            return true;
        }

        if (!player->GetGroup() || !player->GetGroup()->IsLeader(player->GetGUID()))
        {
            handler->SendSysMessage("Doar liderul poate seta modul Veteran!");
            return true;
        }

        if (player->GetBoundInstance(MAP_BWL, DUNGEON_DIFFICULTY_NORMAL, false))
        {
            handler->SendSysMessage("|cffff0000[Eroare]|r Ai deja un ID de instanta salvat (Bind) pentru BWL.");
            handler->SendSysMessage("Trebuie sa resetezi instanta pentru a putea activa modul Veteran!");
            return true;
        }


        HeroicSelection[player->GetGUID()] = !HeroicSelection[player->GetGUID()];

        if (HeroicSelection[player->GetGUID()])
            handler->SendSysMessage("|cffff0000[VETERAN]|r MOD ACTIVAT pentru urmatoarea intrare.");
        else
            handler->SendSysMessage("|cff00ff00[NORMAL]|r MOD Veteran DEZACTIVAT.");

        return true;
    }
};

class npc_veteran_upgrader : public CreatureScript
{
public:
    npc_veteran_upgrader() : CreatureScript("npc_veteran_upgrader") {}

    struct npc_veteran_upgraderAI : public ScriptedAI
    {
        npc_veteran_upgraderAI(Creature* creature) : ScriptedAI(creature) {}


        bool OnGossipHello(Player* player) override
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Categorii Upgrade Veteran:", GOSSIP_SENDER_MAIN, 0);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "[DPS Melee]", GOSSIP_SENDER_MAIN, 100);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "[TANK]", GOSSIP_SENDER_MAIN, 200);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "[CASTER DPS]", GOSSIP_SENDER_MAIN, 300);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "[HEALER]", GOSSIP_SENDER_MAIN, 400);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "[RANGE DPS]", GOSSIP_SENDER_MAIN, 500);

            SendGossipMenuFor(player, 1, me->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            //uint32 const sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);

            ClearGossipMenuFor(player);

            // Dac? juc?torul a ales o categorie (100, 200, etc.)
            if (action >= 100 && action <= 500)
            {
                ScanAndShowEligibleItems(player, me, action);
                return true;
            }

            // Dac? juc?torul a ales un item specific (action este slotul de echipament + categoria)
            // Format action: (Categoria * 100) + SlotID
            if (action > 1000)
            {
                uint32 category = action / 1000;
                uint8 slot = (uint8)(action % 1000);
                ApplyVeteranUpgrade(player, slot, category);
                CloseGossipMenuFor(player);
            }

            return true;
        }

    private:
        void ScanAndShowEligibleItems(Player* player, Creature* creature, uint32 category)
        {
            bool found = false;
            for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
            {
                Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (!item) continue;

                uint32 entry = item->GetEntry();

                // Verific?m dac? item-ul este ?n lista alb?
                if (VeteranEligibleItems.find(entry) != VeteranEligibleItems.end())
                {
                    // Verific?m dac? are deja enchant-ul ?n Slotul 7 (PROP_0) ca marker
                    if (item->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_0) == 0)
                    {
                        std::string itemName = item->GetTemplate()->Name1;
                        // Trimitem Action ca: Categorie * 1000 + slot
                        uint32 actionPrefix = category * 10;
                        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, "Upgradeaza: " + itemName, GOSSIP_SENDER_MAIN, actionPrefix + i);
                        found = true;
                    }
                }
            }

            if (!found)
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nu ai iteme eligibile echipate!", GOSSIP_SENDER_MAIN, 0);

            SendGossipMenuFor(player, 1, creature->GetGUID());
        }

        void ApplyVeteranUpgrade(Player* player, uint8 slot, uint32 category)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item) return;

            // Verificare Resurse
            if (player->GetMoney() < GOLD_COST || !player->HasItemCount(ITEM_UPGRADE_TOKEN, 1))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000Ai nevoie de 500 Gold si 1x Token Veteran!|r");
                return;
            }

            // Aplicare Enchant-uri pe baza categoriei selectate
            switch (category)
            {
            case 1: // Melee DPS (100 -> 1)
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_0, 5000, 0, 0);
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_1, 5001, 0, 0);
                break;
            case 2: // Tank (200 -> 2)
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_0, 5002, 0, 0);
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_1, 5003, 0, 0);
                // Dac? e Scut, punem ?i pachetul de scut pe slotul 2
                if (item->GetTemplate()->InventoryType == INVTYPE_SHIELD)
                    item->SetEnchantment(PROP_ENCHANTMENT_SLOT_2, 5004, 0, 0);
                break;
            case 3: // Caster DPS (300 -> 3)
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_0, 5005, 0, 0);
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_1, 5006, 0, 0);
                break;
            case 4: // Healer (400 -> 4)
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_0, 5007, 0, 0);
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_1, 5008, 0, 0);
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_2, 5009, 0, 0);
                break;
            case 5: // Range DPS (500 -> 5)
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_0, 5010, 0, 0);
                item->SetEnchantment(PROP_ENCHANTMENT_SLOT_1, 5011, 0, 0);
                break;
            }

            player->DestroyItemCount(ITEM_UPGRADE_TOKEN, 1, true);
            player->ModifyMoney(-((int32)GOLD_COST));

            player->ApplyEnchantment(item, PROP_ENCHANTMENT_SLOT_0, true);
            player->ApplyEnchantment(item, PROP_ENCHANTMENT_SLOT_1, true);
            player->ApplyEnchantment(item, PROP_ENCHANTMENT_SLOT_2, true);

            //CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            //player->SaveInventoryAndGoldToDB(trans);
            //CharacterDatabase.CommitTransaction(trans);

            ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00Upgrade finalizat cu succes pentru %s!|r", item->GetTemplate()->Name1.c_str());
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_veteran_upgraderAI(creature);
    }
};

class kitt_bwl_veteran_startup : public WorldScript
{
public:
    kitt_bwl_veteran_startup() : WorldScript("kitt_bwl_veteran_startup") {}

    void OnStartup() override
    {
        ActiveHeroicInstances.clear();

        QueryResult result = CharacterDatabase.Query("SELECT id FROM instance_veteran_status WHERE isVeteran = 1");

        if (!result)
            return;

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            uint32 instanceId = fields[0].GetUInt32();

            ActiveHeroicInstances.insert(instanceId);
            count++;
        } while (result->NextRow());

        TC_LOG_INFO("server.loading", ">> KITT [BWL Veteran] Restaurate {} instante din baza de date.", count);
    }
};






void AddSC_kitt_instance_mod_heroic()
{
    new kitt_bwl_veteran_startup();
    new kitt_bwl_heroic_core();
    new kitt_bwl_commandscript();
    new npc_veteran_upgrader();
}
