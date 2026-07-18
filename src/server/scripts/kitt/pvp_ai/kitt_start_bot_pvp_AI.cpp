// ----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "kitt_bot_world_loader.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "PlayerAI.h"
#include "MotionMaster.h"
#include "Chat.h"
#include "Creature.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceScript.h"
#include "Map.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellAuras.h"
#include "SpellDefines.h"
#include "SpellHistory.h"
#include "Transport.h"
#include "PointMovementGenerator.h"
#include "Vehicle.h"
#include "G3D/Vector3.h"
#include "TalentPackets.h"
#include "PathGenerator.h"
#include "Unit.h"
#include "MoveSpline.h"
#include "Log.h"
#include "Pet.h"

namespace
{
    // Container static privat pentru gestionarea timerelor de AI (retine GUID-ul si timpul ramas)
    static std::unordered_map<ObjectGuid, uint32> G_BotAITimers;
}

BotRole DefinesteSiSalveazaRolulBotului(Player* botPlayer)
{
    if (!botPlayer)
        return BOT_ROLE_NONE;

    uint32 cautatGuid = botPlayer->GetGUID().GetCounter();

    // Cautam botul in tracker-ul global din header
    for (auto& tracker : g_MultiBotTracker)
    {
        if (tracker.charGuid == cautatGuid)
        {
            // Daca rolul a fost deja definit in trecut, il returnam instant (ZERO consum procesor)
            if (tracker.determinatRol != BOT_ROLE_NONE)
                return tracker.determinatRol;

            // Daca e BOT_ROLE_NONE, rulam logica ta de talente O SINGURA DATA
            uint8 clasa = botPlayer->GetClass();
            uint8 activeSpec = botPlayer->GetActiveSpec();

            // 1. Clase fixe Melee
            if (clasa == CLASS_WARRIOR || clasa == CLASS_ROGUE || clasa == CLASS_DEATH_KNIGHT)
            {
                tracker.determinatRol = BOT_ROLE_MELEE;
                return tracker.determinatRol;
            }

            // 2. Clase fixe Caster
            if (clasa == CLASS_MAGE || clasa == CLASS_WARLOCK || clasa == CLASS_HUNTER)
            {
                tracker.determinatRol = BOT_ROLE_CASTER;
                return tracker.determinatRol;
            }

            // 3. Clasa Priest (Hibrid intre Caster si Healer)
            if (clasa == CLASS_PRIEST)
            {
                if (botPlayer->HasTalent(15473, activeSpec) || botPlayer->HasTalent(34914, activeSpec))
                    tracker.determinatRol = BOT_ROLE_CASTER; // Shadow
                else
                    tracker.determinatRol = BOT_ROLE_HEALER; // Disc / Holy
                return tracker.determinatRol;
            }

            // 4. Clasa Paladin (Hibrid intre Melee si Healer)
            if (clasa == CLASS_PALADIN)
            {
                if (botPlayer->HasTalent(53563, activeSpec) || botPlayer->HasTalent(20473, activeSpec))
                    tracker.determinatRol = BOT_ROLE_HEALER; // Holy Shock / Beacon -> Healer
                else
                    tracker.determinatRol = BOT_ROLE_MELEE;  // Retri / Prot -> Melee
                return tracker.determinatRol;
            }

            // 5. Clasa Shaman
            if (clasa == CLASS_SHAMAN)
            {
                if (botPlayer->HasTalent(17364, activeSpec) || botPlayer->HasTalent(51533, activeSpec))
                    tracker.determinatRol = BOT_ROLE_MELEE; // Enhancement
                else if (botPlayer->HasTalent(51490, activeSpec))
                    tracker.determinatRol = BOT_ROLE_CASTER; // Elemental
                else
                    tracker.determinatRol = BOT_ROLE_HEALER; // Resto
                return tracker.determinatRol;
            }

            // 6. Clasa Druid
            if (clasa == CLASS_DRUID)
            {
                if (botPlayer->HasTalent(33876, activeSpec) || botPlayer->HasTalent(50334, activeSpec))
                    tracker.determinatRol = BOT_ROLE_MELEE; // Feral
                else if (botPlayer->HasTalent(33891, activeSpec))
                    tracker.determinatRol = BOT_ROLE_HEALER; // Tree of Life -> Healer
                else if (botPlayer->HasTalent(48505, activeSpec))
                    tracker.determinatRol = BOT_ROLE_CASTER; // Starfall -> Balance
                else
                {
                    uint8 shapeshift = botPlayer->GetShapeshiftForm();
                    if (shapeshift == FORM_CAT || shapeshift == FORM_BEAR || shapeshift == FORM_DIREBEAR)
                        tracker.determinatRol = BOT_ROLE_MELEE;
                    else
                        tracker.determinatRol = BOT_ROLE_CASTER;
                }
                return tracker.determinatRol;
            }

            tracker.determinatRol = BOT_ROLE_MELEE;
            return tracker.determinatRol;
        }
    }

    return BOT_ROLE_NONE;
}

// selectarea victima. true = ataca ce ataca si colegu(daca are tinta). false = prima gasita
Unit* GhostSelectTarget(Player* botPlayer, Unit*& currentVictim, bool focusPeColeg)
{
    if (!currentVictim)
    {
        ObjectGuid targetGuid = botPlayer->GetTarget();
        if (!targetGuid.IsEmpty())
        {
            botPlayer->SetSelection(ObjectGuid::Empty); // Sterge cercul rosu fizic din joc!
            botPlayer->AttackStop();
            botPlayer->CombatStop();
            if (botPlayer->GetMotionMaster())
            {
                botPlayer->GetMotionMaster()->Clear();
            }
            currentVictim = nullptr;
        }
    }

    // Daca are deja o tinta valida, vie si vizibila, nu o schimbam aiurea
    if (currentVictim && currentVictim->IsAlive() && botPlayer->IsWithinDistInMap(currentVictim, 160.0f))
    {
        // invisible & Feign Death
        if (!currentVictim->HasUnitState(UNIT_STATE_DIED))
        {
            if (botPlayer->CanSeeOrDetect(currentVictim, false, true))
            {
                return currentVictim; // Pastreaza focusul curent
            }
        }
    }

    // Daca tinta veche a murit sau a disparut, curatam starea
    if (currentVictim)
    {
        botPlayer->SetSelection(ObjectGuid::Empty);
        botPlayer->AttackStop();
        botPlayer->CombatStop();
        if (botPlayer->GetMotionMaster())
        {
            botPlayer->GetMotionMaster()->Clear();
        }
        currentVictim = nullptr;
    }

    std::list<Player*> playersInCell;
    botPlayer->GetPlayerListInGrid(playersInCell, 50.0f, true);

    std::list<Unit*> peturiInamice;

    for (Player* targetPlayer : playersInCell)
    {
        if (!targetPlayer || !targetPlayer->IsAlive() || targetPlayer == botPlayer || !botPlayer->InSamePhase(targetPlayer))
            continue;

        // Daca jucatorul gasit este inamic (chiar daca e Hunter si va da Feign Death imediat dupa)
        if (botPlayer->IsHostileTo(targetPlayer))
        {
            // Extragem pet-ul lui direct din memorie fara sa mai apelam functii de grid defecte
            if (Pet* tPet = targetPlayer->GetPet())
            {
                if (tPet->IsAlive() && botPlayer->InSamePhase(tPet))
                {
                    peturiInamice.push_back(tPet);
                }
            }
        }
    }

    playersInCell.remove_if([botPlayer, focusPeColeg](Player* target) {
        return !target || !target->IsAlive() || target == botPlayer || target->IsGameMaster() ||
            !botPlayer->InSamePhase(target) || !botPlayer->CanSeeOrDetect(target, false, true) ||
            target->HasUnitState(UNIT_STATE_DIED) || // hunter Feign Death
            (!focusPeColeg && !botPlayer->IsHostileTo(target));
        });

    //TC_LOG_INFO("server.loading", "GHOST_LOG: La 50m au fost gasiti {} jucatori in grid. Dupa filtrare au ramas valabili: {}.", (uint32)gasitiLa50m, (uint32)playersInCell.size());
    
    if (playersInCell.empty())
    {
        playersInCell.clear();
        botPlayer->GetPlayerListInGrid(playersInCell, 160.0f, true);
        
        playersInCell.remove_if([botPlayer, focusPeColeg](Player* target) {
            return !target || !target->IsAlive() || target == botPlayer || target->IsGameMaster() ||
                !botPlayer->InSamePhase(target) || !botPlayer->CanSeeOrDetect(target, false, true) ||
                target->HasUnitState(UNIT_STATE_DIED) || // hunter Feign Death
                (!focusPeColeg && !botPlayer->IsHostileTo(target));
            });
    }

    // --- SCENARIUL 1: ESTE HEALER (Vrea sa ia tinta coechipierului) ---
    if (focusPeColeg)
    {
        for (Player* coechipier : playersInCell)
        {
            if (!coechipier || !coechipier->IsAlive() || coechipier == botPlayer || coechipier->IsGameMaster() || !botPlayer->InSamePhase(coechipier))
                continue;

            // Verificam daca este in aceeasi echipa (coechipier in arena)
            if (!botPlayer->IsHostileTo(coechipier))
            {
                // Preluam victima pe care o ataca coechipierul nostru in acest moment
                Unit* tintaColegului = coechipier->GetVictim();
                if (tintaColegului && tintaColegului->IsInWorld() && tintaColegului->IsAlive() &&
                    botPlayer->CanSeeOrDetect(tintaColegului, false, true) &&
                    !tintaColegului->HasUnitState(UNIT_STATE_DIED))
                {
                    botPlayer->SetSelection(tintaColegului->GetGUID());
                    return tintaColegului; // Healerul a copiat cu succes tinta DPS-ului!
                }
            }
        }
    }

    // --- SCENARIUL 2: ESTE DPS SAU COECHPIERUL NU ARE INCA TINTA (Scanare normala) ---
    for (Player* targetPlayer : playersInCell)
    {
        if (!targetPlayer || !targetPlayer->IsInWorld() || !targetPlayer->IsAlive() || targetPlayer == botPlayer || targetPlayer->IsGameMaster() || !botPlayer->InSamePhase(targetPlayer))
            continue;

        if (!botPlayer->CanSeeOrDetect(targetPlayer, false, true) || targetPlayer->HasUnitState(UNIT_STATE_DIED))
            continue;

        if (botPlayer->IsHostileTo(targetPlayer))
        {
            botPlayer->SetSelection(targetPlayer->GetGUID());
            return targetPlayer; // A gasit o tinta ostila libera
        }
    }

    // pet-uri
    for (Unit* targetPet : peturiInamice)
    {
        if (!targetPet || !targetPet->IsInWorld() || !targetPet->IsAlive())
            continue;

        // Daca botul poate vedea pet-ul si este in raza de 50m
        if (botPlayer->CanSeeOrDetect(targetPet, false, true) && botPlayer->IsWithinDistInMap(targetPet, 50.0f))
        {
            botPlayer->SetSelection(targetPet->GetGUID());
            return targetPet; // Botul selecteaza si ataca pet-ul hunterului!
        }
    }

    return nullptr; // Nu a gasit pe nimeni in viata
}

