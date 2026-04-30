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
#include "Map.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "Transport.h"


namespace
{
    std::set<ObjectGuid> activatedRocketeers; // icc gunship

}

namespace KittBotAI
{
    void KittUpdateRaidStrategies(Creature* bot, Player* master)
    {
        if (!bot || !master || !bot->IsAlive() || !bot->IsInWorld())
            return;

        bot_ai* ai = bot->GetBotAI();
        if (!ai)
            return;

        uint32 mapId = bot->GetMapId();
        uint32 areaId = bot->GetAreaId();

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
            }
        }
    }


    void KittHandleMarrowgar(Creature* bot, Player* /*master*/, bot_ai* ai)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        uint32 const NpcSpike = 36619;

        if (!ai->HasRole(BOT_ROLE_TANK) && !ai->HasRole(BOT_ROLE_HEAL))
        {
            if (Creature* NpcSpikeTar = bot->FindNearestCreature(NpcSpike, 60.0f, true))
            {
                if (NpcSpikeTar->IsInWorld() && NpcSpikeTar->IsAlive())
                {
                    if (bot->GetVictim() != NpcSpikeTar)
                    {
                        bot->SetInCombatWith(NpcSpikeTar);
                        //NpcSpikeTar->SetInCombatWith(bot);

                        //bot->Attack(NpcSpikeTar, true);
                        if (ai->HasRole(BOT_ROLE_RANGED))
                        {
                            bot->Attack(NpcSpikeTar, false);
                        }
                        else
                        {
                            bot->Attack(NpcSpikeTar, true);
                        }
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                        ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcSpikeTar);
                    }
                    return;
                }
            }
        }
    }

    void KittHandleLadyDeathwhisper(Creature* bot, Player* /*master*/, bot_ai* ai)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        uint32 const NpcCult  = 37949;
        uint32 const NpcEmpow = 38136;
        uint32 const NpcReani = 38010;

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

    void KittHandleGunship(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
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
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
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

    void KittHandleSindragosa(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;

        uint32 const spellFrostBeacon = 70126; // mark
        uint32 const entryIceTomb = 36980;    // id cub gheata
        uint32 const NpcSindragosa = 36853; // boss sindra

        // anti drift
        if (bot->IsAlive() && bot->IsInCombat())
        {
            if (bot->GetPositionX() > 4475.49f || bot->GetPositionZ() < 202.000f)
            {
                float const callbackX = 4407.42f;
                float const callbackY = 2483.41f;
                float const callbackZ = 203.67f;
                float const callbackO = 6.27f;
                bot->NearTeleportTo(callbackX, callbackY, callbackZ, callbackO);
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

        if (Creature* tomb = bot->FindNearestCreature(entryIceTomb, 40.0f, true))
        {
            if (Creature* sindra = bot->FindNearestCreature(NpcSindragosa, 200.0f, true))
            {
                if (!sindra || !sindra->IsInWorld() || !sindra->IsAlive())
                    return;

                float distToSindra = bot->GetDistance(sindra);
                float zDiff = std::abs(sindra->GetPositionZ() - bot->GetPositionZ());

                if (zDiff > 18.0f || distToSindra > 80.0f)
                {
                    if (!bot->HasUnitState(UNIT_STATE_ROOT))
                    {
                        float x = tomb->GetPositionX(); // +frand(-1.0f, 1.0f); //4354.33f;
                        float y = tomb->GetPositionY(); // +frand(-1.0f, 1.0f);  //2484.06f;
                        float z = tomb->GetPositionZ();  //204.145f;
                        float o = tomb->GetOrientation();  //6.28f;

                        //bot->GetMotionMaster()->MovePoint(1001, x, y, z);
                        bot->NearTeleportTo(x, y, z, o);
                        bot->AddUnitState(UNIT_STATE_ROOT);

                        //ai->RemoveBotCommandState(BOT_COMMAND_ATTACK);
                        bot->AttackStop();

                        if (!ai->HasRole(BOT_ROLE_HEAL))
                        {
                            ai->SetBotCommandState(BOT_COMMAND_FULLSTOP);
                        }

                        //bot->Yell("am fugit", LANG_UNIVERSAL);
                    }

                    return;
                }
                else
                {
                    if (bot->HasUnitState(UNIT_STATE_ROOT))
                    {
                        bot->ClearUnitState(UNIT_STATE_ROOT);
                        ai->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
                        //bot->Yell("a aterizat....", LANG_UNIVERSAL);

                        if (ai->HasRole(BOT_ROLE_TANK) || ai->HasRole(BOT_ROLE_TANK_OFF) || ai->HasRole(BOT_ROLE_DPS))
                        {
                            float x = tomb->GetPositionX() + 5.0f;
                            float y = tomb->GetPositionY();
                            float z = tomb->GetPositionZ();
                            float o = tomb->GetOrientation();
                            bot->NearTeleportTo(x, y, z, o);
                        }
                    }
                }

                if ((!ai->HasRole(BOT_ROLE_TANK) && !ai->HasRole(BOT_ROLE_HEAL)) && bot->GetVictim() != tomb && (distToSindra < 80.0f || zDiff < 18.0f))
                {
                    bot->SetInCombatWith(tomb);
                    //tomb->SetInCombatWith(bot);

                    //bot->Attack(tomb, true);
                    if (ai->HasRole(BOT_ROLE_RANGED))
                    {
                        bot->Attack(tomb, false);
                    }
                    else
                    {
                        bot->Attack(tomb, true);
                    }
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->BotMovement(BOT_MOVE_CHASE, nullptr, tomb);
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
            }
            return;
        }
    }


}
