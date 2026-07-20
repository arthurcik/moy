// ----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----


// bot roles
enum BotRole
{
    BOT_ROLE_NONE = 0,
    BOT_ROLE_MELEE = 1,
    BOT_ROLE_CASTER = 2,
    BOT_ROLE_HEALER = 3
};

struct BotAsyncTracker
{
    uint32 accountId = 0;
    uint32 charGuid = 0;
    WorldSession* realSession = nullptr;
    std::shared_ptr<CharacterDatabaseQueryHolder> holder = nullptr;
    //std::future<void> futureResult;
    bool isReady = false;
    bool isProcessed = false;
    bool isQueued = false;
    uint32 rejoinTimer = 0;
    bool kickedByPlayer = false;
    uint32 AccRelogDelay = 10000;
    bool AccRealBusy = false;
    bool RemoveFromWorld = false;
    bool AddFromChatCmd = false;
    BotRole determinatRol = BOT_ROLE_NONE;
};
extern std::vector<BotAsyncTracker> g_MultiBotTracker;
extern std::mutex g_BotTrackerMutex;

// arena join multi-Task
struct KittBotArenaTracker
{
    uint32 arenaTeamId = 0; // team ID
    bool areGrupIn2v2 = false; // daca e in coada
    bool areGrupIn3v3 = false; // daca e in coada
    bool areGrupIn5v5 = false; // daca e in coada
    bool inCursDeFormare = false; // blocare sa nu intre altul peste
    ObjectGuid botCareFormeaza; // cine formeaza grup
    ObjectGuid botLiderDeEchipa; // cine formeaza grup

    std::vector<ObjectGuid> botiOcupatiInFormare; // rezervati pt formare

    time_t timpInceputFormare = 0;
    time_t timpIntrareInCoada = 0;
};
extern std::unordered_map<uint32, KittBotArenaTracker> g_KittBotArenaRegistru;
// -------------




// load ghost
void PornesteTotiBotii();
void PornesteBotIndividual(uint32 accountId, uint32 charGuid);
void ForseazaStergereBotFantoma(BotAsyncTracker& tracker);

bool IsPlayerInBotTracker(uint32 charGuidLow);
BotRole DefinesteSiSalveazaRolulBotului(Player* botPlayer);
Unit* GhostSelectTarget(Player* botPlayer, Unit*& currentVictim, bool focusPeColeg);
Unit* GhostSelectFriendlyTarget(Player* botPlayer);
uint32 ObtineRankMaximSpell(uint32 spellId);
bool GhostIsMelee(Player* botPlayer);

// miscarea si attack
void GhostMoveAndAttackMelee(Player* botPlayer, Unit*& victim);
void GhostMoveAndAttackCaster(Player* botPlayer, Unit*& victim);
void GhostMoveAndHeal(Player* botPlayer, Unit* friendlyTarget);

bool IncearcaSaFolosestiMedalionPvP(Player* botPlayer);

void kitt_start_bot_pvp_AI(Player* botPlayer, uint32 diff);

void ExecutaLogicaPaladinPvP(Player* botPaladin, Unit*& victim, BotRole rolBot);
void ExecutaLogicaWarriorPvP(Player* botPlayer, Unit*& victim, BotRole rolBot);
void ExecutaLogicaDruidFeralPvP(Player* botPlayer, Unit*& victim, BotRole rolBot);
void ExecutaLogicaPriestDiscPvP(Player* botPriest, Unit*& victim, BotRole rolBot);
void ExecutaLogicaWarlockPvP(Player* botWarlock, Unit*& victim, BotRole rolBot);
void ExecutaLogicaMagePvP(Player* botWarlock, Unit*& victim, BotRole rolBot);
void ExecutaLogicaRoguePvP(Player* botRogue, Unit*& victim, BotRole rolBot);
void ExecutaLogicaDeathKnightPvP(Player* botDK, Unit*& victim, BotRole rolBot);
void ExecutaLogicaShamanPvP(Player* botShaman, Unit*& victim, BotRole rolBot);