// select prieten
Unit* GhostSelectFriendlyTarget(Player* botPlayer)
{
    std::list<Player*> playersInCell;
    botPlayer->GetPlayerListInGrid(playersInCell, 50.0f, true);

    Player* celMaiRanitColeg = nullptr;
    float celMaiMicProcentViata = 100.0f;

    for (Player* targetPlayer : playersInCell)
    {
        if (!targetPlayer || !targetPlayer->IsAlive() || !botPlayer->InSamePhase(targetPlayer))
            continue;

        // Verificam sa NU fie inamic (adica sa fie coechipier sau el insusi)
        if (!botPlayer->IsHostileTo(targetPlayer))
        {
            float procentViata = targetPlayer->GetHealthPct();

            // Gasim colegul cu procentul cel mai mic de viata
            if (procentViata < celMaiMicProcentViata)
            {
                celMaiMicProcentViata = procentViata;
                celMaiRanitColeg = targetPlayer;
            }
        }
    }

    // Daca toti colegii au viata plina, healerul se selecteaza pe el insusi ca tinta implicita de urmarire
    if (!celMaiRanitColeg)
        return botPlayer;

    return celMaiRanitColeg;
}

// rank maxim disponibil pt spell id
uint32 ObtineRankMaximSpell(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return spellId;

    if (SpellInfo const* lastRank = spellInfo->GetLastRankSpell())
    {
        return lastRank->Id;
    }

    return spellId;
}

// daca e melee sau caster
bool GhostIsMelee(Player* botPlayer)
{
    if (!botPlayer)
        return false;

    uint32 cautatGuid = botPlayer->GetGUID().GetCounter();

    for (auto& tracker : g_MultiBotTracker)
    {
        if (tracker.charGuid == cautatGuid)
        {
            // Daca in tracker este salvat ca MELEE, returneaza true. Altfel (Caster/Healer) returneaza false.
            return (tracker.determinatRol == BOT_ROLE_MELEE);
        }
    }

    return false;
}

// miscare si attack melee
void GhostMoveAndAttackMelee(Player* botPlayer, Unit*& victim)
{
    if (!botPlayer || !victim || !botPlayer->IsAlive() || !victim->IsAlive())
        return;

    if (botPlayer->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_IN_FLIGHT | UNIT_STATE_ROOT | UNIT_STATE_CHARMED | UNIT_STATE_CONFUSED_MOVE | UNIT_STATE_FLEEING_MOVE))
        return;

    float dist = botPlayer->GetDistance(victim);
    uint32 miscareCurenta = botPlayer->GetMotionMaster()->GetCurrentMovementGeneratorType();
    //ObjectGuid botGuid = botPlayer->GetGUID();

    if (botPlayer->GetTarget() != victim->GetGUID())
        botPlayer->SetSelection(victim->GetGUID());

    if (!botPlayer->IsInCombat())
        botPlayer->SetInCombatWith(victim, true);

    if (botPlayer->IsNonMeleeSpellCast(false, false, true) || botPlayer->HasUnitState(UNIT_STATE_CASTING))
    {
        if (miscareCurenta == CHASE_MOTION_TYPE || miscareCurenta == POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->StopMoving();
        }
        return;
    }

    // Control Corp la Corp sub 4 metri pentru Melee
    if (dist <= 4.0f)
    {
        if (!botPlayer->HasInArc(1.74f, victim) && botPlayer->IsWithinLOSInMap(victim))
        {
            botPlayer->StopMoving();
            botPlayer->SetFacingToObject(victim); // intrerupe movement
        }

        if (miscareCurenta != CHASE_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->GetMotionMaster()->MoveChase(victim);
            botPlayer->Attack(victim, true);
        }

        return;
    }

    // arena dalaran
    Map* botMap = botPlayer->GetMap();
    if (!botMap)
        return;

    uint32 mapid = botMap->GetId();
    
    if (mapid && mapid == 617)
    {
        if (botPlayer->GetPositionZ() > 10.0f)
        {
            // verde start
            float VposX = 1361.76f - 36.0f;
            float VposY = 817.336f;
            float VposZ = 14.8f - 10.0f;
            if (botPlayer->GetPositionX() > VposX)
            {
                //botPlayer->Relocate(VposX, VposY);
                botPlayer->GetMotionMaster()->MovePoint(1001, VposX, VposY, VposZ, false);
                return;
            }
            // galben start
            float GposX = 1218.00f + 42.00f;
            float GposY = 764.795f;
            float GposZ = 14.8f - 10.0f;
            if (botPlayer->GetPositionX() < GposX)
            {
                //botPlayer->Relocate(GposX, GposY);
                botPlayer->GetMotionMaster()->MovePoint(1001, GposX, GposY, GposZ, false);
                return;
            }
        }
    }
    // ---------------------

    // Alergare pe linie dreapta catre tinta
    // REZOLVARE ACTUAlIZARE POINT FARA LAG SAU FLOOD
    if (miscareCurenta != POINT_MOTION_TYPE)
    {
        // Daca botul nu are deja un traseu activ de tip punct, ii dam comanda initiala
        botPlayer->GetMotionMaster()->Clear();
        botPlayer->GetMotionMaster()->MovePoint(1001, victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ());
        botPlayer->Attack(victim, true);
    }
    else
    {
        // Daca botul deja alearga prin MovePoint, aflam coordonata exacta spre care se indreapta acum
        float destX = 0.0f, destY = 0.0f, destZ = 0.0f;
        botPlayer->GetMotionMaster()->GetDestination(destX, destY, destZ);

        // Calculam distanta la patrat dintre destinatia curenta a botului si pozitia noua a victimei
        // GetExactDistSq este foarte rapida pentru ca nu foloseste radical (SqrRoot), economisind procesor
        float distantaModificataSq = victim->GetExactDistSq(destX, destY, destZ);

        // 4.0f inseamna 2 metri distanta reala (2 * 2 = 4). 
        // Botul NU va da flood. El va rula MovePoint DOAR cand victima s-a mutat mai mult de 2 metri de vechiul punct.
        if (distantaModificataSq > 4.0f)
        {
            botPlayer->GetMotionMaster()->MovePoint(1001, victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ());
        }
    }

    //if (!botPlayer->HasInArc(1.74f, victim) && botPlayer->IsWithinLOSInMap(victim) && (miscareCurenta != POINT_MOTION_TYPE && miscareCurenta != CHASE_MOTION_TYPE))
      //  botPlayer->SetFacingToObject(victim);
}

