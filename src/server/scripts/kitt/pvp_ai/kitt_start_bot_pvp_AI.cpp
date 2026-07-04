// ----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

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


namespace
{
    // selectarea victima. true = ataca ce ataca si colegu(daca are tinta). false = prima gasita
    Unit* GhostSelectTarget(Player* botPlayer, Unit* currentVictim, bool focusPeColeg)
    {
        // Daca are deja o tinta valida, vie si vizibila, nu o schimbam aiurea
        if (currentVictim && currentVictim->IsAlive() && botPlayer->IsWithinDistInMap(currentVictim, 160.0f))
        {
            if (botPlayer->CanSeeOrDetect(currentVictim, false, true))
            {
                return currentVictim; // Pastreaza focusul curent
            }
        }

        // Daca tinta veche a murit sau a disparut, curatam starea
        if (currentVictim)
        {
            botPlayer->AttackStop();
            botPlayer->CombatStop();
            botPlayer->GetMotionMaster()->Clear();
            currentVictim = nullptr;
        }

        std::list<Player*> playersInCell;
        botPlayer->GetPlayerListInGrid(playersInCell, 160.0f);

        // --- SCENARIUL 1: ESTE HEALER (Vrea sa ia tinta coechipierului) ---
        if (focusPeColeg)
        {
            for (Player* coechipier : playersInCell)
            {
                if (!coechipier || !coechipier->IsAlive() || coechipier == botPlayer || coechipier->IsGameMaster())
                    continue;

                // Verificam daca este in aceeasi echipa (coechipier in arena)
                if (!botPlayer->IsHostileTo(coechipier))
                {
                    // Preluam victima pe care o ataca coechipierul nostru in acest moment
                    Unit* tintaColegului = coechipier->GetVictim();
                    if (tintaColegului && tintaColegului->IsAlive() && botPlayer->CanSeeOrDetect(tintaColegului, false, true))
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
            if (!targetPlayer || !targetPlayer->IsAlive() || targetPlayer == botPlayer || targetPlayer->IsGameMaster())
                continue;

            if (!botPlayer->CanSeeOrDetect(targetPlayer, false, true))
                continue;

            if (botPlayer->IsHostileTo(targetPlayer))
            {
                botPlayer->SetSelection(targetPlayer->GetGUID());
                return targetPlayer; // A gasit o tinta ostila libera
            }
        }

        return nullptr; // Nu a gasit pe nimeni in viata
    }


    // daca este melee sau nu
    bool GhostIsMelee(Player* botPlayer)
    {
        if (!botPlayer)
            return false;

        uint8 clasa = botPlayer->GetClass();

        // 1. Clase care sunt intotdeauna Melee
        if (clasa == CLASS_WARRIOR || clasa == CLASS_ROGUE || clasa == CLASS_DEATH_KNIGHT)
            return true;

        // 2. Clase care sunt intotdeauna Ranged/Casteri
        if (clasa == CLASS_MAGE || clasa == CLASS_PRIEST || clasa == CLASS_WARLOCK || clasa == CLASS_HUNTER)
            return false;

        // 3. Clase hibride: Preluam spec-ul activ si numaram punctele direct pe baza structurii tale
        uint8 activeSpec = botPlayer->GetActiveSpec();
        PlayerTalentMap const* talentMap = botPlayer->GetTalentMap(activeSpec);
        if (!talentMap)
            return false;

        uint32 pointsTab1 = 0;
        uint32 pointsTab2 = 0;
        uint32 pointsTab3 = 0;

        // Parcurgem harta si numaram pur si simplu punctele bazat pe membrul .spec din structura ta
        for (auto const& pair : *talentMap)
        {
            // pair.second este structura PlayerTalent stocata direct (folosim operatorul .)
            uint8 talentSpecTab = pair.second.spec;

            if (talentSpecTab == 0) pointsTab1++;
            else if (talentSpecTab == 1) pointsTab2++;
            else if (talentSpecTab == 2) pointsTab3++;
        }

        // PALADIN (Tab 0: Holy, Tab 1: Protection, Tab 2: Retribution)
        if (clasa == CLASS_PALADIN)
        {
            if (pointsTab3 > pointsTab1 || pointsTab2 > pointsTab1)
                return true; // Retri sau Prot
            return false;   // Holy
        }

        // SHAMAN (Tab 0: Elemental, Tab 1: Enhancement, Tab 2: Restoration)
        if (clasa == CLASS_SHAMAN)
        {
            if (pointsTab2 > pointsTab1 && pointsTab2 > pointsTab3)
                return true; // Doar Enhancement este Melee
            return false;
        }

        // DRUID (Tab 0: Balance, Tab 1: Feral, Tab 2: Restoration)
        if (clasa == CLASS_DRUID)
        {
            if (pointsTab2 > pointsTab1 && pointsTab2 > pointsTab3)
                return true; // Doar Feral (Pisica/Urs) este Melee
            return false;
        }

        return false;
    }

    // miscarea catre victima si attack start si se uitea la victima
    void GhostMoveAndAttack(Player* botPlayer, Unit* victim)
    {
        if (!botPlayer || !victim || !botPlayer->IsAlive() || !victim->IsAlive())
            return;

        // MODIFICARE GLOBALA: Daca botul a pierdut controlul caracterului (Stun, Fear, Sap, Blind, Leap, Charge etc.)
        /*if (botPlayer->HasUnitState(UNIT_STATE_LOST_CONTROL | UNIT_STATE_IN_FLIGHT))
        {
            return;
        }*/
        if (botPlayer->HasUnitState(UNIT_STATE_LOST_CONTROL |
            UNIT_STATE_IN_FLIGHT |
            UNIT_STATE_ROOT |
            UNIT_STATE_CHARMED |
            UNIT_STATE_CONFUSED_MOVE |
            UNIT_STATE_FLEEING_MOVE))
        {
            return;
        }

        bool esteMelee = GhostIsMelee(botPlayer);
        float dist = botPlayer->GetDistance(victim);
        uint32 miscareCurenta = botPlayer->GetMotionMaster()->GetCurrentMovementGeneratorType();
        ObjectGuid botGuid = botPlayer->GetGUID();

        // --- 1. SELECTIA TINTEI SI PORNIRE COMBAT ---
        if (botPlayer->GetTarget() != victim->GetGUID())
        {
            botPlayer->SetSelection(victim->GetGUID());
        }

        if (!botPlayer->IsInCombat())
        {
            botPlayer->SetInCombatWith(victim, true);
        }

        // --- 2. PROTECTIE CASTING ---
        if (botPlayer->IsNonMeleeSpellCast(false, false, true) || botPlayer->HasUnitState(UNIT_STATE_CASTING))
        {
            if (miscareCurenta == CHASE_MOTION_TYPE || miscareCurenta == POINT_MOTION_TYPE)
            {
                botPlayer->GetMotionMaster()->Clear();
                botPlayer->StopMoving();
            }
            return;
        }

        // --- 3. TIMING CONTROL (ANTI-TREMURAT SI ANTI-SPAM) ---
        static std::map<ObjectGuid, uint32> lastPathTime;
        uint32 acum = GameTime::GetGameTimeMS();

        if (!botPlayer->IsInCombat() || dist > 170.0f)
        {
            lastPathTime.erase(botGuid);
        }

        if (lastPathTime.find(botGuid) == lastPathTime.end())
        {
            lastPathTime[botGuid] = 0;
        }

        // --- 4. CONTROL CORP LA CORP (SUB 4 METRI) ---
        if (dist <= 4.0f)
        {
            if (esteMelee)
            {
                if (miscareCurenta != CHASE_MOTION_TYPE)
                {
                    botPlayer->GetMotionMaster()->Clear();
                    botPlayer->GetMotionMaster()->MoveChase(victim);
                    botPlayer->Attack(victim, true);
                }
            }
            else
            {
                if (miscareCurenta == CHASE_MOTION_TYPE || miscareCurenta == POINT_MOTION_TYPE)
                {
                    botPlayer->GetMotionMaster()->Clear();
                    botPlayer->StopMoving();
                }

                if (!botPlayer->HasUnitState(UNIT_STATE_MELEE_ATTACKING))
                {
                    //botPlayer->GetMotionMaster()->MoveChase(victim);
                    botPlayer->Attack(victim, true);
                }
            }

            if (!botPlayer->HasInArc(1.74f, victim))
            {
                botPlayer->SetFacingToObject(victim);
            }

            return;
        }

        // --- 5. LOGICA DE VALIDARE CONFORM ENUM-ULUI TAU ---
        bool folosestePathfinding = true;
        bool esteTimpulPentruPath = (acum - lastPathTime[botGuid] > (esteMelee ? ((dist >= 8.0f && dist <= 25.0f) ? 800U : 400U) : 600U));

        if (esteTimpulPentruPath || miscareCurenta != POINT_MOTION_TYPE)
        {
            PathGenerator path(botPlayer);
            bool pathResult = path.CalculatePath(victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ(), false);

            // Preluam tipul de drum returnat de generatorul tau
            uint32 tipDrum = path.GetPathType();

            // VALIDARE STRUCTURA: Daca functia a intors esec SAU tipul de drum contine flag-uri de blocaj total
            if (!pathResult ||
                (tipDrum & PATHFIND_NOPATH) ||
                (tipDrum & PATHFIND_FARFROMPOLY_END) ||
                (tipDrum & PATHFIND_INCOMPLETE))
            {
                folosestePathfinding = false; // Tinta e pe sfoara/pilon izolat -> devine fantoma pe linie dreapta
            }
        }

        // --- 6. LOGICA DE DEPLASARE DINAMICA ---
        if (esteMelee)
        {
            if (miscareCurenta != POINT_MOTION_TYPE || esteTimpulPentruPath)
            {
                lastPathTime[botGuid] = acum;
                botPlayer->GetMotionMaster()->Clear();

                // Daca folosestePathfinding e false, trece prin obstacole exact ca vechiul tau comportament (PATHFIND_SHORTCUT)
                botPlayer->GetMotionMaster()->MovePoint(1001, victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ(), folosestePathfinding);
                botPlayer->Attack(victim, false);
            }

            if (!botPlayer->HasInArc(0.5f, victim))
            {
                botPlayer->SetFacingToObject(victim);
            }
        }
        else // HEALER / CASTER
        {
            // Daca nu exista drum valid (e pe pilon), fortam casterul sa ignore distanta maxima si sa vina direct pe sfoara
            if (dist > 30.0f || !folosestePathfinding)
            {
                if (miscareCurenta != POINT_MOTION_TYPE || esteTimpulPentruPath)
                {
                    lastPathTime[botGuid] = acum;
                    botPlayer->GetMotionMaster()->Clear();
                    botPlayer->GetMotionMaster()->MovePoint(1001, victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ(), folosestePathfinding);
                }
            }
            else
            {
                if (miscareCurenta == POINT_MOTION_TYPE || esteTimpulPentruPath)
                {
                    lastPathTime[botGuid] = acum;
                    botPlayer->GetMotionMaster()->Clear();
                    //botPlayer->StopMoving();
                    botPlayer->GetMotionMaster()->MovePoint(1001, victim->GetPositionX(), victim->GetPositionY(), victim->GetPositionZ(), folosestePathfinding);
                    botPlayer->Attack(victim, false);

                }
            }

            if (!botPlayer->HasInArc(0.5f, victim))
            {
                botPlayer->SetFacingToObject(victim);
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
                return true;
            }
        }
        return false;
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

    void ExecutaLogicaPaladinPvP(Player* botPaladin, Unit* victim)
    {
        victim = GhostSelectTarget(botPaladin, victim, true);

        // 1. VERIFICARI STRICTE DE SIGURANTA
        if (!victim || !victim->IsAlive())
            return;

        if (!botPaladin || !botPaladin->IsAlive())
            return;

        //float targetDist = botPaladin->GetDistance(victim);

        // Medalionul de PvP se activeaza primul daca si-a pierdut controlul
        if (botPaladin->HasUnitState(UNIT_STATE_LOST_CONTROL) &&
            !botPaladin->HasUnitState(UNIT_STATE_JUMPING | UNIT_STATE_CHARGING))
        {
            IncearcaSaFolosestiMedalionPvP(botPaladin);
        }

        GhostMoveAndAttack(botPaladin, victim);

        uint32 myHp = botPaladin->GetHealthPct();

        // Divine Shield (Bula Mare - ID: 642) - Urgenta pentru Paladin
        if (myHp < 25 && !botPaladin->GetSpellHistory()->HasCooldown(642) && !botPaladin->HasAura(25))
        {
            botPaladin->CastSpell(botPaladin, ObtineRankMaximSpell(642), true);
            return;
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
                if (viataMembru < 30 && !membruGrup->HasAura(25) && !botPaladin->GetSpellHistory()->HasCooldown(1022) && !membruGrup->getAttackers().empty())
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
            if (ceaMaiMicaViataGrup <= 15 && !coechipierDeVindecat->HasAura(25) && !botPaladin->GetSpellHistory()->HasCooldown(633))
            {
                if (botPaladin->HasUnitState(UNIT_STATE_CASTING))
                    botPaladin->InterruptNonMeleeSpells(false);

                botPaladin->CastSpell(coechipierDeVindecat, ObtineRankMaximSpell(633), true);
                //return;
            }

            // B. Urgen?e: Holy Shock (Sub 40% viata) - Costa in jur de 550 Mana la lvl 80
            if (ceaMaiMicaViataGrup < 40 && currentMana >= 550 && !botPaladin->GetSpellHistory()->HasCooldown(20473))
            {
                if (botPaladin->HasUnitState(UNIT_STATE_CASTING))
                    botPaladin->InterruptNonMeleeSpells(false);

                // Reparare tremurat: Activam Divine Favor (100% critic) DOAR daca nu il are deja activ pasiv
                if (!botPaladin->GetSpellHistory()->HasCooldown(20216) && !botPaladin->HasAura(20216))
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
            if (ceaMaiMicaViataGrup < 60 && currentMana >= 1200 && !botPaladin->GetSpellHistory()->HasCooldown(635))
            {
                // Reparare tremurat: Activam Aura Mastery (Imun la kick) in aceeasi milisecunda cu inceputul castului
                if (!botPaladin->GetSpellHistory()->HasCooldown(31821) && !botPaladin->HasAura(31821))
                {
                    botPaladin->CastSpell(botPaladin, ObtineRankMaximSpell(31821), true);
                }

                botPaladin->CastSpell(coechipierDeVindecat, ObtineRankMaximSpell(635), false); // Incepe castul mare de Holy Light
                //return;
            }

            // D. Flash of Light (Mentinere rapida) - Costa doar in jur de 300 Mana la lvl 80 (Ieftin)
            if (currentMana >= 300 && !botPaladin->GetSpellHistory()->HasCooldown(19750))
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
            GhostMoveAndAttack(botPaladin, victim);

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

    void ExecutaLogicaMage(Player* botPlayer, Unit* victim)
    {
        if (!botPlayer->HasUnitState(UNIT_STATE_CASTING))
            botPlayer->CastSpell(victim, 116, false);
    }

    void ExecutaLogicaWarriorPvP(Player* botPlayer, Unit* victim) // warrior Arms
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
        GhostMoveAndAttack(botPlayer, victim);

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
                    botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);

                botPlayer->CastSpell(victim, ObtineRankMaximSpell(100), false); // Charge
                return;
            }
            // Daca Charge e pe cooldown dar are furie, schimba in Berserker si da Intercept
            else if (rage >= 10 && !botPlayer->GetSpellHistory()->HasCooldown(20252))
            {
                if (botPlayer->GetShapeshiftForm() != FORM_BERSERKERSTANCE)
                    botPlayer->SetShapeshiftForm(FORM_BERSERKERSTANCE);

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
                botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);

            botPlayer->CastSpell(victim, ObtineRankMaximSpell(1715), false); // Hamstring
        }

        // Interrupt: Daca victima da cast la o magie (Heal / CC), da Pummel in Berserker Stance
        if (victim->IsNonMeleeSpellCast(false, false, true) && !botPlayer->GetSpellHistory()->HasCooldown(6552))
        {
            if (botPlayer->GetShapeshiftForm() != FORM_BERSERKERSTANCE)
                botPlayer->SetShapeshiftForm(FORM_BERSERKERSTANCE);

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
                botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);

            botPlayer->CastSpell(victim, ObtineRankMaximSpell(5308), false); // Execute
            return;
        }

        // B. REND - Trebuie sa fie mereu activ pe tinta pentru talentul Taste for Blood
        if (!victim->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_WARRIOR, 0x20, 0x0, 0x0, botPlayer->GetGUID()) && rage >= 10)
        {
            // Cere Battle Stance obligatoriu
            if (botPlayer->GetShapeshiftForm() != FORM_BATTLESTANCE)
                botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);

            botPlayer->CastSpell(victim, ObtineRankMaximSpell(772), false); // Rend
            return;
        }

