//----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#ifndef KITT_NPCBOT_AI_H
#define KITT_NPCBOT_AI_H

#include "Common.h"

class Player;
class Creature;
class bot_ai;

namespace KittBotAI
{
    bool IsSafeToCure(Unit* target); // for dispell

    void KittUpdateRaidStrategies(Creature* bot, Player* master); // main start


    void KittHandleMarrowgar(Creature* bot, Player* master, bot_ai* ai);

    void KittHandleLadyDeathwhisper(Creature* bot, Player* master, bot_ai* ai);

    void KittHandleGunship(Creature* bot, Player* master, bot_ai* ai);

    void KittHandleDeathSaurfang(Creature* bot, Player* master, bot_ai* ai);

    void KittHandleRotface(Creature* bot, Player* master, bot_ai* ai);

    void KittHandlePutricide(Creature* bot, Player* master, bot_ai* ai);

    void KittHandleValithia(Creature* bot, Player* master, bot_ai* ai);

    void KittHandleSindragosa(Creature* bot, Player* master, bot_ai* ai);

    void KittHandleLichKing(Creature* bot, Player* master, bot_ai* ai);


}

#endif