// miscare si attack caster
void GhostMoveAndAttackCaster(Player* botPlayer, Unit*& victim)
{
    if (!botPlayer || !victim || !botPlayer->IsAlive() || !victim->IsAlive())
        return;

    if (botPlayer->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_IN_FLIGHT | UNIT_STATE_ROOT | UNIT_STATE_CHARMED | UNIT_STATE_CONFUSED_MOVE | UNIT_STATE_FLEEING_MOVE))
        return;

    float dist = botPlayer->GetDistance(victim);
    uint32 miscareCurenta = botPlayer->GetMotionMaster()->GetCurrentMovementGeneratorType();

    if (botPlayer->GetTarget() != victim->GetGUID())
        botPlayer->SetSelection(victim->GetGUID());

    if (!botPlayer->IsInCombat())
        botPlayer->SetInCombatWith(victim, true);

    if (botPlayer->IsNonMeleeSpellCast(false, false, true) || botPlayer->HasUnitState(UNIT_STATE_CASTING))
    {
        if (miscareCurenta == CHASE_MOTION_TYPE || miscareCurenta == POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->StopMoving();
        }
        return;
    }

    bool areLOS = botPlayer->IsWithinLOSInMap(victim);
    if (!areLOS)
    {
        if (miscareCurenta != POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->GetMotionMaster()->MovePoint(1001, victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ());
        }
        else
        {
            float destX = 0.0f, destY = 0.0f, destZ = 0.0f;
            botPlayer->GetMotionMaster()->GetDestination(destX, destY, destZ);
            float distantaModificataSq = victim->GetExactDistSq(destX, destY, destZ);

            if (distantaModificataSq > 4.0f)
            {
                botPlayer->GetMotionMaster()->MovePoint(1001, victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ());
            }
        }
        return;
    }

    // Control sub 4 metri pentru Caster (Se indeparteaza sau ataca pe loc)
    if (dist <= 4.0f)
    {
        if (miscareCurenta == CHASE_MOTION_TYPE || miscareCurenta == POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->StopMoving();
        }

        /*if (!botPlayer->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
            botPlayer->Attack(victim, false);*/

        if (!botPlayer->HasInArc(1.74f, victim))
            botPlayer->SetFacingToObject(victim);

        return;
    }

    // Deplasare si pozitionare la distanta (30 metri)
    if (dist > 30.0f)
    {
        if (miscareCurenta != POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->GetMotionMaster()->MovePoint(1001, victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ());
        }
    }
    else
    {
        if (miscareCurenta == POINT_MOTION_TYPE || miscareCurenta == CHASE_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->StopMoving();
            botPlayer->Attack(victim, false);
        }
    }

    if (miscareCurenta != POINT_MOTION_TYPE)
    {
        if (!botPlayer->HasInArc(1.74f, victim))
            botPlayer->SetFacingToObject(victim);
    }
}

// miscare si attack healer
void GhostMoveAndHeal(Player* botPlayer, Unit* friendlyTarget)
{
    if (!botPlayer || !friendlyTarget || !botPlayer->IsAlive() || !friendlyTarget->IsAlive())
        return;

    if (botPlayer->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_IN_FLIGHT | UNIT_STATE_ROOT | UNIT_STATE_CHARMED | UNIT_STATE_CONFUSED_MOVE | UNIT_STATE_FLEEING_MOVE))
        return;

    float dist = botPlayer->GetDistance(friendlyTarget);
    uint32 miscareCurenta = botPlayer->GetMotionMaster()->GetCurrentMovementGeneratorType();

    if (!botPlayer->IsInCombat())
        botPlayer->SetInCombatWith(friendlyTarget, true);

    // Daca botul casteaza deja un heal, oprim orice miscare si NU apelam nimic care poate reseta castul
    if (botPlayer->HasUnitState(UNIT_STATE_CASTING))
    {
        if (miscareCurenta == CHASE_MOTION_TYPE || miscareCurenta == POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->StopMoving();
        }
        return;
    }

    // --- FAZA 1: CONTROL LOS (LINIE VIZUALA) ---
    // Chiar daca heal-ul merge cu spatele, avem nevoie de linie vizuala (LOS) ca sa nu dea eroare de texturi
    bool areLOS = botPlayer->IsWithinLOSInMap(friendlyTarget);
    if (!areLOS)
    {
        if (miscareCurenta != POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->GetMotionMaster()->MovePoint(1001, friendlyTarget->GetPositionX(), friendlyTarget->GetPositionY(), friendlyTarget->GetPositionZ());
        }
        else
        {
            float destX = 0.0f, destY = 0.0f, destZ = 0.0f;
            botPlayer->GetMotionMaster()->GetDestination(destX, destY, destZ);
            float distantaModificataSq = friendlyTarget->GetExactDistSq(destX, destY, destZ);

            if (distantaModificataSq > 4.0f)
            {
                botPlayer->GetMotionMaster()->MovePoint(1001, friendlyTarget->GetPositionX(), friendlyTarget->GetPositionY(), friendlyTarget->GetPositionZ());
            }
        }
        return;
    }

    // --- FAZA 2: LOGICA DE POZITIONARE SUB 4 METRI ---
    if (dist <= 4.0f)
    {
        if (miscareCurenta == CHASE_MOTION_TYPE || miscareCurenta == POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->StopMoving();
        }
        return;
    }

    // --- FAZA 3: MECANICA SPECIFICA PENTRU ARENA DALARAN (Lift/Ziduri) ---
    Map* botMap = botPlayer->GetMap();
    if (botMap && botMap->GetId() == 617)
    {
        if (botPlayer->GetPositionZ() > 10.0f)
        {
            // verde start
            float VposX = 1361.76f - 36.0f;
            float VposY = 817.336f;
            float VposZ = 14.8f - 10.0f;
            if (botPlayer->GetPositionX() > VposX)
            {
                botPlayer->GetMotionMaster()->MovePoint(1001, VposX, VposY, VposZ, false);
                return;
            }
            // galben start
            float GposX = 1218.00f + 42.00f;
            float GposY = 764.795f;
            float GposZ = 14.8f - 10.0f;
            if (botPlayer->GetPositionX() < GposX)
            {
                botPlayer->GetMotionMaster()->MovePoint(1001, GposX, GposY, GposZ, false);
                return;
            }
        }
    }

    // --- FAZA 4: LOGICA DE POZITIONARE LA DISTANTA (30 de metri max) ---
    if (dist > 30.0f)
    {
        if (miscareCurenta != POINT_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->GetMotionMaster()->MovePoint(1001, friendlyTarget->GetPositionX(), friendlyTarget->GetPositionY(), friendlyTarget->GetPositionZ());
        }
        else
        {
            float destX = 0.0f, destY = 0.0f, destZ = 0.0f;
            botPlayer->GetMotionMaster()->GetDestination(destX, destY, destZ);
            float distantaModificataSq = friendlyTarget->GetExactDistSq(destX, destY, destZ);

            if (distantaModificataSq > 4.0f)
            {
                botPlayer->GetMotionMaster()->MovePoint(1001, friendlyTarget->GetPositionX(), friendlyTarget->GetPositionY(), friendlyTarget->GetPositionZ());
            }
        }
    }
    else
    {
        // Suntem in range util (intre 4m si 30m) si avem LOS -> oprim miscarea ca sa poata casta liber
        if (miscareCurenta == POINT_MOTION_TYPE || miscareCurenta == CHASE_MOTION_TYPE)
        {
            botPlayer->GetMotionMaster()->Clear();
            botPlayer->StopMoving();
        }
    }
}

