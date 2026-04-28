//----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "kitt_npcbot_ai.h"
#include "Player.h"
#include "Creature.h"
#include "bot_ai.h"
#include "botmgr.h"
#include "Map.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "Transport.h"
//#include "../src/server/scripts/Northrend/IcecrownCitadel/icecrown_citadel.h"


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
                    //KittHandleLadyDeathwhisper(bot, master, ai); // lady
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

        if (!ai->HasRole(BOT_ROLE_TANK) || !ai->HasRole(BOT_ROLE_HEAL))
        {
            if (Creature* NpcSpikeTar = bot->FindNearestCreature(NpcSpike, 60.0f, true))
            {
                if (bot->GetVictim() != NpcSpikeTar)
                {
                    bot->SetInCombatWith(NpcSpikeTar);
                    //NpcSpikeTar->SetInCombatWith(bot);

                    bot->Attack(NpcSpikeTar, true);
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcSpikeTar);
                }
                return;
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

        if (!ai->HasRole(BOT_ROLE_TANK) || !ai->HasRole(BOT_ROLE_HEAL))
        {
            if (Creature* NpcAds1 = bot->FindNearestCreature(NpcCult, 40.0f, true))
            {
                if (bot->GetVictim() != NpcAds1)
                {
                    bot->SetInCombatWith(NpcAds1);
                    //NpcAds1->SetInCombatWith(bot);

                    bot->Attack(NpcAds1, true);
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds1);
                }
                return;
            }
            else if (Creature* NpcAds2 = bot->FindNearestCreature(NpcEmpow, 40.0f, true))
            {
                if (bot->GetVictim() != NpcAds2)
                {
                    bot->SetInCombatWith(NpcAds2);
                    //NpcAds2->SetInCombatWith(bot);

                    bot->Attack(NpcAds2, true);
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds2);
                }
                return;
            }
            else if (Creature* NpcAds3 = bot->FindNearestCreature(NpcReani, 40.0f, true))
            {
                if (bot->GetVictim() != NpcAds3)
                {
                    bot->SetInCombatWith(NpcAds3);
                    //NpcAds3->SetInCombatWith(bot);

                    bot->Attack(NpcAds3, true);
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds3);
                }
                return;
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
        uint32 const NpcMageH = 37117; // ingheata tun alianta
        uint32 const NpcMageA = 37116; // ingheata tun hoara
        uint32 const NpcSoketH = 36982; // ataca alianta
        uint32 const NpcMortalA = 36978; // ataca hoarda
        //uint32 const SpellSokeH = 69678;
        //uint32 const SpellSokeA = 70609;
        uint32 const TransAlliance = 201580;
        uint32 const TransHorde = 201812;

        if (mTrans->GetEntry() != TransAlliance && mTrans->GetEntry() != TransHorde)
            return;


        if (mTrans && mTrans->GetEntry() == TransAlliance) // alliance
        {
            if (master->GetTeamId() == TEAM_HORDE)
                return;

            if (!ai->HasRole(BOT_ROLE_HEAL) || !ai->HasRole(BOT_ROLE_TANK))
            {
                if (Creature* NpcAds1a = bot->FindNearestCreature(NpcMageH, 100.0f, true))
                {
                    if (!NpcAds1a || !NpcAds1a->IsInWorld() || !NpcAds1a->IsAlive())
                        return;

                    if (NpcAds1a && NpcAds1a->IsAlive() && NpcAds1a->GetChannelSpellId() == SpellFreez)
                    {
                        if (bot->GetVictim() != NpcAds1a)
                        {
                            bot->SetInCombatWith(NpcAds1a);
                            NpcAds1a->SetInCombatWith(bot);

                            bot->Attack(NpcAds1a, true);
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds1a);
                        }
                        return;
                    }
                }

                if (Creature* NpcAds2a = bot->FindNearestCreature(NpcSoketH, 100.0f, true))
                {
                    if (!NpcAds2a || !NpcAds2a->IsInWorld())
                        return;

                    ObjectGuid guid = NpcAds2a->GetGUID();

                    if (!guid.IsCreature() || !NpcAds2a->IsAlive())
                        return;

                    bool isAlreadyInMemory = activatedRocketeers.find(guid) != activatedRocketeers.end();
                    Spell* currentSpell = NpcAds2a ? NpcAds2a->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
                    bool isCastingNow = (currentSpell != nullptr);

                    if (isAlreadyInMemory)
                    {
                        bot->GetThreatManager().AddThreat(NpcAds2a, 5000.0f);
                        NpcAds2a->GetThreatManager().AddThreat(bot, 5000.0f);

                        if (bot->GetVictim() != NpcAds2a)
                        {
                            bot->SetInCombatWith(NpcAds2a);
                            NpcAds2a->SetInCombatWith(bot);

                            bot->Attack(NpcAds2a, true);

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
            }
        }

        if (mTrans && mTrans->GetEntry() == TransHorde) // horde
        {
            if (master->GetTeamId() == TEAM_ALLIANCE)
                return;

            if (!ai->HasRole(BOT_ROLE_HEAL) || !ai->HasRole(BOT_ROLE_TANK))
            {
                if (Creature* NpcAds1a = bot->FindNearestCreature(NpcMageA, 100.0f, true))
                {
                    if (!NpcAds1a || !NpcAds1a->IsInWorld() || !NpcAds1a->IsAlive())
                        return;

                    if (NpcAds1a && NpcAds1a->IsAlive() && NpcAds1a->GetChannelSpellId() == SpellFreez)
                    {
                        if (bot->GetVictim() != NpcAds1a)
                        {
                            bot->SetInCombatWith(NpcAds1a);
                            NpcAds1a->SetInCombatWith(bot);

                            bot->Attack(NpcAds1a, true);
                            ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                            ai->BotMovement(BOT_MOVE_CHASE, nullptr, NpcAds1a);
                        }
                        return;
                    }
                }

                if (Creature* NpcAds2a = bot->FindNearestCreature(NpcMortalA, 100.0f, true))
                {
                    if (!NpcAds2a || !NpcAds2a->IsInWorld())
                        return;

                    ObjectGuid guid = NpcAds2a->GetGUID();

                    if (!guid.IsCreature() || !NpcAds2a->IsAlive())
                        return;

                    bool isAlreadyInMemory = activatedRocketeers.find(guid) != activatedRocketeers.end();
                    Spell* currentSpell = NpcAds2a ? NpcAds2a->GetCurrentSpell(CURRENT_GENERIC_SPELL) : nullptr;
                    bool isCastingNow = (currentSpell != nullptr);

                    if (isAlreadyInMemory)
                    {
                        bot->GetThreatManager().AddThreat(NpcAds2a, 5000.0f);
                        NpcAds2a->GetThreatManager().AddThreat(bot, 5000.0f);

                        if (bot->GetVictim() != NpcAds2a)
                        {
                            bot->SetInCombatWith(NpcAds2a);
                            NpcAds2a->SetInCombatWith(bot);

                            bot->Attack(NpcAds2a, true);

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
            }
        }



    }

    void KittHandlePutricide(Creature* bot, Player* master, bot_ai* ai)
    {
        if (!bot || !bot->IsInWorld() || !bot->IsAlive())
            return;


        uint32 const spellUnboundPlague10h = 72855; // 70911, 72855 (10h) 72856 (25h)
        uint32 const spellUnboundPlague25h = 72856; // 70911, 72855 (10h) 72856 (25h)
        uint32 spellUnboundPlague = 0; // select dificulty

        uint32 const NpcVolatileOoze = 37697;    // NPC_VOLATILE_OOZE
        uint32 const NpcGasCloud = 37562;

        Map* map = master->GetMap();
        if (map && map->IsHeroic())
        {
            if (map->Is25ManRaid())
            {
                spellUnboundPlague = spellUnboundPlague25h;
            }
            else
            {
                spellUnboundPlague = spellUnboundPlague10h;
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

        // ooze attack
        if (!ai->HasRole(BOT_ROLE_TANK) || !ai->HasRole(BOT_ROLE_HEAL))
        {
            if (Creature* npcozeverde = bot->FindNearestCreature(NpcVolatileOoze, 60.0f, true))
            {
                if (bot->GetVictim() != npcozeverde)
                {
                    bot->SetInCombatWith(npcozeverde);
                    //npcozeverde->SetInCombatWith(bot);

                    bot->Attack(npcozeverde, true);
                    ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                    ai->BotMovement(BOT_MOVE_CHASE, nullptr, npcozeverde);
                }
                return;
            }
            else
            {
                if (Creature* npcgasoze = bot->FindNearestCreature(NpcGasCloud, 60.0f, true))
                {
                    if (bot->GetVictim() != npcgasoze)
                    {
                        bot->SetInCombatWith(npcgasoze);
                        //npcgasoze->SetInCombatWith(bot);

                        bot->Attack(npcgasoze, true);
                        ai->SetBotCommandState(BOT_COMMAND_ATTACK);
                        ai->BotMovement(BOT_MOVE_CHASE, nullptr, npcgasoze);
                    }
                    return;
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
        //uint32 const IceTombDmg = 70157; // ice tomb dmg (e in cub)
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

                        bot->Yell("am fugit", LANG_UNIVERSAL);
                    }

                    return;
                }
                else
                {
                    if (bot->HasUnitState(UNIT_STATE_ROOT))
                    {
                        bot->ClearUnitState(UNIT_STATE_ROOT);
                        ai->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
                        bot->Yell("a aterizat....", LANG_UNIVERSAL);

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

                if ((!ai->HasRole(BOT_ROLE_TANK) || !ai->HasRole(BOT_ROLE_HEAL)) && bot->GetVictim() != tomb && (distToSindra < 80.0f || zDiff < 18.0f))
                {
                    bot->SetInCombatWith(tomb);
                    tomb->SetInCombatWith(bot);

                    bot->Attack(tomb, true);
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
