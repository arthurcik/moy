// ----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "ScriptMgr.h"
#include "Player.h"
#include "Log.h"
#include "botmgr.h"
#include "WorldSession.h"
#include "GameTime.h"

#include "EventProcessor.h"
#include <chrono>

//#include "EventProcessor.h"
//#include <chrono>

namespace
{
    std::map<ObjectGuid, uint32> KittDuelTimers;
}

class Kitt_BotUnhideEvent : public BasicEvent
{
public:
    Kitt_BotUnhideEvent(Player* player, time_t duelEndTime) : _player(player), _duelEndTime(duelEndTime) {}

    bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
    {
        // Verificam daca jucatorul este inca online si valid in lume
        if (_player && _player->IsInWorld() && _player->GetSession() && _player->HaveBot())
        {
            ObjectGuid guid = _player->GetGUID();

            // Verificam daca timpul salvat in mapa este acelasi cu cel al acestui eveniment
            auto it = KittDuelTimers.find(guid);
            if (it != KittDuelTimers.end())
            {
                // Daca timpul nu mai coincide, inseamna ca s-a pornit/terminat un alt duel intre timp!
                // Oprim acest timer vechi fara sa facem nimic.
                if (it->second != _duelEndTime)
                    return true;
            }

            if (BotMgr* mgr = _player->GetBotMgr())
            {
                if (mgr->GetBotsHidden())
                {
                    mgr->SetBotsHidden(false);
                    _player->GetSession()->SendNotification("Botii tai au revenit.");
                }
            }

            KittDuelTimers.erase(guid);
        }
        return true; // Sterge evenimentul din memorie dupa rulare
    }

private:
    Player* _player;
    time_t _duelEndTime;
};

class kitt_duel_bots_hidden : public PlayerScript
{
public:
    kitt_duel_bots_hidden() : PlayerScript("kitt_duel_bots_hidden") {}

    void OnDuelRequest(Player* /*target*/, Player* /*challenger*/) override
    {

    }

    void OnDuelStart(Player* player1, Player* player2) override
    {
        time_t currentTime = GameTime::GetGameTime();

        // "Ascundem" botii
        if (player1 && player1->HaveBot())
        {
            KittDuelTimers[player1->GetGUID()] = currentTime;

            if (BotMgr* mgr = player1->GetBotMgr())
            {
                if (!mgr->GetBotsHidden())
                {
                    mgr->SetBotsHidden(true);
                    player1->GetSession()->SendNotification("Botii tai au fost ascunsi pe durata duelului.");
                }
            }
        }

        if (player2 && player2->HaveBot())
        {
            if (BotMgr* mgr = player2->GetBotMgr())
            {
                KittDuelTimers[player2->GetGUID()] = currentTime;

                if (!mgr->GetBotsHidden())
                {
                    mgr->SetBotsHidden(true);
                    player2->GetSession()->SendNotification("Botii tai au fost ascunsi pe durata duelului.");
                }
            }
        }
    }

    void OnDuelEnd(Player* winner, Player* loser, DuelCompleteType /*type*/) override
    {
        using namespace std::chrono_literals;
        time_t currentTime = GameTime::GetGameTime();

        if (winner && winner->HaveBot())
        {
            KittDuelTimers[winner->GetGUID()] = currentTime;
            winner->GetSession()->SendNotification("Duelul s-a terminat. Botii vor reaparea in 2 minute.");
            winner->m_Events.AddEvent(new Kitt_BotUnhideEvent(winner, currentTime), winner->m_Events.CalculateTime(120s));
        }

        if (loser && loser->HaveBot())
        {
            KittDuelTimers[loser->GetGUID()] = currentTime;
            loser->GetSession()->SendNotification("Duelul s-a terminat. Botii vor reaparea in 2 minute.");
            loser->m_Events.AddEvent(new Kitt_BotUnhideEvent(loser, currentTime), loser->m_Events.CalculateTime(120s));
        }

        /*if (winner && winner->HaveBot())
        {
            if (BotMgr* mgr = winner->GetBotMgr())
            {
                if (mgr->GetBotsHidden())
                {
                    mgr->SetBotsHidden(false);
                    winner->GetSession()->SendNotification("Botii tai au revenit.");
                }
            }
        }

        if (loser && loser->HaveBot())
        {
            if (BotMgr* mgr = loser->GetBotMgr())
            {
                if (mgr->GetBotsHidden())
                {
                    mgr->SetBotsHidden(false);
                    loser->GetSession()->SendNotification("Botii tai au revenit.");
                }
            }
        }*/
    }
};


void AddSC_kitt_duel_player()
{
    new kitt_duel_bots_hidden();
}