// foloseste medalionul PVP
bool IncearcaSaFolosestiMedalionPvP(Player* botPlayer)
{
    if (!botPlayer || !botPlayer->IsAlive())
        return false;

    // Verifica daca botul este blocat (Stun, Fear, Polymorph, Charm, Sleep)
    if (botPlayer->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
        !botPlayer->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
    {
        // ID-ul magiei din spatele medalionului este 42292
        if (!botPlayer->GetSpellHistory()->HasCooldown(42292))
        {
            botPlayer->CastSpell(botPlayer, 42292, true); // true pentru a ignora GCD sau alte blocaje de cast

            // add cd for spell
            using namespace std::chrono_literals;
            botPlayer->GetSpellHistory()->AddCooldown(42292, 0, 30s); // normal 2min

            return true;
        }
    }
    return false;
}

// pornire AI
void kitt_start_bot_pvp_AI(Player* botPlayer, uint32 diff)
{
    if (!botPlayer || !botPlayer->IsAlive() || botPlayer->IsLoading())
        return;

    ObjectGuid botGuid = botPlayer->GetGUID();

    // ==================== COOLDOWN AI ====================
    auto it = G_BotAITimers.find(botGuid);
    if (it != G_BotAITimers.end())
    {
        if (diff >= it->second)
        {
            G_BotAITimers.erase(it);
        }
        else
        {
            it->second -= diff;
            return;
        }
    }

    G_BotAITimers[botGuid] = 250; // 250 ms
    // ====================================================================

    BotRole rolBot = DefinesteSiSalveazaRolulBotului(botPlayer);

    // Failsafe: Daca rolul este invalid sau entitatea nu este un bot din tracker, oprim executia
    if (rolBot == BOT_ROLE_NONE)
        return;


    Unit* currentVictim = botPlayer->GetVictim();
    uint8 botClass = botPlayer->GetClass();
    switch (botClass)
    {
    case CLASS_WARRIOR: // arms
        ExecutaLogicaWarriorPvP(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_PALADIN: // holy
        ExecutaLogicaPaladinPvP(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_DRUID: // feral
        ExecutaLogicaDruidFeralPvP(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_PRIEST: // discipline
        ExecutaLogicaPriestDiscPvP(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_ROGUE:
        ExecutaLogicaRogue(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_MAGE:
        ExecutaLogicaMage(botPlayer, currentVictim, rolBot);
        break;

    default:
        break;
    }
}


// ghost AI by class
void ExecutaLogicaPaladinPvP(Player* botPaladin, Unit*& victim, BotRole rolBot)
{
    //victim = GhostSelectTarget(botPaladin, victim, true);

    // 1. VERIFICARI STRICTE DE SIGURANTA
    /*if (!victim || !victim->IsAlive())
        return;*/

    if (!botPaladin || !botPaladin->IsAlive())
        return;

    //float targetDist = botPaladin->GetDistance(victim);

    // Medalionul de PvP se activeaza primul daca si-a pierdut controlul
    if (botPaladin->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
        !botPaladin->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
    {
        IncearcaSaFolosestiMedalionPvP(botPaladin);
    }

    uint32 myHp = botPaladin->GetHealthPct();

    bool esteHealer = (rolBot == BOT_ROLE_HEALER);
    if (esteHealer)
    {
        Unit* colegDeVindecat = GhostSelectFriendlyTarget(botPaladin);

        if (colegDeVindecat && colegDeVindecat->IsAlive() && (colegDeVindecat->GetHealthPct() < 85 || myHp < 85))
        {
            // Setam selection pe el ca sa ii dea heal pe target-ul corect
            botPaladin->SetSelection(colegDeVindecat->GetGUID());

            // Trimitem colegul in functia ta, unde va rula perfect logica de 35 de metri!
            GhostMoveAndHeal(botPaladin, colegDeVindecat);
        }
        //GhostMoveAndAttackMelee(botPaladin, victim);
        //GhostMoveAndHeal(botPaladin, victim);
        else
        {
            victim = GhostSelectTarget(botPaladin, victim, true);

            if (!victim || !victim->IsAlive())
                return;

            GhostMoveAndAttackMelee(botPaladin, victim);
        }
    }
    else
    {
        victim = GhostSelectTarget(botPaladin, victim, false);

        if (!victim || !victim->IsAlive())
            return;

        // DPS-ul fuge si ataca inamicul normal corp la corp
        GhostMoveAndAttackMelee(botPaladin, victim);
        //GhostMoveAndAttackMelee(botPaladin, victim);
    }

    //uint32 myHp = botPaladin->GetHealthPct();

    // Divine Shield (Bula Mare - ID: 642) - Urgenta pentru Paladin
    if (myHp < 25 && !botPaladin->GetSpellHistory()->HasCooldown(642) && (!botPaladin->HasAura(25) && !botPaladin->HasAura(25771)))
    {
        botPaladin->CastSpell(botPaladin, ObtineRankMaximSpell(642), true);
        //return;
    }

    // Buff-uri si resurse pasive inainte de lupta
    if (!botPaladin->HasAura(19746)) // Concentration Aura (Baza)
        botPaladin->CastSpell(botPaladin, ObtineRankMaximSpell(19746), false);

    if (!botPaladin->HasAura(20165) && !botPaladin->HasAura(20166)) // Seal of Light
        botPaladin->CastSpell(botPaladin, ObtineRankMaximSpell(20165), false);

    if (botPaladin->GetPower(POWER_MANA) * 100 / botPaladin->GetMaxPower(POWER_MANA) < 50 && !botPaladin->GetSpellHistory()->HasCooldown(54428))
        botPaladin->CastSpell(botPaladin, ObtineRankMaximSpell(54428), false); // Divine Plea

    // --- 2. SCANAREA GRUPULUI PENTRU PROTEJARE SI HEAL ---
    Player* coechipierDeVindecat = nullptr;
    uint32 ceaMaiMicaViataGrup = 100;

    // Verificam intai propria viata ca punct de plecare pentru prioritati
    if (myHp < 85)
    {
        coechipierDeVindecat = botPaladin;
        ceaMaiMicaViataGrup = myHp;
    }

    // Scanarea nativa a membrilor grupului din TrinityCore
    if (Group* arenaGroup = botPaladin->GetGroup())
    {
        for (GroupReference* itr = arenaGroup->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* membruGrup = itr->GetSource();
            if (!membruGrup || !membruGrup->IsAlive() || membruGrup == botPaladin)
                continue;

            // Ne asiguram ca sunt pe aceeasi harta (in aceeasi Arena)
            if (membruGrup->GetMapId() != botPaladin->GetMapId())
                continue;

            uint32 viataMembru = membruGrup->GetHealthPct();

            // A. LOGICA DE SUPORT FLUIDA (Freedom, Cleanse, Sacrifices) pentru coechipier
            // Beacon of Light (Semnul Luminii) - il punem pe coechipierul Warrior
            if (!membruGrup->HasAura(53563))
                botPaladin->CastSpell(membruGrup, ObtineRankMaximSpell(53563), false);

            // Sacred Shield (Scutul Sacru)
            if (!membruGrup->HasAura(53601) && !botPaladin->GetSpellHistory()->HasCooldown(53601))
                botPaladin->CastSpell(membruGrup, ObtineRankMaximSpell(53601), false);

            // Hand of Freedom (Daca este blocat / inghetat in loc)
            if (membruGrup->HasUnitState(UNIT_STATE_ROOT) || membruGrup->HasAura(122) || membruGrup->HasAura(339))
            {
                if (!botPaladin->GetSpellHistory()->HasCooldown(1044))
                    botPaladin->CastSpell(membruGrup, ObtineRankMaximSpell(1044), false);
            }

            // Hand of Protection (Bula fizica la viata critica de arena)
            if (viataMembru < 30 && !membruGrup->HasAura(25) && !membruGrup->HasAura(25771) && !botPaladin->GetSpellHistory()->HasCooldown(1022) && !membruGrup->getAttackers().empty())
            {
                botPaladin->CastSpell(membruGrup, ObtineRankMaximSpell(1022), true);
                return;
            }

            // Hand of Sacrifice (Transfer de damage)
            if (viataMembru < 50 && !botPaladin->GetSpellHistory()->HasCooldown(6940))
                botPaladin->CastSpell(membruGrup, ObtineRankMaximSpell(6940), false);

            // Cleanse (Dispel instant pe CC-urile primite de partener)
            if (membruGrup->HasUnitState(UNIT_STATE_LOST_CONTROL) && !botPaladin->GetSpellHistory()->HasCooldown(4987) && !botPaladin->HasUnitState(UNIT_STATE_CASTING))
                botPaladin->CastSpell(membruGrup, ObtineRankMaximSpell(4987), false);

            // B. DETERMINAM DACA PARTENERUL ARE NEVOIE DE VINDECARE URGENTA
            if (viataMembru < ceaMaiMicaViataGrup)
            {
                ceaMaiMicaViataGrup = viataMembru;
                coechipierDeVindecat = membruGrup;
            }
        }
    }

    // --- 3. EXECUTIA ROTATIEI DE HEAL DACA PROPRIETATEA HP < 85 ---
    if (coechipierDeVindecat && ceaMaiMicaViataGrup < 85)
    {
        // Opreste miscarea de urmarire daca este in combat melee, ca sa poata da cast stabil
        if (botPaladin->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
            botPaladin->GetMotionMaster()->Clear();

        uint32 currentMana = botPaladin->GetPower(POWER_MANA);

        // A. Lay on Hands (Ultima instanta - sub 15% viata) - NU COSTA MANA!
        if (ceaMaiMicaViataGrup <= 15 && !coechipierDeVindecat->HasAura(25) && !coechipierDeVindecat->HasAura(25771) && !botPaladin->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(633)))
        {
            if (botPaladin->HasUnitState(UNIT_STATE_CASTING))
                botPaladin->InterruptNonMeleeSpells(false);

            botPaladin->CastSpell(coechipierDeVindecat, ObtineRankMaximSpell(633), true);
            //return;
        }

        // B. Urgen?e: Holy Shock (Sub 40% viata) - Costa in jur de 550 Mana la lvl 80
        if (ceaMaiMicaViataGrup < 40 && currentMana >= 550 && !botPaladin->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(20473)))
        {
            if (botPaladin->HasUnitState(UNIT_STATE_CASTING))
                botPaladin->InterruptNonMeleeSpells(false);

            // Reparare tremurat: Activam Divine Favor (100% critic) DOAR daca nu il are deja activ pasiv
            if (!botPaladin->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(20216)) && !botPaladin->HasAura(ObtineRankMaximSpell(20216)))
            {
                botPaladin->CastSpell(botPaladin, ObtineRankMaximSpell(20216), true); // true = ignora GCD si se pune instant
            }

            botPaladin->CastSpell(coechipierDeVindecat, ObtineRankMaximSpell(20473), false); // Se lanseaza imediat Holy Shock
            //return;
        }

        // Daca deja casteaza o magie lunga si viata nu e critica, il lasam sa termine castul curent
        if (botPaladin->HasUnitState(UNIT_STATE_CASTING))
            return;

        // C. Holy Light (Heal-ul mare - Sub 60% viata) - Costa in jur de 1200 Mana la lvl 80 (Foarte scump!)
        if (ceaMaiMicaViataGrup < 60 && currentMana >= 1200 && !botPaladin->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(635)))
        {
            // Reparare tremurat: Activam Aura Mastery (Imun la kick) in aceeasi milisecunda cu inceputul castului
            if (!botPaladin->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(31821)) && !botPaladin->HasAura(ObtineRankMaximSpell(31821)))
            {
                botPaladin->CastSpell(botPaladin, ObtineRankMaximSpell(31821), true);
            }

            botPaladin->CastSpell(coechipierDeVindecat, ObtineRankMaximSpell(635), false); // Incepe castul mare de Holy Light
            //return;
        }

        // D. Flash of Light (Mentinere rapida) - Costa doar in jur de 300 Mana la lvl 80 (Ieftin)
        if (currentMana >= 300 && !botPaladin->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(19750)))
        {
            botPaladin->CastSpell(coechipierDeVindecat, ObtineRankMaximSpell(19750), false);
            //return;
        }

        return; // Daca viata e sub 85%, oprim executia aici ca sa nu treaca la logica ofensiva
    }


    // --- 4. LOGICA OFENSIVA (DOAR DACA TOATA LUMEA E Sanatoasa, PESTE 85% HP) ---
    if (victim && victim->IsAlive() && botPaladin->IsHostileTo(victim))
    {
        float targetDist = botPaladin->GetDistance(victim);

        // Porneste urmarirea fizica
        //GhostMoveAndAttackMelee(botPaladin, victim);

        // Repentance (ID: 20066) - Il folosim automat pe inamicul curent din argument daca viata grupului e sigura
        if (targetDist <= 20.0f && !botPaladin->GetSpellHistory()->HasCooldown(20066))
        {
            botPaladin->CastSpell(victim, ObtineRankMaximSpell(20066), false);
            return;
        }



        if (targetDist <= 5.0f)
        {
            if (!botPaladin->HasInArc(float(M_PI), victim))
            {
                botPaladin->SetFacingToObject(victim);
            }

            if (!botPaladin->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
            {
                botPaladin->Attack(victim, true);
            }

            //botPaladin->Attack(victim, true);

            // Judgement of Light
            if (!botPaladin->GetSpellHistory()->HasCooldown(20271))
                botPaladin->CastSpell(victim, ObtineRankMaximSpell(20271), false);

            // Crusader Strike
            if (!botPaladin->GetSpellHistory()->HasCooldown(35395))
                botPaladin->CastSpell(victim, ObtineRankMaximSpell(35395), false);
        }

        // Hammer of Justice (Stun la 10 metri)
        if (targetDist <= 10.0f && !botPaladin->GetSpellHistory()->HasCooldown(853))
        {
            botPaladin->CastSpell(victim, ObtineRankMaximSpell(853), false);
        }

        // Hammer of Wrath (Toporul sub 20% viata)
        if (victim->GetHealthPct() <= 20 && !botPaladin->GetSpellHistory()->HasCooldown(24275))
        {
            botPaladin->CastSpell(victim, ObtineRankMaximSpell(24275), false);
        }
    }
}

void ExecutaLogicaMage(Player* botPlayer, Unit*& victim, BotRole /*rolBot*/)
{
    if (!botPlayer->HasUnitState(UNIT_STATE_CASTING))
        botPlayer->CastSpell(victim, 116, false);
}

void ExecutaLogicaWarriorPvP(Player* botPlayer, Unit*& victim, BotRole rolBot) // warrior Arms
{
    victim = GhostSelectTarget(botPlayer, victim, false);

    if (!botPlayer || !victim || !botPlayer->IsAlive() || !victim->IsAlive())
        return;

    if (botPlayer->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED))
    {
        if (!botPlayer->GetSpellHistory()->HasCooldown(18499))
        {
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(18499), false);
        }
        else // Daca Berserker Rage e pe cooldown, abia atunci spargem trinket-ul
        {
            IncearcaSaFolosestiMedalionPvP(botPlayer);
        }
    }
    // Pasul B: Daca e in Stun sau alt control greu, da Trinket instant
    else if (botPlayer->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_CHARMED))
    {
        IncearcaSaFolosestiMedalionPvP(botPlayer);
    }

    if (!botPlayer->IsHostileTo(victim))
        return;

    // Adauga doar aceasta linie. Ea rezolva automat miscarea (Point/Chase), unghiul de 100 grade si auto-atacul.
    bool esteHealer = (rolBot == BOT_ROLE_HEALER);
    if (esteHealer)
    {
        GhostMoveAndHeal(botPlayer, victim);
    }
    else
    {
        GhostMoveAndAttackMelee(botPlayer, victim);
    }

    // Preluam distanta simplu pentru restul vrajilor de mai jos (Charge, Intercept, etc.)
    float dist = botPlayer->GetDistance(victim);

    // se intoarce cu fata la victima
    /*if (!botPlayer->HasInArc(float(M_PI), victim))
    {
        botPlayer->SetFacingToObject(victim);
    }*/

    // 1. MANAGEMENTUL DE RESURSE (Furia nativa a jucatorului)
    uint32 rage = botPlayer->GetPower(POWER_RAGE);

    // 3. MOBILITATE SI GAP CLOSERS (Charge / Intercept / Heroic Throw)

    if (dist > 8.0f && dist < 25.0f)
    {
        // Daca nu e in combat, da Charge. Daca are talentul Juggernaut pus de tine, merge si in combat.
        if (!botPlayer->GetSpellHistory()->HasCooldown(100))
        {
            // Fortam Battle Stance pentru Charge
            if (botPlayer->GetShapeshiftForm() != FORM_BATTLESTANCE)
            {
                botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);
                botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2457), true);
            }

            botPlayer->CastSpell(victim, ObtineRankMaximSpell(100), false); // Charge
            return;
        }
        // Daca Charge e pe cooldown dar are furie, schimba in Berserker si da Intercept
        else if (rage >= 10 && !botPlayer->GetSpellHistory()->HasCooldown(20252))
        {
            if (botPlayer->GetShapeshiftForm() != FORM_BERSERKERSTANCE)
            {
                botPlayer->SetShapeshiftForm(FORM_BERSERKERSTANCE);
                botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2458), true);
            }

            botPlayer->CastSpell(victim, ObtineRankMaximSpell(20252), false); // Intercept
            return;
        }
        // Daca nu poate ajunge la tinta dar are Heroic Throw liber
        else if (!botPlayer->GetSpellHistory()->HasCooldown(57755))
        {
            botPlayer->CastSpell(victim, ObtineRankMaximSpell(57755), false); // Heroic Throw
        }
    }

    if (dist > 2.0f)
        return;

    // Daca are cooldown-ul gata si e in lupta cu putina furie, da Bloodrage
    if (rage < 30 && botPlayer->IsInCombat() && !botPlayer->GetSpellHistory()->HasCooldown(2687))
    {
        botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2687), false); // Bloodrage
    }

    // 2. LOGICA DEFENSIVA SI SUPRAVIETUIRE (Ubers / Self-Heal)
    uint32 healthPct = botPlayer->GetHealthPct();

    // Last Stand - se activeaza cand viata scade periculos sub 30%
    if (healthPct < 30 && !botPlayer->GetSpellHistory()->HasCooldown(12975))
    {
        botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(12975), false); // Last Stand
        return;
    }

    // Enraged Regeneration - daca are sub 40% viata si are efect de Enrage activ
    if (healthPct < 40 && !botPlayer->GetSpellHistory()->HasCooldown(55694))
    {
        // Verifica daca are un buff din familia Enrage ca sa poata da spell-ul
        if (botPlayer->HasAuraWithMechanic(1u << MECHANIC_ENRAGED) || botPlayer->HasAura(18499))
        {
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(55694), false); // Enraged Regeneration
            return;
        }
    }

    // Berserker Rage - Folosit reactiv pentru a scapa din Fear / Sap / Gouge
    if (botPlayer->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED) && !botPlayer->GetSpellHistory()->HasCooldown(18499))
    {
        botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(18499), false); // Berserker Rage
    }

    // 4. CONTROL SI INTERRUPT (Hamstring / Pummel / Intimidating Shout)

    // Incetinire: Aplica Hamstring daca tinta nu are deja debuff-ul ca sa nu poata fugi din Arena
    if (!victim->HasAura(1715) && rage >= 10 && !botPlayer->GetSpellHistory()->HasCooldown(1715))
    {
        // Cere Battle sau Berserker Stance
        if (botPlayer->GetShapeshiftForm() == FORM_DEFENSIVESTANCE)
        {
            botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2457), true);
        }

        botPlayer->CastSpell(victim, ObtineRankMaximSpell(1715), false); // Hamstring
    }

    // Interrupt: Daca victima da cast la o magie (Heal / CC), da Pummel in Berserker Stance
    if (victim->IsNonMeleeSpellCast(false, false, true) && !botPlayer->GetSpellHistory()->HasCooldown(6552))
    {
        if (botPlayer->GetShapeshiftForm() != FORM_BERSERKERSTANCE)
        {
            botPlayer->SetShapeshiftForm(FORM_BERSERKERSTANCE);
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2458), true);
        }

        botPlayer->CastSpell(victim, ObtineRankMaximSpell(6552), false); // Pummel
        return;
    }

    // Fear defensiv / ofensiv: Daca inamicul da cast aproape sau daca botul e incercuit
    if (victim->IsNonMeleeSpellCast(false, false, true) && dist < 5.0f && rage >= 25 && !botPlayer->GetSpellHistory()->HasCooldown(5246))
    {
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(5246), false); // Intimidating Shout
        return;
    }

    // 5. ROTATIA DE BURST SI DAMAGE (Arms PvP Standard)

    // A. EXECUTE - Tinta are sub 20% viata (Prioritate Maxima)
    if (victim->GetHealthPct() <= 20 && rage >= 15)
    {
        // Se poate da in Battle sau Berserker
        if (botPlayer->GetShapeshiftForm() == FORM_DEFENSIVESTANCE)
        {
            botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2457), true);
        }

        botPlayer->CastSpell(victim, ObtineRankMaximSpell(5308), false); // Execute
        return;
    }

    // B. REND - Trebuie sa fie mereu activ pe tinta pentru talentul Taste for Blood
    if (!victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_WARRIOR, 0x20, 0x0, 0x0, botPlayer->GetGUID()) && rage >= 10)
    {
        // Cere Battle Stance obligatoriu
        if (botPlayer->GetShapeshiftForm() != FORM_BATTLESTANCE)
        {
            botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2457), true);
        }

        botPlayer->CastSpell(victim, ObtineRankMaximSpell(772), false); // Rend
        return;
    }

    // C. OVERPOWER - Daca s-a activat procul de Taste for Blood sau de la Dodge
    if (botPlayer->HasReactive(REACTIVE_OVERPOWER) || botPlayer->HasAura(56636) || botPlayer->HasAura(56637) || botPlayer->HasAura(56638))
    {
        if (botPlayer->GetShapeshiftForm() != FORM_BATTLESTANCE)
        {
            botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2457), true);
        }

        if (rage >= 5 && !botPlayer->GetSpellHistory()->HasCooldown(7384))
        {
            botPlayer->CastSpell(victim, ObtineRankMaximSpell(7384), false); // Overpower
            return;
        }
    }

    // D. MORTAL STRIKE - Atacul de baza (Reduce heal-ul primit de inamic cu 50%) [3]
    if (rage >= 30 && !botPlayer->GetSpellHistory()->HasCooldown(12294))
    {
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(12294), false); // Mortal Strike
        return;
    }

    // E. BLADESTORM - Folosit ca finisher sau daca are Recklessness / Death Wish active
    if (!botPlayer->GetSpellHistory()->HasCooldown(46924) && rage >= 25)
    {
        if (botPlayer->HasAura(1719) || botPlayer->HasAura(12292) || healthPct > 60)
        {
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(46924), false); // Bladestorm
            return;
        }
    }

    // F. OFF-GCD COOLDOWNS (Death Wish / Recklessness)
    if (!botPlayer->GetSpellHistory()->HasCooldown(12292) && rage >= 10 && healthPct > 70)
    {
        botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(12292), false); // Death Wish
    }
    if (!botPlayer->GetSpellHistory()->HasCooldown(1719))
    {
        if (botPlayer->GetShapeshiftForm() != FORM_BERSERKERSTANCE)
        {
            botPlayer->SetShapeshiftForm(FORM_BERSERKERSTANCE);
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(2458), true);
        }

        botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(1719), false); // Recklessness
    }

    // G. RAGE DUMP (Heroic Strike) - Daca aduna prea multa furie si totul e pe cooldown
    if (rage > 70 && !botPlayer->GetCurrentSpell(CURRENT_MELEE_SPELL))
    {
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(78), false); // Heroic Strike
    }
}

