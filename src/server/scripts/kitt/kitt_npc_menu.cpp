#include "ScriptMgr.h"
#include "CellImpl.h"
#include "CombatAI.h"
#include "Containers.h"
#include "CreatureTextMgr.h"
#include "GameEventMgr.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "GossipDef.h"
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PassiveAI.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "SmartAI.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "DatabaseEnv.h"
#include "Chat.h"
#include "Config.h"
#include <sstream>
#include <string>

/*enum kittGossipOptionIcon : uint8
{
    GOSSIP_ICON_CHAT = 0,                    // white chat bubble
    GOSSIP_ICON_VENDOR = 1,                  // brown bag
    GOSSIP_ICON_TAXI = 2,                    // flightmarker (paperplane)
    GOSSIP_ICON_TRAINER = 3,                 // brown book (trainer)
    GOSSIP_ICON_MONEY_BAG = 6,               // brown bag (with gold coin in lower corner)
    GOSSIP_ICON_TALK = 7,                    // white chat bubble (with "..." inside)
    GOSSIP_ICON_TABARD = 8,                  // white tabard
    GOSSIP_ICON_BATTLE = 9,                  // two crossed swords
    GOSSIP_ICON_DOT = 10                     // yellow dot/point
};*/


static std::string KittResetCode = "RESET"; // Valoare default config
static std::string KittNuApasaCode = "fun"; // default Fun Zone NuApasa

// valoarea in gold
static const uint32 MoneyDungeons    = 30 * 10000;   // galbeni necesari pentru Dungeons
static const uint32 MoneyRaid10      = 100 * 10000;  // galbeni necesari pentru Raid 10
static const uint32 MoneyRaid25      = 350 * 10000;  // galbeni necesari pentru Raid 25
static const uint32 MoneyRaid10H     = 200 * 10000;  // galbeni necesari pentru Raid 10 Heroic
static const uint32 MoneyRaid25H     = 500 * 10000;  // galbeni necesari pentru Raid 25 Heroic
static const uint32 NuApasaPret      = 1500 * 10000; // Fun Zone nu apasa pret
static const uint32 ResetAllSpellCd  = 100 * 10000;  // pret Reset all spell cooldown
static const uint32 ResetAllAura     = 50 * 10000;   // pret UnAura all spell


// conversie la string ca sa poate fi adaugat in mesaj
std::string sMoneyDungeons = std::to_string(MoneyDungeons / 10000);
static const std::string sMoneyRaid10 = std::to_string(MoneyRaid10 / 10000);
static const std::string sMoneyRaid25 = std::to_string(MoneyRaid25 / 10000);
static const std::string sMoneyRaid10H = std::to_string(MoneyRaid10H / 10000);
static const std::string sMoneyRaid25H = std::to_string(MoneyRaid25H / 10000);
static const std::string sNuApasaPret = std::to_string(NuApasaPret / 10000);
static const std::string sResetAllSpellCd = std::to_string(ResetAllSpellCd / 10000);
static const std::string sResetAllAura = std::to_string(ResetAllAura / 10000);






namespace KittNpcText
{
    // 1. Definim ID-urile aici
    // npc_text.ID
    enum Texts
    {
        KITT_NPC_HELLO                = 90014,
        KITT_INSTANCE_RESET           = 90015
        //KITT_INSTANCE_RESET_NO_SHOW   = 90016
    };

    // 2. Le punem automat in aceasta lista pentru verificare
    static const std::vector<uint32> KittAllNpcTexts = {
        KITT_NPC_HELLO,
        KITT_INSTANCE_RESET,
        //KITT_INSTANCE_RESET_NO_SHOW
    };
}

enum KittSender
{
    // uniq ID start 0
    KITT_SENDER_MENU_DIRECT_SELECT      = 0,   // comun pentru action select
    KITT_SENDER_OPEN_SUBMENU            = 1,   // comun pentru meniu
    KITT_SENDER_MENU_FUN_ZONE           = 2,   // comun pentru meniu fun zone
    KITT_SENDER_MENU_INSTANCE_RESET     = 3,   // comun pentru meniu instance reset
    KITT_SENDER_TELEPORT_TO             = 4    // comun pentru Teleport To
};

enum KittAutoSender
{
    // uniq ID start 50
    // pentru generare automata meniuri
    KITT_AUTO_SENDER_HORDE              = 50,
    KITT_AUTO_SENDER_ALLIANCE           = 51,
    KITT_AUTO_SENDER_E_KINGDOM          = 52,
    KITT_AUTO_SENDER_KALIMDOR           = 53,
    KITT_AUTO_SENDER_OUTLAND            = 54,
    KITT_AUTO_SENDER_NORTHREND          = 55,
    KITT_AUTO_SENDER_CLASSIC_DUNGEONS   = 56,
    KITT_AUTO_SENDER_BC_DUNGEONS        = 57,
    KITT_AUTO_SENDER_WRATH_DUNGEONS     = 58,
    KITT_AUTO_SENDER_RAID               = 59,
    KITT_AUTO_SENDER_FUN_ZONE           = 60,

    KITT_AUTO_SENDER_END_NO     = 150   // ultimul ID
};

enum KittAction
{

    // Action IDs
    // uniq ID start 100
//    KITT_ACTION_OPEN_SUBMENU            = 100,   // comun pentru sub-menu
    KITT_ACTION_MENU_HORDE              = 101,   // meniu Horde
    KITT_ACTION_MENU_ALLIANCE           = 102,   // meniu Alliance
    KITT_ACTION_MENU_E_KINGDOM          = 103,   // meniu Eastern Kingdoms
    KITT_ACTION_MENU_KALIMDOR           = 104,   // meniu Kalimdor
    KITT_ACTION_MENU_OUTLAND            = 105,   // meniu Outland
    KITT_ACTION_MENU_NORTHREND          = 106,   // meniu Northrend
    KITT_ACTION_MENU_CLASSIC_DUNGEONS   = 107,   // meniu Classic Dungeons
    KITT_ACTION_MENU_BC_DUNGEONS        = 108,   // meniu BC Dungeons
    KITT_ACTION_MENU_WRATH_DUNGEONS     = 109,   // meniu Wrath Dungeons
    KITT_ACTION_MENU_RAID               = 110,   // meniu Raid Teleports
    KITT_ACTION_MENU_FUN_ZONE           = 111,   // meniu Fun Zone (in constructie)
    KITT_ACTION_TELE_FUN_ZONE           = 112,   // teleport to Zona Fun (PVP Arena)
    KITT_ACTION_NU_APASA                = 113,   // action Nu Apasa!!! (1500 g)
    KITT_ACTION_MENU_INSTANCE_RESET     = 114,   // Instance reset cooldown
    KITT_ACTION_RESET_DUNGEON           = 115,   // Instance Reset Dungeons
    KITT_ACTION_RESET_RAID10            = 116,   // Instance Reset Raid 10
    KITT_ACTION_RESET_RAID25            = 117,   // Instance Reset Raid 25
    KITT_ACTION_RESET_RAID10H           = 118,   // Instance Reset Raid 10 Heroic
    KITT_ACTION_RESET_RAID25H           = 119,   // Instance Reset Raid 25 Heroic
    KITT_ACTION_TELEPORT_TO             = 120,   // meniu Teleport To
    KITT_ACTION_RESET_ALL_BUFF          = 121,   // Reset all buff & aura
    KITT_ACTION_RESET_ALL_CD_SPELL      = 122,   // Reset all cooldown spell
    KITT_ACTION_FLY_BABY_FLY            = 123,   // Poti sa zbori pe map 0 & 1




    KITT_ACTION_AH_OPEN                 = 294,   // AH open
    KITT_ACTION_BANK_OPEN               = 295,   // bank open
    KITT_ACTION_GV_OPEN                 = 296,   // GV bank
    KITT_ACTION_MAIL_OPEN               = 297,   // mail open
    KITT_ACTION_VENDOR_OPEN             = 298,   // vendor open
    //KITT_ACTION_TELE_ZONE               = 299,   // Teleport Zone
    KITT_ACTION_BACK_MAIN_MENU          = 300    // Inapoi la Meniul Principal

};

// Structura Meniu Principal
struct MainMenuOption {
    GossipOptionIcon icon;
    std::string name;
    uint32 sender;
    uint32 action;
};

struct MainMenuOptionConfirm {
    GossipOptionIcon icon;  // option icon
    std::string name;       // option name
    uint32 sender;          // sender id
    uint32 action;          // sender action
    std::string ctext;      // pop-up confirm text
    uint32 money;           // pop-up money
    bool confirm;           // activate code true  false
};

// Meniu Principal
// maxim 32 elemente per pagina cu tot cu Back
// pentru viteza adaugam "const" si "array" in loc de "vector"
// metoda veche: static std::vector ....
// nr de dupa "," reprezinta nr de meniuri
static const std::array<MainMenuOption, 9> KittMainMenu = { {
    { GOSSIP_ICON_CHAT, "Fun Zone (in constructie)", KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_MENU_FUN_ZONE },
    { GOSSIP_ICON_TALK, "Teleport to: Categorii",    KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_TELEPORT_TO },
    { GOSSIP_ICON_TALK, "Horde",                     KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_MENU_HORDE },
    { GOSSIP_ICON_TALK, "Alliance",                  KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_MENU_ALLIANCE },
    { GOSSIP_ICON_CHAT, "Deschide AH",               KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_AH_OPEN },
    { GOSSIP_ICON_CHAT, "Deschide Bank",             KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_BANK_OPEN },
    { GOSSIP_ICON_CHAT, "Adauga Guild Bank",         KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_GV_OPEN },
    { GOSSIP_ICON_CHAT, "Deschide Mail",             KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_MAIL_OPEN },
    { GOSSIP_ICON_CHAT, "Deschide Vendor",           KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_VENDOR_OPEN }
} };

