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


std::string KittResetCode = "RESET"; // Valoare default config
uint32 MoneyDungeons = 300000;   // galbeni necesari pentru Dungeons
uint32 MoneyRaid10   = 1000000;  // galbeni necesari pentru Raid 10
uint32 MoneyRaid25   = 3500000;  // galbeni necesari pentru Raid 25
uint32 MoneyRaid10H  = 2000000;  // galbeni necesari pentru Raid 10 Heroic
uint32 MoneyRaid25H  = 5000000;  // galbeni necesari pentru Raid 25 Heroic
// conversie la string ca sa poate fi adaugat in mesaj
//uint32 kittMoneyDungeons = MoneyDungeons / 10000;




namespace KittNpcText
{
    // 1. Definim ID-urile aici
    // npc_text.ID
    enum Texts
    {
        KITT_NPC_HELLO                = 90014,
        KITT_INSTANCE_RESET           = 90015,
        KITT_INSTANCE_RESET_NO_SHOW   = 90016
    };

    // 2. Le punem automat in aceasta lista pentru verificare
    static const std::vector<uint32> KittAllNpcTexts = {
        KITT_NPC_HELLO,
        KITT_INSTANCE_RESET,
        KITT_INSTANCE_RESET_NO_SHOW
    };
}

enum KittSender
{
    // uniq ID start 0
    KITT_SENDER_MAIN_MENU_SELECT        = 0,   // comun pentru action select
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
    KITT_ACTION_RESET_DUNGEON          = 115,   // Instance Reset Dungeons
    KITT_ACTION_RESET_RAID10            = 116,   // Instance Reset Raid 10
    KITT_ACTION_RESET_RAID25            = 117,   // Instance Reset Raid 25
    KITT_ACTION_RESET_RAID10H           = 118,   // Instance Reset Raid 10 Heroic
    KITT_ACTION_RESET_RAID25H           = 119,   // Instance Reset Raid 25 Heroic
    KITT_ACTION_TELEPORT_TO             = 120,   // meniu Teleport To