void ExecutaLogicaRogue(Player* botPlayer, Unit*& victim, BotRole /*rolBot*/)
{
    botPlayer->SetPower(POWER_ENERGY, 100);
    if (botPlayer->IsWithinMeleeRange(victim))
        botPlayer->CastSpell(victim, 1752, false);
}

void ExecutaLogicaPriestDiscPvP(Player* botPriest, Unit*& victim, BotRole /*rolBot*/)
{
    if (!botPriest || !botPriest->IsAlive())
        return;

    // 1. MEDALION PVP (Prioritate absoluta daca e in CC)
    if (botPriest->HasUnitState(UNIT_STATE_LOST_CONTROL))
    {
        IncearcaSaFolosestiMedalionPvP(botPriest);
    }

    // Resurse si Procente
    uint32 myHp = botPriest->GetHealthPct();
    uint32 myMana = botPriest->GetPower(POWER_MANA);
    float manaPct = ((float)myMana / (float)botPriest->GetMaxPower(POWER_MANA)) * 100.0f;

    // MENTINERE BUFF-URI DE BAZA (ID-uri numerice fixe de Rank 1)
    if (!botPriest->HasAura(ObtineRankMaximSpell(1243))) // Power Word: Fortitude (Rank 1 = 1243)
        botPriest->CastSpell(botPriest, ObtineRankMaximSpell(1243), false);

    if (!botPriest->HasAura(ObtineRankMaximSpell(588))) // Inner Fire (Rank 1 = 588)
        botPriest->CastSpell(botPriest, ObtineRankMaximSpell(588), false);

    // Identificare coechipier arena
    Unit* friendlyTarget = GhostSelectFriendlyTarget(botPriest);
    uint32 targetHp = friendlyTarget ? friendlyTarget->GetHealthPct() : 100;

    // --- FAZA 1: PRIORITATE DEFENSIVA (HEAL) ---
    if (friendlyTarget && (targetHp < 90 || myHp < 90))
    {
        // Miscare fluida defensiva (Urmarire coechipier + Corectia ta de LoS)
        GhostMoveAndHeal(botPriest, friendlyTarget);

        // A. Pain Suppression (Rank 1 = 33206) - Salvare sub 35% HP
        if (targetHp <= 35 && !botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(33206)))
        {
            if (!friendlyTarget->HasAura(ObtineRankMaximSpell(33206)) && !friendlyTarget->HasAura(ObtineRankMaximSpell(642)) && !friendlyTarget->HasAura(ObtineRankMaximSpell(45438)))
            {
                if (botPriest->HasUnitState(UNIT_STATE_CASTING))
                    botPriest->InterruptNonMeleeSpells(false);

                botPriest->CastSpell(friendlyTarget, ObtineRankMaximSpell(33206), true); // Ignora GCD pentru salvare instant
                return;
            }
        }

        // B. Power Word: Shield (Rank 1 = 17) - Verificare debuff Weakened Soul (6788)
        if (!friendlyTarget->HasAura(6788)) // 6788 este ID-ul fix de debuff global
        {
            botPriest->CastSpell(friendlyTarget, ObtineRankMaximSpell(17), false);
            return;
        }

        // C. Penance Defensive (Rank 1 = 47540) - Sub 75% HP
        if (targetHp < 75 && !botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(47540)))
        {
            if (botPriest->HasUnitState(UNIT_STATE_CASTING))
                botPriest->InterruptNonMeleeSpells(false);

            // Activare Inner Focus (Rank 1 = 14751) daca are mana putina
            if (manaPct < 60 && !botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(14751)))
                botPriest->CastSpell(botPriest, ObtineRankMaximSpell(14751), true);

            botPriest->CastSpell(friendlyTarget, ObtineRankMaximSpell(47540), false);
            return;
        }

        // D. Prayer of Mending (Rank 1 = 33076) - Amortizare daune
        if (targetHp < 85 && !botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(33076)))
        {
            botPriest->CastSpell(friendlyTarget, ObtineRankMaximSpell(33076), false);
            return;
        }

        // E. Flash Heal (Rank 1 = 2061) - Filler rapid
        if (targetHp < 70 && !botPriest->HasUnitState(UNIT_STATE_CASTING))
        {
            botPriest->CastSpell(friendlyTarget, ObtineRankMaximSpell(2061), false);
            return;
        }

        // Daca viata partenerului e sub pragul critic (75%), Priest-ul nu ataca
        if (targetHp < 75)
            return;
    }

    // --- FAZA 2: CONTROL SI REACTIE ANTI-CC (Psychic Scream & SW: Death) ---
    victim = GhostSelectTarget(botPriest, victim, true);
    if (victim && victim->IsAlive() && botPriest->IsHostileTo(victim))
    {
        float targetDist = botPriest->GetDistance(victim);

        // A. Psychic Scream (Rank 1 = 8122) - Control de apropiere (Fear)
        if (targetDist <= 8.0f && !botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(8122)))
        {
            botPriest->CastSpell(botPriest, ObtineRankMaximSpell(8122), false);
            return;
        }

        // B. Mecanica SW: Death (Rank 1 = 32379) pentru a sparge CC-ul inamic (Polymorph / Blind)
        if (victim->HasUnitState(UNIT_STATE_CASTING))
        {
            if (Spell* enemySpell = victim->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            {
                if (enemySpell->GetSpellInfo())
                {
                    uint32 spellId = enemySpell->GetSpellInfo()->Id;
                    // Verificam formele de Polymorph (118, 12826, 28271) sau Blind (2094)
                    if (spellId == 118 || spellId == 12826 || spellId == 28271 || spellId == 2094)
                    {
                        if (!botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(32379)))
                        {
                            botPriest->CastSpell(victim, ObtineRankMaximSpell(32379), true); // Instant
                            return;
                        }
                    }
                }
            }
        }


        // --- FAZA 3: PRESIUNE OFENSIVA SI SPAM DISPEL ---
        GhostMoveAndAttackCaster(botPriest, victim);

        // A. Recuperare Mana: Shadowfiend (Rank 1 = 34433)
        if (manaPct < 50.0f && !botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(34433)))
        {
            botPriest->CastSpell(victim, ObtineRankMaximSpell(34433), false);
            return;
        }

        // B. Logica de Spargere Imunitati (Mass Dispel - Rank 1 = 32375)
        // Daca Paladinul are Divine Shield (642) sau Mage-ul are Ice Block (45438), dam Mass Dispel direct sub ei
        if (victim->HasAura(642) || victim->HasAura(45438))
        {
            if (!botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(32375)) && !botPriest->HasUnitState(UNIT_STATE_CASTING))
            {
                botPriest->CastSpell(victim->GetPosition(), ObtineRankMaximSpell(32375), false);
                return;
            }
        }

        // C. Dispel Magic Ofensiv (Rank 1 = 527) - Curatarea scuturilor inamice active
        if (targetDist <= 30.0f)
        {
            // Curata Sacred Shield (53601), Ice Barrier (43039), PW: Shield (17) sau Innervate (29166)
            if (victim->HasAura(53601) || victim->HasAura(43039) || victim->HasAura(17) || victim->HasAura(29166))
            {
                botPriest->CastSpell(victim, ObtineRankMaximSpell(527), false);
                return;
            }
        }

        // D. Mana Burn Ofensiv (Rank 1 = 8129) - Presiune pe mana healerului inamic
        if (victim && victim->GetTypeId() == TYPEID_PLAYER && victim->GetPowerType() == POWER_MANA && victim->GetPower(POWER_MANA) > 1500 && targetDist <= 30.0f)
        {
            // Accesam AI-ul jucatorului inamic si verificam daca este Healer folosind functia nativa gasita de tine
            if (PlayerAI* enemyAI = dynamic_cast<PlayerAI*>(victim->GetAI()))
            {
                if (enemyAI->IsHealer(victim->ToPlayer()))
                {
                    if (!botPriest->HasUnitState(UNIT_STATE_CASTING) && manaPct > 35.0f)
                    {
                        botPriest->CastSpell(victim, ObtineRankMaximSpell(8129), false); // Mana Burn
                        return;
                    }
                }
            }
        }

        // E. Asistenta Daune (Mentinere DoT Shadow Word: Pain - Rank 1 = 589)
        if (!victim->HasAura(ObtineRankMaximSpell(589)) && targetDist <= 30.0f)
        {
            botPriest->CastSpell(victim, ObtineRankMaximSpell(589), false);
            return;
        }

        // Penance folosit ofensiv (Rank 1 = 47540) daca partenerul are viata plina (HP > 85%)
        if (targetHp > 85 && !botPriest->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(47540)) && targetDist <= 30.0f)
        {
            botPriest->CastSpell(victim, ObtineRankMaximSpell(47540), false);
            return;
        }
    }
}