static const std::array<MainMenuOption, 8> KittTeleportTo = { {
//    { GOSSIP_ICON_CHAT, "Teleport to: Custom Zone",  KITT_SENDER_MENU_DIRECT_SELECT, KITT_ACTION_TELE_ZONE },
    { GOSSIP_ICON_TALK, "Eastern Kingdoms",          KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_E_KINGDOM },
    { GOSSIP_ICON_TALK, "Kalimdor",                  KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_KALIMDOR },
    { GOSSIP_ICON_CHAT, "Outland",                   KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_OUTLAND },
    { GOSSIP_ICON_CHAT, "Northrend",                 KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_NORTHREND },
    { GOSSIP_ICON_CHAT, "Classic Dungeons",          KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_CLASSIC_DUNGEONS },
    { GOSSIP_ICON_CHAT, "BC Dungeons",               KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_BC_DUNGEONS },
    { GOSSIP_ICON_CHAT, "Wrath Dungeons",            KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_WRATH_DUNGEONS },
    { GOSSIP_ICON_CHAT, "Raid Teleports",            KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_RAID }
} };
// Meniu Fun Zone
static const std::array<MainMenuOptionConfirm, 6> KittFunZone = { {
    { GOSSIP_ICON_CHAT, "Fun Zone (Teleport)",              KITT_SENDER_MENU_FUN_ZONE,       KITT_ACTION_TELE_FUN_ZONE },
    { GOSSIP_ICON_CHAT, "Fly baby! Fly...",     KITT_SENDER_MENU_FUN_ZONE, KITT_ACTION_FLY_BABY_FLY },
    { GOSSIP_ICON_CHAT, "Nu Apasa!!! (" + sNuApasaPret + " g)",  KITT_SENDER_MENU_FUN_ZONE,       KITT_ACTION_NU_APASA, "Esti sigur?", NuApasaPret, true},
    { GOSSIP_ICON_CHAT, "Instance Reset CD",     KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_MENU_INSTANCE_RESET },
    { GOSSIP_ICON_CHAT, "Reset All Aura & Buff (" + sResetAllAura + " g)",  KITT_SENDER_MENU_FUN_ZONE,      KITT_ACTION_RESET_ALL_BUFF, "UnBuff all spell & aura", ResetAllAura, false},
    { GOSSIP_ICON_CHAT, "Reset All Spell cooldown (" + sResetAllSpellCd + " g)",  KITT_SENDER_MENU_FUN_ZONE,      KITT_ACTION_RESET_ALL_CD_SPELL, "Reset all cooldown", ResetAllSpellCd, false},
} };

// Meniu Instance Reset Cooldown cu confirmare.
// menu code. daca are TRUE optiunea finala se adauga la OnGossipSelectCode

static const std::array<MainMenuOptionConfirm, 6> KittInstanceReset = { {
    { GOSSIP_ICON_CHAT, "Reset Dungeon Heroic (" + sMoneyDungeons + " g)", KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_DUNGEON,  "Reset Dungeon Heroic",  MoneyDungeons, false},
    { GOSSIP_ICON_CHAT, "Reset Raid 10 Normal (" + sMoneyRaid10 + " g)",   KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_RAID10,   "Reset Raid 10 Normal",  MoneyRaid10,   false },
    { GOSSIP_ICON_CHAT, "Reset Raid 25 Normal (" + sMoneyRaid25 + " g)",   KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_RAID25,   "Reset Raid 25 Normal",  MoneyRaid25,   false },
    { GOSSIP_ICON_CHAT, "Reset Raid 10 Heroic (" + sMoneyRaid10H + " g)",  KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_RAID10H,  "Reset Raid 10 Heroic",  MoneyRaid10H,  false },
    { GOSSIP_ICON_CHAT, "Reset Raid 25 Heroic (" + sMoneyRaid25H + " g)",  KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_RAID25H,  "Reset Raid 25 Heroic",  MoneyRaid25H,  false },
    { GOSSIP_ICON_CHAT, "<<< Back <<<",      KITT_SENDER_OPEN_SUBMENU,        KITT_ACTION_MENU_FUN_ZONE,  "",  0,  false }
} };


// Structura teleport
struct TeleportLocation {
    uint32 map;         // map ID
    float x, y, z, o;   // teleport coordonate
    char const* name;   // Nume Meniu Teleport
};


// Tabel pentru Horde
static const std::array<TeleportLocation, 4> HordeLocs = { {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "Orgrimmar"},
    {0, 1572.00f, 213.637f, -43.1031f, 0.85f, "Undercity"},
    {1, -1386.75f, 138.474f, 23.0348f, 0.0f, "Thunder Bluff"},
    {530, 9380.18f, -7277.81f, 14.2404f, 0.0f, "Silvermoon"}
} };

// Tabel pentru Alliance
static const std::array<TeleportLocation, 4> AllianceLocs = { {
    {0, -8837.57f, 637.394f, 94.8055f, 4.96f, "Stormwind"},
    {1, 9946.02f, 2588.77f, 1316.19f, 0.0f, "Darnassus"},
    {0, -5002.33f, -857.664f, 497.054f, 0.0f, "Ironforge"},
    {530, -4004.39f, -11878.5f, -1.01131f, 0.0f, "Exodar"}
} };

// Tabel pentru Horde factiune Alianta
static const std::array<TeleportLocation, 4> HordeALocs = { {
    {1, 1107.23f, -4360.45f, 26.2786f, 6.056f, "Orgrimmar A"},
    {0, 1993.49f, 284.063f, 47.8923f, 3.673f, "Undercity A"},
    {1, -1457.53f, 73.2452f, 13.9765f, 0.580f, "Thunder Bluff A"},
    {530, 9269.27f, -7305.51f, 22.4224f, 0.426f, "Silvermoon A"}
} };

// Tabel pentru Alliance factiune Hoarda
static const std::array<TeleportLocation, 4> AllianceHLocs = { {
    {0, -9121.74f, 286.842f, 100.404f, 1.333f, "Stormwind H"},
    {1, 10023.5f, 1945.38f, 1331.05f, 2.527f, "Darnassus H"},
    {0, -5102.12f, -728.699f, 467.572f, 5.420f, "Ironforge H"},
    {530, -4059.18f, -12150.0f, -0.78815f, 1.672f, "Exodar H"}
} };


// Eastern Kingdoms
static const std::array<TeleportLocation, 24> EKingdomLocs = { {
    {0, -9449.06f, 64.8392f, 56.3581f, 3.07047f, "Elwynn Forest (lvl 1-14)"},
    {530, 9024.37f, -6682.55f, 16.8973f, 3.14131f, "Eversong Woods (lvl 1-14)"},
    {0, -5603.76f, -482.704f, 396.98f, 5.23499f, "Dun Morogh (lvl 1-14)"},
    {0, 2274.95f, 323.918f, 34.1137f, 4.24367f, "Tirisfal Glades (lvl 1-14)"},
    {530, 7595.73f, -6819.6f, 84.3718f, 2.56561f, "Ghostlands (lvl 8-24)"},
    {0, -5405.85f, -2894.15f, 341.972f, 5.48238f, "Loch modan (lvl 8-24)"},
    {0, 505.126f, 1504.63f, 124.808f, 1.77987f, "Silverpine Forest (lvl 8-24)"},
    {0, -10684.9f, 1033.63f, 32.5389f, 6.07384f, "Westfall (lvl 8-24)"},
    {0, -9447.8f, -2270.85f, 71.8224f, 0.283853f, "Redridge mountains (lvl 13-30)"},
    {0, -10531.7f, -1281.91f, 38.8647f, 1.56959f, "Duskwood (lvl 18-34)"},
    {0, -385.805f, -787.954f, 54.6655f, 1.03926f, "Hillsbrad Foothills (lvl 18-34)"},
    {0, -3517.75f, -913.401f, 8.86625f, 2.60705f, "Wetlands (lvl 18-34)"},
    {0, 275.049f, -652.044f, 130.296f, 0.502032f, "Alterac Mountains (lvl 28-44)"},
    {0, -1581.45f, -2704.06f, 35.4168f, 0.490373f, "Arathi Highlands (lvl 28-44)"},
    {0, -11921.7f, -59.544f, 39.7262f, 3.73574f, "Stranglethorn Vale (lvl 28-50)"},
    {0, -6782.56f, -3128.14f, 240.48f, 5.65912f, "Badlands (lvl 33-50)"},
    {0, -10368.6f, -2731.3f, 21.6537f, 5.29238f, "Swamp of Sorrows (lvl 33-50)"},
    {0, 112.406f, -3929.74f, 136.358f, 0.981903f, "The Hinterlands (lvl 38-54)"},
    {0, -6686.33f, -1198.55f, 240.027f, 0.916887f, "Searing Gorge (lvl 43-60)"},
    {0, -11184.7f, -3019.31f, 7.29238f, 3.20542f, "Blasted Lands (lvl 43-60)"},
    {0, -7979.78f, -2105.72f, 127.919f, 5.10148f, "Burning Steppes (lvl 48-60)"},
    {0, 1743.69f, -1723.86f, 59.6648f, 5.23722f, "Western Plaguelands (lvl 48-60)"},
    {0, 2280.64f, -5275.05f, 82.0166f, 4.7479f, "Eastern Plaguelands (lvl 53-63)"},
    {530, 12806.5f, -6911.11f, 41.1156f, 2.22935f, "Isle of Quel\'Danas (lvl 69-72)"}
} };