    KITT_ACTION_AH_OPEN                 = 295,   // AH open
    KITT_ACTION_BANK_OPEN               = 296,   // bank open
    KITT_ACTION_MAIL_OPEN               = 297,   // mail open
    KITT_ACTION_VENDOR_OPEN             = 298,   // vendor open
    KITT_ACTION_TELE_ZONE               = 299,   // Teleport Zone
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
static std::vector<MainMenuOption> KittMainMenu = {
    { GOSSIP_ICON_CHAT, "Teleport to: Custom Zone",  KITT_SENDER_MAIN_MENU_SELECT, KITT_ACTION_TELE_ZONE },
    { GOSSIP_ICON_TALK, "Horde",                     KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_MENU_HORDE },
    { GOSSIP_ICON_TALK, "Alliance",                  KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_MENU_ALLIANCE },
    { GOSSIP_ICON_TALK, "Teleport to:",              KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_TELEPORT_TO },
    { GOSSIP_ICON_CHAT, "Deschide AH",               KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_AH_OPEN },
    { GOSSIP_ICON_CHAT, "Deschide Bank",             KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_BANK_OPEN },
    { GOSSIP_ICON_CHAT, "Deschide Mail",             KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_MAIL_OPEN },
    { GOSSIP_ICON_CHAT, "Deschide Vendor",           KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_VENDOR_OPEN },
    { GOSSIP_ICON_CHAT, "Fun Zone (in constructie)", KITT_SENDER_OPEN_SUBMENU,     KITT_ACTION_MENU_FUN_ZONE }
};

static std::vector<MainMenuOption> KittTeleportTo = {
    { GOSSIP_ICON_TALK, "Eastern Kingdoms",          KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_E_KINGDOM },
    { GOSSIP_ICON_TALK, "Kalimdor",                  KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_KALIMDOR },
    { GOSSIP_ICON_CHAT, "Outland",                   KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_OUTLAND },
    { GOSSIP_ICON_CHAT, "Northrend",                 KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_NORTHREND },
    { GOSSIP_ICON_CHAT, "Classic Dungeons",          KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_CLASSIC_DUNGEONS },
    { GOSSIP_ICON_CHAT, "BC Dungeons",               KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_BC_DUNGEONS },
    { GOSSIP_ICON_CHAT, "Wrath Dungeons",            KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_WRATH_DUNGEONS },
    { GOSSIP_ICON_CHAT, "Raid Teleports",            KITT_SENDER_TELEPORT_TO,     KITT_ACTION_MENU_RAID }
};
// Meniu Fun Zone
static std::vector<MainMenuOption> KittFunZone = {
    { GOSSIP_ICON_CHAT, "Zona Fun (PVP Arena)",  KITT_SENDER_OPEN_SUBMENU,       KITT_ACTION_TELE_FUN_ZONE },
    { GOSSIP_ICON_CHAT, "Nu Apasa!!! (1500 g)",  KITT_SENDER_MENU_FUN_ZONE,       KITT_ACTION_NU_APASA },
    { GOSSIP_ICON_CHAT, "Instance Reset CD",     KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_MENU_INSTANCE_RESET }
};

// Meniu Instance Reset Cooldown cu confirmare.
// menu code. daca are TRUE optiunea finala se adauga la OnGossipSelectCode

static std::vector<MainMenuOptionConfirm> KittInstanceReset = {
    { GOSSIP_ICON_CHAT, "Reset Dungeon Heroic (" + std::to_string(MoneyDungeons / 10000) + " g)", KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_DUNGEON,  "Reset Dungeon Heroic",  MoneyDungeons, false},
    { GOSSIP_ICON_CHAT, "Reset Raid 10 Normal (" + std::to_string(MoneyRaid10 / 10000) + " g)",   KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_RAID10,   "Reset Raid 10 Normal",  MoneyRaid10,   false },
    { GOSSIP_ICON_CHAT, "Reset Raid 25 Normal (" + std::to_string(MoneyRaid25 / 10000) + " g)",   KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_RAID25,   "Reset Raid 25 Normal",  MoneyRaid25,   false },
    { GOSSIP_ICON_CHAT, "Reset Raid 10 Heroic (" + std::to_string(MoneyRaid10H / 10000) + " g)",  KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_RAID10H,  "Reset Raid 10 Heroic",  MoneyRaid10H,  false },
    { GOSSIP_ICON_CHAT, "Reset Raid 25 Heroic (" + std::to_string(MoneyRaid25H / 10000) + " g)",  KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_RESET_RAID25H,  "Reset Raid 25 Heroic",  MoneyRaid25H,  false },
    { GOSSIP_ICON_CHAT, "<<< Back <<<",      KITT_SENDER_OPEN_SUBMENU,        KITT_ACTION_MENU_FUN_ZONE,  "",  0,  false }
};


// Structura teleport
struct TeleportLocation {
    uint32 map;         // map ID
    float x, y, z, o;   // teleport coordonate
    char const* name;   // Nume Meniu Teleport
};


// Tabel pentru Alliance
static std::vector<TeleportLocation> AllianceLocs = {
    {0, -8837.57f, 637.394f, 94.8055f, 4.96f, "Stormwind"},
    {1, 9946.02f, 2588.77f, 1316.19f, 0.0f, "Darnassus"},
    {0, -5002.33f, -857.664f, 497.054f, 0.0f, "Ironforge"},
    {530, -4004.39f, -11878.5f, -1.01131f, 0.0f, "Exodar"}
};

// Tabel pentru Horde
static std::vector<TeleportLocation> HordeLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "Orgrimmar"},
    {0, 1572.00f, 213.637f, -43.1031f, 0.85f, "Undercity"},
    {1, -1386.75f, 138.474f, 23.0348f, 0.0f, "Thunder Bluff"},
    {530, 9380.18f, -7277.81f, 14.2404f, 0.0f, "Silvermoon"}
};

