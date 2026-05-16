//----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "kitt_npcbot_ai.h"
#include "Player.h"
#include "Chat.h"
#include "Creature.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "botspell.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "Transport.h"
#include "MotionMaster.h"
#include "Vehicle.h"

//#include "botcommon.h"


namespace
{
    // Lich King
    static std::map<ObjectGuid, uint32> teleportCooldownMap; // map for cd
    static uint32 const TELEPORT_CD = 1000; // in ms / 1 secunde intre teleportari pentru acelasi obiect
    static ObjectGuid triangleTargetGUID = ObjectGuid::Empty; // icon set faza sabiei normal mode
}

namespace KittBotAI
{
    // daca vrem sa blocam dispell undeva
    bool IsSafeToCure(Unit* target)
    {
        if (!target || !target->GetMap())
        {
            return true;
        }

        uint32 mapId = target->GetMapId();
        uint32 areaId = target->GetAreaId();

        if (mapId == 631)
        {
            switch (areaId)
            {
               case 4812: // Marrowgar
               {
                   // code
                   break;
               }
               case 4890: // profesor
               {
                   // code
                   break;
               }

               case 4889: // sindragosa
               {
                   // code
                   break;
               }
               case 4859: // Lich King
               {
                   uint32 const npcShamblingHorror = 37698;
                   uint32 spellNecroticPlague = 0;
                   uint32 const spellNecroticPlague10N = 70337;
                   uint32 const spellNecroticPlague10HC = 73913;
                   uint32 const spellNecroticPlague25N = 73912;
                   uint32 const spellNecroticPlague25HC = 73914;

                   Map* map = target->GetMap();
                   if (map)
                   {
                       if (map->IsHeroic())
                       {
                           if (map->Is25ManRaid())
                           {
                               spellNecroticPlague = spellNecroticPlague25HC;
                           }
                           else
                           {
                               spellNecroticPlague = spellNecroticPlague10HC;
                           }
                       }
                       else
                       {
                           if (map->Is25ManRaid())
                           {
                               spellNecroticPlague = spellNecroticPlague25N;
                           }
                           else
                           {
                               spellNecroticPlague = spellNecroticPlague10N;
                           }
                       }
                   }

                   if (spellNecroticPlague == 0)
                       break;



                   // Necrotic Plague block
                   if (target->HasAura(spellNecroticPlague))
                   {
                       if (Creature* horror = target->FindNearestCreature(npcShamblingHorror, 100.0f, true))
                       {
                           if (target->GetDistance(horror) > 5.0f)
                           {
                               return false;
                           }
                       }
                       else
                       {
                           return true;
                       }
                   }

                   break;
               }
            }
        }

        return true;
    }

    void KittUpdateRaidStrategies(Creature* bot, Player* master)
    {
        if (!bot || !master || !bot->IsAlive() || !bot->IsInWorld())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        uint32 mapId = master->GetMapId();
        uint32 areaId = master->GetAreaId();

        if (mapId != 631)
            return;

        InstanceScript* instance = master->GetInstanceScript();
        if (!instance)
            return;

        uint32 LordMarrStart      = static_cast<uint32>(instance->GetBossState(0)); //  DATA_LORD_MARROWGAR
        uint32 LadyDeathStart     = static_cast<uint32>(instance->GetBossState(1)); //  DATA_LADY_DEATHWHISPER
        uint32 GunshipStart       = static_cast<uint32>(instance->GetBossState(2)); //  DATA_ICECROWN_GUNSHIP_BATTLE
        uint32 DeathSaurfangStart = static_cast<uint32>(instance->GetBossState(3)); //  NPC_DEATHBRINGER_SAURFANG
        uint32 RotfaceStart       = static_cast<uint32>(instance->GetBossState(5)); //  DATA_ROTFACE
        uint32 PutricideStart     = static_cast<uint32>(instance->GetBossState(6)); // DATA_PROFESSOR_PUTRICIDE
        uint32 PrinceCouncilStart = static_cast<uint32>(instance->GetBossState(7)); // DATA_BLOOD_PRINCE_COUNCIL
        uint32 BloodQueenStart    = static_cast<uint32>(instance->GetBossState(8)); // DATA_BLOOD_QUEEN_LANA_THEL
        uint32 ValithiaStart      = static_cast<uint32>(instance->GetBossState(9)); // DATA_VALITHRIA_DREAMWALKER
        uint32 SindragosaStart    = static_cast<uint32>(instance->GetBossState(10)); // DATA_SINDRAGOSA
        uint32 LichKingStart      = static_cast<uint32>(instance->GetBossState(11)); // DATA_THE_LICH_KING



        if (mapId == 631)
        {
            switch (areaId)
            {
                case 4812: // Marrowgar
                {
                    if (LordMarrStart == IN_PROGRESS)
                    {
                        KittHandleMarrowgar(bot, master, ai); // marrow
                    }

                    if (LadyDeathStart == IN_PROGRESS)
                    {
                        KittHandleLadyDeathwhisper(bot, master, ai); // lady
                    }

                    if (GunshipStart == IN_PROGRESS)
                    {
                        KittHandleGunship(bot, master, ai); // gunship
                    }

                    if (DeathSaurfangStart == IN_PROGRESS)
                    {
                        KittHandleDeathSaurfang(bot, master, ai); // Death Saurfang
                    }

                    if (RotfaceStart == IN_PROGRESS)
                    {
                        KittHandleRotface(bot, master, ai); // Rotface
                    }

                    if (ValithiaStart == IN_PROGRESS)
                    {
                        KittHandleValithia(bot, master, ai); // Valithia
                    }

                    break;
                }

                case 4890: // profesor
                {
                    if (PutricideStart == IN_PROGRESS)
                    {
                        KittHandlePutricide(bot, master, ai);
                    }
                    break;
                }

                case 4892: // Prince Council
                {
                    if (PrinceCouncilStart == IN_PROGRESS)
                    {
                        KittHandlePrinceCouncil(bot, master, ai);
                    }
                    break;
                }

                case 4891: // Blood Queen Lana
                {
                    if (BloodQueenStart == IN_PROGRESS)
                    {
                        KittHandleBloodQueen(bot, master, ai);
                    }
                    break;
                }

                case 4889: // sindragosa
                {
                    if (SindragosaStart == IN_PROGRESS)
                    {
                        KittHandleSindragosa(bot, master, ai);
                    }
                    break;
                }

                case 4859: // Lich King
                {
                    if (LichKingStart == IN_PROGRESS)
                    {
                        KittHandleLichKing(bot, master, ai);
                    }
                    break;
                }

            }
        }
    }