// Kalimdor
static const std::array<TeleportLocation, 19> KalimadorLocs = { {
    {530, -4192.62f, -12576.7f, 36.7598f, 1.62813f, "Azuremyst Isle (lvl 1-14)"},
    {1, 9889.03f, 915.869f, 1307.43f, 1.9336f, "Teldrassil (lvl 1-14)"},
    {1, 228.978f, -4741.87f, 10.1027f, 0.416883f, "Durotar (lvl 1-14)"},
    {1, -2473.87f, -501.225f, -9.42465f, 0.6525f, "Mulgore (lvl 1-14)"},
    {530, -2095.7f, -11841.1f, 51.1557f, 6.19288f, "Bloodmyst Isle (lvl 8-24)"},
    {1, 6463.25f, 683.986f, 8.92792f, 4.33534f, "Darkshore (lvl 8-24)"},
    {1, -575.772f, -2652.45f, 95.6384f, 0.006469f, "The Barrens (lvl 8-30)"},
    {1, 1574.89f, 1031.57f, 137.442f, 3.8013f, "Stonetalon Mountains (lvl 13-32)"},
    {1, 1919.77f, -2169.68f, 94.6729f, 6.14177f, "Ashenvale (lvl 18-34)"},
    {1, -5375.53f, -2509.2f, -40.432f, 2.41885f, "Thousand Needles (lvl 22-40)"},
    {1, -656.056f, 1510.12f, 88.3746f, 3.29553f, "Desolace (lvl 28-44)"},
    {1, -3350.12f, -3064.85f, 33.0364f, 5.12666f, "Dustwallow Marsh (lvl 33-50)"},
    {1, -4808.31f, 1040.51f, 103.769f, 2.90655f, "Feralas (lvl 38-54)"},
    {1, -6940.91f, -3725.7f, 48.9381f, 3.11174f, "Tanaris (lvl 38-54)"},
    {1, 3117.12f, -4387.97f, 91.9059f, 5.49897f, "Azshara (lvl 43-60)"},
    {1, 3898.8f, -1283.33f, 220.519f, 6.24307f, "Felwood (lvl 46-60)"},
    {1, -6291.55f, -1158.62f, -258.138f, 0.457099f, "Un\'Goro Crater (lvl 45-60)"},
    {1, -6815.25f, 730.015f, 40.9483f, 2.39066f, "Silithus (lvl 53-60)"},
    {1, 6658.57f, -4553.48f, 718.019f, 5.18088f, "Winterspring (lvl 53-63)"}
} };

// Outland
static const std::array<TeleportLocation, 8> OutlandLocs = { {
    {530, -1812.93f, 5452.93f, 2.68198f, 3.5534f, "Shattrath"},
    {530, -207.335f, 2035.92f, 96.464f, 1.59676f, "Hellfire Peninsula"},
    {530, -220.297f, 5378.58f, 23.3223f, 1.61718f, "Zangarmarsh"},
    {530, -2266.23f, 4244.73f, 1.47728f, 3.68426f, "Terokkar Forest"},
    {530, -1610.85f, 7733.62f, -17.2773f, 1.33522f, "Nagrand"},
    {530, 2029.75f, 6232.07f, 133.495f, 1.30395f, "Blade\'s Edge Mountains"},
    {530, 3271.2f, 3811.61f, 143.153f, 3.44101f, "Netherstorm"},
    {530, -3681.01f, 2350.76f, 76.587f, 4.25995f, "Shadowmoon Valley"}
} };

// Northrend
static const std::array<TeleportLocation, 11> NorthrendLocs = { {
    {571, 5804.15f, 624.771f, 647.767f, 0.0f, "Dalaran"},
    {571, 2954.24f, 5379.13f, 60.4538f, 2.55544f, "Borean Tundra"},
    {571, 682.848f, -3978.3f, 230.161f, 1.54207f, "Howling Fjord"},
    {571, 2678.17f, 891.826f, 4.37494f, 0.101121f, "Dragonblight"},
    {571, 4017.35f, -3404.22f, 289.925f, 5.35431f, "Grizzly Hills"},
    {571, 5560.23f, -3211.66f, 371.709f, 5.55055f, "Zul\'Drak"},
    {571, 5614.67f, 5818.86f, -69.722f, 3.60807f, "Sholazar Basin"},
    {571, 5411.17f, -966.37f, 167.082f, 1.57167f, "Crystalsong Forest"},
    {571, 6120.46f, -1013.89f, 408.39f, 5.12322f, "Storm Peaks"},
    {571, 8323.28f, 2763.5f, 655.093f, 2.87223f, "Icecrown"},
    {571, 4522.23f, 2828.01f, 389.975f, 0.215009f, "Wintergrasp"},
} };

// Classic Dungeons
static const std::array<TeleportLocation, 19> ClassicDungeonsLocs = { {
    {0, -5163.54f, 925.423f, 257.181f, 1.57423f, "Gnomeregan (lvl 25-28)"},
    {0, -11209.6f, 1666.54f, 24.6974f, 1.42053f, "The Deadmines (lvl 17-20)"},
    {0, -8799.15f, 832.718f, 97.6348f, 6.04085f, "The Stockade (lvl 22-25)"},
    {1, 1811.78f, -4410.5f, -18.4704f, 5.20165f, "Ragefire Chasm (lvl 15-16)"},
    {1, -4657.3f, -2519.35f, 81.0529f, 4.54808f, "Razorfen Downs (lvl 34-37)"},
    {1, -4470.28f, -1677.77f, 81.3925f, 1.16302f, "Razorfen Kraul (lvl 24-27)"},
    {0, 2873.15f, -764.523f, 160.332f, 5.10447f, "Scarlet Monastery (lvl 29-40)"},
    {0, -234.675f, 1561.63f, 76.8921f, 1.24031f, "Shadowfang Keep (lvl 18-21)"},
    {1, -731.607f, -2218.39f, 17.0281f, 2.78486f, "Wailing Caverns (lvl 17-20)"},
    {1, 4249.99f, 740.102f, -25.671f, 1.34062f, "Blackfathom Deeps (lvl 21-24)"},
    {0, -7179.34f, -921.212f, 165.821f, 5.09599f, "Blackrock Depths (lvl 49-56)"},
    {0, -7527.05f, -1226.77f, 285.732f, 5.29626f, "Blackrock Spire (lvl 57-63)"},
    {1, -3520.14f, 1119.38f, 161.025f, 4.70454f, "Dire Maul (lvl 55-60)"},
    {1, -1421.42f, 2907.83f, 137.415f, 1.70718f, "Maraudon (lvl 41-48)"},
    {0, 1269.64f, -2556.21f, 93.6088f, 0.620623f, "Scholomance (lvl 55-60)"},
    {0, 3352.92f, -3379.03f, 144.782f, 6.25978f, "Stratholme (lvl 55-60)"},
    {0, -10177.9f, -3994.9f, -111.239f, 6.01885f, "Sunken Temple (lvl 47-50)"},
    {0, -6071.37f, -2955.16f, 209.782f, 0.015708f, "Uldaman (lvl 37-40)"},
    {1, -6801.19f, -2893.02f, 9.00388f, 0.158639f, "Zul\'Farrak (lvl 43-46)"},
} };

// BC Dungeons
static const std::array<TeleportLocation, 6> BcDungeonsLocs = { {
    {530, -3324.49f, 4943.45f, -101.239f, 4.63901f, "Auchindoun"},
    {1, -8369.65f, -4253.11f, -204.272f, -2.70526f, "The Caverns of Time"},
    {530, 738.865f, 6865.77f, -69.4659f, 6.27655f, "Coilfang Reservoir"},
    {530, -347.29f, 3089.82f, 21.394f, 5.68114f, "Hellfire Citadel"},
    {530, 12884.6f, -7317.69f, 65.5023f, 4.799f, "Magisters\' Terrace"},
    {530, 3100.48f, 1536.49f, 190.3f, 4.62226f, "Tempest Keep"},
} };

// Wrath Dungeons
static const std::array<TeleportLocation, 12> WrathDungeonsLocs = { {
    {571, 3707.86f, 2150.23f, 36.76f, 3.22f, "Azjol-Nerub"},
    {1, -8756.39f, -4440.68f, -199.489f, 4.66289f, "The Culling of Stratholme"},
    {571, 8590.95f, 791.792f, 558.235f, 3.13127f, "Trial of the Champion"},
    {571, 4765.59f, -2038.24f, 229.363f, 0.887627f, "Drak\'Tharon Keep"},
    {571, 6722.44f, -4640.67f, 450.632f, 3.91123f, "Gundrak"},
    {571, 5643.16f, 2028.81f, 798.274f, 4.60242f, "Icecrown Citadel Dungeons"},
    {571, 3782.89f, 6965.23f, 105.088f, 6.14194f, "The Nexus Dungeons"},
    {571, 5693.08f, 502.588f, 652.672f, 4.0229f, "The Violet Hold"},
    {571, 9136.52f, -1311.81f, 1066.29f, 5.19113f, "Halls of Lightning"},
    {571, 8922.12f, -1009.16f, 1039.56f, 1.57044f, "Halls of Stone"},
    {571, 1203.41f, -4868.59f, 41.2486f, 0.283237f, "Utgarde Keep"},
    {571, 1267.24f, -4857.3f, 215.764f, 3.22768f, "Utgarde Pinnacle"},
} };

