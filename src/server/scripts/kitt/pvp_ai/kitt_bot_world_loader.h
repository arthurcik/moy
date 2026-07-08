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
static std::vector<BotAsyncTracker> g_MultiBotTracker;
