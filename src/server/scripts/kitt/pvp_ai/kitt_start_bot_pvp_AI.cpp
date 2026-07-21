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
    if (dist > 27.0f)
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

    // --- FAZA 4: LOGICA DE POZITIONARE LA DISTANTA (30 de metri max) ---
    if (dist > 25.0f)
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
        ExecutaLogicaRoguePvP(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_MAGE:
        ExecutaLogicaMagePvP(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_WARLOCK:
        ExecutaLogicaWarlockPvP(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_DEATH_KNIGHT:
        ExecutaLogicaDeathKnightPvP(botPlayer, currentVictim, rolBot);
        break;
    case CLASS_SHAMAN:
        ExecutaLogicaShamanPvP(botPlayer, currentVictim, rolBot);
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

void ExecutaLogicaDruidFeralPvP(Player* botDruid, Unit*& victim, BotRole rolBot)
{
    if (!botDruid || !botDruid->IsAlive())
        return;

    // 1. Managementul pierderii controlului (CC)
    if (botDruid->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
        !botDruid->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
    {
        IncearcaSaFolosestiMedalionPvP(botDruid);
    }

    // 2. Selectie tinta si miscare
    victim = GhostSelectTarget(botDruid, victim, false);
    if (!victim || !victim->IsAlive())
        return;

    GhostMoveAndAttackMelee(botDruid, victim);

    // 3. Asigurarea formei de pisica (Form Shift Failsafe)
    if (botDruid->GetShapeshiftForm() != FORM_CAT)
    {
        // ID 768 este Cat Form. Folosim true pentru a ignora GCD la shift
        botDruid->SetShapeshiftForm(FORM_CAT);
        botDruid->CastSpell(botDruid, 768, true);
        return;
    }

    // 4. Buff-uri si cooldown-uri de auto-sustinere (Omen of Clarity, Tiger's Fury)
    if (!botDruid->HasAura(16864)) // Omen of Clarity
    {
        botDruid->CastSpell(botDruid, ObtineRankMaximSpell(16864), false);
    }

    uint32 energie = botDruid->GetPower(POWER_ENERGY);
    uint32 viataMea = botDruid->GetHealthPct();

    // Tiger's Fury daca energia este scazuta (sub 30) pentru spike de damage
    if (energie < 30 && !botDruid->GetSpellHistory()->HasCooldown(5217))
    {
        botDruid->CastSpell(botDruid, ObtineRankMaximSpell(5217), true);
    }

    // Survival Instincts sau Barkskin la viata scazuta
    if (viataMea < 35 && !botDruid->GetSpellHistory()->HasCooldown(61336))
    {
        botDruid->CastSpell(botDruid, ObtineRankMaximSpell(61336), true); // Survival Instincts
    }

    // 5. Rotatia de baze si interactiunea cu distanta (Raza de atac)
    float distanta = botDruid->GetDistance(victim);
    if (distanta > 5.0f)
    {
        // Feral Charge (Cat) daca tinta este la distanta (ID: 49376)
        if (distanta >= 8.0f && distanta <= 25.0f && !botDruid->GetSpellHistory()->HasCooldown(49376))
        {
            botDruid->CastSpell(victim, 49376, false);
        }
        return; // Nu poate da magiile de melee daca este prea departe
    }

    // Preluam punctele de combo acumulate pe victima curenta
    uint8 puncteCombo = botDruid->GetComboPoints();

    // 6. Finisheri (Magiile care consuma puncte de combo - prioritare la 4-5 puncte)
    if (puncteCombo >= 4)
    {
        // Aplica Rip (Sangerare principala - ID: 1079) daca tinta nu are deja debuff-ul
        if (!victim->HasAura(ObtineRankMaximSpell(1079)) && energie >= 30)
        {
            botDruid->CastSpell(victim, ObtineRankMaximSpell(1079), false);
            return;
        }

        // Savage Roar (Buff damage propriu - ID: 52610) daca nu e activ
        if (!botDruid->HasAura(52610) && energie >= 25)
        {
            botDruid->CastSpell(botDruid, ObtineRankMaximSpell(52610), false);
            return;
        }

        // Ferocious Bite (Burst damage direct - ID: 22568) daca restul sunt active
        if (energie >= 35)
        {
            botDruid->CastSpell(victim, ObtineRankMaximSpell(22568), false);
            return;
        }
    }

    // 7. Generatoare de puncte de combo (Atacuri de baza)
    // Mangle (Cat) - ID: 33876 (Mentine debuff-ul de damage marit pentru sangerari)
    if (!victim->HasAura(ObtineRankMaximSpell(33876)) && energie >= 40)
    {
        botDruid->CastSpell(victim, ObtineRankMaximSpell(33876), false);
        return;
    }

    // Rake (Sangerare rapida care genereaza puncte - ID: 1822)
    if (!victim->HasAura(ObtineRankMaximSpell(1822)) && energie >= 35)
    {
        botDruid->CastSpell(victim, ObtineRankMaximSpell(1822), false);
        return;
    }

    // Shred (Atac de baza cand spatele este accesibil sau ca umplutura - ID: 5221)
    if (energie >= 42)
    {
        botDruid->CastSpell(victim, ObtineRankMaximSpell(5221), false);
        return;
    }
}

// affiction
void ExecutaLogicaWarlockPvP(Player* botWarlock, Unit*& victim, BotRole /*rolBot*/)
{
    // 1. VERIFICARI STRICTE DE SIGURANTA
    if (!botWarlock || !botWarlock->IsAlive() || botWarlock->IsLoading())
        return;

    // Activare Medalion PvP daca botul este blocat (Stun, Fear, Silenced etc.)
    if (botWarlock->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
        !botWarlock->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
    {
        IncearcaSaFolosestiMedalionPvP(botWarlock);
    }

    // Selectam tinta inamica (Warlock-ul este intotdeauna CASTER in PvP)
    victim = GhostSelectTarget(botWarlock, victim, false);
    if (!victim || !victim->IsAlive() || !botWarlock->IsHostileTo(victim))
        return;

    // Apelam miscarea ta nativa de Caster pe care am decis sa o pastram neatinsa
    GhostMoveAndAttackCaster(botWarlock, victim);

    float targetDist = botWarlock->GetDistance(victim);
    uint32 myHp = botWarlock->GetHealthPct();
    uint32 myMana = botWarlock->GetPower(POWER_MANA) * 100 / botWarlock->GetMaxPower(POWER_MANA);

    // Daca deja casteaza o magie, oprim orice alta actiune (fara intreruperi/tremurat)
    if (botWarlock->HasUnitState(UNIT_STATE_CASTING))
        return;

    // VERIFICARE IMUNITATI PvP DIRECTE (Divine Shield = 642, Ice Block = 45438)
    if (victim->HasAura(642) || victim->HasAura(45438))
        return;

    // Scurtatura inteligenta pentru a verifica daca o abilitate nu este in cooldown
    auto SpellPregatit = [&](uint32 spellId) -> bool
        {
            return !botWarlock->GetSpellHistory()->HasCooldown(spellId);
        };

    // ==================== MANAGEMENT PET (SUMMON, STATUS & ATTACK IMP) ====================
    // Pastram logica functionala pe care ai testat-o cu succes pentru Imp-ul tau defensiv
    Pet* botPet = botWarlock->GetPet();
    if (!botPet)
    {
        if (SpellPregatit(688))
        {
            botWarlock->CastSpell(botWarlock, ObtineRankMaximSpell(688), false);
            return;
        }
    }
    else if (botPet->IsAlive() && !botPet->IsLoading())
    {
        if (botPet->GetReactState() != REACT_DEFENSIVE)
        {
            botPet->SetReactState(REACT_DEFENSIVE);
        }

        if (!botWarlock->HasAura(ObtineRankMaximSpell(6307)) && !botPet->GetSpellHistory()->HasCooldown(ObtineRankMaximSpell(6307)))
        {
            botPet->CastSpell(botPet, ObtineRankMaximSpell(6307), false);
        }

        if (SpellInfo const* fireboltInfo = sSpellMgr->GetSpellInfo(27267))
        {
            bool dejaActiv = false;
            for (uint8 i = 0; i < botPet->GetPetAutoSpellSize(); ++i)
            {
                if (botPet->GetPetAutoSpellOnPos(i) == fireboltInfo->Id)
                {
                    dejaActiv = true;
                    break;
                }
            }

            if (!dejaActiv)
            {
                botPet->ToggleAutocast(fireboltInfo, true);
            }
        }

        if (botPet->GetVictim() != victim)
        {
            botPet->Attack(victim, true);
        }
    }

    // ==================== MANAGEMENT BUFFERI DEFENSIVI / RESURSE ====================

    // Fel Armor (ID: 28176): Creste spell power-ul si ofera auto-heal pasiv din toate daunele magice aplicate
    if (!botWarlock->HasAura(ObtineRankMaximSpell(28176)))
    {
        botWarlock->CastSpell(botWarlock, ObtineRankMaximSpell(28176), false);
        return;
    }

    // Life Tap (ID: 1454): Converteste viata in mana daca stocul de mana e mic (< 20%) dar viata e stabila (> 50%)
    if (myMana < 20 && myHp > 50 && SpellPregatit(1454))
    {
        botWarlock->CastSpell(botWarlock, ObtineRankMaximSpell(1454), false);
        return;
    }

    // Shadow Ward (ID: 6229): Scut absorbtie daune de tip umbra, activat rapid daca primeste damage in timp
    if (botWarlock->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) && SpellPregatit(6229))
    {
        botWarlock->CastSpell(botWarlock, ObtineRankMaximSpell(6229), false);
        return;
    }

    // Death Coil (ID: 6789): Abilitatea suprema de salvare (instant). Ofera heal direct si frica de 3 secunde tintei sub 35% HP
    if (myHp < 35 && targetDist < 30.0f && SpellPregatit(6789))
    {
        botWarlock->CastSpell(victim, ObtineRankMaximSpell(6789), false);
        return;
    }

    // ==================== CROWD CONTROL IN ARENA ====================

    // Howl of Terror (ID: 5484): Frica AoE instantanee daca inamicii melee ajung periculos de aproape (< 8 metri)
    if (targetDist <= 8.0f && SpellPregatit(5484))
    {
        botWarlock->CastSpell(botWarlock, ObtineRankMaximSpell(5484), false);
        return;
    }

    // Fear controlat (ID: 5782): Spam controlat tactic cu timp de cast, rulat doar daca tinta nu fuge deja dintr-un efect similar
    if (targetDist <= 30.0f && !victim->HasAuraType(SPELL_AURA_MOD_FEAR) && SpellPregatit(5782))
    {
        if (urand(0, 100) < 20)
        {
            botWarlock->CastSpell(victim, ObtineRankMaximSpell(5782), false);
            return;
        }
    }

    // ==================== ROTATIE BLOCKED / CURSES (BLESTEME) ====================

    if (!botWarlock->IsWithinLOSInMap(victim))
        return;

    if (targetDist <= 30.0f)
    {
        // Curse of Tongues (ID: 1714): Aplicat instant pe healer/caster advers daca schimba intr-un cast activ
        if (victim->IsNonMeleeSpellCast(false, false, true) && !victim->HasAura(ObtineRankMaximSpell(1714)))
        {
            botWarlock->CastSpell(victim, ObtineRankMaximSpell(1714), false);
            return;
        }

        // Curse of Exhaustion (ID: 18223): Incetinire instanta de Subtlety/Affli aplicata claselor fizice care alearga spre bot
        if (victim->IsControlledByPlayer() && !victim->HasAuraWithMechanic(1 << MECHANIC_SNARE) && !victim->HasAura(ObtineRankMaximSpell(18223)))
        {
            botWarlock->CastSpell(victim, ObtineRankMaximSpell(18223), false);
            return;
        }

        // Curse of Agony (ID: 980): DoT-ul de baza de blestem daca nu exista alta urgenta de control tactict pe tinta
        if (!victim->HasAura(ObtineRankMaximSpell(980)) && !victim->HasAura(ObtineRankMaximSpell(1714)) && !victim->HasAura(ObtineRankMaximSpell(18223)))
        {
            botWarlock->CastSpell(victim, ObtineRankMaximSpell(980), false);
            return;
        }
    }

    // ==================== ROTATIE DE DAMAGE PUR AFFLICTION (DAMAGE IN TIMP) ====================

    // 1. Corruption (ID: 172): Cel mai important DoT instant. Aplicat obligatoriu pe tinta
    if (targetDist <= 36.0f && !victim->HasAura(ObtineRankMaximSpell(172)))
    {
        botWarlock->CastSpell(victim, ObtineRankMaximSpell(172), false);
        return;
    }

    // 2. Unstable Affliction (ID: 30108): DoT-ul de final de arbore Affliction (cu timp de cast). 
    // Protejeaza toate celelalte DoT-uri (daca un healer advers ii da dispell, primeste silent instant de 5 secunde si daune mari)
    if (targetDist <= 30.0f && !victim->HasAura(ObtineRankMaximSpell(30108)))
    {
        botWarlock->CastSpell(victim, ObtineRankMaximSpell(30108), false);
        return;
    }

    // 3. Haunt (ID: 48181): Proiectil de baza (cu timp de cast). Sporeste cu 20% toate daunele in timp aplicate pe inamic si ofera heal la intoarcere
    if (targetDist <= 30.0f && SpellPregatit(48181))
    {
        botWarlock->CastSpell(victim, ObtineRankMaximSpell(48181), false);
        return;
    }

    // 4. Drain Soul (ID: 1120): Vrajitorie canalizata de executie (Channeled). Se porneste doar cand inamicul intra in pragul critic de viata scazuta (< 25% HP)
    if (targetDist <= 30.0f && victim->GetHealthPct() < 25)
    {
        botWarlock->CastSpell(victim, ObtineRankMaximSpell(1120), false);
        return;
    }

    // 5. Shadow Bolt (ID: 686): Proiectilul clasic (filler) cu timp de cast lung. Lansat doar cand toate DoT-urile ruleaza pe inamic
    if (targetDist <= 30.0f)
    {
        botWarlock->CastSpell(victim, ObtineRankMaximSpell(686), false);
        return;
    }
}

// frost
void ExecutaLogicaMagePvP(Player* botMage, Unit*& victim, BotRole /*rolBot*/)
{
    // 1. VERIFICARI STRICTE DE SIGURANTA
    if (!botMage || !botMage->IsAlive() || botMage->IsLoading())
        return;

    // Activare Medalion PvP instant la pierderea controlului (Fara return ca sa nu piarda frame-ul)
    if (botMage->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
        !botMage->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
    {
        IncearcaSaFolosestiMedalionPvP(botMage);
    }

    // Selectam tinta inamica (Mage-ul este intotdeauna CASTER in PvP)
    victim = GhostSelectTarget(botMage, victim, false);
    if (!victim || !victim->IsAlive() || !botMage->IsHostileTo(victim))
        return;

    // Apelam miscarea ta nativa de Caster pe care o ai in core
    GhostMoveAndAttackCaster(botMage, victim);

    float targetDist = botMage->GetDistance(victim);
    uint32 myHp = botMage->GetHealthPct();
    uint32 myMana = botMage->GetPower(POWER_MANA) * 100 / botMage->GetMaxPower(POWER_MANA);

    // Ignoram complet tintele care au imunitati totale de PvP (Bula de Paladin / Ice Block)
    if (victim->HasAura(642) || victim->HasAura(45438))
        return;

    // Scurtatura inteligenta pentru a verifica daca o abilitate nu este in cooldown
    auto SpellPregatit = [&](uint32 spellId) -> bool
        {
            return !botMage->GetSpellHistory()->HasCooldown(spellId);
        };

    // Daca deja a reusit sa inceapa un cast lung (ex: Frostbolt), il lasam sa termine si dam return instant ca sa nu tremure
    if (botMage->HasUnitState(UNIT_STATE_CASTING))
        return;

    // ==================== REFRESH PRE-BUFFURI SI SCUTURI (FARA RETURN - OUT OF GCD) ====================

    // MANAGEMENT ACTIV AL ARMAREI (Daca are mana putina < 35%, pune Mage Armor ID: 6117 pentru regenerare, altfel tine Ice Armor ID: 7302)
    uint32 armuraNecesara = (myMana < 35) ? 6117 : 7302;
    if (!botMage->HasAura(ObtineRankMaximSpell(armuraNecesara)))
    {
        // Stergem cealalta armura pentru a nu se bloca
        botMage->RemoveAurasDueToSpell(ObtineRankMaximSpell(armuraNecesara == 6117 ? 7302 : 6117));
        botMage->CastSpell(botMage, ObtineRankMaximSpell(armuraNecesara), false);
    }

    // Ice Barrier (ID: 11426): Scutul emblematic de Frost. Se pune instant daca lipseste
    if (!botMage->HasAura(ObtineRankMaximSpell(11426)) && SpellPregatit(11426))
    {
        botMage->CastSpell(botMage, ObtineRankMaximSpell(11426), false);
    }

    // Icy Veins (ID: 12472): Cooldown ofensiv major care mareste viteza de cast. Se porneste instant
    if (botMage->IsInCombat() && SpellPregatit(12472) && urand(0, 100) < 40)
    {
        botMage->CastSpell(botMage, ObtineRankMaximSpell(12472), false);
    }

    // ==================== REGENERARE REGLATA MANA CRITICA (EVOCATION) ====================
    // Evocation (ID: 12051): Daca mana scade sub 20% si botul are viata stabila (> 40%), se opreste sa isi faca mana instant full [3.1]
    if (myMana < 20 && myHp > 40 && SpellPregatit(12051))
    {
        botMage->CastSpell(botMage, ObtineRankMaximSpell(12051), false);
        return; // Return obligatoriu pentru a lasa canalizarea sa inceapa
    }

    // ==================== PRIORITATE 1: ROTATIE SUPREMA SHATTER COMBO (DAMAGE SI BURST INSTANT) ====================

    // Linie vizuala obligatorie pentru a putea incepe atacurile
    if (!botMage->IsWithinLOSInMap(victim))
        return;

    // Verificam starea critica de Shatter: Daca inamicul e inghetat (Frozen) sau botul are proc de Fingers of Frost (ID: 44544)
    bool tintaEsteInghetata = victim->IsFrozen() || botMage->HasAura(44544);

    if (tintaEsteInghetata && targetDist <= 30.0f)
    {
        // Prioritate Sub-1: Deep Freeze (ID: 44572) - Stun-ul de final de Frost de 5 secunde.
        if (SpellPregatit(44572))
        {
            botMage->CastSpell(victim, ObtineRankMaximSpell(44572), false);
            return;
        }

        // Prioritate Sub-2: Ice Lance (ID: 30455) - Burst instantaneu devastator (x3 damage pe tinte inghetate).
        botMage->CastSpell(victim, ObtineRankMaximSpell(30455), false);
        return;
    }

    // ==================== PRIORITATE 2: PROC-URI SI ABILITATI INSTANTANEE (ANTI-KICK OFFENSIVE) ====================

    // 1. Proc-ul Brain Freeze (ID aura: 57761): Permite lansarea unui Fireball (ID: 133) complet INSTANT.
    if (botMage->HasAura(57761) && targetDist <= 35.0f)
    {
        botMage->CastSpell(victim, ObtineRankMaximSpell(133), false);
        return;
    }

    // 2. Cone of Cold (ID: 120): Atac instantaneu in semi-cerc daca inamicul este aproape (< 10 metri).
    if (targetDist <= 10.0f && SpellPregatit(120))
    {
        botMage->CastSpell(victim, ObtineRankMaximSpell(120), false);
        return;
    }

    // 3. Fire Blast (ID: 2136): Executie rapida la distanta (20m) daca inamicul are sub 30% HP.
    if (targetDist <= 20.0f && victim->GetHealthPct() < 30 && SpellPregatit(2136))
    {
        botMage->CastSpell(victim, ObtineRankMaximSpell(2136), false);
        return;
    }

    // ==================== PRIORITATE 3: GENERATOARE DE CRITICE SI PROC-URI (FILLERS CU TIMP DE CAST) ====================
    if (targetDist <= 30.0f)
    {
        // Frostbolt (ID: 116): Ultimul filler, genereaza degetele si procul de Fireball instant.
        botMage->CastSpell(victim, ObtineRankMaximSpell(116), false);
        return;
    }

    // ==================== LOGICA SECUNDARA / UTILITIES (KICK SI BUFFERI) ====================

    // Counterspell (ID: 2139): Da Kick instant la distanta (30m) daca tinta casteaza o magie activa
    if (targetDist <= 30.0f && victim->IsNonMeleeSpellCast(false, false, true) && SpellPregatit(2139))
    {
        botMage->CastSpell(victim, ObtineRankMaximSpell(2139), false);
        return;
    }

    // Frost Nova (ID: 122): Fortam inghetarea daca clasele melee ajung prea aproape (< 8m).
    if (targetDist <= 8.0f && SpellPregatit(122))
    {
        botMage->CastSpell(botMage, ObtineRankMaximSpell(122), false);
        return;
    }
}

// subtlety
void ExecutaLogicaRoguePvP(Player* botRogue, Unit*& victim, BotRole /*rolBot*/)
{
    // 1. VERIFICARI STRICTE DE SIGURANTA
    if (!botRogue || !botRogue->IsAlive() || botRogue->IsLoading())
        return;

    // Activare Medalion PvP instant la pierderea controlului (Fara return ca sa nu piarda DPS)
    if (botRogue->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
        !botRogue->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
    {
        IncearcaSaFolosestiMedalionPvP(botRogue);
    }

    // Selectam tinta inamica (Rogue este intotdeauna MELEE in PvP)
    victim = GhostSelectTarget(botRogue, victim, false);
    if (!victim || !victim->IsAlive() || !botRogue->IsHostileTo(victim))
        return;

    // Pentru clasa Melee folosim miscarea nativa de urmarire stransa pe care o ai in core
    GhostMoveAndAttackMelee(botRogue, victim);

    float targetDist = botRogue->GetDistance(victim);
    uint32 myHp = botRogue->GetHealthPct();
    uint32 energy = botRogue->GetPower(POWER_ENERGY);
    uint8 comboPoints = botRogue->GetComboPoints();

    // Ignoram tintele care au imunitati totale active (Bula Paladin / Ice Block)
    if (victim->HasAura(642) || victim->HasAura(45438))
        return;

    // Detectam starile specifice de Subtlety (Stealth sau Shadow Dance activ)
    bool stealthed = botRogue->HasAuraType(SPELL_AURA_MOD_STEALTH);
    bool shadowDance = botRogue->HasAura(51713);

    // Scurtatura inteligenta pentru a verifica daca o abilitate nu este in cooldown
    auto SpellPregatit = [&](uint32 spellId) -> bool
        {
            return !botRogue->GetSpellHistory()->HasCooldown(spellId);
        };

    // ==================== COOLDOWNS OFENSIVE SUPREME (BURST OUT OF GCD) ====================

    // Shadow Dance (ID: 51713): Activam cooldown-ul suprem de Subtlety pentru a debloca Ambush din picioare.
    // Il pornim fara return atunci cand energia este aproape plina (> 70) si suntem in melee.
    if (!stealthed && !shadowDance && energy >= 70 && targetDist <= 5.0f && SpellPregatit(51713) && urand(0, 100) < 40)
    {
        botRogue->CastSpell(botRogue, ObtineRankMaximSpell(51713), false);
        shadowDance = true; // Il marcam ca activ pentru frame-ul curent
    }

    // Shadowstep (ID: 36554): Teleportare instanta in spatele inamicului.
    // Crucial pentru DPS pentru a anula timpul pierdut alergand daca inamicul fuge (8m - 25m).
    if (targetDist >= 8.0f && targetDist <= 25.0f && SpellPregatit(36554))
    {
        botRogue->CastSpell(victim, ObtineRankMaximSpell(36554), false);
        return;
    }

    // ==================== PRIORITATE 1: ROTATIE DIN STEALTH / SHADOW DANCE (OPENERS MASIVI) ====================
    if (stealthed || shadowDance)
    {
        if (targetDist <= 5.0f)
        {
            // Deschidem intotdeauna cu Cheap Shot (ID: 1833) daca tinta nu are deja stun, pentru control si 2 puncte combo instant
            if (!victim->HasAuraType(SPELL_AURA_MOD_STUN) && energy >= 40)
            {
                botRogue->CastSpell(victim, ObtineRankMaximSpell(1833), false);
                return;
            }

            // Daca are deja stun, dam SPAM la Ambush (ID: 8676) - sursa principala de burst urias din Subtlety
            if (energy >= 60)
            {
                botRogue->CastSpell(victim, ObtineRankMaximSpell(8676), false);
                return;
            }
        }
        return; // Oprim frame-ul aici daca suntem in mod Stealth/Dance pentru a nu strica energia pe abilitati slabe
    }

    // ==================== PRIORITATE 2: ABILITATI DE FINISARE (FINISHERS LA 4-5 COMBO POINTS) ====================
    if (comboPoints >= 4)
    {
        if (targetDist <= 5.0f)
        {
            // Kidney Shot (ID: 408): Stun de 5-6 secunde. Il dam doar daca tinta nu este deja blocata de alt stun.
            if (!victim->HasAuraType(SPELL_AURA_MOD_STUN) && energy >= 25 && SpellPregatit(408))
            {
                botRogue->CastSpell(victim, ObtineRankMaximSpell(408), false);
                return;
            }

            // Eviscerate (ID: 2098): Lovitura finala suprema pentru DPS maxim direct. Consuma punctele ramase in mii de daune.
            if (energy >= 35)
            {
                botRogue->CastSpell(victim, ObtineRankMaximSpell(2098), false);
                return;
            }
        }
        return;
    }

    // ==================== PRIORITATE 3: GENERATOARE DE COMBO POINTS (MAIN ATTACK FILLER) ====================
    if (targetDist <= 5.0f)
    {
        // Hemorrhage (ID: 16511): Generatorul ultra-rapid si ieftin de puncte combo (doar 35 energie).
        // Lasa si un debuff pe tinta care creste tot damage-ul fizic facut de Rogue. Spam masiv aici!
        if (energy >= 35)
        {
            botRogue->CastSpell(victim, ObtineRankMaximSpell(16511), false);
            return;
        }
    }

    // ==================== LOGICA SECUNDARA / UTILITIES (SE DEDECLANSEAZA DOAR DACA NU EXISTA ENERGIE DE ATAC ACUM) ====================

    // Kick (ID: 1766): Da intrerupere instantanee daca tinta casteaza o magie si botul are 15+ energie
    if (targetDist <= 5.0f && victim->IsNonMeleeSpellCast(false, false, true) && energy >= 15 && SpellPregatit(1766))
    {
        botRogue->CastSpell(victim, ObtineRankMaximSpell(1766), false);
        return;
    }

    // MANAGEMENT DEFENSIV TIMP LIBER (Fisat fara return ca sa nu fure din frame-ul de atac viitor)
    // Cloak of Shadows (ID: 31224): Scut imun magic daca viata scade sub 40% si are DoT-uri active
    if (myHp < 40 && botRogue->HasAuraType(SPELL_AURA_PERIODIC_DAMAGE) && SpellPregatit(31224))
    {
        botRogue->CastSpell(botRogue, ObtineRankMaximSpell(31224), false);
    }

    // Vanish (ID: 1856): Salvare extrema in Stealth la HP critic (< 25%)
    if (myHp < 25 && !stealthed && !shadowDance && SpellPregatit(1856))
    {
        botRogue->CastSpell(botRogue, ObtineRankMaximSpell(1856), false);
        return;
    }
}

// unholy
void ExecutaLogicaDeathKnightPvP(Player* botDK, Unit*& victim, BotRole /*rolBot*/)
{
    // 1. VERIFICARI STRICTE DE SIGURANTA
    if (!botDK || !botDK->IsAlive() || botDK->IsLoading())
        return;

    // Activare Medalion PvP instant la pierderea controlului (Fara return ca sa nu piarda DPS)
    if (botDK->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
        !botDK->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
    {
        IncearcaSaFolosestiMedalionPvP(botDK);
    }

    // Selectam tinta inamica (DK este intotdeauna clasa MELEE in PvP)
    victim = GhostSelectTarget(botDK, victim, false);
    if (!victim || !victim->IsAlive() || !botDK->IsHostileTo(victim))
        return;

    // Gestionarea miscarii native de urmarire stransa pe care o detii in core
    GhostMoveAndAttackMelee(botDK, victim);

    float targetDist = botDK->GetDistance(victim);
    uint32 myHp = botDK->GetHealthPct();
    uint32 runicPower = botDK->GetPower(POWER_RUNIC_POWER);

    // Ignoram tintele aflate in imunitati absolute de PvP (Bula de Paladin / Ice Block)
    if (victim->HasAura(642) || victim->HasAura(45438))
        return;

    // Verificarea nativa a disponibilitatii resurselor direct din istoricul serverului
    auto PotiDaSpell = [&](uint32 spellId) -> bool
        {
            return !botDK->GetSpellHistory()->HasCooldown(spellId);
        };

    // ==================== REFRESH PRE-BUFFURI OPTIMIZAT (FARA RETURN) ====================

    // Bone Shield (ID: 49222): Pune scutul doar daca nu e in lupta stransa sau daca are o secunda libera
    if (!botDK->HasAura(ObtineRankMaximSpell(49222)) && PotiDaSpell(49222) && targetDist > 5.0f)
    {
        botDK->CastSpell(botDK, ObtineRankMaximSpell(49222), false);
    }

    // Horn of Winter (ID: 57330): Da buff si genereaza 10 RP. Fara return pentru a nu bloca atacul fizic
    if (!botDK->HasAura(ObtineRankMaximSpell(57330)) && PotiDaSpell(57330))
    {
        botDK->CastSpell(botDK, ObtineRankMaximSpell(57330), false);
    }

    // ==================== URMARIRE SI TRAS INAMIC (DEATH GRIP & CHAINS) ====================

    // Death Grip (ID: 49576): Trage inamicul instant daca fuge la distanta mare (intre 10m si 30m)
    if (targetDist >= 10.0f && targetDist <= 30.0f && PotiDaSpell(49576))
    {
        botDK->CastSpell(victim, ObtineRankMaximSpell(49576), false);
        return;
    }

    // Chains of Ice (ID: 45524): Incetineala de 95% daca tinta fuge (distanta > 8m)
    if (targetDist >= 8.0f && targetDist <= 30.0f && !victim->HasAura(ObtineRankMaximSpell(45524)) && PotiDaSpell(45524))
    {
        botDK->CastSpell(victim, ObtineRankMaximSpell(45524), false);
        return;
    }

    // ==================== ROTATIE DE BOLI (DISEASES - FACTORUL MULTIPLICATOR DE DPS) ====================
    if (targetDist <= 30.0f)
    {
        // 1. Frost Fever via Icy Touch (ID: 45477)
        if (!victim->HasAura(ObtineRankMaximSpell(55095)) && PotiDaSpell(45477))
        {
            botDK->CastSpell(victim, ObtineRankMaximSpell(45477), false);
            return;
        }

        // 2. Blood Plague via Plague Strike (ID: 45462) - Doar in melee
        if (targetDist <= 5.0f && !victim->HasAura(ObtineRankMaximSpell(55078)) && PotiDaSpell(45462))
        {
            botDK->CastSpell(victim, ObtineRankMaximSpell(45462), false);
            return;
        }
    }

    // Linie vizuala obligatorie pentru a continua atacurile de baza
    if (!botDK->IsWithinLOSInMap(victim))
        return;

    // Extragem starea bolilor de pe tinta
    bool areBoliActive = victim->HasAura(ObtineRankMaximSpell(55095)) && victim->HasAura(ObtineRankMaximSpell(55078));

    // ==================== ROTATIE DE DAMAGE MAXIM (BURST MELEE OBLIGATORIU) ====================
    if (targetDist <= 5.0f)
    {
        // PRIORITATE 1: Scourge Strike (ID: 55090) - Sursa principala de 2k+ DPS. Se da de fiecare data cand bolile sunt sus!
        if (areBoliActive && PotiDaSpell(55090))
        {
            botDK->CastSpell(victim, ObtineRankMaximSpell(55090), false);
            return;
        }

        // PRIORITATE 2: Death Strike (ID: 49998) - Se foloseste DOAR ca urgenta majora daca viata scade sub 45% (lasam rune pentru Scourge Strike)
        if (myHp < 45 && areBoliActive && PotiDaSpell(49998))
        {
            botDK->CastSpell(victim, ObtineRankMaximSpell(49998), false);
            return;
        }

        // PRIORITATE 3: Blood Strike (ID: 45902) - Pentru rune de Sange libere
        if (PotiDaSpell(45902))
        {
            botDK->CastSpell(victim, ObtineRankMaximSpell(45902), false);
            return;
        }

        // PRIORITATE 4: Death Coil (ID: 47541) - Descarca rapid puterea runica adunata pentru burst magic de completare
        if (runicPower >= 40)
        {
            botDK->CastSpell(victim, ObtineRankMaximSpell(47541), false);
            return;
        }
    }

    // ==================== BURST COOLDOWNS SUPREME (GARGOYLE & RESET RUNE) ====================

    // Summon Gargoyle (ID: 49206): Porneste al doilea burst de DPS in fundal de indata ce are ambele boli si 60+ RP
    if (targetDist <= 30.0f && runicPower >= 60 && areBoliActive && PotiDaSpell(49206))
    {
        botDK->CastSpell(victim, ObtineRankMaximSpell(49206), false);
        return;
    }

    // Empower Rune Weapon (ID: 47568): Daca a ramas blocat fara rune de Scourge Strike, le reseteaza instant pentru a continua DPS-ul
    if (targetDist <= 5.0f && !PotiDaSpell(55090) && PotiDaSpell(47568))
    {
        botDK->CastSpell(botDK, ObtineRankMaximSpell(47568), false);
        return;
    }

    // ==================== LOGICA SECUNDARA / UTILITIES (SE RULEAZA DOAR DACA NU ARE RUNES DE ATAC) ====================

    // ANTI-CC DEFENSIV (LICHBORNE)
    if (botDK->HasAuraWithMechanic((1 << MECHANIC_FEAR) | (1 << MECHANIC_SLEEP) | (1 << MECHANIC_CHARM)))
    {
        if (PotiDaSpell(49039))
        {
            botDK->CastSpell(botDK, ObtineRankMaximSpell(49039), false);
            return;
        }
    }

    // Mind Freeze (ID: 47528): Kick instant in melee daca tinta casteaza
    if (targetDist <= 5.0f && victim->IsNonMeleeSpellCast(false, false, true) && runicPower >= 20)
    {
        botDK->CastSpell(victim, ObtineRankMaximSpell(47528), false);
        return;
    }

    // Strangulate (ID: 47476): Silence de la distanta
    if (targetDist > 5.0f && targetDist <= 30.0f && victim->IsNonMeleeSpellCast(false, false, true) && PotiDaSpell(47476))
    {
        botDK->CastSpell(victim, ObtineRankMaximSpell(47476), false);
        return;
    }

    // DEFENSIVE UNDER COOLDOWNS (AMS, AMZ, IBF)
    if (myHp < 50 && victim->IsNonMeleeSpellCast(false, false, true) && PotiDaSpell(50464)) botDK->CastSpell(botDK, ObtineRankMaximSpell(50464), false);
    if (myHp < 65 && victim->IsNonMeleeSpellCast(false, false, true) && PotiDaSpell(48707)) botDK->CastSpell(botDK, ObtineRankMaximSpell(48707), false);
    if (myHp < 40 && runicPower >= 20 && PotiDaSpell(48792)) botDK->CastSpell(botDK, ObtineRankMaximSpell(48792), false);

    // INVOCARE GHOUL (RAISE DEAD) - Ruleaza doar la final ca sa nu piarda frame-ul de atac fizic
    Pet* dkPet = botDK->GetPet();
    if (!dkPet || !dkPet->IsAlive())
    {
        if (PotiDaSpell(46584))
        {
            botDK->CastSpell(botDK, ObtineRankMaximSpell(46584), false);
            return;
        }
    }
    else if (dkPet->IsAlive() && !dkPet->IsLoading())
    {
        if (dkPet->GetTarget() != victim->GetGUID()) dkPet->Attack(victim, true);
        if (targetDist <= 5.0f && !dkPet->GetSpellHistory()->HasCooldown(47481)) dkPet->CastSpell(victim, 47481, false); // Gnaw Stun
    }
}

// elemental
void ExecutaLogicaShamanPvP(Player* botShaman, Unit*& victim, BotRole /*rolBot*/)
{
    // 1. VERIFICARI STRICTE DE SIGURANTA
    if (!botShaman || !botShaman->IsAlive() || botShaman->IsLoading())
        return;

    // Activare Medalion PvP instant la pierderea controlului (Fara return ca sa nu piarda DPS)
    if (botShaman->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
        !botShaman->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
    {
        IncearcaSaFolosestiMedalionPvP(botShaman);
    }

    // Selectam tinta inamica (Shaman-ul Elemental este intotdeauna CASTER in PvP)
    victim = GhostSelectTarget(botShaman, victim, false);
    if (!victim || !victim->IsAlive() || !botShaman->IsHostileTo(victim))
        return;

    // Apelam miscarea ta nativa de Caster pe care o ai in core
    GhostMoveAndAttackCaster(botShaman, victim);

    float targetDist = botShaman->GetDistance(victim);
    uint32 myHp = botShaman->GetHealthPct();
    uint32 myMana = botShaman->GetPower(POWER_MANA) * 100 / botShaman->GetMaxPower(POWER_MANA);

    // Ignoram tintele aflate in imunitati absolute de PvP (Bula de Paladin / Ice Block)
    if (victim->HasAura(642) || victim->HasAura(45438))
        return;

    // Scurtatura inteligenta pentru a verifica daca o abilitate nu este in cooldown
    auto SpellPregatit = [&](uint32 spellId) -> bool
        {
            return !botShaman->GetSpellHistory()->HasCooldown(spellId);
        };

    // Daca deja casteaza o magie lunga, il lasam sa termine si dam return instant (fara tremurat)
    if (botShaman->HasUnitState(UNIT_STATE_CASTING))
        return;

    // ==================== AUTO-DEFENSA SI VINDECARE DE URGENTA (HP & MANA CRITIC) ====================

    // 1. Vindecare activa la HP Critic: Daca scade sub 45% viata, isi da un Healing Wave (ID: 332) de urgenta
    if (myHp < 45)
    {
        botShaman->CastSpell(botShaman, ObtineRankMaximSpell(332), false);
        return; // Return obligatoriu pentru a lasa cast-ul de heal sa se execute
    }

    // 2. Urgenta Management Mana: Daca mana scade sub 35% si Thunderstorm (ID: 51490) e gata, o da pe loc pentru 8% mana instant
    if (myMana < 35 && SpellPregatit(51490))
    {
        botShaman->CastSpell(botShaman, ObtineRankMaximSpell(51490), false);
        // Nu dam return deoarece Thunderstorm este instant si nu consuma GCD
    }

    // ==================== MANAGEMENT BUFFERI PASIVI & SCUTURI (FARA RETURN - OUT OF GCD) ====================

    // Management dinamic scuturi: Daca are mana putina (< 40%), forteaza Water Shield (ID: 52127). 
    // Daca are mana ok, pune Lightning Shield (ID: 324) pentru a face damage reflexiv claselor melee care il lovesc.
    uint32 scutNecesar = (myMana < 40) ? 52127 : 324;
    if (!botShaman->HasAura(ObtineRankMaximSpell(scutNecesar)))
    {
        // Curatam scutul vechi opus pentru a preveni blocajele de auras
        botShaman->RemoveAurasDueToSpell(ObtineRankMaximSpell(scutNecesar == 52127 ? 324 : 52127));
        botShaman->CastSpell(botShaman, ObtineRankMaximSpell(scutNecesar), false);
    }

    // Elemental Mastery (ID: 16166): Cooldown ofensiv major. Face urmatorul cast instant.
    if (botShaman->IsInCombat() && SpellPregatit(16166) && urand(0, 100) < 40)
    {
        botShaman->CastSpell(botShaman, ObtineRankMaximSpell(16166), false);
    }

    // ==================== ANTI-CC SI DEFENSE TOTEMS (OUT OF GCD / PASIV) ====================

    // Tremor Totem (ID: 8143): Scoate instant efectele de frica (Fear) de pe el sau coechipieri
    if (botShaman->HasAuraWithMechanic((1 << MECHANIC_FEAR) | (1 << MECHANIC_SLEEP) | (1 << MECHANIC_CHARM)) && SpellPregatit(8143))
    {
        botShaman->CastSpell(botShaman, ObtineRankMaximSpell(8143), false);
    }

    // Grounding Totem (ID: 8177): Absoarbe magiile trimise de inamici
    if (victim->IsNonMeleeSpellCast(false, false, true) && targetDist <= 30.0f && SpellPregatit(8177) && urand(0, 100) < 30)
    {
        botShaman->CastSpell(botShaman, ObtineRankMaximSpell(8177), false);
    }

    // ==================== PRIORITATE 1: ROTATIE SUPREMA DE BURST ELEMENTAL (DPS MAXIM) ====================

    // Linie vizuala obligatorie pentru atacuri
    if (!botShaman->IsWithinLOSInMap(victim))
        return;

    if (targetDist <= 30.0f)
    {
        // Pasul 1: Flame Shock (ID: 8050) - Obligatoriu primul pentru a garanta critica pe Lava Burst
        if (!victim->HasAura(ObtineRankMaximSpell(8050)))
        {
            botShaman->CastSpell(victim, ObtineRankMaximSpell(8050), false);
            return;
        }

        // Pasul 2: Lava Burst (ID: 51505) - 100% Sansa de Critica daca tinta are Flame Shock
        if (victim->HasAura(ObtineRankMaximSpell(8050)) && SpellPregatit(51505))
        {
            botShaman->CastSpell(victim, ObtineRankMaximSpell(51505), false);
            return;
        }

        // Pasul 3: Chain Lightning (ID: 421) - Burst-ul rapid secundar
        if (SpellPregatit(421))
        {
            botShaman->CastSpell(victim, ObtineRankMaximSpell(421), false);
            return;
        }

        // Pasul 4: Earth Shock (ID: 8042) - Lovitura instanta din mers ca filler
        if (SpellPregatit(8042) && urand(0, 100) < 50)
        {
            botShaman->CastSpell(victim, ObtineRankMaximSpell(8042), false);
            return;
        }

        // Pasul 5: Lightning Bolt (ID: 403) - Fillerul de baza cu cast lung
        botShaman->CastSpell(victim, ObtineRankMaximSpell(403), false);
        return;
    }

    // ==================== LOGICA SECUNDARA / UTILITIES (KICK-URI SI CONTROL) ====================

    // Wind Shear (ID: 57994): Kick instant la distanta (raza 25m) cu cooldown mic
    if (targetDist <= 25.0f && victim->IsNonMeleeSpellCast(false, false, true) && SpellPregatit(57994))
    {
        botShaman->CastSpell(victim, ObtineRankMaximSpell(57994), false);
        return;
    }

    // Thunderstorm knockback de siguranta: Daca inamicii melee vin prea aproape (< 8m), ii arunca in spate
    if (targetDist <= 8.0f && SpellPregatit(51490))
    {
        botShaman->CastSpell(botShaman, ObtineRankMaximSpell(51490), false);
        return;
    }

    // Earthbind Totem (ID: 2484): Incetineaza inamicii in melee
    if (targetDist <= 10.0f && SpellPregatit(2484))
    {
        botShaman->CastSpell(botShaman, ObtineRankMaximSpell(2484), false);
        return;
    }
}