        // C. OVERPOWER - Daca s-a activat procul de Taste for Blood sau de la Dodge
        if (botPlayer->HasReactive(REACTIVE_OVERPOWER) || botPlayer->HasAura(56636) || botPlayer->HasAura(56637) || botPlayer->HasAura(56638))
        {
            if (botPlayer->GetShapeshiftForm() != FORM_BATTLESTANCE)
                botPlayer->SetShapeshiftForm(FORM_BATTLESTANCE);

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
                botPlayer->SetShapeshiftForm(FORM_BERSERKERSTANCE);

            botPlayer->CastSpell(botPlayer, ObtineRankMaximSpell(1719), false); // Recklessness
        }

        // G. RAGE DUMP (Heroic Strike) - Daca aduna prea multa furie si totul e pe cooldown
        if (rage > 70 && !botPlayer->GetCurrentSpell(CURRENT_MELEE_SPELL))
        {
            botPlayer->CastSpell(victim, ObtineRankMaximSpell(78), false); // Heroic Strike
        }
    }

    void ExecutaLogicaRogue(Player* botPlayer, Unit* victim)
    {
        botPlayer->SetPower(POWER_ENERGY, 100);
        if (botPlayer->IsWithinMeleeRange(victim))
            botPlayer->CastSpell(victim, 1752, false);
    }
}

void kitt_start_bot_pvp_AI(Player* botPlayer)
{
    if (!botPlayer || !botPlayer->IsAlive() || botPlayer->IsLoading())
        return;

    Unit* currentVictim = botPlayer->GetVictim();

    uint8 botClass = botPlayer->GetClass();
    switch (botClass)
    {
    case CLASS_PALADIN:
        ExecutaLogicaPaladinPvP(botPlayer, currentVictim);
        break;
    case CLASS_MAGE:
        ExecutaLogicaMage(botPlayer, currentVictim);
        break;
    case CLASS_WARRIOR:
        ExecutaLogicaWarriorPvP(botPlayer, currentVictim);
        break;
    case CLASS_ROGUE:
        ExecutaLogicaRogue(botPlayer, currentVictim);
        break;
    default:
        break;
    }
}