// Raid Teleports
static const std::array<TeleportLocation, 22> RaidLocs = { {
    {530, -3649.92f, 317.469f, 35.2827f, 2.94285f, "Black Temple (lvl 70+)"},
    {229, 152.451f, -474.881f, 116.84f, 0.001073f, "Blackwing Lair (req. Raid lvl 60+)"},
    {1, -8177.89f, -4181.23f, -167.552f, 0.913338f, "Hyjal Summit (lvl 70+)"},
    {530, 797.855f, 6865.77f, -65.4165f, 0.005938f, "Serpentshrine Cavern (lvl 68+)"},
    {571, 8515.61f, 714.153f, 558.248f, 1.57753f, "Trial of the Crusader (lvl 80)"},
    {530, 3530.06f, 5104.08f, 3.50861f, 5.51117f, "Gruul\'s Lair (lvl 70+)"},
    {530, -336.411f, 3130.46f, -102.928f, 5.20322f, "Magtheridon\'s Lair (lvl 65+)"},
    {571, 5855.22f, 2102.03f, 635.991f, 3.57899f, "Icecrown Citadel (lvl 80)"},
    {0, -11118.9f, -2010.33f, 47.0819f, 0.649895f, "Karazhan (lvl 68+)"},
    {230, 1126.64f, -459.94f, -102.535f, 3.46095f, "Molten Core (req. Raid lvl 50+)"},
    {571, 3668.72f, -1262.46f, 243.622f, 4.785f, "Naxxramas (lvl 80+)"},
    {1, -4708.27f, -3727.64f, 54.5589f, 3.72786f, "Onyxia\'s Lair')"},
    {1, -8409.82f, 1499.06f, 27.7179f, 2.51868f, "Ruins of Ahn\'Qiraj (lvl 50+)"},
    {530, 12574.1f, -6774.81f, 15.0904f, 3.13788f, "Sunwell Plateau (lvl 70+)"},
    {530, 3088.49f, 1381.57f, 184.863f, 4.61973f, "The Eye"},
    {1, -8240.09f, 1991.32f, 129.072f, 0.941603f, "Temple of Ahn\'Qiraj"},
    {571, 3784.17f, 7028.84f, 161.258f, 5.79993f, "The Eye of Eternity (lvl 80)"},
    {571, 3472.43f, 264.923f, -120.146f, 3.27923f, "The Obsidian Sanctum (lvl 80)"},
    {571, 9222.88f, -1113.59f, 1216.12f, 6.27549f, "Ulduar (lvl 80)"},
    {571, 5453.72f, 2840.79f, 421.28f, 0.0f, "Vault of Archavon (lvl 80)"},
    {0, -11916.7f, -1215.72f, 92.289f, 4.72454f, "Zul\'Gurub (lvl 50)"},
    {530, 6851.78f, -7972.57f, 179.242f, 4.64691f, "Zul\'Aman (lvl 70)"},
} };



// Tabel pentru Fun Zone
static const std::array<TeleportLocation, 1> FunZoneLocs = { {
    {0, -4854.04f, -1004.33f, 453.76f, 0.0f, "Custom Zone"}
} };


enum Spells
{
    SPELL_DEATH = 5 //27255
};

enum KittWinterTransform
{
    SPELL_KITT_WINTER_WONDERVOLT_TRANSFORM_1 = 26157,
    SPELL_KITT_WINTER_WONDERVOLT_TRANSFORM_2 = 26272,
    SPELL_KITT_WINTER_WONDERVOLT_TRANSFORM_3 = 26273,
    SPELL_KITT_WINTER_WONDERVOLT_TRANSFORM_4 = 26274
};

std::array<uint32, 4> const KittWinterTransformSpells =
{
    SPELL_KITT_WINTER_WONDERVOLT_TRANSFORM_1,
    SPELL_KITT_WINTER_WONDERVOLT_TRANSFORM_2,
    SPELL_KITT_WINTER_WONDERVOLT_TRANSFORM_3,
    SPELL_KITT_WINTER_WONDERVOLT_TRANSFORM_4
};


static void BeginDelayedTeleportOFF1(Player* player, TeleportLocation const& loc)
{
    //if (!player || player->HasAura(36901)) // Folosim 36901 doar ca block intern rapid
    //    return;

    player->PlayerTalkClass->SendCloseGossip();

    // Model 169
    // creature_template set faction 32, flags_extra=128 (trigger)
    player->CastSpell(player, 45451, false);   // hearthstone cosmetic
    TempSummon* visualTrigger = player->SummonCreature(90015, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 5500ms);
    if (visualTrigger)
    {
        visualTrigger->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1 | UNIT_FLAG_IMMUNE | UNIT_FLAG_PACIFIED);

        visualTrigger->AddAura(42050, visualTrigger);
        visualTrigger->AddAura(48387, visualTrigger);
        visualTrigger->AddAura(50771, visualTrigger);

    }

    //ObjectGuid playerGuid = player->GetGUID();
    //ObjectGuid triggerGuid = visualTrigger ? visualTrigger->GetGUID() : ObjectGuid::Empty;

    player->m_Events.AddEventAtOffset([player, loc, visualTrigger]()
    {
            //Player* player = ObjectAccessor::FindPlayer(playerGuid);
            if (player && player->IsInWorld())
            {
                //player->RemoveAura(36901);
                //if (visualTrigger)
                //    visualTrigger->DespawnOrUnsummon(); // Curatam trigger-ul
                if (!player->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                {
                    if (visualTrigger)
                        visualTrigger->DespawnOrUnsummon(); // Stergem daca a anulat
                    return; // Iesim din functie, deci NU mai are loc teleportarea
                }

                if (player->IsAlive())
                {
                    player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                    // verificam aura
                    bool hasAura = false;
                    for (uint32 kittspell : KittWinterTransformSpells)
                        if (player->HasAura(kittspell))
                            hasAura = true;

                    if (!hasAura)
                    {
                        uint32 randomSpell = Trinity::Containers::SelectRandomContainerElement(KittWinterTransformSpells);
                        player->CastSpell(player, randomSpell, true);
                    }
                }
            }
            return;
    }, 4410ms);  // teleport delay
}


class kitt_npc_menu : public CreatureScript
{
public:
    kitt_npc_menu() : CreatureScript("kitt_npc_menu") {}

    struct kitt_npc_menuAI : public ScriptedAI
    {
        kitt_npc_menuAI(Creature* creature) : ScriptedAI(creature) {}

        uint32 m_uiSpell1Timer = 2000;
        uint32 m_uiSpell2Timer = 3000;
        uint32 m_uiSpell3Timer = 4000;
        uint32 m_uiSpell4Timer = 5000;
        uint32 m_uiSpell5Timer = 6000;
        uint32 m_uiSpell6Timer = 7000;
        uint32 m_uiSpell7Timer = 8000;
        uint32 m_uiSpell8Timer = 9000;
        uint32 m_uiSpell9Timer = 10000;
        uint32 m_uiSpell10Timer = 11000;
        uint32 m_uiSpell11Timer = 12000;
        uint32 m_uiSpell_Death_Timer = 14000;