    void KittHandleMarrowgar(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        uint32 const BossMarrow = 36612;
        uint32 const NpcSpike1 = 36619;
        uint32 const NpcSpike2 = 38711;
        uint32 const NpcSpike3 = 38712;

        uint8 iconIndex5 = 5; // patrat
        uint8 iconIndex7 = 7; // skull

        if (Creature* TarBossMarrow = bot->FindNearestCreature(BossMarrow, 80.0f, true))
        {
            if (TarBossMarrow->IsAlive() && TarBossMarrow->IsInWorld())
            {
                if (ai->HasRole(BOT_ROLE_TANK))
                {
                    if (gr)
                    {
                        if (TarBossMarrow && gr->GetTargetIcons()[iconIndex7] != TarBossMarrow->GetGUID())
                        {
                            gr->SetTargetIcon(iconIndex7, bot->GetGUID(), TarBossMarrow->GetGUID());
                        }
                    }
                }
            }
        }

        if (!ai->HasRole(BOT_ROLE_TANK) && !ai->HasRole(BOT_ROLE_HEAL))
        {
            std::list<Creature*> allSpikes;
            bot->GetCreatureListWithEntryInGrid(allSpikes, NpcSpike1, 80.0f);
            if (allSpikes.empty())
            {
                bot->GetCreatureListWithEntryInGrid(allSpikes, NpcSpike2, 80.0f);
            }
            if (allSpikes.empty())
            {
                bot->GetCreatureListWithEntryInGrid(allSpikes, NpcSpike3, 80.0f);
            }

            allSpikes.remove_if([](Creature* npc) {
                return !npc || !npc->IsAlive() || !npc->IsInWorld();
                });

            Creature* NpcSpikeTar = nullptr;
            bool iconDejaExista = false;
            ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex5];

            allSpikes.sort([](Creature* a, Creature* b) {
                return a->GetGUID() < b->GetGUID();
                });

            for (Creature* s : allSpikes)
            {
                if (!s->IsAlive()) continue;

                if (s->GetGUID() == currentIconGuid)
                {
                    iconDejaExista = true;
                    NpcSpikeTar = s;
                    break;
                }
            }

            if (!iconDejaExista)
            {
                for (Creature* s : allSpikes)
                {
                    s = allSpikes.front();

                    if (s->IsAlive())
                    {
                        NpcSpikeTar = s;
                        gr->SetTargetIcon(iconIndex5, bot->GetGUID(), NpcSpikeTar->GetGUID());
                    }

                    break;
                }
            }

            if (NpcSpikeTar)
            {
                if (bot->GetVictim() != NpcSpikeTar)
                {
                    bot->AttackStop();
                    bot->SetInCombatWith(NpcSpikeTar);
                    //bot->GetThreatManager().FixateTarget(NpcSpikeTar);
                    bot->Attack(NpcSpikeTar, !ai->HasRole(BOT_ROLE_RANGED));
                    ai->AttackStart(NpcSpikeTar);
                }
            }
        }
    }

    void KittHandleLadyDeathwhisper(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        uint32 const BossLady = 36855;
        uint32 const NpcCultFan = 37890;
        uint32 const NpcReanFan = 38009;
        uint32 const NpcCultAdh  = 37949;
        uint32 const NpcEmpowAdh = 38136;
        uint32 const NpcReaniAdh = 38010; // pri 1
        uint32 const NpcDefoFan = 38135; // pri 2
        uint32 const spellDominateMind = 71290; // Dominate Mind scale



        if (Creature* TarBossLady = bot->FindNearestCreature(BossLady, 80.0f, true))
        {
            if (!TarBossLady || !TarBossLady->IsInWorld() || !TarBossLady->IsAlive())
                return;

            if (Group* group = master->GetGroup())
            {
                for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                {
                    Player* member = itr->GetSource();
                    if (!member || !member->IsAlive() || !member->HasAura(spellDominateMind))
                        continue;

                    float dist = bot->GetDistance(member);
                    if (dist < 30.0f)
                    {
                        if (member->HasAura(spellDominateMind))
                        {
                            bool areDejaCC = member->HasAura(10308) || // Hammer of Justice
                                member->HasAura(118) || // Polymorph
                                member->HasAura(2094) || // Blind
                                member->HasAura(33786) || // Cyclone
                                member->HasAura(5782) || // Fear
                                member->HasAura(14311) || // Freezing Trap (debuff-ul Freezing)
                                member->HasAura(8122) || // Psychic Scream
                                member->HasAura(51514);    // Hex

                            if (!areDejaCC && !member->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_ROOT))
                            {
                                float dist = bot->GetDistance(member);
                                float razaCast = 8.0f; // 8 cast

                                if (dist > razaCast)
                                {
                                    float x, y, z;
                                    member->GetContactPoint(bot, x, y, z, razaCast);
                                    //bot->GetMotionMaster()->Clear();
                                    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                                    {
                                        bot->GetMotionMaster()->MovePoint(1, x, y, z);
                                    }
                                }
                                else
                                {
                                    switch (bot->GetBotClass())
                                    {
                                       case BOT_CLASS_PALADIN:
                                       {
                                           // Hammer of Justice (ID: 10308)
                                           if (!areDejaCC)
                                           {
                                               if (bot->IsNonMeleeSpellCast(true))
                                               {
                                                   bot->InterruptNonMeleeSpells(true);
                                               }
                                               bot->AttackStop();

                                               bot->CastSpell(member, 10308, true);
                                           }
                                           break;
                                       }
                                       case BOT_CLASS_MAGE:
                                       {
                                           // Polymorph (ID: 118)
                                           if (!areDejaCC)
                                           {
                                               if (bot->IsNonMeleeSpellCast(true))
                                               {
                                                   bot->InterruptNonMeleeSpells(true);
                                               }
                                               bot->AttackStop();

                                               bot->CastSpell(member, 118, true);
                                           }
                                           break;
                                       }
                                       case BOT_CLASS_ROGUE:
                                       {
                                           // Blind (ID: 2094)
                                           if (!areDejaCC)
                                           {
                                               if (bot->IsNonMeleeSpellCast(true))
                                               {
                                                   bot->InterruptNonMeleeSpells(true);
                                               }
                                               bot->AttackStop();

                                               bot->CastSpell(member, 2094, true);
                                           }
                                           break;
                                       }
                                       case BOT_CLASS_DRUID:
                                       {
                                           // Cyclone (ID: 33786)
                                           if (!areDejaCC)
                                           {
                                               if (bot->IsNonMeleeSpellCast(true))
                                               {
                                                   bot->InterruptNonMeleeSpells(true);
                                               }
                                               bot->AttackStop();

                                               bot->CastSpell(member, 33786, true);
                                           }
                                           break;
                                       }
                                       case BOT_CLASS_WARRIOR:
                                       {
                                           // Concussion Blow sau Intercept Stun
                                           break;
                                       }
                                       case BOT_CLASS_WARLOCK:
                                       {
                                           if (!areDejaCC)
                                           {
                                               if (bot->IsNonMeleeSpellCast(true))
                                               {
                                                   bot->InterruptNonMeleeSpells(true);
                                               }
                                               bot->AttackStop();

                                               bot->CastSpell(member, 5782, true);  // Fear
                                           }
                                           break;
                                       }
                                       case BOT_CLASS_HUNTER:
                                       {
                                           if (!areDejaCC)
                                           {
                                               if (bot->IsNonMeleeSpellCast(true))
                                               {
                                                   bot->InterruptNonMeleeSpells(true);
                                               }
                                               bot->AttackStop();

                                               //bot->CastSpell(member, 19503, true); // Scatter Shot
                                               bot->CastSpell(member, 14311, true); // Freezing Trap
                                           }
                                           break;
                                       }
                                       case BOT_CLASS_PRIEST:
                                       {
                                           if (!areDejaCC)
                                           {
                                               if (bot->IsNonMeleeSpellCast(true))
                                               {
                                                   bot->InterruptNonMeleeSpells(true);
                                               }
                                               bot->AttackStop();

                                               bot->CastSpell(member, 8122, true);  // Psychic Scream
                                           }
                                           break;
                                       }
                                       case BOT_CLASS_SHAMAN:
                                       {
                                           if (!areDejaCC)
                                           {
                                               if (bot->IsNonMeleeSpellCast(true))
                                               {
                                                   bot->InterruptNonMeleeSpells(true);
                                               }
                                               bot->AttackStop();

                                               bot->CastSpell(member, 51514, true); // Hex
                                           }
                                           break;
                                       }
                                       case BOT_CLASS_DEATH_KNIGHT:
                                       {
                                           //bot->CastSpell(member, 47476, true); // Strangulate
                                           break;
                                       }
                                    }
                                }
                                return;
                            }
                        }
                        //return;
                    }
                }
            }

            if (!ai->HasRole(BOT_ROLE_TANK) && !ai->HasRole(BOT_ROLE_HEAL))
            {
                std::list<Creature*> NpcList;
                bot->GetCreatureListWithEntryInGrid(NpcList, NpcCultFan, 100.0f); // prioritar 
                if (NpcList.empty())
                {
                    bot->GetCreatureListWithEntryInGrid(NpcList, NpcEmpowAdh, 100.0f); // urmatorul
                }
                if (NpcList.empty())
                {
                    bot->GetCreatureListWithEntryInGrid(NpcList, NpcReanFan, 100.0f); // urmatorul
                }
                if (NpcList.empty())
                {
                    bot->GetCreatureListWithEntryInGrid(NpcList, NpcCultAdh, 100.0f); // urmatorul
                }
                if (NpcList.empty())
                {
                    bot->GetCreatureListWithEntryInGrid(NpcList, NpcReaniAdh, 100.0f); // urmatorul
                }
                if (NpcList.empty())
                {
                    bot->GetCreatureListWithEntryInGrid(NpcList, NpcDefoFan, 100.0f); // urmatorul
                }

                if (!NpcList.empty())
                {
                    Creature* NpcTar = nullptr;
                    bool iconExist = false;
                    uint8 iconIndex = 5; // patrat
                    ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex];

                    NpcList.sort([](Creature* a, Creature* b) {
                        return a->GetGUID() < b->GetGUID();
                        });

                    for (Creature* s : NpcList)
                    {
                        if (!s->IsAlive()) continue;

                        if (s->GetGUID() == currentIconGuid)
                        {
                            iconExist = true;
                            NpcTar = s;
                            break;
                        }
                    }

                    if (!iconExist && gr)
                    {
                        for (Creature* s : NpcList)
                        {
                            s = NpcList.front();

                            if (s->IsAlive())
                            {
                                NpcTar = s;
                                gr->SetTargetIcon(iconIndex, bot->GetGUID(), NpcTar->GetGUID());
                            }

                            break;
                        }
                    }

                    if (NpcTar && NpcTar->IsAlive())
                    {
                        if (bot->GetVictim() && bot->GetVictim()->GetGUID() == NpcTar->GetGUID())
                        {
                            return;
                        }

                        if (bot->GetVictim() != NpcTar)
                        {
                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(NpcTar, false);
                            }
                            else
                            {
                                bot->Attack(NpcTar, true);
                            }
                            ai->AttackStart(NpcTar);
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                        }
                        //return;
                    }
                }
            }
        }
    }

    void KittHandleGunship(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        // --- block instance team > bots ---
        InstanceScript* instance = master->GetInstanceScript();
        if (!instance)
            return;

        uint32 instanceTeam = static_cast<uint32>(instance->GetData(27)); // icc DATA_TEAM_IN_INSTANCE
        uint32 GunshipStart = static_cast<uint32>(instance->GetBossState(2)); //  DATA_ICECROWN_GUNSHIP_BATTLE       = 2,

        if (GunshipStart != 1)
        {
            if (BotMgr* mgr = master->GetBotMgr())
            {
                // Verificam daca are boti
                if (!master->HaveBot())
                    return;

                // Verificam daca este viu
                if (!master->IsAlive())
                    return;

                // Daca sunt deja ascunsi, ii aducem inapoi
                if (mgr->GetBotsHidden())
                {
                    mgr->SetBotsHidden(false);
                }
            }
            return;
        }

        bool HideBots = false;

        if (master->GetTeamId() == TEAM_HORDE && instanceTeam == ALLIANCE)
        {
            HideBots = true;
        }

        if (master->GetTeamId() == TEAM_ALLIANCE && instanceTeam == HORDE)
        {
            HideBots = true;
        }

        if (HideBots)
        {
            if (BotMgr* mgr = master->GetBotMgr())
            {
                // Verificam daca are boti
                if (!master->HaveBot())
                    return;

                if (!master->IsAlive())
                    return;

                // Daca nu sunt deja ascunsi, ii ascundem
                if (!mgr->GetBotsHidden())
                {
                    mgr->SetBotsHidden(true);
                    ChatHandler(master->GetSession()).PSendSysMessage("|cffff0000[B0ts]:|r Partenerii tai nu sunt eligibili si au fost ascunsi pe durata luptei.");
                    master->GetSession()->SendNotification("Partenerii tai nu sunt eligibili si au fost ascunsi pe durata luptei.");
                }
            }
            return;
        }
        // --------- end block --------


        Group* gr = master->GetGroup();
        if (!gr)
            return;

        Transport* mTrans = master->GetTransport();
        if (!mTrans)
            return;

        uint32 const SpellFreez = 69705; // channel spell

        //uint32 NpcIGB = 0; // modular
        uint32 NpcMage = 0; // modular
        uint32 NpcMargine = 0; // Modular
        uint32 NpcVizita1 = 0; // se duce in vizita
        uint32 NpcVizita2 = 0; // se duce in vizita

        //uint32 const NpcIgbHighOverlordH = 36939; // IGB Horde
        uint32 const NpcMageH            = 37117; // Mage
        uint32 const NpcAxethH           = 36968; // Margine
        uint32 const NpcKronReaverH      = 36957; // se duce in vizita
        uint32 const NpcKronSergeantH    = 36960; // se duce in vizita

        //uint32 const NpcIgbMuradinA   = 36948; // IGB Alliance
        uint32 const NpcSorcererA     = 37116; // Mage
        uint32 const NpcRiflemanA     = 36969; // margine
        uint32 const NpcSkybMarineA   = 36950; // se duce in vizita
        uint32 const NpcSkybSergeantA = 36961; // se duce in vizita


        //uint32 const TransAlliance = 201580;
        //uint32 const TransHorde = 201812;

        //uint8 iconIndex4 = 4; // Luna
        uint8 iconIndex5 = 5; // patrat
        uint8 iconIndex7 = 7; // skelet



        if (master->GetTeamId() == TEAM_HORDE)
        {
            //NpcIGB = NpcIgbHighOverlordH;
            NpcMage = NpcSorcererA;
            NpcMargine = NpcRiflemanA;
            NpcVizita1 = NpcSkybMarineA;
            NpcVizita2 = NpcSkybSergeantA;

        }
        else // alliance
        {
            //NpcIGB = NpcIgbMuradinA;
            NpcMage = NpcMageH;
            NpcMargine = NpcAxethH;
            NpcVizita1 = NpcKronReaverH;
            NpcVizita2 = NpcKronSergeantH;
        }

        if (ai->HasRole(BOT_ROLE_TANK))
        {
            std::list<Creature*> NpcList; // lista generala

            bot->GetCreatureListWithEntryInGrid(NpcList, NpcVizita1, 30.0f); // 50 prioritar
            NpcList.remove_if([](Creature* npc) { return !npc->IsAlive(); }); // stergem ce nu e in viata

            if (NpcList.empty())
            {
                bot->GetCreatureListWithEntryInGrid(NpcList, NpcVizita2, 30.0f); // urmatorul
                NpcList.remove_if([](Creature* npc) { return !npc->IsAlive(); }); // stergem ce nu e in viata
            }

            if (!NpcList.empty())
            {
                Creature* NpcTar = nullptr;
                bool iconExist = false;
                ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex7];

                NpcList.sort([](Creature* a, Creature* b) {
                    return a->GetGUID() < b->GetGUID();
                    });

                for (Creature* s : NpcList)
                {
                    if (!s->IsAlive()) continue;

                    if (s->GetGUID() == currentIconGuid)
                    {
                        iconExist = true;
                        NpcTar = s;
                        break;
                    }
                }

                if (!iconExist && gr)
                {
                    for (Creature* s : NpcList)
                    {
                        s = NpcList.front();

                        if (s->IsAlive())
                        {
                            NpcTar = s;
                            gr->SetTargetIcon(iconIndex7, bot->GetGUID(), NpcTar->GetGUID());
                        }

                        break;
                    }
                }

                if (NpcTar && NpcTar->IsAlive())
                {
                    if (bot->GetVictim() && bot->GetVictim()->GetGUID() == NpcTar->GetGUID())
                    {
                        return;
                    }

                    if (bot->GetVictim() != NpcTar)
                    {
                        bot->GetThreatManager().AddThreat(NpcTar, 300300.0f);
                        bot->SetInCombatWith(NpcTar);
                        bot->GetThreatManager().FixateTarget(NpcTar);

                        ai->AttackStart(NpcTar);
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);

                        if (ai->HasRole(BOT_ROLE_RANGED))
                        {
                            bot->Attack(NpcTar, false);
                        }
                        else
                        {
                            bot->Attack(NpcTar, true);
                        }
                    }
                }
            }
        }

        if (!ai->HasRole(BOT_ROLE_TANK) && !ai->HasRole(BOT_ROLE_HEAL))
        {
            std::list<Creature*> NpcList; // lista generala

            bot->GetCreatureListWithEntryInGrid(NpcList, NpcMage, 100.0f); // prioritar
            // stergem din lista mage-ii care nu dau cast la spell-ul
            NpcList.remove_if([](Creature* npc) {
                return !npc->IsAlive() || npc->GetChannelSpellId() != SpellFreez;
                });
            // --------------------------------------------

            if (NpcList.empty())
            {
                bot->GetCreatureListWithEntryInGrid(NpcList, NpcMargine, 100.0f); // urmatorul
                NpcList.remove_if([](Creature* npc) { return !npc->IsAlive(); }); // stergem ce nu e in viata
            }

            if (!NpcList.empty())
            {
                Creature* NpcTar = nullptr;
                bool iconExist = false;
                ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex5];

                NpcList.sort([](Creature* a, Creature* b) {
                    return a->GetGUID() < b->GetGUID();
                    });

                for (Creature* s : NpcList)
                {
                    if (!s->IsAlive()) continue;

                    if (s->GetGUID() == currentIconGuid)
                    {
                        iconExist = true;
                        NpcTar = s;
                        break;
                    }
                }

                if (!iconExist && gr)
                {
                    for (Creature* s : NpcList)
                    {
                        if (s->IsAlive())
                        {
                            NpcTar = s;
                            gr->SetTargetIcon(iconIndex5, bot->GetGUID(), NpcTar->GetGUID());
                            break;
                        }
                    }
                }

                if (NpcTar && NpcTar->IsAlive())
                {
                    if (bot->GetVictim() && bot->GetVictim()->GetGUID() == NpcTar->GetGUID())
                    {
                        return;
                    }

                    if (bot->GetVictim() != NpcTar)
                    {
                        bot->GetThreatManager().AddThreat(NpcTar, 1300300.0f);
                        bot->SetInCombatWith(NpcTar);
                        bot->GetThreatManager().FixateTarget(NpcTar);

                        ai->AttackStart(NpcTar);
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);

                        if (ai->HasRole(BOT_ROLE_RANGED))
                        {
                            bot->Attack(NpcTar, false);
                        }
                        else
                        {
                            bot->Attack(NpcTar, true);
                        }
                    }
                }
            }
        }
    }

    void KittHandleDeathSaurfang(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        uint32 NpcBloodDeast = 38508;
        uint8 iconIndex5 = 5; // patrat


        if (!ai->HasRole(BOT_ROLE_TANK) && !ai->HasRole(BOT_ROLE_HEAL))
        {
            std::list<Creature*> NpcList; // lista generala

            bot->GetCreatureListWithEntryInGrid(NpcList, NpcBloodDeast, 50.0f); // prioritar
            NpcList.remove_if([](Creature* npc) { return !npc->IsAlive(); }); // stergem ce nu e in viata

            if (!NpcList.empty())
            {
                Creature* NpcTar = nullptr;
                bool iconExist = false;
                ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex5];

                NpcList.sort([](Creature* a, Creature* b) {
                    return a->GetGUID() < b->GetGUID();
                    });

                for (Creature* s : NpcList)
                {
                    if (!s->IsAlive()) continue;

                    if (s->GetGUID() == currentIconGuid)
                    {
                        iconExist = true;
                        NpcTar = s;
                        break;
                    }
                }

                if (!iconExist && gr)
                {
                    for (Creature* s : NpcList)
                    {
                        s = NpcList.front();

                        if (s->IsAlive())
                        {
                            NpcTar = s;
                            gr->SetTargetIcon(iconIndex5, bot->GetGUID(), NpcTar->GetGUID());
                        }

                        break;
                    }
                }

                if (NpcTar && NpcTar->IsAlive())
                {
                    if (bot->GetVictim() && bot->GetVictim()->GetGUID() == NpcTar->GetGUID())
                    {
                        return;
                    }

                    if (bot->GetVictim() != NpcTar)
                    {
                        bot->GetThreatManager().AddThreat(NpcTar, 300300.0f);
                        bot->SetInCombatWith(NpcTar);
                        bot->GetThreatManager().FixateTarget(NpcTar);

                        ai->AttackStart(NpcTar);
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);

                        if (ai->HasRole(BOT_ROLE_RANGED))
                        {
                            bot->Attack(NpcTar, false);
                        }
                        else
                        {
                            bot->Attack(NpcTar, true);
                        }
                    }
                }
            }
        }
    }

    void KittHandleRotface(Creature* bot, Player* master, bot_ai* /*ai*/)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        /*Group* gr = master->GetGroup();
        if (!gr)
            return;*/

        //uint32 NpcBloodDeast = 38508;
        //uint8 iconIndex5 = 5; // patrat

        uint32 SpellOozeFlood = 69785; // spell aura pata verde de pe margine 25h
        uint32 SpellOozeFlood2 = 69788; // spell aura pata verde 25h

        uint32 NpcPuddleStalker = 37013; // NPC_PUDDLE_STALKER
        uint32 NpcOozeFlood = 37006; // sa fuga din ele, raza 6m

        // petele mici aleatorii
        if (Creature* npcOoze = bot->FindNearestCreature(NpcOozeFlood, 5.0f, true))
        {
            if (bot->IsNonMeleeSpellCast(true))
            {
                bot->InterruptNonMeleeSpells(true);
            }

            bot->AttackStop();
            bot->GetMotionMaster()->Clear();
            float angle = npcOoze->GetAbsoluteAngle(bot);
            float runDist = 7.0f;
            float x = bot->GetPositionX() + (runDist * std::cos(angle));
            float y = bot->GetPositionY() + (runDist * std::sin(angle));

            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
            {
                bot->GetMotionMaster()->MovePoint(1, x, y, bot->GetPositionZ());
            }
        }


        // pata verde
        float RangeDist = 25.0f;
        std::list<Creature*> puddleList;
        bot->GetCreatureListWithEntryInGrid(puddleList, NpcPuddleStalker, RangeDist);

        puddleList.remove_if([SpellOozeFlood, SpellOozeFlood2](Creature* pataOoze) {
            return !pataOoze || !pataOoze->IsAlive() || (!pataOoze->HasAura(SpellOozeFlood) && !pataOoze->HasAura(SpellOozeFlood2));
            });

        if (puddleList.empty())
            return;

        puddleList.sort([bot](Creature* a, Creature* b) {
            return bot->GetDistance2d(a) < bot->GetDistance2d(b);
            });

        Creature* pataOoze = puddleList.front();
        if (pataOoze)
        {
            if (bot->IsNonMeleeSpellCast(true))
            {
                bot->InterruptNonMeleeSpells(true);
            }

            bot->AttackStop();
            bot->GetMotionMaster()->Clear();

            float angle = pataOoze->GetOrientation();
            float runDist = RangeDist + 5.0f;
            float x = bot->GetPositionX() + (runDist * std::cos(angle));
            float y = bot->GetPositionY() + (runDist * std::sin(angle));

            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
            {
                bot->GetMotionMaster()->MovePoint(2, x, y, bot->GetPositionZ());
            }
        }
    }

    void KittHandlePutricide(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        uint32 const NpcBossProfesor = 36678; // boss Profesor

        uint32 spellUnboundPlague = 0; // select dificulty
        uint32 const spellUnboundPlague10h = 72855; // 70911, 72855 (10h) 72856 (25h)
        uint32 const spellUnboundPlague25h = 72856; // 70911, 72855 (10h) 72856 (25h)

        uint32 spellMutatedPlague = 0;
        uint32 const spellMutatedPlague25H = 72672; // Mutated Plague faza 3 // 72672(25H) 72671(10H) 72463(25n)
        uint32 const spellMutatedPlague10H = 72671; // 10 Heroic
        uint32 const spellMutatedPlague25N = 72463; // 25 Normal
        uint32 const spellMutatedPlague10N = 72451; // 10 Normal

        uint32 const NpcVolatileOoze = 37697;    // NPC_VOLATILE_OOZE
        uint32 const NpcGasCloud = 37562;
        uint32 const NpcOozePuddle = 37690; // trigger pata verde aleatorie
        //uint32 const spellGrowPuddle = 70347; // aura pe trigger ... stack 1=20%


        // icon
        uint8 iconIndex5 = 5; // patrat


        Map* map = master->GetMap();
        if (map)
        {
            if (map->IsHeroic())
            {
                if (map->Is25ManRaid())
                {
                    spellUnboundPlague = spellUnboundPlague25h;
                    spellMutatedPlague = spellMutatedPlague25H;
                }
                else
                {
                    spellUnboundPlague = spellUnboundPlague10h;
                    spellMutatedPlague = spellMutatedPlague10H;
                }
            }
            else
            {
                if (map->Is25ManRaid())
                {
                    spellMutatedPlague = spellMutatedPlague25N;
                }
                else
                {
                    spellMutatedPlague = spellMutatedPlague10N;
                }
            }
        }

        if (spellUnboundPlague == 0)
            return;

        if (bot->HasAura(spellUnboundPlague) && !ai->HasRole(BOT_ROLE_TANK))
        {
            if (!bot->HasUnitState(UNIT_STATE_ROOT))
            {
                if (ai->HasBotCommandState(BOT_COMMAND_ATTACK))
                {
                    ai->RemoveBotCommandState(BOT_COMMAND_ATTACK);
                    bot->AttackStop();
                }

                ai->SetBotCommandState(BOT_COMMAND_STAY);
                bot->StopMoving();

                float x = 4356.61f;
                float y = 3161.82f;
                float z = 389.398f;
                float o = 1.53f;

                //bot->GetMotionMaster()->MovePoint(1001, x, y, z);
                bot->NearTeleportTo(x, y, z, o);
                bot->AddUnitState(UNIT_STATE_ROOT);
                bot->Yell("am fugit", LANG_UNIVERSAL);
            }
            return;
        }
        else
        {
            if (!bot->HasAura(spellUnboundPlague) && bot->HasUnitState(UNIT_STATE_ROOT))
            {
                ai->RemoveBotCommandState(BOT_COMMAND_STAY);
                bot->ClearUnitState(UNIT_STATE_ROOT);
            }
        }

        // Ooze Puddle pata verde aleatorie
        if (Creature* OozePuddle = bot->FindNearestCreature(NpcOozePuddle, 40.0f, true))
        {
            float currentScale = OozePuddle->GetObjectScale();
            float safetyMargin = 1.0f;
            float dynamicRadius = (1.0f * currentScale) + safetyMargin;

            float distToOozePuddle = bot->GetDistance(OozePuddle);

            if (distToOozePuddle < dynamicRadius)
            {
                if (bot->IsNonMeleeSpellCast(true))
                    bot->InterruptNonMeleeSpells(true);

                bot->AttackStop();

                float angle = OozePuddle->GetAbsoluteAngle(bot);
                float runDist = (dynamicRadius - distToOozePuddle);
                float x = bot->GetPositionX() + (runDist * std::cos(angle));
                float y = bot->GetPositionY() + (runDist * std::sin(angle));

                if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                {
                    bot->GetMotionMaster()->MovePoint(1, x, y, bot->GetPositionZ());
                }
            }
        }


        // final stage stack change tank
        if (ai->HasRole(BOT_ROLE_TANK) || ai->HasRole(BOT_ROLE_TANK_OFF))
        {
            if (Creature* putricide = bot->FindNearestCreature(NpcBossProfesor, 30.0f, true))
            {
                Unit* currentVictim = putricide->GetVictim();

                if (currentVictim && currentVictim->IsAlive() && currentVictim != bot)
                {
                    if (Aura* targetPlague = currentVictim->GetAura(spellMutatedPlague))
                    {
                        Aura* myPlague = bot->GetAura(spellMutatedPlague);

                        uint32 targetStacks = targetPlague->GetStackAmount();
                        uint32 myStacks = myPlague ? myPlague->GetStackAmount() : 0;

                        if (targetStacks >= 2 && targetStacks > myStacks)
                        {
                            float currentVictimThreat = putricide->GetThreatManager().GetThreat(currentVictim);

                            putricide->GetThreatManager().AddThreat(bot, currentVictimThreat + 10.0f);

                            putricide->SetInCombatWith(bot);
                            putricide->Attack(bot, true);
                            putricide->GetThreatManager().FixateTarget(bot);

                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, putricide);
                            bot->Attack(putricide, true);
                        }
                    }
                }
            }
        }

        // ooze attack
        if (!ai->HasRole(BOT_ROLE_TANK) && !ai->HasRole(BOT_ROLE_HEAL))
        {
            std::list<Creature*> NpcList; // lista generala

            bot->GetCreatureListWithEntryInGrid(NpcList, NpcVolatileOoze, 100.0f); // prioritar
            NpcList.remove_if([](Creature* npc) { return !npc->IsAlive(); }); // stergem ce nu e in viata

            if (NpcList.empty())
            {
                bot->GetCreatureListWithEntryInGrid(NpcList, NpcGasCloud, 100.0f); // urmatorul
                NpcList.remove_if([](Creature* npc) { return !npc->IsAlive(); }); // stergem ce nu e in viata
            }

            if (!NpcList.empty())
            {
                Creature* NpcTar = nullptr;
                bool iconExist = false;
                ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex5];

                NpcList.sort([](Creature* a, Creature* b) {
                    return a->GetGUID() < b->GetGUID();
                    });

                for (Creature* s : NpcList)
                {
                    if (!s->IsAlive()) continue;

                    if (s->GetGUID() == currentIconGuid)
                    {
                        iconExist = true;
                        NpcTar = s;
                        break;
                    }
                }

                if (!iconExist && gr)
                {
                    for (Creature* s : NpcList)
                    {
                        s = NpcList.front();

                        if (s->IsAlive())
                        {
                            NpcTar = s;
                            gr->SetTargetIcon(iconIndex5, bot->GetGUID(), NpcTar->GetGUID());
                        }

                        break;
                    }
                }

                if (NpcTar && NpcTar->IsAlive())
                {
                    if (bot->GetVictim() && bot->GetVictim()->GetGUID() == NpcTar->GetGUID())
                    {
                        return;
                    }

                    if (bot->GetVictim() != NpcTar)
                    {
                        bot->GetThreatManager().AddThreat(NpcTar, 300300.0f);
                        bot->SetInCombatWith(NpcTar);
                        bot->GetThreatManager().FixateTarget(NpcTar);

                        ai->AttackStart(NpcTar);
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);

                        if (ai->HasRole(BOT_ROLE_RANGED))
                        {
                            bot->Attack(NpcTar, false);
                        }
                        else
                        {
                            bot->Attack(NpcTar, true);
                        }
                    }
                }
            }
        }
    }

    void KittHandlePrinceCouncil(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        uint32 bossKeleseth = 37972;
        uint32 bossTaldaram = 37973;
        uint32 bossValanar = 37970;
        uint32 npcKineticBomb = 38454;

        // icon
        uint8 iconIndex4 = 4; // triunghi
        //uint8 iconIndex5 = 5; // patrat
        uint8 iconIndex7 = 7; // skull

        if (ai->HasRole(BOT_ROLE_TANK))
        {
            std::list<Creature*> NpcList; // lista generala

            bot->GetCreatureListWithEntryInGrid(NpcList, bossKeleseth, 100.0f);
            bot->GetCreatureListWithEntryInGrid(NpcList, bossTaldaram, 100.0f);
            bot->GetCreatureListWithEntryInGrid(NpcList, bossValanar, 100.0f);
            NpcList.remove_if([](Creature* npc) { return npc->GetHealthPct() < 1.0f; });

            if (!NpcList.empty())
            {
                NpcList.sort([](Creature* a, Creature* b) {
                    return a->GetPositionZ() < b->GetPositionZ();
                    });

                if (!NpcList.empty())
                {
                    Creature* NpcTar = nullptr;
                    bool iconExist = false;
                    ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex7];

                    NpcList.sort([](Creature* a, Creature* b) {
                        return a->GetGUID() < b->GetGUID();
                        });

                    for (Creature* s : NpcList)
                    {
                        if (!s->IsAlive()) continue;

                        if (s->GetGUID() == currentIconGuid)
                        {
                            iconExist = true;
                            NpcTar = s;
                            break;
                        }
                    }

                    if (!iconExist && gr)
                    {
                        for (Creature* s : NpcList)
                        {
                            s = NpcList.front();

                            if (s->IsAlive())
                            {
                                NpcTar = s;
                                gr->SetTargetIcon(iconIndex7, bot->GetGUID(), NpcTar->GetGUID());
                            }

                            break;
                        }
                    }

                    if (NpcTar && NpcTar->IsAlive())
                    {
                        /*if (bot->GetVictim() && bot->GetVictim()->GetGUID() == NpcTar->GetGUID())
                        {
                            return;
                        }*/

                        if (bot->GetVictim() != NpcTar)
                        {
                            bot->SetInCombatWith(NpcTar);
                            ai->AttackStart(NpcTar);
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);

                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(NpcTar, false);
                            }
                            else
                            {
                                bot->Attack(NpcTar, true);
                            }
                        }
                    }
                }
            }
        }

        if (ai->HasRole(BOT_ROLE_RANGED))
        {
            std::list<Creature*> NpcList; // lista generala

            bot->GetCreatureListWithEntryInGrid(NpcList, npcKineticBomb, 100.0f); // prioritar
            NpcList.remove_if([](Creature* npc) { return !npc->IsAlive(); }); // stergem ce nu e in viata

            if (!NpcList.empty())
            {
                NpcList.sort([](Creature* a, Creature* b) {
                    return a->GetPositionZ() < b->GetPositionZ();
                    });

                Creature* absoluteLowest = NpcList.front();
                Creature* NpcTar = absoluteLowest;

                if (NpcTar && gr)
                {
                    ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex4];
                    Creature* currentMarkedBomb = nullptr;

                    if (!currentIconGuid.IsEmpty())
                    {
                        for (Creature* s : NpcList)
                        {
                            if (s->GetGUID() == currentIconGuid)
                            {
                                currentMarkedBomb = s;
                                break;
                            }
                        }
                    }

                    if (currentMarkedBomb && currentMarkedBomb != absoluteLowest)
                    {
                        float heightDifference = currentMarkedBomb->GetPositionZ() - absoluteLowest->GetPositionZ();
                        float toleranceZ = 2.0f; // toleranta Z inaltime

                        if (heightDifference < toleranceZ)
                        {
                            NpcTar = currentMarkedBomb;
                        }
                    }

                    if (currentIconGuid != NpcTar->GetGUID())
                    {
                        gr->SetTargetIcon(iconIndex4, bot->GetGUID(), NpcTar->GetGUID());
                    }
                }

                if (NpcTar && NpcTar->IsAlive())
                {
                    if (bot->GetVictim() && bot->GetVictim()->GetGUID() == NpcTar->GetGUID())
                    {
                        return;
                    }

                    if (bot->GetVictim() != NpcTar)
                    {
                        bot->SetInCombatWith(NpcTar);

                        ai->AttackStart(NpcTar);
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);

                        if (ai->HasRole(BOT_ROLE_RANGED))
                        {
                            bot->Attack(NpcTar, false);
                        }
                        else
                        {
                            bot->Attack(NpcTar, true);
                        }
                    }
                }
            }
        }

    }

    void KittHandleBloodQueen(Creature* bot, Player* master, bot_ai* /*ai*/)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;


        //uint32 BossBloodQueen = 37955;
        uint32 NpcSwarShadows = 38163;

        if (Creature* npcSwarShado = bot->FindNearestCreature(NpcSwarShadows, 5.0f, true))
        {
            if (bot->IsNonMeleeSpellCast(true))
            {
                bot->InterruptNonMeleeSpells(true);
            }

            bot->AttackStop();
            bot->GetMotionMaster()->Clear();
            float angle = npcSwarShado->GetAbsoluteAngle(bot);
            float runDist = 7.0f;
            float x = bot->GetPositionX() + (runDist * std::cos(angle));
            float y = bot->GetPositionY() + (runDist * std::sin(angle));

            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
            {
                bot->GetMotionMaster()->MovePoint(1, x, y, bot->GetPositionZ());
            }
        }
    }

    void KittHandleValithia(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        if (!ai->HasRole(BOT_ROLE_HEAL))
            return;

        uint32 const ValithiaID = 36789;
        Creature* valithia = bot->FindNearestCreature(ValithiaID, 10.0f, true);

        if (!valithia || !valithia->IsAlive())
            return;

        if (valithia->GetHealthPct() >= 100.0f)
        {
            if (ai->GetBotCommandState() == BOT_COMMAND_STAY)
            {
                ai->RemoveBotCommandState(BOT_COMMAND_STAY);
                //ai->SetBotCommandState(BOT_COMMAND_FOLLOW);
            }

            return;
        }

        // verificam daca cineva din raid are nevoie de heal
        bool raidNeedsUrgentHeal = false;
        if (gr)
        {
            for (GroupReference* itr = gr->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || !member->IsInMap(bot) || !member->IsAlive())
                    continue;

                if (member && member->IsInMap(bot) && member->IsAlive() && member->GetHealthPct() < 60.0f)
                {
                    if (ai->GetBotCommandState() == BOT_COMMAND_STAY)
                    {
                        ai->RemoveBotCommandState(BOT_COMMAND_STAY);
                        //ai->SetBotCommandState(BOT_COMMAND_FOLLOW);
                    }

                    raidNeedsUrgentHeal = true;
                    break;
                }

                /*Unit::AuraApplicationMap const& auras = member->GetAppliedAuras();
                for (auto const& pair : auras)
                {
                    SpellInfo const* spellInfo = pair.second->GetBase()->GetSpellInfo();

                    if (pair.second->GetBase()->GetSpellInfo()->IsPositive())
                        continue;

                    uint32 dispelType = spellInfo->Dispel;
                    uint8 BotClass = bot->GetClass();

                    bool canDispel = false;
                    if (BotClass == BOT_CLASS_PRIEST && (dispelType == DISPEL_MAGIC || dispelType == DISPEL_DISEASE)) canDispel = true;
                    else if (BotClass == BOT_CLASS_PALADIN && (dispelType == DISPEL_MAGIC || dispelType == DISPEL_POISON || dispelType == DISPEL_DISEASE)) canDispel = true;
                    else if (BotClass == BOT_CLASS_SHAMAN && (dispelType == DISPEL_CURSE || dispelType == DISPEL_POISON)) canDispel = true;
                    else if (BotClass == BOT_CLASS_DRUID && (dispelType == DISPEL_CURSE || dispelType == DISPEL_POISON)) canDispel = true;

                    if (canDispel)
                    {
                        if (ai->GetBotCommandState() == BOT_COMMAND_STAY)
                            ai->RemoveBotCommandState(BOT_COMMAND_STAY);

                        raidNeedsUrgentHeal = true;
                        break;
                    }
                }


                if (raidNeedsUrgentHeal)
                {
                    break;
                }*/
            }
        }

        // 2. daca raid e ok si boss se afla in raza a 20m
        if (!raidNeedsUrgentHeal && bot->GetDistance(valithia) <= 20.0f)
        {
            uint8 BotClass = bot->GetClass();

            switch (BotClass)
            {
               case BOT_CLASS_PRIEST:
               {
                   if (!bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_STUNNED | UNIT_STATE_CHARGING))
                   {
                       // Greater Heal (Rank 9)
                       uint32 healSpell = 48063;
                       if (bot->GetPower(POWER_MANA) >= 2300)
                       {
                           if (bot->isMoving())
                           {
                               bot->StopMoving();
                           }

                           /*if (ai->GetBotCommandState() != BOT_COMMAND_STAY)
                           {
                               ai->SetBotCommandState(BOT_COMMAND_STAY);
                           }*/

                           bot->CastSpell(valithia, healSpell, false);
                       }
                   }
                   break;
               }
               case BOT_CLASS_PALADIN:
               {
                   if (!bot->HasUnitState(UNIT_STATE_CASTING))
                   {
                       // Holy Light (Rank 13)
                       uint32 healSpell = 48952;
                       if (bot->GetPower(POWER_MANA) >= 2300)
                       {
                           if (bot->isMoving())
                           {
                               bot->StopMoving();
                           }

                           bot->CastSpell(valithia, healSpell, false);
                       }
                   }
                   break;
               }
               case BOT_CLASS_SHAMAN:
               {
                   if (!bot->HasUnitState(UNIT_STATE_CASTING))
                   {
                       // Healing Wave (Rank 14)
                       uint32 healSpell = 49273;
                       if (bot->GetPower(POWER_MANA) >= 2300)
                       {
                           if (bot->isMoving())
                           {
                               bot->StopMoving();
                           }

                           bot->CastSpell(valithia, healSpell, false);
                       }
                   }
                   break;
               }
               case BOT_CLASS_DRUID:
               {
                   if (!bot->HasUnitState(UNIT_STATE_CASTING))
                   {
                       // Healing Touch (Rank 15)
                       uint32 healSpell = 48378;
                       if (bot->GetPower(POWER_MANA) >= 2300)
                       {
                           if (bot->isMoving())
                           {
                               bot->StopMoving();
                           }

                           bot->CastSpell(valithia, healSpell, false);
                       }
                   }
                   break;
               }
            }
        }
    }

    void KittHandleSindragosa(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        Map* map = master->GetMap();


        uint32 const spellFrostBeacon = 70126; // mark
        uint32 const spellAuraIceTomb = 70157; // SPELL_ICE_TOMB_DAMAGE
        uint32 const entryIceTomb   = 36980;  // id cub gheata
        uint32 const entryFrostBomb = 37186; // frost bomb
        uint32 const NpcSindragosa  = 36853; // boss sindra

        //uint8 iconIndex2 = 2; // diamant
        uint8 iconIndex5 = 5; // patrat
        uint8 iconIndex7 = 7; // skelet

        Creature* NpcTar = nullptr;



        if (bot->HasAura(spellAuraIceTomb))
            return;

        // anti drift
        if (bot->IsAlive() && bot->IsInCombat())
        {
            if (bot->GetPositionX() > 4475.49f || bot->GetPositionZ() < 202.000f)
            {
                float const callbackX = 4407.42f;
                float const callbackY = 2483.41f;
                float const callbackZ = 203.67f;
                float const callbackO = 6.27f;

                bot->AttackStop();
                //bot->StopMoving();
                bot->NearTeleportTo(callbackX, callbackY, callbackZ, callbackO);

                return;
            }
        }
        // anti drift

        if (master->HasAura(spellFrostBeacon))
        {
            float dist = bot->GetDistance(master);

            if (dist < 20.0f)
            {
                if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
                {
                    ai->SetBotCommandState(BOT_COMMAND_STAY);
                    bot->StopMoving();
                }
            }
        }
        else
        {
            if (ai->HasBotCommandState(BOT_COMMAND_STAY))
            {
                ai->RemoveBotCommandState(BOT_COMMAND_STAY);
                bot->ClearUnitState(UNIT_STATE_ROOT);
            }
        }


        Creature* sindra = bot->FindNearestCreature(NpcSindragosa, 250.0f, true);
        if (!sindra || !sindra->IsInWorld() || !sindra->IsAlive())
            return;

        float distToSindra = bot->GetDistance(sindra);
        float zDiff = std::abs(sindra->GetPositionZ() - bot->GetPositionZ());

        if (zDiff > 18.0f || distToSindra > 80.0f)
        {
            if (Creature* tomb = bot->FindNearestCreature(entryIceTomb, 200.0f, true))
            {
                if (!bot->HasUnitState(UNIT_STATE_ROOT))
                {
                    float angleTowardsTomb = sindra->GetAbsoluteAngle(tomb);
                    float distantaInSpate = 5.0f;
                    float x = tomb->GetPositionX() + (distantaInSpate * cos(angleTowardsTomb));
                    float y = tomb->GetPositionY() + (distantaInSpate * sin(angleTowardsTomb));
                    float z = tomb->GetPositionZ();

                    bot->NearTeleportTo(x, y, z, Position::NormalizeOrientation(angleTowardsTomb + M_PI));

                    bot->AddUnitState(UNIT_STATE_ROOT);

                    if (map && !ai->HasRole(BOT_ROLE_HEAL))
                    {
                        ai->SetBotCommandState(BOT_COMMAND_FULLSTOP);
                    }
                }
            }

            if (Creature* tomb = bot->FindNearestCreature(entryIceTomb, 10.0f, true))
            {
                if (bot->HasUnitState(UNIT_STATE_ROOT))
                {
                    if (Creature* bomb = bot->FindNearestCreature(entryFrostBomb, 200.0f, true))
                    {
                        bot->AttackStop();
                        float angleFromBombToTomb = bomb->GetAbsoluteAngle(tomb);
                        float distantaInSpate = 0.5f;
                        float x = tomb->GetPositionX() + (distantaInSpate * std::cos(angleFromBombToTomb));
                        float y = tomb->GetPositionY() + (distantaInSpate * std::sin(angleFromBombToTomb));
                        float z = tomb->GetPositionZ();

                        bot->NearTeleportTo(x, y, z, Position::NormalizeOrientation(angleFromBombToTomb + M_PI));
                    }
                }
            }
            return;
        }
        else
        {
            // tank tele to sindra
            if (bot->HasUnitState(UNIT_STATE_ROOT) && ai->HasRole(BOT_ROLE_TANK) && zDiff < 2)
            {
                if (bot->HasUnitState(UNIT_STATE_ROOT))
                {
                    bot->ClearUnitState(UNIT_STATE_ROOT);
                }

                if (ai->HasBotCommandState(BOT_COMMAND_FULLSTOP))
                {
                    ai->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
                }

                //bot->SetInCombatWith(sindra);
                //ai->AttackStart(sindra);

                float distanta = 5.0f;
                float angleInFata = sindra->GetOrientation();

                float x = sindra->GetPositionX() + (distanta * cos(angleInFata));
                float y = sindra->GetPositionY() + (distanta * sin(angleInFata));
                float z = sindra->GetPositionZ();

                bot->NearTeleportTo(x, y, z, angleInFata + M_PI);
                bot->GetMotionMaster()->Clear();

                if (gr)
                {
                    if (gr->GetTargetIcons()[iconIndex7] != sindra->GetGUID())
                    {
                        gr->SetTargetIcon(iconIndex7, bot->GetGUID(), sindra->GetGUID());
                    }
                }

            }

            if (bot->HasUnitState(UNIT_STATE_ROOT))
            {
                bot->ClearUnitState(UNIT_STATE_ROOT);
            }

            if (ai->HasBotCommandState(BOT_COMMAND_FULLSTOP))
            {
                ai->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
            }

            if (!ai->HasRole(BOT_ROLE_TANK) && ai->HasRole(BOT_ROLE_DPS) && (distToSindra < 80.0f || zDiff < 18.0f))
            {
                std::list<Creature*> NpcList;
                bot->GetCreatureListWithEntryInGrid(NpcList, entryIceTomb, 200.0f);

                NpcList.remove_if([](Creature* npc) {
                    return !npc || !npc->IsAlive() || !npc->IsInWorld();
                    });

                if (!NpcList.empty())
                {
                    bool iconExist = false;
                    ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex5];

                    NpcList.sort([](Creature* a, Creature* b) {
                        return a->GetGUID() < b->GetGUID();
                        });

                    NpcTar = NpcList.front();

                    /*if (gr)
                    {
                        //ObjectGuid currentIconGuid = gr->GetTargetIcons()[iconIndex5];
                        if (currentIconGuid != NpcTar->GetGUID())
                        {
                            gr->SetTargetIcon(iconIndex5, bot->GetGUID(), NpcTar->GetGUID());
                        }
                    }*/


                    if (!currentIconGuid.IsEmpty())
                    {
                        for (Creature* s : NpcList)
                        {
                            if (!s->IsAlive()) continue;

                            if (s->GetGUID() == currentIconGuid)
                            {
                                iconExist = true;
                                NpcTar = s;
                                break;
                            }
                        }
                    }

                    if (!iconExist && gr)
                    {
                        for (Creature* s : NpcList)
                        {
                            s = NpcList.front();
                            if (s && s->IsAlive())
                            {
                                NpcTar = s;
                                gr->SetTargetIcon(iconIndex5, bot->GetGUID(), NpcTar->GetGUID());
                            }
                            break;
                        }
                    }

                    if (NpcTar && NpcTar->IsAlive())
                    {
                        float zDiffTomb = bot->GetDistance(NpcTar);

                        if (zDiffTomb < 3.0f && ai->HasRole(BOT_ROLE_RANGED))
                        {
                            bot->GetMotionMaster()->Clear();
                            float angle = NpcTar->GetAbsoluteAngle(bot);
                            float runDist = 8.0f; // 8
                            float x = bot->GetPositionX() + (runDist * std::cos(angle));
                            float y = bot->GetPositionY() + (runDist * std::sin(angle));

                            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                            {
                                bot->GetMotionMaster()->MovePoint(102, x, y, bot->GetPositionZ());
                            }
                        }

                        if (bot->GetVictim() != NpcTar)
                        {
                            bot->SetInCombatWith(NpcTar);
                            NpcTar->SetInCombatWith(bot);

                            ai->AttackStart(NpcTar);

                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(NpcTar, false);
                                //bot->GetMotionMaster()->MoveChase(NpcTar, 8.0f); // 15
                            }
                            else
                            {
                                bot->Attack(NpcTar, true);
                                //bot->GetMotionMaster()->MoveChase(NpcTar);
                            }
                        }
                    }
                }
                return;
            }
        }


    }

    void KittHandleLichKing(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!master || !master->IsInWorld() || !master->GetSession())
            return;

        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        time_t currentTimeMS = GameTime::GetGameTimeMS();

        uint32 const npcBossLichKing = 36597;
        uint32 const npcDefileTrigger = 38757;
        uint32 const npcShadowTrapTrigger = 39137;
        uint32 const npcIceSphere = 36633;
        uint32 const npcValkyr = 36609;
        uint32 const npcShamblingHorror = 37698;
        uint32 const npcRacingSpirit = 36701;
        uint32 const npcDrudgeGhoul = 37695;


        // harvest souls aura 74297(main)
        uint32 const spellHarvestSouls = 73655; // 25hc/10hc // in camera
        uint32 const spellHarvestSoul = 72546; // 25n/10n // in camera
        uint32 const spellFurryFrostNoRez = 72351; // aura no rez

        // buff inainte de teleportare
        uint32 spellHarvestSoulStartHC = 0; // heroic
        uint32 spellHarvestSoulStartN = 0; // normal
        uint32 const spellHarvestSouls25HC = 74297; // 25 heroic
        uint32 const spellHarvestSouls10HC = 74296; // 10 heroic
        uint32 const spellHarvestSoul25N = 74325; // 25 normal
        uint32 const spellHarvestSoul10N = 68980; // 10 normal

        uint32 spellNecroticPlague = 0;
        uint32 const spellNecroticPlague10N = 70337;
        uint32 const spellNecroticPlague10HC = 73913;
        uint32 const spellNecroticPlague25N = 73912;
        uint32 const spellNecroticPlague25HC = 73914;

        // intre faze aura
        uint32 spell1Winter = 0; // 74272 25hc // 74271 10hc // 74270 25N // 68981 10N
        uint32 const spell1Winter10N = 68981;
        uint32 const spell1Winter25N = 74270;
        uint32 const spell1Winter10HC = 74271;
        uint32 const spell1Winter25HC = 74272;

        uint32 spell2Winter = 0; // 74275 25hc // 74274 10hc // 74273 25N  // 72259 10N
        uint32 const spell2Winter10N = 72259;
        uint32 const spell2Winter25N = 74273;
        uint32 const spell2Winter10HC = 74274;
        uint32 const spell2Winter25HC = 74275;


        bool DefilesPrezent = false;
        bool ShadowTrapPrez = false;

        Map* map = master->GetMap();
        if (map)
        {
            if (map->IsHeroic())
            {
                if (map->Is25ManRaid())
                {
                    spellNecroticPlague = spellNecroticPlague25HC;
                    spell1Winter = spell1Winter25HC;
                    spell2Winter = spell2Winter25HC;
                    spellHarvestSoulStartHC = spellHarvestSouls25HC;

                }
                else
                {
                    spellNecroticPlague = spellNecroticPlague10HC;
                    spell1Winter = spell1Winter10HC;
                    spell2Winter = spell2Winter10HC;
                    spellHarvestSoulStartHC = spellHarvestSouls10HC;
                }
            }
            else
            {
                if (map->Is25ManRaid())
                {
                    spellNecroticPlague = spellNecroticPlague25N;
                    spell1Winter = spell1Winter25N;
                    spell2Winter = spell2Winter25N;
                    spellHarvestSoulStartN = spellHarvestSoul25N;
                }
                else
                {
                    spellNecroticPlague = spellNecroticPlague10N;
                    spell1Winter = spell1Winter10N;
                    spell2Winter = spell2Winter10N;
                    spellHarvestSoulStartN = spellHarvestSoul10N;
                }
            }
        }

        if (Creature* bossLichK = bot->FindNearestCreature(npcBossLichKing, 80.0f, true))
        {
            bool isWinter = (bossLichK->HasAura(spell1Winter) || bossLichK->HasAura(spell2Winter));

            // cand face cast sa fuga toti
            if (bossLichK->HasUnitState(UNIT_STATE_CASTING))
            {
                if (Spell const* spell = bossLichK->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                {
                    uint32 spellcast = spell->GetSpellInfo()->Id;

                    if (spellcast == spell1Winter || spellcast == spell2Winter)
                    {
                        isWinter = true;
                    }
                }
            }

            if (isWinter)
            {
                float angle = bossLichK->GetOrientation() + 1.57f;
                float dist = 55.0f; // distanta de siguranta

                float targetX = bossLichK->GetPositionX() + (dist * std::cos(angle));
                float targetY = bossLichK->GetPositionY() + (dist * std::sin(angle));
                float targetZ = bossLichK->GetPositionZ();

                if (bot->GetDistance(targetX, targetY, targetZ) > 5.0f)
                {
                    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                    {
                        bot->GetMotionMaster()->MovePoint(9, targetX, targetY, targetZ);
                    }
                }
            }

            // 1. shadow trap, faza 1
            if (Creature* trap = bot->FindNearestCreature(npcShadowTrapTrigger, 8.0f, true)) // 5
            {
                if (bot->IsNonMeleeSpellCast(true))
                {
                    bot->InterruptNonMeleeSpells(true);
                }

                bot->AttackStop();
                bot->GetMotionMaster()->Clear();
                float angle = trap->GetAbsoluteAngle(bot);
                float runDist = 11.0f; // 8
                float x = bot->GetPositionX() + (runDist * std::cos(angle));
                float y = bot->GetPositionY() + (runDist * std::sin(angle));

                if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                {
                    bot->GetMotionMaster()->MovePoint(2, x, y, bot->GetPositionZ());
                }
                ShadowTrapPrez = true;
                //return;
            }

            // 2. Defile, faza 2
            if (Creature* defile = bot->FindNearestCreature(npcDefileTrigger, 40.0f, true))
            {
                float currentScale = defile->GetObjectScale();
                float safetyMargin = 2.0f;
                float dynamicRadius = (8.0f * currentScale) + safetyMargin;

                float distToDefile = bot->GetDistance(defile);

                if (distToDefile < dynamicRadius)
                {
                    if (bot->IsNonMeleeSpellCast(true))
                        bot->InterruptNonMeleeSpells(true);

                    bot->AttackStop();

                    float angle = defile->GetAbsoluteAngle(bot);
                    float runDist = (dynamicRadius - distToDefile) + 10.0f;
                    float x = bot->GetPositionX() + (runDist * std::cos(angle));
                    float y = bot->GetPositionY() + (runDist * std::sin(angle));

                    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                    {
                        bot->GetMotionMaster()->MovePoint(1, x, y, bot->GetPositionZ());
                    }
                    DefilesPrezent = true;
                    //return;
                }
            }

            // 3. sphere, intre faze
            if (Creature* sphere = bot->FindNearestCreature(npcIceSphere, 35.0f, true))
            {
                if (ai->HasRole(BOT_ROLE_RANGED) && ai->HasRole(BOT_ROLE_DPS))
                {
                    if (bot->GetVictim() != sphere)
                    {
                        bot->AttackStop();
                        bot->GetMotionMaster()->Clear();
                        if (bot->IsNonMeleeSpellCast(true))
                        {
                            bot->InterruptNonMeleeSpells(true);
                        }

                        if (gr)
                        {
                            if (sphere && gr->GetTargetIcons()[4] != sphere->GetGUID())
                            {
                                gr->SetTargetIcon(4, bot->GetGUID(), sphere->GetGUID());
                            }
                        }
                        bot->SetInCombatWith(sphere);
                        ai->AttackStart(sphere);
                        bot->Attack(sphere, false);
                    }
                }
            }

            // 5. valkyr, faza 2
            if (Creature* valkyr = bot->FindNearestCreature(npcValkyr, 40.0f, true))
            {
                //bool arePasager = valkyr->GetVehicleKit() && valkyr->GetVehicleKit()->GetPassenger(0);
                if (valkyr->IsAlive() && !valkyr->HasUnitFlag(UNIT_FLAG_UNINTERACTIBLE)/* && valkyr->GetHealthPct() > 48.0f*/)
                {
                    if (ai->HasRole(BOT_ROLE_DPS) && !ai->HasRole(BOT_ROLE_HEAL) && !ai->HasRole(BOT_ROLE_TANK))
                    {
                        if (bot->GetVictim() != valkyr)
                        {
                            bot->AttackStop();
                            //bot->GetMotionMaster()->Clear();
                            if (bot->IsNonMeleeSpellCast(true))
                            {
                                bot->InterruptNonMeleeSpells(true);
                            }

                            if (gr)
                            {
                                if (valkyr && gr->GetTargetIcons()[5] != valkyr->GetGUID())
                                {
                                    gr->SetTargetIcon(5, bot->GetGUID(), valkyr->GetGUID());
                                }
                            }

                            bot->SetInCombatWith(valkyr);
                            ai->AttackStart(valkyr);

                            if (bot->GetBotAI()->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(valkyr, false);
                            }
                            else
                            {
                                bot->Attack(valkyr, true);
                            }
                        }
                    }
                }
                else
                {
                    if (bot->GetVictim() == valkyr)
                    {
                        if (gr)
                        {
                            if (valkyr && gr->GetTargetIcons()[5] == valkyr->GetGUID())
                            {
                                gr->SetTargetIcon(5, bot->GetGUID(), ObjectGuid::Empty);
                            }
                        }
                        bot->AttackStop();
                    }
                }
            }

            // Drudge Ghoul
            std::list<Creature*> npcdrudgeList;
            bot->GetCreatureListWithEntryInGrid(npcdrudgeList, npcDrudgeGhoul, 50.0f);

            if (!npcdrudgeList.empty())
            {
                float const centerZ = 841.90f;

                if (bot->GetPositionZ() < 839.0f)
                {
                    bot->NearTeleportTo(bot->GetPositionX(), bot->GetPositionY(), centerZ, bot->GetOrientation());
                    bot->GetMotionMaster()->Clear();
                }

                for (Creature* npcdrudge : npcdrudgeList)
                {
                    if (!npcdrudge || !npcdrudge->IsAlive())
                        continue;

                    if (npcdrudge->GetPositionZ() < 839.0f)
                    {
                        // --- anti-flood / curatare mapa ---
                        // -- se pune doar in primul loop --
                        if (teleportCooldownMap.size() > 100)
                        {
                            teleportCooldownMap.clear();
                        }
                        // --------------------------------------------

                        float tx = npcdrudge->GetPositionX();
                        float ty = npcdrudge->GetPositionY();

                        if (teleportCooldownMap[npcdrudge->GetGUID()] <= currentTimeMS)
                        {
                            npcdrudge->NearTeleportTo(tx, ty, centerZ, npcdrudge->GetOrientation());
                            teleportCooldownMap[npcdrudge->GetGUID()] = currentTimeMS + TELEPORT_CD;

                            npcdrudge->GetMotionMaster()->Clear();

                            if (Unit* target = npcdrudge->GetVictim())
                            {
                                npcdrudge->GetMotionMaster()->MoveChase(target);
                            }
                            else
                            {
                                npcdrudge->GetMotionMaster()->MoveChase(bot);
                            }
                        }
                    }
                }
            }

            // Spirit Racing
            std::list<Creature*> npcspiritList;
            bot->GetCreatureListWithEntryInGrid(npcspiritList, npcRacingSpirit, 50.0f);

            if (!npcspiritList.empty())
            {
                float const centerZ = 841.90f;

                if (bot->GetPositionZ() < 839.0f)
                {
                    bot->NearTeleportTo(bot->GetPositionX(), bot->GetPositionY(), centerZ, bot->GetOrientation());
                    bot->GetMotionMaster()->Clear();
                }

                for (Creature* spirit : npcspiritList)
                {
                    if (!spirit || !spirit->IsAlive())
                        continue;

                    if (spirit->GetPositionZ() < 839.0f)
                    {
                        float tx = spirit->GetPositionX();
                        float ty = spirit->GetPositionY();

                        if (teleportCooldownMap[spirit->GetGUID()] <= currentTimeMS)
                        {
                            spirit->NearTeleportTo(tx, ty, centerZ, spirit->GetOrientation());
                            teleportCooldownMap[spirit->GetGUID()] = currentTimeMS + TELEPORT_CD;

                            spirit->GetMotionMaster()->Clear();

                            if (Unit* target = spirit->GetVictim())
                            {
                                spirit->GetMotionMaster()->MoveChase(target);
                            }
                            else
                            {
                                spirit->GetMotionMaster()->MoveChase(bot);
                            }
                        }
                    }
                }
            }

            // Definim o singura locatie sigura pentru ambele mecanici (Ora 2 pe platforma)
            float safeX = 485.707f;
            float safeY = -2088.52f;
            float const safeZ = 840.90f;

            // 1. LOGICA PENTRU NECROTIC PLAGUE (Dinamica)
            if (bot->HasAura(spellNecroticPlague))
            {
                if (Creature* horror = bot->FindNearestCreature(npcShamblingHorror, 50.0f, true))
                {
                    if (bot->GetDistance(horror) > 3.0f)
                    {
                        if (bot->IsNonMeleeSpellCast(true))
                        {
                            bot->InterruptNonMeleeSpells(true);
                        }

                        bot->AttackStop();

                        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                        {
                            bot->GetMotionMaster()->MovePoint(4, horror->GetPositionX(), horror->GetPositionY(), horror->GetPositionZ());
                        }
                    }
                    else
                    {
                        if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
                        {
                            ai->SetBotCommandState(BOT_COMMAND_STAY);
                        }
                    }
                }
                else // Daca nu exista niciun Horror pe harta, mergem la punctul fix
                {
                    if (bot->GetDistance(safeX, safeY, safeZ) > 3.0f)
                    {
                        if (bot->IsNonMeleeSpellCast(true))
                        {
                            bot->InterruptNonMeleeSpells(true);
                        }

                        bot->AttackStop();

                        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                        {
                            bot->GetMotionMaster()->MovePoint(5, safeX, safeY, safeZ);
                        }
                    }
                    else
                    {
                        if (!ai->HasBotCommandState(BOT_COMMAND_STAY))
                        {
                            ai->SetBotCommandState(BOT_COMMAND_STAY);
                        }
                    }
                }
            }
            else if (ai->HasBotCommandState(BOT_COMMAND_STAY))
            {
                // Scoatem STAY
                ai->RemoveBotCommandState(BOT_COMMAND_STAY);
            }


            // 2. LOGICA PENTRU OFF-TANK
            if (ai->HasRole(BOT_ROLE_TANK_OFF))
            {
                std::list<Creature*> horrorList;
                bot->GetCreatureListWithEntryInGrid(horrorList, npcShamblingHorror, 80.0f);

                if (!horrorList.empty())
                {
                    ObjectGuid currentIcon6 = (gr) ? gr->GetTargetIcons()[6] : ObjectGuid::Empty;

                    bool iconAlreadyOnAHorror = false;

                    for (Creature* horror : horrorList)
                    {
                        if (!horror || !horror->IsAlive())
                            continue;

                        Unit* hVictim = horror->GetVictim();

                        // Adaugam threat pe fiecare ads din lista
                        if (hVictim && hVictim != bot)
                        {
                            horror->GetThreatManager().AddThreat(bot, 3000300.0f); // Adaugam intrarea in lista
                            horror->GetThreatManager().MatchUnitThreatToHighestThreat(bot); // il pun pe locul 1
                            horror->GetThreatManager().FixateTarget(bot);


                            horror->SetInCombatWith(bot);

                            if (UnitAI* horrorAi = horror->GetAI())
                            {
                                horrorAi->AttackStart(bot);
                            }
                        }

                        // setare icon
                        if (gr && !currentIcon6.IsEmpty() && horror->GetGUID() == currentIcon6)
                        {
                            iconAlreadyOnAHorror = true;
                        }
                    }

                    if (Creature* nearhorror = bot->FindNearestCreature(npcShamblingHorror, 5.0f, true))
                    {
                        if (gr)
                        {
                            // CROSS (6) pe Shambling Horror.
                            if (!iconAlreadyOnAHorror && gr->GetTargetIcons()[6] != nearhorror->GetGUID())
                            {
                                gr->SetTargetIcon(6, bot->GetGUID(), nearhorror->GetGUID());
                            }

                            if (bossLichK && !bossLichK->HasAura(spell1Winter) && !bossLichK->HasAura(spell2Winter))
                            {
                                if (bot->GetHealthPct() < 90.0f)
                                {
                                    if (gr->GetTargetIcons()[3] != bot->GetGUID())
                                    {
                                        gr->SetTargetIcon(3, bot->GetGUID(), bot->GetGUID());
                                    }
                                }
                                else
                                {
                                    if (gr->GetTargetIcons()[3] == bot->GetGUID())
                                    {
                                        gr->SetTargetIcon(3, bot->GetGUID(), ObjectGuid::Empty);
                                    }
                                }
                            }
                        }

                        if (bot->GetVictim() != nearhorror)
                        {
                            bot->SetInCombatWith(nearhorror);
                            ai->AttackStart(nearhorror);
                            bot->Attack(nearhorror, true);
                        }
                        
                    }

                    // --- LOGICA DE MISCARE INTELIGENTA PENTRU OT ---
                    if (ShadowTrapPrez || DefilesPrezent || isWinter)
                    {
                        // Daca exista un Shadow Trap fix in calea lui sau sub el
                        if (Creature* nearTrap = bot->FindNearestCreature(npcShadowTrapTrigger, 8.0f, true))
                        {
                            // Daca e o capcana aproape, OT nu se mai duce orbeste la SafeX
                            // Il fortam sa se miste lateral fata de capcana (unghi de 90 grade) ca sa o ocoleasca
                            float avoidAngle = nearTrap->GetAbsoluteAngle(bot) + 1.57f; // 90 grade fata de capcana
                            float dist = 10.0f;
                            float ax = bot->GetPositionX() + (dist * std::cos(avoidAngle));
                            float ay = bot->GetPositionY() + (dist * std::sin(avoidAngle));

                            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                            {
                                bot->GetMotionMaster()->MovePoint(7, ax, ay, bot->GetPositionZ());
                            }
                        }
                        /*else if (isWinter)
                        {
                            if (bot->GetDistance(safeX2, safeY2, safeZ) > 1.0f)
                            {
                                bot->GetMotionMaster()->MovePoint(8, safeX2, safeY2, safeZ);
                            }
                        }*/
                        else if (bot->GetDistance(safeX, safeY, safeZ) > 15.0f)
                        {
                            // Daca nu are nicio capcana imediata, merge spre SafeX (ajustat)
                            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                            {
                                bot->GetMotionMaster()->MovePoint(6, safeX + 5.0f, safeY + 5.0f, safeZ);
                            }
                        }
                    }
                    else
                    {
                        // Cand zona e curata, sta la Safe Point
                        if (bot->GetDistance(safeX, safeY, safeZ) > 10.0f)
                        {
                            if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                            {
                                bot->GetMotionMaster()->MovePoint(6, safeX, safeY, safeZ);
                            }
                        }
                    }
                }
            }

            // boss aura faza centru, faza armei, faza de final
            // set icon
            if (gr)
            {
                bool iconRezervat = false;
                ObjectGuid bossGUID = bossLichK->GetGUID();


                if (!iconRezervat && bossLichK->IsAlive() && bossLichK->IsInWorld())
                {
                    if (master && bot)
                    {
                        if (isWinter || bossLichK->HasAura(spell1Winter) || bossLichK->HasAura(spell2Winter)
                            || master->HasAura(spellHarvestSoulStartHC) || bot->HasAura(spellHarvestSoulStartHC)
                            || master->HasAura(spellHarvestSouls) || bot->HasAura(spellHarvestSouls)
                            || master->HasAura(spellFurryFrostNoRez) || bot->HasAura(spellFurryFrostNoRez))
                        {
                            // setam Diamond
                            if (gr)
                            {
                                if (gr->GetTargetIcons()[2] != bossGUID)
                                {
                                    gr->SetTargetIcon(2, bot->GetGUID(), bossGUID);
                                }
                            }
                            iconRezervat = true;
                        }
                    }
                }

                if (/*!iconRezervat && */ai->HasRole(BOT_ROLE_TANK) && bossLichK->IsAlive() && bossLichK->IsInWorld())
                {
                    if (master && bot)
                    {
                        if (master->HasAura(spellHarvestSoulStartN) || bot->HasAura(spellHarvestSoulStartN)
                            || master->HasAura(spellHarvestSoul) || bot->HasAura(spellHarvestSoul))
                        {
                            Unit* victim = bossLichK->GetVictim();
                            // setam Triunghi pe tank
                            if (victim)
                            {
                                ObjectGuid victimGUID = victim->GetGUID();
                                if (gr->GetTargetIcons()[3] != victimGUID)
                                {
                                    gr->SetTargetIcon(3, bot->GetGUID(), victimGUID);
                                    triangleTargetGUID = victimGUID;
                                    //iconRezervat = true;
                                }
                            }
                        }
                        else
                        {
                            // Daca triunghi este inca pe cineva marcat de noi
                            if (!triangleTargetGUID.IsEmpty())
                            {
                                if (gr->GetTargetIcons()[3] == triangleTargetGUID)
                                {
                                    gr->SetTargetIcon(3, bot->GetGUID(), ObjectGuid::Empty);
                                }
                                triangleTargetGUID = ObjectGuid::Empty;
                            }
                        }
                    }
                }

                if (!iconRezervat)
                {
                    if (gr)
                    {
                        if (gr->GetTargetIcons()[2] == bossGUID)
                        {
                            gr->SetTargetIcon(2, bot->GetGUID(), ObjectGuid::Empty); // sterge intex vechi
                            gr->SetTargetIcon(7, bot->GetGUID(), bossGUID); // set skull
                        }
                    }
                }
            }
        }
    }


}
