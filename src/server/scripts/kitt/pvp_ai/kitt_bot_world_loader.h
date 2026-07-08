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
    std::future<void> futureResult;
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

BotRole DefinesteSiSalveazaRolulBotului(Player* botPlayer);
Unit* GhostSelectTarget(Player* botPlayer, Unit* currentVictim, bool focusPeColeg);
uint32 ObtineRankMaximSpell(uint32 spellId);
bool GhostIsMelee(Player* botPlayer);

// miscarea si attack
void GhostMoveAndAttackMelee(Player* botPlayer, Unit* victim);
void GhostMoveAndAttackCaster(Player* botPlayer, Unit* victim);
void GhostMoveAndHeal(Player* botPlayer, Unit* friendlyTarget);

bool IncearcaSaFolosestiMedalionPvP(Player* botPlayer);

void kitt_start_bot_pvp_AI(Player* botPlayer);

void ExecutaLogicaPaladinPvP(Player* botPaladin, Unit* victim, BotRole rolBot);
void ExecutaLogicaMage(Player* botPlayer, Unit* victim, BotRole rolBot);
void ExecutaLogicaWarriorPvP(Player* botPlayer, Unit* victim, BotRole rolBot);
void ExecutaLogicaDruidFeralPvP(Player* botPlayer, Unit* victim, BotRole rolBot);
void ExecutaLogicaPriestDiscPvP(Player* botPriest, Unit* victim, BotRole rolBot);
void ExecutaLogicaRogue(Player* botPlayer, Unit* victim, BotRole rolBot);