        void Reset() override
        {
            me->SetMaxHealth(551000);
            me->SetHealth(550500);
            me->RemoveFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);
            me->SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_ALLOW_CHEAT_SPELLS);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_MAILBOX);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_AUCTIONEER);
            me->SetReactState(REACT_PASSIVE);
            me->SetFaction(35);

            m_uiSpell1Timer = 2000;
            m_uiSpell2Timer = 3000;
            m_uiSpell3Timer = 4000;
            m_uiSpell4Timer = 5000;
            m_uiSpell5Timer = 6000;
            m_uiSpell6Timer = 7000;
            m_uiSpell7Timer = 8000;
            m_uiSpell8Timer = 9000;
            m_uiSpell9Timer = 10000;
            m_uiSpell10Timer = 11000;
            m_uiSpell11Timer = 12000;
            m_uiSpell_Death_Timer = 14000;

        }

        void JustDied(Unit* /*killer*/) override
        {
            me->Yell("bbbhhhrrr brrr hrrrr a a Aaa-a-a", LANG_UNIVERSAL);

            int32 totalAmount = 20000000; // 2000 Aur
            float distantaParticipare = 100.0f;
            std::set<Player*> jucatoriDePremiat;

            // METODA SUPREMA: Verificam cine are dreptul de loot (Tap List)
            // GetLootRecipient() returneaza Jucatorul care a "tapat" NPC-ul (sau liderul grupului)
            Player* recipient = me->GetLootRecipient();

            if (recipient)
            {
                if (Group* group = recipient->GetGroup())
                {
                    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                    {
                        Player* member = itr->GetSource();
                        // Verificam sa fie OM (GetSession) si sa fie in zona
                        if (member && member->GetSession() && member->IsWithinDistInMap(me, distantaParticipare))
                            jucatoriDePremiat.insert(member);
                    }
                }
                else if (recipient->GetSession()) // Daca e singur si e om
                {
                    jucatoriDePremiat.insert(recipient);
                }
            }

            // FALLBACK: Daca recipientul e NULL (cazuri rare), scanam zona pentru oameni in combat
            if (jucatoriDePremiat.empty())
            {
                Map::PlayerList const& players = me->GetMap()->GetPlayers();
                for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                {
                    Player* p = itr->GetSource();
                    if (p && p->GetSession() && p->IsWithinDistInMap(me, distantaParticipare) && p->IsInCombat())
                        jucatoriDePremiat.insert(p);
                }
            }

            // Distribuirea banilor
            if (!jucatoriDePremiat.empty())
            {
                int32 share = totalAmount / (int32)jucatoriDePremiat.size();
                for (Player* player : jucatoriDePremiat)
                {
                    player->ModifyMoney(share);
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[Recompensa]|r Ai primit |cffffd700%u|r aur.", share / 10000);
                }
            }

/*            // Aici adaugi logica ta de dupa moarte.

            // Exemplu 1: Trimite un mesaj catre jucatorul care l-a omorat
            if (killer->GetTypeId() == TYPEID_PLAYER)
            {
                me->Yell("bbbhhhrrr brrr  hrrrr a a Aaa-a-a", LANG_UNIVERSAL);

                Player* playerKiller = killer->ToPlayer();

                if (playerKiller)
                {
                    // Suma in cupru: 20 aur * 100 argint * 100 cupru = 200000 cupru
                    int32 amount = 20000000;

                    // Adauga banii in inventarul jucatorului
                    playerKiller->ModifyMoney(amount);

                    // Optional: Trimite un mesaj de notificare jucatorului
                    // Mesajul va aparea in chatul general/system
                    std::string message = "Ai primit 2000 de aur de la NPC!";

                    // Foloseste ChatHandler::PSendSysMessage pentru simplitate si compatibilitate
                    // Aceasta functie stie sa formateze pachetul corect.
                    // Primul parametru (ses) este sesiunea jucatorului
                    ChatHandler(playerKiller->GetSession()).PSendSysMessage("%s", message.c_str());
                }
            }
            // Exemplu 2: Face spawn la un alt NPC sau obiect
            // me->SummonCreature(ENTRY_ID_ALT_NPC, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 30000);

            // Exemplu 3: Activeaza un flag de quest (daca ai un sistem de questuri custom)
            // Daca ai nevoie de logica complexa de quest, probabil folosesti deja SmartAI Event 11 (SMART_EVENT_ON_DEATH)

            // Exemplu 4: Afiseaza un text pe ecranul tuturor jucatorilor din zona
            // me->MonsterYell("Am fost invins!", LANG_UNIVERSAL, 0);*/
        }

        void KilledUnit(Unit* /*victim*/) override
        {
            if (me->IsAlive())
            {
                Player* nearestPlayer = me->SelectNearestPlayer(50.0f);

                if (nearestPlayer)
                {
                    AttackStart(nearestPlayer);
                    me->Yell(/*me->GetVictim()->GetName() + */ " Vin dupa tine!", LANG_UNIVERSAL, 0);
                }
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            if (!UpdateVictim())
                return;

            //Spell 1 time
            if (m_uiSpell1Timer <= uiDiff)
            {
                me->Yell("10", LANG_UNIVERSAL, 0);
                m_uiSpell1Timer = 3600000;
            }
            else
                m_uiSpell1Timer -= uiDiff;

            //Spell 2 time
            if (m_uiSpell2Timer <= uiDiff)
            {
                me->Yell("9", LANG_UNIVERSAL, 0);
                m_uiSpell2Timer = 3600000;
            }
            else
                m_uiSpell2Timer -= uiDiff;

            //Spell 3 time
            if (m_uiSpell3Timer <= uiDiff)
            {
                me->Yell("8", LANG_UNIVERSAL, 0);
                m_uiSpell3Timer = 3600000;
            }
            else
                m_uiSpell3Timer -= uiDiff;

            //Spell 4 time
            if (m_uiSpell4Timer <= uiDiff)
            {
                me->Yell("7", LANG_UNIVERSAL, 0);
                m_uiSpell4Timer = 3600000;
            }
            else
                m_uiSpell4Timer -= uiDiff;

            //Spell 5 time
            if (m_uiSpell5Timer <= uiDiff)
            {
                me->Yell("6", LANG_UNIVERSAL, 0);
                m_uiSpell5Timer = 3600000;
            }
            else
                m_uiSpell5Timer -= uiDiff;

            //Spell 6 time
            if (m_uiSpell6Timer <= uiDiff)
            {
                me->Yell("5", LANG_UNIVERSAL, 0);
                m_uiSpell6Timer = 3600000;
            }
            else
                m_uiSpell6Timer -= uiDiff;

            //Spell 7 time
            if (m_uiSpell7Timer <= uiDiff)
            {
                me->Yell("4", LANG_UNIVERSAL, 0);
                m_uiSpell7Timer = 3600000;
            }
            else
                m_uiSpell7Timer -= uiDiff;

            //Spell 8 time
            if (m_uiSpell8Timer <= uiDiff)
            {
                me->Yell("3", LANG_UNIVERSAL, 0);
                m_uiSpell8Timer = 3600000;
            }
            else
                m_uiSpell8Timer -= uiDiff;

            //Spell 9 time
            if (m_uiSpell9Timer <= uiDiff)
            {
                me->Yell("2", LANG_UNIVERSAL, 0);
                m_uiSpell9Timer = 3600000;
            }
            else
                m_uiSpell9Timer -= uiDiff;

            //Spell 10 time
            if (m_uiSpell10Timer <= uiDiff)
            {
                me->Yell("1", LANG_UNIVERSAL, 0);
                m_uiSpell10Timer = 3600000;
            }
            else
                m_uiSpell10Timer -= uiDiff;

            //Spell 11 time
            if (m_uiSpell11Timer <= uiDiff)
            {
                me->Yell("Cine nu-i gata il iau cu lopata! xa xa xa", LANG_UNIVERSAL, 0);
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                me->RemoveFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);
                me->SetReactState(REACT_AGGRESSIVE);
                me->GetMotionMaster()->MoveChase(me->GetVictim());
                m_uiSpell11Timer = 3600000;
            }
            else
                m_uiSpell11Timer -= uiDiff;

            //Spell Death
            if (m_uiSpell_Death_Timer <= uiDiff)
            {
                DoCast(me->GetVictim(), SPELL_DEATH);
                me->Say("Voiai sa fugi de mine?!", LANG_UNIVERSAL, 0);
                m_uiSpell_Death_Timer = 6000;
            }
            else
                m_uiSpell_Death_Timer -= uiDiff;

            DoMeleeAttackIfReady();
        }

        bool OnGossipHello(Player* player) override
        {
            player->PlayerTalkClass->ClearMenus();

            if (player->IsInCombat())
            {
                //std::string message = "|cffff0000[Eroare]:|r Nu poti folosi meniul in timpul luptei!";
                //ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                //"Actiune imposibila in timpul luptei!"
                //"Esti prea ocupat cu batalia! Termin-o inainte de a folosi meniul."
                player->PlayerTalkClass->SendCloseGossip();
                player->GetSession()->SendNotification("Nu poti folosi meniul in timpul luptei!");
                //ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Eroare]:|r Nu poti folosi meniul in timpul luptei!");
                return true;
            }
            else
            {
                for (auto const& option : KittMainMenu)
                {
                    AddGossipItemFor(player, option.icon, option.name, option.sender, option.action);
                }

                SendGossipMenuFor(player, KittNpcText::KITT_NPC_HELLO, me->GetGUID());
                return true;
            }
            return true;
        }

        bool OnGossipSelectCode(Player* player, uint32 /*menuId*/, uint32 gossipListId, char const* code) override
        {
            // aici se adauga optiunea doar daca are TRUE la meniu. se adauga doar actiunea finala.
            KittResetCode = sConfigMgr->GetStringDefault("Kitt.Reset.Code", "test");
            KittNuApasaCode = sConfigMgr->GetStringDefault("Kitt.Fun.NuApasa.Code", "test");
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            uint32 const sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
            std::string codeStr = code; // Transformam codul primit in string pentru verificare

            if (sender == KITT_SENDER_MENU_INSTANCE_RESET && action == KITT_ACTION_RESET_RAID10)
            {
                    // difficulty raid 0 = 10N  1 = 25N  2 = 10H  3 = 25H  dungeons 1 = 5hc
                    QueryResult result = CharacterDatabase.PQuery("SELECT `guid` FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 0)", player->GetGUID().GetCounter());

                    // Verificam daca jucatorul a scris exact cuvantul corect
                    if (codeStr != KittResetCode)
                    {
                        ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000Eroare:|r Cod de confirmare incorect. Trebuie sa scrii ....");
                        CloseGossipMenuFor(player);
                        return true;
                    }

                    if (!result/* || !result->GetRowCount() == 0 */)
                    {
                        /*                        me->Whisper("Nu ai nicio instanta (10-N) blocata (cu cooldown) pe care sa o resetezi.", LANG_UNIVERSAL, player); */
                        std::string message = "|cffff0000!...|r Nu ai nicio instanta (10-N) blocata (cu cooldown) pe care sa o resetezi.";
                        ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        CloseGossipMenuFor(player);
                        return true;
                    }

                    if (!player->HasEnoughMoney(MoneyRaid10))
                    {
                        player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                        me->Whisper(player->GetName() + ", ai nevoie de 100g pentru a reseta aceste instante (10-N)!", LANG_UNIVERSAL, player);
                        CloseGossipMenuFor(player);
                        return true;
                    }

                    else
                    {
                        player->ModifyMoney(-int32(MoneyRaid10));
                        // 10 N
                        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 0)", player->GetGUID().GetCounter());
                        CloseGossipMenuFor(player);
                        /*                            me->Whisper("Instance reset: 10-N (Necesita re-log), leave party/raid and re-log.", LANG_UNIVERSAL, player); */
                        std::string message = "|cffff0000Atentie|r Instance reset: 10-N (Necesita re-log), leave party/raid and re-log.";
                        ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                    }
                    return true;
            }

            if (sender == KITT_SENDER_MENU_FUN_ZONE && action == KITT_ACTION_NU_APASA)
            {
                if (codeStr != KittNuApasaCode)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000Eroare:|r Cod incorect. Scrie |cff00ff00%s|r si apasa butonul |cff00ff00[Accept]|r cu mouse-ul.", KittNuApasaCode);
                    CloseGossipMenuFor(player);
                    return true;
                }

                if (!player->HasEnoughMoney(NuApasaPret))
                {
                    player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                    me->Say(player->GetName() + ", ai nevoie de 1500g.", LANG_UNIVERSAL, 0);
                    CloseGossipMenuFor(player);
                }
                else
                {
                    player->ModifyMoney(-int32(NuApasaPret));
                    CloseGossipMenuFor(player);

                    me->Say("Ai dat de naiba!!! Iti dau 10 secunde sa fugi.", LANG_UNIVERSAL, 0);
                    me->SetFaction(14);
                    me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                    me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                    me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR);
                    me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR);
                    me->SetReactState(REACT_PASSIVE);
                    me->GetMotionMaster()->MoveIdle();
                    me->GetVictim();
                    me->SetInCombatWith(me->SelectNearestPlayer(5.0f));
                    me->Attack(player, 0);
                }
                return true;
                //break;
            }


            CloseGossipMenuFor(player);
            return false;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            uint32 const sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);

            if (player->IsInCombat())
            {
                player->PlayerTalkClass->SendCloseGossip();
                player->GetSession()->SendNotification("Nu poti folosi meniul in timpul luptei!");
                return true;
            }

            switch (sender)
            {
                // 2. Logica pentru deschiderea sub-meniurilor Principale
                case KITT_SENDER_OPEN_SUBMENU:
                {
                    player->PlayerTalkClass->ClearMenus();
                    switch (action)
                    {

                        case KITT_ACTION_TELEPORT_TO:
                        {
                            for (auto const& option : KittTeleportTo)
                            {
                                AddGossipItemFor(player, option.icon, option.name, option.sender, option.action);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_HORDE:
                        {
                            if (player->GetTeam() == HORDE)
                            {
                                for (size_t i = 0; i < HordeLocs.size(); ++i)
                                {
                                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, HordeLocs[i].name, KITT_AUTO_SENDER_HORDE, i);
                                }
                            }
                            else
                            {
                                for (size_t i = 0; i < HordeALocs.size(); ++i)
                                {
                                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, HordeALocs[i].name, KITT_AUTO_SENDER_HORDE, i);
                                }
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_ALLIANCE:
                        {
                            if (player->GetTeam() == ALLIANCE)
                            {
                                for (size_t i = 0; i < AllianceLocs.size(); ++i)
                                {
                                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, AllianceLocs[i].name, KITT_AUTO_SENDER_ALLIANCE, i);
                                }
                            }
                            else
                            {
                                for (size_t i = 0; i < AllianceHLocs.size(); ++i)
                                {
                                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, AllianceHLocs[i].name, KITT_AUTO_SENDER_ALLIANCE, i);
                                }
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_FUN_ZONE:
                        {
                            // daca jucatorul este pe harta 0, 1, 530, 571 va aparea si menu 
                            if (player->GetMapId() == 0 || player->GetMapId() == 1 || player->GetMapId() == 530 || player->GetMapId() == 571)
                            {
                                for (auto const& option : KittFunZone)
                                {
                                    AddGossipItemFor(player, option.icon, option.name, option.sender, option.action, option.ctext, option.money, option.confirm);
                                }
                            }
                            else
                            {
                                //AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<< Back", KITT_SENDER_MAIN_MENU_SELECT, KITT_ACTION_BACK_MAIN_MENU);
                                std::string message = "|cffff0000!...|r Meniul nu functioneaza in instanta.";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }
                            break;
                        }

                        case KITT_ACTION_AH_OPEN:
                        {

                            uint32 oldFaction = me->GetFaction();

                            // team_alliance = 0 / team_horde = 1
                            if (player->GetTeam() == ALLIANCE)
                            {
                                // Setam temporar factiunea la 12 (Human/Alliance) sau 29 (Orc/Horde)
                                me->SetFaction(12);
                            }
                            else
                            {
                                me->SetFaction(29);
                            }
                            player->GetSession()->SendAuctionHello(me->GetGUID(), me);

                            // Resetam factiunea la cea originala imediat
                            me->SetFaction(oldFaction);

                            CloseGossipMenuFor(player);
                            return true;
                        }

                        case KITT_ACTION_BANK_OPEN:
                        {
                            player->GetSession()->SendShowBank(me->GetGUID());
                            CloseGossipMenuFor(player);
                            return true;
                        }

                        case KITT_ACTION_GV_OPEN:
                        {
                            uint32 KittGVObj = 187299;    // Guild Value object
                            Seconds despawnTime = Seconds(120);
                            // X = +fata/-spate , Y = +stanga/-dreapta , Z = +sus/-jos , 0.0 = orientarea
                            Position Kittoffset(-1.0f, 2.0f, 0.0f, 0.0f);
                            Position KittspawnPos = me->GetPositionWithOffset(Kittoffset);
                            // verifica ca pozitiile sa fie pe sol dupa map si vmap (anuleaza pozitia Z personalizata)
                            me->UpdateAllowedPositionZ(KittspawnPos.GetPositionX(), KittspawnPos.GetPositionY(), KittspawnPos.m_positionZ);

                            QuaternionData KittmyRotation = QuaternionData();
                            float distance = 50.0f;  // distanta de a verifica daca exista deja
                            if (player->FindNearestGameObject(KittGVObj, distance, true))
                            {
                                player->GetSession()->SendNotification("Exista deja o banca activa in apropiere (50yd)!");
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            if (me->SummonGameObject(KittGVObj, KittspawnPos.GetPositionX(), KittspawnPos.GetPositionY(), KittspawnPos.GetPositionZ(), me->GetOrientation(), KittmyRotation, despawnTime, GO_SUMMON_TIMED_DESPAWN))
                            {
                                player->GetSession()->SendNotification("Guild Bank invocat pentru 2 minute!");
                                CloseGossipMenuFor(player);
                                return true;
                            }

                            CloseGossipMenuFor(player);
                            return true;
                        }


                        case KITT_ACTION_MAIL_OPEN:
                        {
                            player->GetSession()->SendShowMailBox(me->GetGUID());

                            // Nu inchide meniul de gossip aici, deoarece interfata de mail va aparea deasupra
                            // CloseGossipMenuFor(player);
                            return true;
                        }

                        case KITT_ACTION_VENDOR_OPEN:
                        {
                            player->GetSession()->SendListInventory(me->GetGUID());
                            CloseGossipMenuFor(player);
                            return true;
                        }

                        default:
                            break;
                    }
                    // Adaugam butonul de inapoi care sa trimita spre Main Menu
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<< Back", KITT_SENDER_MENU_DIRECT_SELECT, KITT_ACTION_BACK_MAIN_MENU);
                    SendGossipMenuFor(player, KittNpcText::KITT_NPC_HELLO, me->GetGUID());
                    return true; // Returnam true pentru a confirma afisarea meniului
                }

                case KITT_SENDER_MENU_DIRECT_SELECT:
                {
                    player->PlayerTalkClass->ClearMenus();
                    switch (action)
                    {
                        case KITT_ACTION_BACK_MAIN_MENU:
                        {
                            return OnGossipHello(player);
                            return true; // scriptul se opreste aici. cu breack sau false scriptul continua
                        }

                        /*case KITT_ACTION_TELE_ZONE:
                        {
                            player->TeleportTo(0, -4854.04f, -1004.33f, 453.76f, 0.0f);
                            CloseGossipMenuFor(player);
                            return true;
                        }*/

                        default:
                            break;
                    }
                    // adaugam optiuni finale
                    return true; // aici se opreste script
                }

                // 2.2 Logica pentru deschiderea sub-meniurilor Principale
                case KITT_SENDER_TELEPORT_TO:
                {
                    player->PlayerTalkClass->ClearMenus();

                    switch (action)
                    {

                        case KITT_ACTION_MENU_E_KINGDOM:
                        {
                            for (size_t i = 0; i < EKingdomLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, EKingdomLocs[i].name, KITT_AUTO_SENDER_E_KINGDOM, i);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_KALIMDOR:
                        {
                            for (size_t i = 0; i < KalimadorLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, KalimadorLocs[i].name, KITT_AUTO_SENDER_KALIMDOR, i);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_OUTLAND:
                        {
                            for (size_t i = 0; i < OutlandLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, OutlandLocs[i].name, KITT_AUTO_SENDER_OUTLAND, i);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_NORTHREND:
                        {
                            for (size_t i = 0; i < NorthrendLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, NorthrendLocs[i].name, KITT_AUTO_SENDER_NORTHREND, i);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_CLASSIC_DUNGEONS:
                        {
                            for (size_t i = 0; i < ClassicDungeonsLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, ClassicDungeonsLocs[i].name, KITT_AUTO_SENDER_CLASSIC_DUNGEONS, i);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_BC_DUNGEONS:
                        {
                            for (size_t i = 0; i < BcDungeonsLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, BcDungeonsLocs[i].name, KITT_AUTO_SENDER_BC_DUNGEONS, i);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_WRATH_DUNGEONS:
                        {
                            for (size_t i = 0; i < WrathDungeonsLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, WrathDungeonsLocs[i].name, KITT_AUTO_SENDER_WRATH_DUNGEONS, i);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_RAID:
                        {
                            for (size_t i = 0; i < RaidLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, RaidLocs[i].name, KITT_AUTO_SENDER_RAID, i);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<<< Back", KITT_SENDER_OPEN_SUBMENU, KITT_ACTION_TELEPORT_TO);
                    SendGossipMenuFor(player, KittNpcText::KITT_NPC_HELLO, me->GetGUID());
                    return true; // Returnam true pentru a confirma afisarea meniului
                }

                // instance reset menu
                case KITT_SENDER_MENU_INSTANCE_RESET:
                {
                    player->PlayerTalkClass->ClearMenus();

                    switch (action)
                    {
                        case KITT_ACTION_MENU_INSTANCE_RESET:
                        {

                            for (auto const& option : KittInstanceReset)
                            {
                                AddGossipItemFor(player, option.icon, option.name, option.sender, option.action, option.ctext, option.money, option.confirm);
                            }
                            break;
                        }
                        // Actiune directa din Instance Reset
/*                        case KITT_ACTION_RESET_DUNGEONS:
                             // Metoda Veche de resetare
                        {
                            // difficulty raid 0 = 10N  1 = 25N  2 = 10H  3 = 25H  dungeons 1 = 5hc
                            QueryResult result = CharacterDatabase.PQuery("SELECT `guid` FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 1)", player->GetGUID().GetCounter());

                            if (!result)
                            {
                                std::string message = "|cffff0000!...|r Nu ai nicio instanta (Dungeons Heroic) blocata (cu cooldown) pe care sa o resetezi.";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            if (!player->HasEnoughMoney(MoneyDungeons))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                                me->Whisper(player->GetName() + ", ai nevoie de 30g pentru a reseta aceste instante (Dungeons Heroic)!", LANG_UNIVERSAL, player);
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else
                            {
                                player->ModifyMoney(-int32(MoneyDungeons));
                                // 10 N
                                CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 1)", player->GetGUID().GetCounter());
                                CloseGossipMenuFor(player);
                                std::string message = "|cffff0000ATENTIE|r Instance reset: Dungeons Heroic (Necesita re-log), leave party/raid and re-log.";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }
                            return true;
                        }*/
                        case KITT_ACTION_RESET_RAID10:
                        {
                            Difficulty diff = RAID_DIFFICULTY_10MAN_NORMAL;
                            bool instanceReset = false;

                            Player::BoundInstancesMap& binds = player->GetBoundInstances(diff);

                            if (binds.empty())
                            {
                                ChatHandler(player->GetSession()).PSendSysMessage("|cff822424[Sistem]|r Nu s-au gasit instante de |cffffffffRaid 10 Normal|r blocate pentru personajul tau.");
                                CloseGossipMenuFor(player);
                                return true;
                            }

                            if (!player->HasEnoughMoney(MoneyDungeons))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else
                            {
                                player->ModifyMoney(-int32(MoneyDungeons));

                                for (Player::BoundInstancesMap::iterator itr = binds.begin(); itr != binds.end(); )
                                {
                                    player->UnbindInstance(itr, diff, false);
                                    instanceReset = true;
                                }

                                if (instanceReset)
                                {
                                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00Succes:|r Toate instantele |cffffffffRaid 10 Normal|r au fost resetate. Poti intra fara re-log.");
                                }
                            }
                            CloseGossipMenuFor(player);
                            return true;
                        }
                        case KITT_ACTION_RESET_RAID25:
                        {
                            Difficulty diff = RAID_DIFFICULTY_25MAN_NORMAL;
                            bool instanceReset = false;

                            Player::BoundInstancesMap& binds = player->GetBoundInstances(diff);

                            if (binds.empty())
                            {
                                ChatHandler(player->GetSession()).PSendSysMessage("|cff822424[Sistem]|r Nu s-au gasit instante de |cffffffffRaid 25 Normal|r blocate pentru personajul tau.");
                                CloseGossipMenuFor(player);
                                return true;
                            }

                            if (!player->HasEnoughMoney(MoneyDungeons))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else
                            {
                                player->ModifyMoney(-int32(MoneyDungeons));

                                for (Player::BoundInstancesMap::iterator itr = binds.begin(); itr != binds.end(); )
                                {
                                    player->UnbindInstance(itr, diff, false);
                                    instanceReset = true;
                                }

                                if (instanceReset)
                                {
                                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00Succes:|r Toate instantele |cffffffffRaid 25 Normal|r au fost resetate. Poti intra fara re-log.");
                                }
                            }
                            CloseGossipMenuFor(player);
                            return true;
                        }
                        case KITT_ACTION_RESET_RAID10H:
                        {
                            Difficulty diff = RAID_DIFFICULTY_10MAN_HEROIC;
                            bool instanceReset = false;

                            Player::BoundInstancesMap& binds = player->GetBoundInstances(diff);

                            if (binds.empty())
                            {
                                ChatHandler(player->GetSession()).PSendSysMessage("|cff822424[Sistem]|r Nu s-au gasit instante de |cffffffffRaid 10 Heroic|r blocate pentru personajul tau.");
                                CloseGossipMenuFor(player);
                                return true;
                            }

                            if (!player->HasEnoughMoney(MoneyDungeons))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else
                            {
                                player->ModifyMoney(-int32(MoneyDungeons));

                                for (Player::BoundInstancesMap::iterator itr = binds.begin(); itr != binds.end(); )
                                {
                                    player->UnbindInstance(itr, diff, false);
                                    instanceReset = true;
                                }

                                if (instanceReset)
                                {
                                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00Succes:|r Toate instantele |cffffffffRaid 10 Heroic|r au fost resetate. Poti intra fara re-log.");
                                }
                            }
                            CloseGossipMenuFor(player);
                            return true;
                        }
                        case KITT_ACTION_RESET_RAID25H:
                        {
                            Difficulty diff = RAID_DIFFICULTY_25MAN_HEROIC;
                            bool instanceReset = false;

                            Player::BoundInstancesMap& binds = player->GetBoundInstances(diff);

                            if (binds.empty())
                            {
                                ChatHandler(player->GetSession()).PSendSysMessage("|cff822424[Sistem]|r Nu s-au gasit instante de |cffffffffRaid 25 Heroic|r blocate pentru personajul tau.");
                                CloseGossipMenuFor(player);
                                return true;
                            }

                            if (!player->HasEnoughMoney(MoneyDungeons))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else
                            {
                                player->ModifyMoney(-int32(MoneyDungeons));

                                for (Player::BoundInstancesMap::iterator itr = binds.begin(); itr != binds.end(); )
                                {
                                    player->UnbindInstance(itr, diff, false);
                                    instanceReset = true;
                                }

                                if (instanceReset)
                                {
                                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00Succes:|r Toate instantele |cffffffffRaid 25 Heroic|r au fost resetate. Poti intra fara re-log.");
                                }
                            }
                            CloseGossipMenuFor(player);
                            return true;
                        }
                        case KITT_ACTION_RESET_DUNGEON:
                        {
                            Difficulty diff = DUNGEON_DIFFICULTY_HEROIC;
                            bool instanceReset = false;

                            Player::BoundInstancesMap& binds = player->GetBoundInstances(diff);

                            if (binds.empty())
                            {
                                ChatHandler(player->GetSession()).PSendSysMessage("|cff822424[Sistem]|r Nu s-au gasit instante de |cffffffffDungeon Heroic|r blocate pentru personajul tau.");
                                CloseGossipMenuFor(player);
                                return true;
                            }

                            if (!player->HasEnoughMoney(MoneyDungeons))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else
                            {
                                player->ModifyMoney(-int32(MoneyDungeons));

                                for (Player::BoundInstancesMap::iterator itr = binds.begin(); itr != binds.end(); )
                                {
                                    player->UnbindInstance(itr, diff, false);
                                    instanceReset = true;
                                }

                                if (instanceReset)
                                {
                                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00Succes:|r Toate instantele |cffffffffDungeon Heroic|r au fost resetate. Poti intra fara re-log.");
                                }
                            }
                            CloseGossipMenuFor(player);
                            return true;
                        }

                        default:
                            break;
                    }
                    SendGossipMenuFor(player, KittNpcText::KITT_INSTANCE_RESET, me->GetGUID());
                    return true;
                }

                case KITT_SENDER_MENU_FUN_ZONE:
                {
                    player->PlayerTalkClass->ClearMenus();

                    switch (action)
                    {
                        case KITT_ACTION_TELE_FUN_ZONE:
                        {
                            {
                                for (size_t i = 0; i < FunZoneLocs.size(); ++i)
                                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, FunZoneLocs[i].name, KITT_AUTO_SENDER_FUN_ZONE, i);
                            }

                            break;
                        }

                        case KITT_ACTION_NU_APASA:
                        {
                            if (!player->HasEnoughMoney(NuApasaPret))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                                me->Say(player->GetName() + ", ai nevoie de 1500g.", LANG_UNIVERSAL, 0);
                                CloseGossipMenuFor(player);
                            }
                            else
                            {
                                player->ModifyMoney(-int32(NuApasaPret));
                                CloseGossipMenuFor(player);

                                me->Say("Ai dat de naiba!!! Iti dau 10 secunde sa fugi.", LANG_UNIVERSAL, 0);
                                me->SetFaction(14);
                                me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                                me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                                me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR);
                                me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR);
                                me->SetReactState(REACT_PASSIVE);
                                me->GetMotionMaster()->MoveIdle();
                                me->GetVictim();
                                me->SetInCombatWith(me->SelectNearestPlayer(5.0f));
                                me->Attack(player, 0);
                            }
                            return true;
                            //break;
                        }

                        case KITT_ACTION_RESET_ALL_BUFF:
                        {
                            // Verificam daca jucatorul are vreo aura activa
                            if (player->GetAppliedAuras().empty())
                            {
                                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000Eroare:|r Nu ai nicio aura activa pentru a fi eliminata.");
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else if (!player->HasEnoughMoney(ResetAllAura))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else
                            {
                                player->RemoveAllAuras();
                                player->ModifyMoney(-int32(ResetAllAura));
                                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00Succes:|r Toate |cffffffffaura\'s & buff|r au fost eliminate.");
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            break;
                        }

                        case KITT_ACTION_RESET_ALL_CD_SPELL:
                        {
                            // Verificam daca jucatorul are cooldown-uri active
                            if (player->GetSpellHistory()->GetCooldownsSizeForPacket() == 0)
                            {
                                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000Eroare:|r Nu ai niciun cooldown activ pentru a fi resetat.");
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else if (!player->HasEnoughMoney(ResetAllSpellCd))
                            {
                                player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            else
                            {
                                player->GetSpellHistory()->ResetAllCooldowns();
                                player->ModifyMoney(-int32(ResetAllSpellCd));
                                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00Succes:|r Toate |cffffffffspell\'s cooldown|r au fost resetate.");
                                CloseGossipMenuFor(player);
                                return true;
                            }
                            break;
                        }

                        case KITT_ACTION_FLY_BABY_FLY:
                        {
                            CloseGossipMenuFor(player);
                            if (player->GetSkillValue(762) < 75)
                            {
                                player->GetSession()->SendNotification("Ai nevoie de skill-ul Riding (75) pentru a folosi acest meniu!");
                                return true;
                            }
                            player->CastSpell(player, 47977, false);
                            return true;
                        }

                        default:
                            break;
                    }

                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<<< Back <<<", KITT_SENDER_OPEN_SUBMENU, KITT_ACTION_MENU_FUN_ZONE);
                    SendGossipMenuFor(player, KittNpcText::KITT_NPC_HELLO, me->GetGUID());
                    return true; // Returnam true pentru a confirma afisarea meniului
                }

                //  3. Logica pentru Teleportare sau Logica finala action
                // menu de cautare automata pentru enum
                case KITT_AUTO_SENDER_HORDE:
                {
                    if (player->GetTeam() == HORDE)
                    {
                        if (action < HordeLocs.size())
                        {
                            TeleportLocation const& loc = HordeLocs[action];
                            //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                            BeginTeleport(player, loc);
                        }
                    }
                    else
                    {
                        if (action < HordeALocs.size())
                        {
                            TeleportLocation const& loc = HordeALocs[action];
                            //BeginDelayedTeleport(player, loc.map, loc.x, loc.y, loc.z, loc.o);
                            BeginTeleport(player, loc);
                            //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        }
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_ALLIANCE:
                {
                    if (player->GetTeam() == ALLIANCE)
                    {
                        if (action < AllianceLocs.size())
                        {
                            TeleportLocation const& loc = AllianceLocs[action];
                            //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                            BeginTeleport(player, loc);
                        }
                    }
                    else
                    {
                        if (action < AllianceHLocs.size())
                        {
                            TeleportLocation const& loc = AllianceHLocs[action];
                            BeginTeleport(player, loc);
                            //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        }
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_E_KINGDOM:
                {
                    if (action < EKingdomLocs.size())
                    {
                        TeleportLocation const& loc = EKingdomLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_KALIMDOR:
                {
                    if (action < KalimadorLocs.size())
                    {
                        TeleportLocation const& loc = KalimadorLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_OUTLAND:
                {
                    if (action < OutlandLocs.size())
                    {
                        TeleportLocation const& loc = OutlandLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_NORTHREND:
                {
                    if (action < NorthrendLocs.size())
                    {
                        TeleportLocation const& loc = NorthrendLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_CLASSIC_DUNGEONS:
                {
                    if (action < ClassicDungeonsLocs.size())
                    {
                        TeleportLocation const& loc = ClassicDungeonsLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_BC_DUNGEONS:
                {
                    if (action < BcDungeonsLocs.size())
                    {
                        TeleportLocation const& loc = BcDungeonsLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_WRATH_DUNGEONS:
                {
                    if (action < WrathDungeonsLocs.size())
                    {
                        TeleportLocation const& loc = WrathDungeonsLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_RAID:
                {
                    if (action < RaidLocs.size())
                    {
                        TeleportLocation const& loc = RaidLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_FUN_ZONE:
                {
                    if (action < FunZoneLocs.size())
                    {
                        TeleportLocation const& loc = FunZoneLocs[action];
                        //player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                        BeginTeleport(player, loc);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }
            }

            CloseGossipMenuFor(player);
            return true;
        }

        static void BeginTeleport(Player* player, TeleportLocation const& loc)
        {
            if (!player || !player->IsInWorld())
                return;

            player->PlayerTalkClass->SendCloseGossip();

            if (player->IsAlive())
            {
                // efect winter, random spell to player
                /*bool hasAura = false;
                for (uint32 kittspell : KittWinterTransformSpells)
                {
                    if (player->HasAura(kittspell))
                    {
                        hasAura = true;
                        break;
                    }
                }

                if (!hasAura)
                {
                    uint32 randomSpell = Trinity::Containers::SelectRandomContainerElement(KittWinterTransformSpells);
                    player->CastSpell(player, randomSpell, true);
                }*/
                player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
            }
        }

        static void BeginDelayedTeleportOFF(Player* player, TeleportLocation const& loc)
        {
            //if (!player || player->HasAura(36901)) // Folosim 36901 doar ca block intern rapid
            //    return;

            player->PlayerTalkClass->SendCloseGossip();

            // Model 169
            // creature_template set faction 32, flags_extra=128 (trigger)
            player->CastSpell(player, 45451, false);   // hearthstone cosmetic
            TempSummon* visualTrigger = player->SummonCreature(90015, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 5500ms);
            if (visualTrigger)
            {
                visualTrigger->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1 | UNIT_FLAG_IMMUNE | UNIT_FLAG_PACIFIED);

                visualTrigger->AddAura(42050, visualTrigger);
                visualTrigger->AddAura(48387, visualTrigger);
                visualTrigger->AddAura(50771, visualTrigger);
                
            }

            player->m_Events.AddEventAtOffset([player, loc, visualTrigger]()
            {
                if (player && player->IsInWorld())
                {
                    //player->RemoveAura(36901);
                    //if (visualTrigger)
                    //    visualTrigger->DespawnOrUnsummon(); // Curatam trigger-ul
                    if (!player->IsNonMeleeSpellCast(false))
                    {
                        if (visualTrigger)
                            visualTrigger->DespawnOrUnsummon(); // Stergem efectele daca a anulat
                        return; // Iesim din functie, deci NU mai are loc teleportarea
                    }

                    if (player->IsAlive())
                        player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                }
            }, 4410ms);  // teleport delay
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new kitt_npc_menuAI(creature);
    }
};

class kitt_npc_menu_validator : public WorldScript
{
public:
    kitt_npc_menu_validator() : WorldScript("kitt_npc_menu_validator") {}

    void OnStartup() override
    {
        // 1. Definim lista de ID-uri pe care vrem sa le verificam
        // Folosim direct lista definita an namespace
        const std::vector<uint32>& requiredTexts = KittNpcText::KittAllNpcTexts;

        if (requiredTexts.empty())
            return;

        // 2. Construim string-ul pentru SQL: NPC_TEXT
        std::ostringstream ss;
        for (size_t i = 0; i < requiredTexts.size(); ++i)
        {
            ss << requiredTexts[i];
            if (i < requiredTexts.size() - 1) ss << ", ";
        }

        // 3. Interogam baza de date pentru a vedea care din ele EXISTA
        QueryResult result = WorldDatabase.PQuery("SELECT `ID` FROM `npc_text` WHERE `ID` in ({})", ss.str().c_str());

        // 4. Verificam ce lipseste
        std::set<uint32> foundIds;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                foundIds.insert(fields[0].GetUInt32());
            } while (result->NextRow());
        }

        // 5. Comparam lista ceruta cu ce am gasit in DB si logam erorile
        for (uint32 id : requiredTexts)
        {
            if (foundIds.find(id) == foundIds.end())
            {
                TC_LOG_ERROR("sql.sql", ">> Eroare Script: kitt_npc_menu -> npc_text ID {} lipseste din tabelul `npc_text`!", id);
            }
        }
    }
};

void AddSC_kitt_npc_menu()
{
    new kitt_npc_menu();
    new kitt_npc_menu_validator();
}