// Eastern Kingdoms
static std::vector<TeleportLocation> EKingdomLocs = {
    {0, -9449.06f, 64.8392f, 56.3581f, 3.07047f, "Elwynn Forest"},
    {530, 9024.37f, -6682.55f, 16.8973f, 3.14131f, "Eversong Woods"},
    {0, -5603.76f, -482.704f, 396.98f, 5.23499f, "Dun Morogh"},
    {0, 2274.95f, 323.918f, 34.1137f, 4.24367f, "Tirisfal Glades"},
    {530, 7595.73f, -6819.6f, 84.3718f, 2.56561f, "Ghostlands"},
    {0, -5405.85f, -2894.15f, 341.972f, 5.48238f, "Loch Modan"},
    {0, 505.126f, 1504.63f, 124.808f, 1.77987f, "Silverpine Forest"},
    {0, -10684.9f, 1033.63f, 32.5389f, 6.07384f, "Westfall"},
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
};

// Kalimdor
static std::vector<TeleportLocation> KalimadorLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "kalimador"},
};

// Outland
static std::vector<TeleportLocation> OutlandLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "outland"},
};

// Northrend
static std::vector<TeleportLocation> NorthrendLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "northrend"},
};

// Classic Dungeons
static std::vector<TeleportLocation> ClassicDungeonsLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "classic dungeons"},
};

// BC Dungeons
static std::vector<TeleportLocation> BcDungeonsLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "bc dungeons"},
};

// Wrath Dungeons
static std::vector<TeleportLocation> WrathDungeonsLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "wrath dungeons"},
};

// Raid Teleports
static std::vector<TeleportLocation> RaidLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "raid"},
};



// Tabel pentru Fun Zone
static std::vector<TeleportLocation> FunZoneLocs = {
    {1, 1493.17f, -4414.95f, 23.04f, 0.0f, "Orgrimmar Test"},
    {0, 1572.00f, 213.637f, -43.1031f, 0.85f, "Undercity Test"}
};




class kitt_npc_menu : public CreatureScript
{
public:
    kitt_npc_menu() : CreatureScript("kitt_npc_menu") {}

    struct kitt_npc_menuAI : public ScriptedAI
    {
        kitt_npc_menuAI(Creature* creature) : ScriptedAI(creature) {}