void ExecutaLogicaDruidFeralPvP(Player* botPlayer, Unit*& victim, BotRole /*rolBot*/)
{
    // Pasul A: Selectare tinta pentru clasa Melee (false)
    victim = GhostSelectTarget(botPlayer, victim, false);

    if (!botPlayer || !victim || !botPlayer->IsAlive() || !victim->IsAlive())
        return;

    // Pasul B: GESTIONARE CC SI DEFENSIVA NATIVA (Corectata impotriva spamului)
    // Verificam STRICT daca are debuff de Fear sau Confused pe el prin auri si UnitState izolate
    if (botPlayer->HasAuraType(SPELL_AURA_MOD_FEAR) || botPlayer->HasUnitState(UNIT_STATE_FLEEING) || botPlayer->HasUnitState(UNIT_STATE_CONFUSED))
    {
        if (!botPlayer->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(50334))) // Berserk_1 = 50334
        {
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(50334), true); // Ignora GCD
            botPlayer->InterruptNonMeleeSpells(false);

            // Curatam exclusiv starile daca am dat Berserk
            botPlayer->ClearUnitState(UNIT_STATE_FLEEING);
            botPlayer->ClearUnitState(UNIT_STATE_CONFUSED);
            return;
        }
        else // Daca Berserk e pe cooldown, abia atunci spargem trinket-ul
        {
            IncearcaSaFolosestiMedalionPvP(botPlayer);
        }
    }
    // Daca este in Stun sau Charm, da Trinket instant
    else if (botPlayer->HasUnitState(UNIT_STATE_STUNNED) || botPlayer->HasUnitState(UNIT_STATE_CHARMED))
    {
        IncearcaSaFolosestiMedalionPvP(botPlayer);
    }


    if (!botPlayer->IsHostileTo(victim))
        return;

    // Resurse si Procente
    uint8 formaCurenta = botPlayer->GetShapeshiftForm();
    uint32 energy = botPlayer->GetPower(POWER_ENERGY);
    uint32 comboPoints = botPlayer->GetComboPoints();
    uint32 myHp = botPlayer->GetHealthPct();
    float targetDist = botPlayer->GetDistance(victim);

    // Fortam forma de Pisica daca din cauza unui CC sau shift a fost scos (CAT_FORM_1 = 768)
    if (botPlayer->GetShapeshiftForm() != FORM_CAT)
    {
        if (!botPlayer->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(768)))
        {
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(768), false);
            return;
        }
    }

    // --- FAZA 1: SUPRAVIETUIRE SI DEFENSIVA NATIVA ---
    if (myHp < 40)
    {
        if (formaCurenta != FORM_BEAR)
        {
            if (!botPlayer->HasAura(ObtineRankMaximSpell(5487)))
            {
                botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(5487), true);
            }
            botPlayer->SetShapeshiftForm(FORM_BEAR); // Reintra instant in Urs
            return;
        }

        if (!botPlayer->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(61336))) // Survival Instincts = 61336
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(61336), false);

        if (!botPlayer->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(22812))) // Barkskin = 22812
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(22812), false);
    }
    else
    {
        if (formaCurenta != FORM_CAT)
        {
            if (!botPlayer->HasAura(ObtineRankMaximSpell(768)))
            {
                botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(768), true);
            }
            botPlayer->SetShapeshiftForm(FORM_CAT);
            return;
        }
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(49376), false);
        return;
    }

    // --- FAZA 2: SECTIUNEA DE MISCARE SECURE (REGULA TA ANTI-CRASH VMAP) ---
    if (targetDist > 5.0f)
    {
        // De la distanta mare, folosim exclusiv MovePoint pe pozitia inamicului (fara spam)
        if (!botPlayer->isMoving())
        {
            botPlayer->GetMotionMaster()->MovePoint(1006, victim->GetPosition());
        }

        // Feral Charge Cat (ID 49376) - Sare pe inamic daca acesta fuge la distanta
        if (targetDist > 8.0f && targetDist < 25.0f && !botPlayer->GetSpellHistory()->HasCooldown(49376))
        {
            if (formaCurenta != FORM_CAT)
            {
                if (!botPlayer->HasAura(ObtineRankMaximSpell(768)))
                {
                    botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(768), true);
                }
                botPlayer->SetShapeshiftForm(FORM_CAT);
                return;
            }
            botPlayer->CastSpell(victim, ObtineRankMaximSpell(49376), false);
            return;
        }

        // Dash (ID: 1850) pentru a ajunge rapid in melee
        if (targetDist > 15.0f && !botPlayer->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(1850)) && !botPlayer->HasAura(ObtineRankMaximSpell(1850)))
        {
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(1850), true);
        }

        // Fiind clasa de Melee, nu rulam atacurile daca suntem departe. Lasam MovePoint sa lucreze.
        return;
    }
    else
    {
        // REGULA TA VERIFICATA: Doar de aproape (sub 5m) cuplam Attack(true) si urm?rirea Chase
        botPlayer->GetMotionMaster()->MoveChase(victim);
        botPlayer->Attack(victim, true);
        

        // Micro-pozitionare in spatele tintei folosind functia ta nativa cu coliziuni (tot sub 5m)
        /*if (!victim->HasInArc(float(M_PI), botPlayer))
        {
            float angle = victim->GetOrientation() + M_PI;
            Position pos = victim->GetFirstCollisionPosition(1.5f, angle);
            botPlayer->GetMotionMaster()->MovePoint(1005, pos);
        }*/
    }

    // --- FAZA 3: ROTATIE DE DAMAGE SI FINISHERS (DOAR IN MELEE) ---

    // Faerie Fire Feral (ID: 16857) - Opreste invizibilitatea (Rogue / Mage)
    if (!victim->HasAura(ObtineRankMaximSpell(16857)))
    {
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(16857), false);
        //return;
    }

    // Tiger's Fury (ID: 5217) - Management de energie
    if (energy <= 30 && !botPlayer->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(5217)))
    {
        botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(5217), true);
        //return;
    }

    // Berserk folosit ofensiv pentru burst daca energia e stabila
    /*if (energy >= 60 && !botPlayer->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(50334)))
    {
        botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(50334), true);
        return;
    }*/

    // EXECUTIE FINISHERS (4-5 Combo Points)
    if (comboPoints >= 4)
    {
        // Savage Roar (ID: 52610) - Auto-buff obligatoriu pentru cresterea damage-ului
        if (!botPlayer->HasAura(ObtineRankMaximSpell(52610)))
        {
            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(52610), false);
            //return;
        }

        // Rip (ID: 1079) - DoT principal sangerare pe tinte cu multa viata
        if (victim->GetHealth() > botPlayer->GetMaxHealth() && !victim->HasAura(ObtineRankMaximSpell(1079)))
        {
            botPlayer->CastSpell(victim, ObtineRankMaximSpell(1079), false);
            //return;
        }

        // Ferocious Bite (ID: 22568) - Burst final direct
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(22568), false);
        //return;
    }

    // GENERARE PUNCTE (Combo Builders)
    // Mangle - Cat (ID: 33876) - Aplica debuff-ul de 30% daune extra din sangerari
    if (!victim->HasAura(ObtineRankMaximSpell(33876)))
    {
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(33876), false);
        //return;
    }

    // Rake (ID: 1822) - Sangerare secundara mica
    if (!victim->HasAura(ObtineRankMaximSpell(1822)))
    {
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(1822), false);
        //return;
    }

    // Shred (ID: 5221) daca este pozitionat corect in spate, altfel Mangle din fata
    if (!victim->HasInArc(float(M_PI), botPlayer))
    {
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(5221), false);
        //return;
    }
    else
    {
        botPlayer->CastSpell(victim, ObtineRankMaximSpell(33876), false);
        //return;
    }
}

