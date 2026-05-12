//----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "kitt_npcbot_ai.h"
#include "Player.h"
#include "Creature.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "botspell.h"
#include "GameTime.h"
#include "Group.h"
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

        if (mapId == 631)
        {
            switch (areaId)
            {
                case 4812: // Marrowgar
                {
                    KittHandleMarrowgar(bot, master, ai); // marrow
                    KittHandleLadyDeathwhisper(bot, master, ai); // lady
                    KittHandleGunship(bot, master, ai); // gunship
                    KittHandleValithia(bot, master, ai); // Valithia
                    break;
                }
                case 4890: // profesor
                {
                    KittHandlePutricide(bot, master, ai);
                    break;
                }

                case 4889: // sindragosa
                {
                    KittHandleSindragosa(bot, master, ai);
                    break;
                }
                case 4859: // Lich King
                {
                    KittHandleLichKing(bot, master, ai);
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

        uint32 const NpcSpike = 36619;

        if (ai->HasRole(BOT_ROLE_TANK) || ai->HasRole(BOT_ROLE_HEAL))
            return;

        std::list<Creature*> allSpikes;
        bot->GetCreatureListWithEntryInGrid(allSpikes, NpcSpike, 60.0f);
        if (allSpikes.empty())
            return;

        Creature* NpcSpikeTar = nullptr;
        bool iconDejaExista = false;
        ObjectGuid currentIconGuid = gr->GetTargetIcons()[4];

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
                if (s->IsAlive())
                {
                    NpcSpikeTar = s;
                    gr->SetTargetIcon(4, bot->GetGUID(), NpcSpikeTar->GetGUID());
                    break;
                }
            }
        }

        if (NpcSpikeTar)
        {
            if (bot->GetVictim() != NpcSpikeTar)
            {
                bot->AttackStop();
                bot->SetInCombatWith(NpcSpikeTar);
                bot->GetThreatManager().FixateTarget(NpcSpikeTar);
                bot->Attack(NpcSpikeTar, !ai->HasRole(BOT_ROLE_RANGED));
                ai->AttackStart(NpcSpikeTar);
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
        uint32 const NpcCult  = 37949;
        uint32 const NpcEmpow = 38136;
        uint32 const NpcReani = 38010;
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
                            if (!member->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED | UNIT_STATE_ROOT))
                            {
                                float dist = bot->GetDistance(member);
                                float razaCast = 8.0f; // 8 cast

                                if (dist > razaCast)
                                {
                                    float x, y, z;
                                    member->GetContactPoint(bot, x, y, z, razaCast);
                                    //bot->GetMotionMaster()->Clear();
                                    bot->GetMotionMaster()->MovePoint(1, x, y, z);
                                }
                                else
                                {
                                    switch (bot->GetBotClass())
                                    {
                                       case BOT_CLASS_PALADIN:
                                       {
                                           // Hammer of Justice (ID: 10308)
                                           bot->CastSpell(member, 10308, true);
                                           break;
                                       }
                                       case BOT_CLASS_MAGE:
                                       {
                                           // Polymorph (ID: 118)
                                           bot->CastSpell(member, 118, true);
                                           break;
                                       }
                                       case BOT_CLASS_ROGUE:
                                       {
                                           // Blind (ID: 2094)
                                           bot->CastSpell(member, 2094, true);
                                           break;
                                       }
                                       case BOT_CLASS_DRUID:
                                       {
                                           // Cyclone (ID: 33786)
                                           bot->AttackStop();
                                           bot->CastSpell(member, 33786, true);
                                           break;
                                       }
                                       case BOT_CLASS_WARRIOR:
                                       {
                                           // Concussion Blow sau Intercept Stun
                                           break;
                                       }
                                       case BOT_CLASS_WARLOCK:
                                       {
                                           bot->CastSpell(member, 5782, true);  // Fear
                                           break;
                                       }
                                       case BOT_CLASS_HUNTER:
                                       {
                                           //bot->CastSpell(member, 19503, true); // Scatter Shot
                                           bot->CastSpell(member, 14311, true); // Freezing Trap
                                           break;
                                       }
                                       case BOT_CLASS_PRIEST:
                                       {
                                           bot->CastSpell(member, 8122, true);  // Psychic Scream
                                           break;
                                       }
                                       case BOT_CLASS_SHAMAN:
                                       {
                                           bot->CastSpell(member, 51514, true); // Hex
                                           break;
                                       }
                                       case BOT_CLASS_DEATH_KNIGHT:
                                       {
                                           //bot->CastSpell(member, 47476, true); // Strangulate
                                           break;
                                       }
                                    }
                                }
                            }
                        }
                        return;
                    }
                }
            }

            if (!ai->HasRole(BOT_ROLE_TANK) && !ai->HasRole(BOT_ROLE_HEAL))
            {
                if (Creature* NpcAds1 = bot->FindNearestCreature(NpcCult, 40.0f, true))
                {
                    if (NpcAds1->IsInWorld() && NpcAds1->IsAlive())
                    {
                        if (bot->GetVictim() != NpcAds1)
                        {
                            bot->SetInCombatWith(NpcAds1);
                            //NpcAds1->SetInCombatWith(bot);

                            //bot->Attack(NpcAds1, true);
                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(NpcAds1, false);
                            }
                            else
                            {
                                bot->Attack(NpcAds1, true);
                            }
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds1);
                        }
                        return;
                    }
                }
                else if (Creature* NpcAds2 = bot->FindNearestCreature(NpcEmpow, 40.0f, true))
                {
                    if (NpcAds2->IsInWorld() && NpcAds2->IsAlive())
                    {
                        if (bot->GetVictim() != NpcAds2)
                        {
                            bot->SetInCombatWith(NpcAds2);
                            //NpcAds2->SetInCombatWith(bot);

                            //bot->Attack(NpcAds2, true);
                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(NpcAds2, false);
                            }
                            else
                            {
                                bot->Attack(NpcAds2, true);
                            }
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds2);
                        }
                        return;
                    }
                }
                else if (Creature* NpcAds3 = bot->FindNearestCreature(NpcReani, 40.0f, true))
                {
                    if (NpcAds3->IsInWorld() && NpcAds3->IsAlive())
                    {
                        if (bot->GetVictim() != NpcAds3)
                        {
                            bot->SetInCombatWith(NpcAds3);
                            //NpcAds3->SetInCombatWith(bot);

                            //bot->Attack(NpcAds3, true);
                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(NpcAds3, false);
                            }
                            else
                            {
                                bot->Attack(NpcAds3, true);
                            }
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds3);
                        }
                        return;
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

        Group* gr = master->GetGroup();
        if (!gr)
            return;

        Transport* mTrans = master->GetTransport();
        if (!mTrans)
            return;

        uint32 const SpellFreez = 69705; // channel spell

        uint32 const NpcMageH  = 37117; // Mage
        uint32 const NpcAxethH = 36968; // Margine

        uint32 const NpcSorcererA = 37116; // Mage
        uint32 const NpcRiflemanA = 36969; // margine


        uint32 const TransAlliance = 201580;
        uint32 const TransHorde    = 201812;

        if (mTrans->GetEntry() != TransAlliance && mTrans->GetEntry() != TransHorde)
            return;


        if (mTrans && mTrans->GetEntry() == TransAlliance) // alliance
        {
            if (master->GetTeamId() == TEAM_HORDE)
                return;

            if (!ai->HasRole(BOT_ROLE_HEAL) && !ai->HasRole(BOT_ROLE_TANK))
            {
                if (Creature* NpcAds1a = bot->FindNearestCreature(NpcMageH, 100.0f, true))
                {
                    if (NpcAds1a->IsInWorld() && NpcAds1a->IsAlive())
                    {
                        if (NpcAds1a && NpcAds1a->IsAlive() && NpcAds1a->GetChannelSpellId() == SpellFreez)
                        {
                            if (bot->GetVictim() != NpcAds1a)
                            {
                                bot->SetInCombatWith(NpcAds1a);
                                //NpcAds1a->SetInCombatWith(bot);
                                bot->GetThreatManager().FixateTarget(NpcAds1a);

                                if (ai->HasRole(BOT_ROLE_RANGED))
                                {
                                    bot->Attack(NpcAds1a, false);
                                }
                                else
                                {
                                    bot->Attack(NpcAds1a, true);
                                }
                                ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                                ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds1a);
                            }
                            return;
                        }
                    }
                }
                else if (Creature* NpcAds3a = bot->FindNearestCreature(NpcAxethH, 100.0f, true))
                {
                    if (NpcAds3a->IsInWorld() && NpcAds3a->IsAlive())
                    {
                        if (bot->GetVictim() != NpcAds3a)
                        {
                            bot->SetInCombatWith(NpcAds3a);
                            //NpcAds2a->SetInCombatWith(bot);
                            //bot->GetThreatManager().FixateTarget(NpcAds3a);
                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(NpcAds3a, false);
                            }
                            else
                            {
                                bot->Attack(NpcAds3a, true);
                            }

                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds3a);
                        }
                        return;
                    }
                }

                /*if (Creature* NpcAds2a = bot->FindNearestCreature(NpcSoketH, 100.0f, true))
                {
                    if (NpcAds2a && NpcAds2a->IsAlive() && NpcAds2a->IsInWorld())
                    {
                        ObjectGuid guid = NpcAds2a->GetGUID();

                        if (!guid.IsCreature() || !NpcAds2a->IsAlive())
                            return;

                        bool isAlreadyInMemory = activatedRocketeers.find(guid) != activatedRocketeers.end();

                        Spell* currentSpell = NpcAds2a ? NpcAds2a->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
                        bool isCastingNow = (currentSpell != nullptr);

                        if (isAlreadyInMemory)
                        {
                            //bot->GetThreatManager().AddThreat(NpcAds2a, 5000.0f);
                            //NpcAds2a->GetThreatManager().AddThreat(bot, 5000.0f);

                            if (bot->GetVictim() != NpcAds2a)
                            {
                                bot->SetInCombatWith(NpcAds2a);
                                //NpcAds2a->SetInCombatWith(bot);
                                //bot->GetThreatManager().FixateTarget(NpcAds2a);
                                //bot->Attack(NpcAds2a, true);
                                if (ai->HasRole(BOT_ROLE_RANGED))
                                {
                                    bot->Attack(NpcAds2a, false);
                                }
                                else
                                {
                                    bot->Attack(NpcAds2a, true);
                                }

                                ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                                ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds2a);
                            }
                            return;
                        }

                        if (isCastingNow)
                        {
                            activatedRocketeers.insert(guid);
                            if (activatedRocketeers.size() > 200) activatedRocketeers.clear();

                            return;
                        }
                    }
                }*/
            }
        }

        if (mTrans && mTrans->GetEntry() == TransHorde) // horde
        {
            if (master->GetTeamId() == TEAM_ALLIANCE)
                return;

            if (!ai->HasRole(BOT_ROLE_HEAL) && !ai->HasRole(BOT_ROLE_TANK))
            {
                if (Creature* NpcAds1a = bot->FindNearestCreature(NpcSorcererA, 100.0f, true))
                {
                    if (NpcAds1a->IsInWorld() && NpcAds1a->IsAlive())
                    {
                        if (NpcAds1a && NpcAds1a->IsAlive() && NpcAds1a->GetChannelSpellId() == SpellFreez)
                        {
                            if (bot->GetVictim() != NpcAds1a)
                            {
                                bot->SetInCombatWith(NpcAds1a);
                                //NpcAds1a->SetInCombatWith(bot);
                                bot->GetThreatManager().FixateTarget(NpcAds1a);

                                if (ai->HasRole(BOT_ROLE_RANGED))
                                {
                                    bot->Attack(NpcAds1a, false);
                                }
                                else
                                {
                                    bot->Attack(NpcAds1a, true);
                                }
                                ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                                ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds1a);
                            }
                            return;
                        }
                    }
                }
                else if (Creature* NpcAds3a = bot->FindNearestCreature(NpcRiflemanA, 100.0f, true))
                {
                    if (NpcAds3a->IsInWorld() && NpcAds3a->IsAlive())
                    {
                        if (bot->GetVictim() != NpcAds3a)
                        {
                            bot->SetInCombatWith(NpcAds3a);
                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(NpcAds3a, false);
                            }
                            else
                            {
                                bot->Attack(NpcAds3a, true);
                            }

                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds3a);
                        }
                        return;
                    }
                }
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

        // final stage stack change tank
        if (ai->HasRole(BOT_ROLE_TANK) || ai->HasRole(BOT_ROLE_TANK_OFF)) //|| ai->HasRole(BOT_ROLE_DPS))
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

                            //putricide->GetThreatManager().ModifyThreatByPercent(currentVictim, -100);
                            //putricide->GetThreatManager().ModifyThreatByPercent(bot, 100);

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
            if (Creature* npcozeverde = bot->FindNearestCreature(NpcVolatileOoze, 60.0f, true))
            {
                if (npcozeverde->IsInWorld() && npcozeverde->IsAlive())
                {
                    if (bot->GetVictim() != npcozeverde)
                    {
                        bot->SetInCombatWith(npcozeverde);
                        //npcozeverde->SetInCombatWith(bot);

                        //bot->Attack(npcozeverde, true);
                        if (ai->HasRole(BOT_ROLE_RANGED))
                        {
                            bot->Attack(npcozeverde, false);
                        }
                        else
                        {
                            bot->Attack(npcozeverde, true);
                        }
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                        ai->BotMovement(BOT_MOVE_CHASE, nullptr, npcozeverde);
                    }
                    return;
                }
            }
            else
            {
                if (Creature* npcgasoze = bot->FindNearestCreature(NpcGasCloud, 60.0f, true))
                {
                    if (npcgasoze->IsInWorld() && npcgasoze->IsAlive())
                    {
                        if (bot->GetVictim() != npcgasoze)
                        {
                            bot->SetInCombatWith(npcgasoze);
                            //npcgasoze->SetInCombatWith(bot);

                            //bot->Attack(npcgasoze, true);
                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(npcgasoze, false);
                            }
                            else
                            {
                                bot->Attack(npcgasoze, true);
                            }
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, npcgasoze);
                        }
                        return;
                    }
                }
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
        Group* group = master->GetGroup();
        if (group)
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
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
        if (!raidNeedsUrgentHeal && bot->GetDistance(valithia) <= 10.0f)
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

        uint32 const spellFrostBeacon = 70126; // mark
        uint32 const spellAuraIceTomb = 70157; // SPELL_ICE_TOMB_DAMAGE
        uint32 const entryIceTomb   = 36980;  // id cub gheata
        uint32 const entryFrostBomb = 37186; // frost bomb
        uint32 const NpcSindragosa  = 36853; // boss sindra

        Map* map = master->GetMap();


        // daca bot este in Ice Tomb
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

        // fereste de bombe
        if (Creature* tomb = bot->FindNearestCreature(entryIceTomb, 10.0f, true))
        {
            if (bot->HasUnitState(UNIT_STATE_ROOT))
            {
                if (Creature* bomb = bot->FindNearestCreature(entryFrostBomb, 200.0f, true))
                {
                    float zDiff = std::abs(bomb->GetPositionZ() - bot->GetPositionZ());

                    if (zDiff <= 5.0f)
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
        }

        // dps pe ice tomb pana la X%
        if (Creature* tomb = bot->FindNearestCreature(entryIceTomb, 7.0f, true))
        {
            if (bot->HasUnitState(UNIT_STATE_ROOT))
            {
                if (!ai->HasRole(BOT_ROLE_HEAL))
                {
                    float MinHpProc = 100.0f;
                    if (map)
                    {
                        if (map->IsHeroic())
                        {
                            if (map->Is25ManRaid())
                            {
                                MinHpProc = 50.0f;
                            }
                            else
                            {
                                MinHpProc = 100.0f;
                            }
                        }
                        else
                        {
                            if (map->Is25ManRaid())
                            {
                                MinHpProc = 60.0f;
                            }
                            else
                            {
                                MinHpProc = 100.0f;
                            }
                        }
                    }

                    
                    if (tomb->GetHealthPct() > MinHpProc)
                    {
                        if (tomb->IsAlive()/*bot->GetVictim() != tomb*/)
                        {
                            ai->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
                            bot->GetMotionMaster()->Clear();
                            bot->SetInCombatWith(tomb);
                            //ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->AttackStart(tomb);

                            if (ai->HasRole(BOT_ROLE_RANGED))
                            {
                                bot->Attack(tomb, false);
                            }
                            else
                            {
                                bot->Attack(tomb, true);
                            }
                        }
                        return;
                    }
                    else
                    {
                        bot->AttackStop();
                        ai->SetBotCommandState(BOT_COMMAND_FULLSTOP);
                    }
                }
            }
        }

        if (Creature* tomb = bot->FindNearestCreature(entryIceTomb, 80.0f, true))
        {
            // faza in aer
            if (Creature* sindra = bot->FindNearestCreature(NpcSindragosa, 300.0f, true))
            {
                if (!sindra || !sindra->IsInWorld() || !sindra->IsAlive())
                    return;

                float distToSindra = bot->GetDistance(sindra);
                float zDiff = std::abs(sindra->GetPositionZ() - bot->GetPositionZ());

                if (zDiff > 18.0f || distToSindra > 80.0f)
                {
                    if (!bot->HasUnitState(UNIT_STATE_ROOT))
                    {
                        float angleTowardsTomb = sindra->GetAbsoluteAngle(tomb);

                        float distantaInSpate = 5.0f;
                        float x = tomb->GetPositionX() + (distantaInSpate * cos(angleTowardsTomb));
                        float y = tomb->GetPositionY() + (distantaInSpate * sin(angleTowardsTomb));
                        float z = tomb->GetPositionZ();

                        bot->NearTeleportTo(x, y, z, angleTowardsTomb + M_PI);

                        bot->AddUnitState(UNIT_STATE_ROOT);

                        if (map && !ai->HasRole(BOT_ROLE_HEAL))
                        {
                            if (map->IsHeroic())
                            {
                                if (map->Is25ManRaid())
                                {
                                    // 25 heroic
                                }
                                else
                                {
                                    ai->SetBotCommandState(BOT_COMMAND_FULLSTOP);
                                }
                            }
                            else
                            {
                                if (map->Is25ManRaid())
                                {
                                    // 25 normal
                                }
                                else
                                {
                                    ai->SetBotCommandState(BOT_COMMAND_FULLSTOP);
                                }
                            }
                        }
                    }

                    return;
                }
                else
                {
                    if (bot->HasUnitState(UNIT_STATE_ROOT))
                    {
                        bot->ClearUnitState(UNIT_STATE_ROOT);
                        ai->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
                        //ai->RemoveBotCommandState(BOT_COMMAND_STAY);
                        //ai->SetBotCommandState(BOT_COMMAND_FOLLOW);
                        //bot->Yell("a aterizat....", LANG_UNIVERSAL);
                    }
                }

                if (bot->GetVictim() != sindra && ai->HasRole(BOT_ROLE_TANK) && zDiff < 2)
                {
                    if (bot->HasUnitState(UNIT_STATE_ROOT))
                    {
                        bot->ClearUnitState(UNIT_STATE_ROOT);
                    }

                    if (ai->HasBotCommandState(BOT_COMMAND_FULLSTOP))
                    {
                        ai->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
                    }

                    bot->SetInCombatWith(sindra);
                    ai->AttackStart(sindra);

                    float distanta = 5.0f;
                    float angleInFata = sindra->GetOrientation();

                    float x = sindra->GetPositionX() + (distanta * cos(angleInFata));
                    float y = sindra->GetPositionY() + (distanta * sin(angleInFata));
                    float z = sindra->GetPositionZ();

                    bot->NearTeleportTo(x, y, z, angleInFata + M_PI);
                    bot->GetMotionMaster()->Clear();

                }

                if (!ai->HasRole(BOT_ROLE_TANK) && ai->HasRole(BOT_ROLE_DPS) && tomb->IsAlive() /*bot->GetVictim() != tomb*/ && (distToSindra < 80.0f || zDiff < 18.0f))
                {
                    float zDiffX = std::abs(tomb->GetPositionX() - bot->GetPositionX());
                    bot->GetThreatManager().ClearAllThreat();
                    bot->GetThreatManager().AddThreat(tomb, 1300300.0f);


                    bot->SetInCombatWith(tomb);
                    //ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->AttackStart(tomb);

                    if (ai->HasRole(BOT_ROLE_RANGED))
                    {
                        if (zDiffX < 2.0f)
                        {
                            bot->GetMotionMaster()->Clear();
                            bot->SetInFront(tomb);
                            bot->SendMovementFlagUpdate();
                        }
                        bot->Attack(tomb, false);
                    }
                    else
                    {
                        if (zDiffX < 2.0f)
                        {
                            bot->GetMotionMaster()->Clear();
                            bot->SetInFront(tomb);
                            bot->SendMovementFlagUpdate();
                        }
                        bot->Attack(tomb, true);
                        //ai->BotMovement(BOT_MOVE_CHASE, nullptr, tomb);
                    }
                }
            }
            return;
        }
        else
        {
            if (bot->HasUnitState(UNIT_STATE_ROOT))
            {
                bot->ClearUnitState(UNIT_STATE_ROOT);
            }

            if (ai->HasBotCommandState(BOT_COMMAND_FULLSTOP))
            {
                ai->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
            }
            return;
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

                bot->GetMotionMaster()->MovePoint(2, x, y, bot->GetPositionZ());
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

                    bot->GetMotionMaster()->MovePoint(1, x, y, bot->GetPositionZ());
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