        void Reset() override
        {
            me->SetMaxHealth(551000);
            me->SetHealth(550500);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_MAILBOX);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_BANKER);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_VENDOR);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_REPAIR);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_AUCTIONEER);
            me->RemoveFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);
            me->SetReactState(REACT_PASSIVE);
            me->SetFaction(35);
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
                player->GetSession()->SendNotification("Nu poti face asta in lupta!");
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
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            uint32 const sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
            std::string codeStr = code; // Transformam codul primit in string pentru verificare

            if (sender == KITT_SENDER_MENU_INSTANCE_RESET)
            {
                if (action == KITT_ACTION_RESET_RAID10)
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
            }

            CloseGossipMenuFor(player);
            return false;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            uint32 const sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
            // std::string codeStr = code;


            // Verificam prima data butonul de BACK sau REVENIRE LA MAIN MENU
            if (sender == KITT_SENDER_MAIN_MENU_SELECT && action == KITT_ACTION_BACK_MAIN_MENU)
            {
                return OnGossipHello(player);
            }

            // Actiune direct din Main Menu
            if (sender == KITT_SENDER_MAIN_MENU_SELECT && action == KITT_ACTION_TELE_ZONE)
            {
                player->TeleportTo(0, -4854.04f, -1004.33f, 453.76f, 0.0f);
                CloseGossipMenuFor(player);
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
                            for (size_t i = 0; i < HordeLocs.size(); ++i)
                            {
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, HordeLocs[i].name, KITT_AUTO_SENDER_HORDE, i);
                            }
                            break;
                        }

                        case KITT_ACTION_MENU_ALLIANCE:
                        {
                            for (size_t i = 0; i < AllianceLocs.size(); ++i)
                                AddGossipItemFor(player, GOSSIP_ICON_CHAT, AllianceLocs[i].name, KITT_AUTO_SENDER_ALLIANCE, i);
                            break;
                        }

                        case KITT_ACTION_MENU_FUN_ZONE:
                        {
                            // daca jucatorul este pe harta 0, 1, 530, 571 va aparea si menu 
                            if (player->GetMapId() == 0 || player->GetMapId() == 1 || player->GetMapId() == 530 || player->GetMapId() == 571)
                            {
                                for (auto const& option : KittFunZone)
                                {
                                    AddGossipItemFor(player, option.icon, option.name, option.sender, option.action);
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

                        case KITT_ACTION_TELE_FUN_ZONE:
                        {
                            {
                                for (size_t i = 0; i < FunZoneLocs.size(); ++i)
                                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, FunZoneLocs[i].name, KITT_AUTO_SENDER_FUN_ZONE, i);
                            }
                            break;
                        }

                        case KITT_ACTION_AH_OPEN:
                        {
                            player->GetSession()->SendAuctionHello(me->GetGUID(), me);
                            CloseGossipMenuFor(player);
                            return false;
                        }

                        case KITT_ACTION_BANK_OPEN:
                        {
                            player->GetSession()->SendShowBank(me->GetGUID());
                            CloseGossipMenuFor(player);
                            return false;
                        }

                        case KITT_ACTION_MAIL_OPEN:
                        {
                            player->GetSession()->SendShowMailBox(me->GetGUID());

                            // Nu inchide meniul de gossip aici, deoarece interfata de mail va aparea deasupra
                            // CloseGossipMenuFor(player);
                            return false;
                        }

                        case KITT_ACTION_VENDOR_OPEN:
                        {
                            player->GetSession()->SendListInventory(me->GetGUID());
                            CloseGossipMenuFor(player);
                            return false;
                        }
                        default:
                            break;
                    }
                    // Adaugam butonul de inapoi care sa trimita spre Main Menu
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<< Back Back <<", KITT_SENDER_MAIN_MENU_SELECT, KITT_ACTION_BACK_MAIN_MENU);
                    SendGossipMenuFor(player, KittNpcText::KITT_NPC_HELLO, me->GetGUID());
                    return true; // Returnam true pentru a confirma afisarea meniului
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
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<< Inapoi la Categorii <<", KITT_SENDER_OPEN_SUBMENU, KITT_ACTION_TELEPORT_TO);
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

                //  3. Logica pentru Teleportare sau Logica finala action
                // menu de cautare automata pentru enum
                case KITT_AUTO_SENDER_HORDE:
                {
                    if (action < HordeLocs.size())
                    {
                        TeleportLocation const& loc = HordeLocs[action];
                        player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_ALLIANCE:
                {
                    if (action < AllianceLocs.size())
                    {
                        TeleportLocation const& loc = AllianceLocs[action];
                        player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_E_KINGDOM:
                {
                    if (action < EKingdomLocs.size())
                    {
                        TeleportLocation const& loc = EKingdomLocs[action];
                        player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }

                case KITT_AUTO_SENDER_FUN_ZONE:
                {
                    if (action < FunZoneLocs.size())
                    {
                        TeleportLocation const& loc = FunZoneLocs[action];
                        player->TeleportTo(loc.map, loc.x, loc.y, loc.z, loc.o);
                    }
                    CloseGossipMenuFor(player);
                    return true;
                }
            }

            CloseGossipMenuFor(player);
            return true;
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
    // KittResetCode = sConfigMgr->GetStringDefault("Kitt.Reset.Code", "test");
    new kitt_npc_menu();
    new kitt_npc_menu_validator();
}
