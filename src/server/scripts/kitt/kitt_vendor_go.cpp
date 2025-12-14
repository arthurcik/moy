#include "ScriptMgr.h"
#include "CellImpl.h"
#include "CombatAI.h"
#include "Containers.h"
#include "CreatureTextMgr.h"
#include "GameEventMgr.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "GossipDef.h" // NECESAR
#include "MotionMaster.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PassiveAI.h"
#include "Pet.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "SmartAI.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "Vehicle.h"
#include "World.h"
#include "WorldSession.h"
#include "DatabaseEnv.h"
#include "Chat.h"
#include <sstream>
#include <string>



// Enum pentru a defini constantele scriptului, folosind prefixe unice KITT_
enum TeleporterVendorMenuKitt
{
    KITT_MENU_ID_TELEPORT = 60901,   // gossip_menu_option.MenuID
    KITT_NPC_TEXT_HELLO = 90014,     // npc_text.entry
    KITT_NPC_TEXT_INSTANCE = 90015,  // text instance reset

    // SENDER IDs: IMPORTANT!
    // GOSSIP_SENDER_MAIN este de obicei 0

    KITT_SENDER_MAIN_MENU_NAV = 0, // common for tele select
    KITT_SENDER_MENU_TELE_ZONE = 1, // Teleport Zone Custom
    KITT_SENDER_MENU_NAV_HORDE = 2, // menu Horde
    KITT_SENDER_MENU_NAV_ALLIANCE = 3,   // menu Alliance
    KITT_SENDER_EKINGDOMS_NAV = 4,   // Eastern Kingdoms
    KITT_SENDER_KALIMDOR_NAV = 5,    // Kalimdor
    KITT_SENDER_MENU_OUTLAND = 6,     // menu Outland
    KITT_SENDER_NORTHREND_NAV = 7,   // Northrend
    KITT_SENDER_C_DUNGEONS_NAV = 8,   // Classic Dungeons
    KITT_SENDER_BC_DUNGEONS_NAV = 9,   // BC Dungeons
    KITT_SENDER_WRATH_DUNGEONS_NAV = 10,   // Wrath Dungeons
    KITT_SENDER_RAID_NAV = 11,   // Raid Teleports
    KITT_SENDER_MENU_FUN_ZONE = 12, // menu Fun Zone
    KITT_SENDER_VENDOR_NAV = 13,   // vendor
    KITT_SENDER_MAIL_NAV = 14,  // mail open
    KITT_SENDER_BANK_NAV = 15,   // bank open
    KITT_SENDER_AH_NAV = 16,   // AH open
    KITT_SENDER_MENU_INSTANCE_RESET = 17, // menu Instance Reset submenu in fun zone


    // GOSSIP OPTION IDs (din DB) - gossip_menu_option.optionID pentru afisare menu si actiune
    // ordinea de afisare este dupa nr de id de la mic la mare
    // daca un id este lipsa in DB nu este afisat deloc
    // main menu text
    KITT_GOSSIP_TELE_ZONE = 101,   // Teleport Custom Zone
    KITT_GOSSIP_OPTION_1 = 102,   // menu horde
    KITT_GOSSIP_OPTION_2 = 103,   // menu alliance
    KITT_GOSSIP_OPTION_17 = 104,   // Eastern Kingdoms
    KITT_GOSSIP_OPTION_18 = 105,   // Kalimdor
    KITT_GOSSIP_OPTION_5 = 106,   // Outland
    KITT_GOSSIP_OPTION_19 = 107,   // Northrend
    KITT_GOSSIP_OPTION_20 = 108,   // Classic Dungeons
    KITT_GOSSIP_OPTION_21 = 109,   // BC Dungeons
    KITT_GOSSIP_OPTION_22 = 110,   // Wrath Dungeons
    KITT_GOSSIP_OPTION_23 = 111,   // Raid Teleports
    KITT_GOSSIP_OPTION_6 = 112,   // menu Fun Zone
    // sub menu horde
    KITT_GOSSIP_OPTION_3 = 121,   // tele orgrimmar
    KITT_GOSSIP_OPTION_7 = 122,   // tele Undercity
    KITT_GOSSIP_OPTION_8 = 123,   // tele Thunder Bluff
    KITT_GOSSIP_OPTION_9 = 124,   // tele Silvermoon
    // sub menu alliance
    KITT_GOSSIP_OPTION_4 = 125,   // tele stormwind
    KITT_GOSSIP_OPTION_10 = 126,   // tele Darnassus
    KITT_GOSSIP_OPTION_11 = 127,   // tele Ironforge
    KITT_GOSSIP_OPTION_12 = 128,   // tele Exodar
    // sub menu Eastern Kingdoms 24
    KITT_GOSSIP_OPTION_24 = 140,   // Elwynn Forest
    KITT_GOSSIP_OPTION_25 = 141,   // Eversong Woods
    KITT_GOSSIP_OPTION_26 = 142,   // Dun Morogh
    KITT_GOSSIP_OPTION_27 = 143,   // Tirisfal Glades
    KITT_GOSSIP_OPTION_28 = 144,   // Ghostlands
    KITT_GOSSIP_OPTION_29 = 145,   // Loch modan
    KITT_GOSSIP_OPTION_30 = 146,   // Silverpine Forest
    KITT_GOSSIP_OPTION_31 = 147,   // Westfall
    KITT_GOSSIP_OPTION_32 = 148,   // Redridge mountains
    KITT_GOSSIP_OPTION_33 = 149,   // Duskwood
    KITT_GOSSIP_OPTION_34 = 150,   // Hillsbrad Foothills
    KITT_GOSSIP_OPTION_35 = 151,   // Wetlands
    KITT_GOSSIP_OPTION_36 = 152,   // Alterac Mountains
    KITT_GOSSIP_OPTION_37 = 153,   // Arathi Highlands
    KITT_GOSSIP_OPTION_38 = 154,   // Stranglethorn Vale
    KITT_GOSSIP_OPTION_39 = 155,   // Badlands
    KITT_GOSSIP_OPTION_40 = 156,   // Swamp of Sorrows
    KITT_GOSSIP_OPTION_41 = 157,   // The Hinterlands
    KITT_GOSSIP_OPTION_42 = 158,   // Searing Gorge
    KITT_GOSSIP_OPTION_43 = 159,   // Blasted Lands
    KITT_GOSSIP_OPTION_44 = 160,   // Burning Steppes
    KITT_GOSSIP_OPTION_45 = 161,   // Western Plaguelands
    KITT_GOSSIP_OPTION_46 = 162,   // Eastern Plaguelands
    KITT_GOSSIP_OPTION_47 = 163,   // Isle of Quel\'Danas
    // sub menu Kalimdor 48
    KITT_GOSSIP_OPTION_48 = 164,   // Azuremyst Isle
    KITT_GOSSIP_OPTION_49 = 165,   // Teldrassil
    KITT_GOSSIP_OPTION_50 = 166,   // Durotar
    KITT_GOSSIP_OPTION_51 = 167,   // Mulgore
    KITT_GOSSIP_OPTION_52 = 168,   // Bloodmyst Isle
    KITT_GOSSIP_OPTION_53 = 169,   // Darkshore
    KITT_GOSSIP_OPTION_54 = 170,   // The Barrens
    KITT_GOSSIP_OPTION_55 = 171,   // Stonetalon Mountains
    KITT_GOSSIP_OPTION_56 = 172,   // Ashenvale
    KITT_GOSSIP_OPTION_57 = 173,   // Thousand Needles
    KITT_GOSSIP_OPTION_58 = 174,   // Desolace
    KITT_GOSSIP_OPTION_59 = 175,   // Dustwallow Marsh
    KITT_GOSSIP_OPTION_60 = 176,   // Feralas
    KITT_GOSSIP_OPTION_61 = 177,   // Tanaris
    KITT_GOSSIP_OPTION_62 = 178,   // Azshara
    KITT_GOSSIP_OPTION_63 = 179,   // Felwood
    KITT_GOSSIP_OPTION_64 = 180,   // Un\'Goro Crater
    KITT_GOSSIP_OPTION_65 = 181,   // Silithus
    KITT_GOSSIP_OPTION_66 = 182,   // Winterspring
    // sub menu Outland 67
    KITT_GOSSIP_OPTION_14 = 183,   // tele Shattrath
    KITT_GOSSIP_OPTION_67 = 184,   // Hellfire Peninsula
    KITT_GOSSIP_OPTION_68 = 185,   // Zangarmarsh
    KITT_GOSSIP_OPTION_69 = 186,   // Terokkar Forest
    KITT_GOSSIP_OPTION_70 = 187,   // Nagrand
    KITT_GOSSIP_OPTION_71 = 188,   // Blade\'s Edge Mountains
    KITT_GOSSIP_OPTION_72 = 189,   // Netherstorm
    KITT_GOSSIP_OPTION_73 = 190,   // Shadowmoon Valley
    // sub menu Northrend 74
    KITT_GOSSIP_OPTION_15 = 191,   // tele Dalaran
    KITT_GOSSIP_OPTION_75 = 192,   // Borean Tundra
    KITT_GOSSIP_OPTION_76 = 193,   // Howling Fjord
    KITT_GOSSIP_OPTION_77 = 194,   // Dragonblight
    KITT_GOSSIP_OPTION_78 = 195,   // Grizzly Hills
    KITT_GOSSIP_OPTION_79 = 196,   // Zul\'Drak
    KITT_GOSSIP_OPTION_80 = 197,   // Sholazar Basin
    KITT_GOSSIP_OPTION_81 = 198,   // Crystalsong Forest
    KITT_GOSSIP_OPTION_82 = 199,   // Storm Peaks
    KITT_GOSSIP_OPTION_83 = 200,   // Icecrown
    KITT_GOSSIP_OPTION_84 = 201,   // Wintergrasp
    // sub menu Classic Dungeons 85
    KITT_GOSSIP_OPTION_85 = 202,   // Gnomeregan
    KITT_GOSSIP_OPTION_86 = 203,   // The Deadmines
    KITT_GOSSIP_OPTION_87 = 204,   // The Stockade
    KITT_GOSSIP_OPTION_88 = 205,   // Ragefire Chasm
    KITT_GOSSIP_OPTION_89 = 206,   // Razorfen Downs
    KITT_GOSSIP_OPTION_90 = 207,   // Razorfen Kraul
    KITT_GOSSIP_OPTION_91 = 208,   // Scarlet Monastery
    KITT_GOSSIP_OPTION_92 = 209,   // Shadowfang Keep
    KITT_GOSSIP_OPTION_93 = 210,   // Wailing Caverns
    KITT_GOSSIP_OPTION_94 = 211,   // Blackfathom Deeps
    KITT_GOSSIP_OPTION_95 = 212,   // Blackrock Depths
    KITT_GOSSIP_OPTION_96 = 213,   // Blackrock Spire
    KITT_GOSSIP_OPTION_97 = 214,   // Dire Maul
    KITT_GOSSIP_OPTION_98 = 215,   // Maraudon
    KITT_GOSSIP_OPTION_99 = 216,   // Scholomance
    KITT_GOSSIP_OPTION_100 = 217,   // Stratholme
    KITT_GOSSIP_OPTION_101 = 218,   // Sunken Temple
    KITT_GOSSIP_OPTION_102 = 219,   // Uldaman
    KITT_GOSSIP_OPTION_103 = 220,   // Zul\'Farrak
    // sub menu BC Dungeons 104
    KITT_GOSSIP_OPTION_104 = 221,   // Auchindoun
    KITT_GOSSIP_OPTION_105 = 222,   // The Caverns of Time
    KITT_GOSSIP_OPTION_106 = 223,   // Coilfang Reservoir
    KITT_GOSSIP_OPTION_107 = 224,   // Hellfire Citadel
    KITT_GOSSIP_OPTION_108 = 225,   // Magisters\' Terrace
    KITT_GOSSIP_OPTION_109 = 226,   // Tempest Keep
    // sub menu Wrath Dungeons 110
    KITT_GOSSIP_OPTION_110 = 227,   // Azjol-Nerub
    KITT_GOSSIP_OPTION_111 = 228,   // The Culling of Stratholme
    KITT_GOSSIP_OPTION_112 = 229,   // Trial of the Champion
    KITT_GOSSIP_OPTION_113 = 230,   // Drak\'Tharon Keep
    KITT_GOSSIP_OPTION_114 = 231,   // Gundrak
    KITT_GOSSIP_OPTION_115 = 232,   // Icecrown Citadel Dungeons
    KITT_GOSSIP_OPTION_116 = 233,   // The Nexus Dungeons
    KITT_GOSSIP_OPTION_117 = 234,   // The Violet Hold
    KITT_GOSSIP_OPTION_118 = 235,   // Halls of Lightning
    KITT_GOSSIP_OPTION_119 = 236,   // Halls of Stone
    KITT_GOSSIP_OPTION_120 = 237,   // Utgarde Keep
    KITT_GOSSIP_OPTION_121 = 238,   // Utgarde Pinnacle
    // sub menu Raid Teleports 122
    KITT_GOSSIP_OPTION_122 = 250,   // Black Temple
    KITT_GOSSIP_OPTION_123 = 251,   // Blackwing Lair
    KITT_GOSSIP_OPTION_124 = 252,   // Hyjal Summit
    KITT_GOSSIP_OPTION_125 = 253,   // Serpentshrine Cavern
    KITT_GOSSIP_OPTION_126 = 254,   // Trial of the Crusader
    KITT_GOSSIP_OPTION_127 = 255,   // Gruul\'s Lair
    KITT_GOSSIP_OPTION_128 = 256,   // Magtheridon\'s Lair
    KITT_GOSSIP_OPTION_129 = 257,   // Icecrown Citadel
    KITT_GOSSIP_OPTION_130 = 258,   // Karazhan
    KITT_GOSSIP_OPTION_131 = 259,   // Molten Core
    KITT_GOSSIP_OPTION_132 = 260,   // Naxxramas
    KITT_GOSSIP_OPTION_133 = 261,   // Onyxia\'s Lair
    KITT_GOSSIP_OPTION_134 = 262,   // Ruins of Ahn\'Qiraj
    KITT_GOSSIP_OPTION_135 = 263,   // Sunwell Plateau
    KITT_GOSSIP_OPTION_136 = 264,   // The Eye
    KITT_GOSSIP_OPTION_137 = 265,   // Temple of Ahn\'Qiraj
    KITT_GOSSIP_OPTION_138 = 266,   // The Eye of Eternity
    KITT_GOSSIP_OPTION_139 = 267,   // The Obsidian Sanctum
    KITT_GOSSIP_OPTION_140 = 268,   // Ulduar
    KITT_GOSSIP_OPTION_141 = 269,   // Vault of Archavon
    KITT_GOSSIP_OPTION_142 = 270,   // Zul\'Gurub
    KITT_GOSSIP_OPTION_143 = 271,   // Zul\'Aman
    // sub menu fun zone
    KITT_GOSSIP_OPTION_13 = 290,   // tele pvp arena
    KITT_GOSSIP_OPTION_16 = 291,   // Nu Apasa
    KITT_GOSSIP_DUNGEONS_RESET = 292,   // Dungeons reset cd
    KITT_GOSSIP_RAID_10_RESET = 293,   // Instance reset 10 normal
    KITT_GOSSIP_RAID_25_RESET = 294,   // Instance reset 25 normal
    KITT_GOSSIP_RAID_10H_RESET = 295,   // Instance reset 10 heroic
    KITT_GOSSIP_RAID_25H_RESET = 296,   // Instance reset 25 heroic
    KITT_GOSSIP_INSTANCE_RESET = 297,   // Instance Reset CD menu
    // main common action
    KITT_GOSSIP_AH_OPEN = 395,   // ah open
    KITT_GOSSIP_BANK_OPEN = 396,   // bank open
    KITT_GOSSIP_MAIL_OPEN = 397,   //mail open
    KITT_GOSSIP_OPTION_VENDOR = 398,   // menu vendor
    KITT_GOSSIP_OPTION_BACK = 399,   // menu back


    // Action IDs (GOSSIP_ACTION_INFO_DEF + X)
    KITT_ACTION_OPEN_SUBMENU = 30,  // common for sub-menu
    // horde sub menu
    KITT_ACTION_TELEPORT_3 = 121,   // tele orgrimmar
    KITT_ACTION_TELEPORT_7 = 122,   // tele Undercity
    KITT_ACTION_TELEPORT_8 = 123,   // tele Thunder Bluff
    KITT_ACTION_TELEPORT_9 = 124,   // tele Silvermoon
    // sub menu alliance
    KITT_ACTION_TELEPORT_4 = 125,   // tele stormwind
    KITT_ACTION_TELEPORT_10 = 126,   // tele Darnassus
    KITT_ACTION_TELEPORT_11 = 127,   // tele Ironforge
    KITT_ACTION_TELEPORT_12 = 128,   // tele Exodar
    // sub menu Eastern Kingdoms 24
    KITT_ACTION_TELEPORT_24 = 140,   // Elwynn Forest
    KITT_ACTION_TELEPORT_25 = 141,   // Eversong Woods
    KITT_ACTION_TELEPORT_26 = 142,   // Dun Morogh
    KITT_ACTION_TELEPORT_27 = 143,   // Tirisfal Glades
    KITT_ACTION_TELEPORT_28 = 144,   // Ghostlands
    KITT_ACTION_TELEPORT_29 = 145,   // Loch modan
    KITT_ACTION_TELEPORT_30 = 146,   // Silverpine Forest
    KITT_ACTION_TELEPORT_31 = 147,   // Westfall
    KITT_ACTION_TELEPORT_32 = 148,   // Redridge mountains
    KITT_ACTION_TELEPORT_33 = 149,   // Duskwood
    KITT_ACTION_TELEPORT_34 = 150,   // Hillsbrad Foothills
    KITT_ACTION_TELEPORT_35 = 151,   // Wetlands
    KITT_ACTION_TELEPORT_36 = 152,   // Alterac Mountains
    KITT_ACTION_TELEPORT_37 = 153,   // Arathi Highlands
    KITT_ACTION_TELEPORT_38 = 154,   // Stranglethorn Vale
    KITT_ACTION_TELEPORT_39 = 155,   // Badlands
    KITT_ACTION_TELEPORT_40 = 156,   // Swamp of Sorrows
    KITT_ACTION_TELEPORT_41 = 157,   // The Hinterlands
    KITT_ACTION_TELEPORT_42 = 158,   // Searing Gorge
    KITT_ACTION_TELEPORT_43 = 159,   // Blasted Lands
    KITT_ACTION_TELEPORT_44 = 160,   // Burning Steppes
    KITT_ACTION_TELEPORT_45 = 161,   // Western Plaguelands
    KITT_ACTION_TELEPORT_46 = 162,   // Eastern Plaguelands
    KITT_ACTION_TELEPORT_47 = 163,   // Isle of Quel\'Danas
    // sub menu Kalimdor 48
    KITT_ACTION_TELEPORT_48 = 164,   // Azuremyst Isle
    KITT_ACTION_TELEPORT_49 = 165,   // Teldrassil
    KITT_ACTION_TELEPORT_50 = 166,   // Durotar
    KITT_ACTION_TELEPORT_51 = 167,   // Mulgore
    KITT_ACTION_TELEPORT_52 = 168,   // Bloodmyst Isle
    KITT_ACTION_TELEPORT_53 = 169,   // Darkshore
    KITT_ACTION_TELEPORT_54 = 170,   // The Barrens
    KITT_ACTION_TELEPORT_55 = 171,   // Stonetalon Mountains
    KITT_ACTION_TELEPORT_56 = 172,   // Ashenvale
    KITT_ACTION_TELEPORT_57 = 173,   // Thousand Needles
    KITT_ACTION_TELEPORT_58 = 174,   // Desolace
    KITT_ACTION_TELEPORT_59 = 175,   // Dustwallow Marsh
    KITT_ACTION_TELEPORT_60 = 176,   // Feralas
    KITT_ACTION_TELEPORT_61 = 177,   // Tanaris
    KITT_ACTION_TELEPORT_62 = 178,   // Azshara
    KITT_ACTION_TELEPORT_63 = 179,   // Felwood
    KITT_ACTION_TELEPORT_64 = 180,   // Un\'Goro Crater
    KITT_ACTION_TELEPORT_65 = 181,   // Silithus
    KITT_ACTION_TELEPORT_66 = 182,   // Winterspring
    // sub menu Outland 67
    KITT_ACTION_TELEPORT_14 = 183,   // tele Shattrath
    KITT_ACTION_TELEPORT_67 = 184,   // Hellfire Peninsula
    KITT_ACTION_TELEPORT_68 = 185,   // Zangarmarsh
    KITT_ACTION_TELEPORT_69 = 186,   // Terokkar Forest
    KITT_ACTION_TELEPORT_70 = 187,   // Nagrand
    KITT_ACTION_TELEPORT_71 = 188,   // Blade\'s Edge Mountains
    KITT_ACTION_TELEPORT_72 = 189,   // Netherstorm
    KITT_ACTION_TELEPORT_73 = 190,   // Shadowmoon Valley
    // sub menu Northrend 74
    KITT_ACTION_TELEPORT_15 = 191,   // tele Dalaran
    KITT_ACTION_TELEPORT_75 = 192,   // Borean Tundra
    KITT_ACTION_TELEPORT_76 = 193,   // Howling Fjord
    KITT_ACTION_TELEPORT_77 = 194,   // Dragonblight
    KITT_ACTION_TELEPORT_78 = 195,   // Grizzly Hills
    KITT_ACTION_TELEPORT_79 = 196,   // Zul\'Drak
    KITT_ACTION_TELEPORT_80 = 197,   // Sholazar Basin
    KITT_ACTION_TELEPORT_81 = 198,   // Crystalsong Forest
    KITT_ACTION_TELEPORT_82 = 199,   // Storm Peaks
    KITT_ACTION_TELEPORT_83 = 200,   // Icecrown
    KITT_ACTION_TELEPORT_84 = 201,   // Wintergrasp
    // sub menu Classic Dungeons 85
    KITT_ACTION_TELEPORT_85 = 202,   // Gnomeregan
    KITT_ACTION_TELEPORT_86 = 203,   // The Deadmines
    KITT_ACTION_TELEPORT_87 = 204,   // The Stockade
    KITT_ACTION_TELEPORT_88 = 205,   // Ragefire Chasm
    KITT_ACTION_TELEPORT_89 = 206,   // Razorfen Downs
    KITT_ACTION_TELEPORT_90 = 207,   // Razorfen Kraul
    KITT_ACTION_TELEPORT_91 = 208,   // Scarlet Monastery
    KITT_ACTION_TELEPORT_92 = 209,   // Shadowfang Keep
    KITT_ACTION_TELEPORT_93 = 210,   // Wailing Caverns
    KITT_ACTION_TELEPORT_94 = 211,   // Blackfathom Deeps
    KITT_ACTION_TELEPORT_95 = 212,   // Blackrock Depths
    KITT_ACTION_TELEPORT_96 = 213,   // Blackrock Spire
    KITT_ACTION_TELEPORT_97 = 214,   // Dire Maul
    KITT_ACTION_TELEPORT_98 = 215,   // Maraudon
    KITT_ACTION_TELEPORT_99 = 216,   // Scholomance
    KITT_ACTION_TELEPORT_100 = 217,   // Stratholme
    KITT_ACTION_TELEPORT_101 = 218,   // Sunken Temple
    KITT_ACTION_TELEPORT_102 = 219,   // Uldaman
    KITT_ACTION_TELEPORT_103 = 220,   // Zul\'Farrak
    // sub menu BC Dungeons 104
    KITT_ACTION_TELEPORT_104 = 221,   // Auchindoun
    KITT_ACTION_TELEPORT_105 = 222,   // The Caverns of Time
    KITT_ACTION_TELEPORT_106 = 223,   // Coilfang Reservoir
    KITT_ACTION_TELEPORT_107 = 224,   // Hellfire Citadel
    KITT_ACTION_TELEPORT_108 = 225,   // Magisters\' Terrace
    KITT_ACTION_TELEPORT_109 = 226,   // Tempest Keep
    // sub menu Wrath Dungeons 110
    KITT_ACTION_TELEPORT_110 = 227,   // Azjol-Nerub
    KITT_ACTION_TELEPORT_111 = 228,   // The Culling of Stratholme
    KITT_ACTION_TELEPORT_112 = 229,   // Trial of the Champion
    KITT_ACTION_TELEPORT_113 = 230,   // Drak\'Tharon Keep
    KITT_ACTION_TELEPORT_114 = 231,   // Gundrak
    KITT_ACTION_TELEPORT_115 = 232,   // Icecrown Citadel Dungeons
    KITT_ACTION_TELEPORT_116 = 233,   // The Nexus Dungeons
    KITT_ACTION_TELEPORT_117 = 234,   // The Violet Hold
    KITT_ACTION_TELEPORT_118 = 235,   // Halls of Lightning
    KITT_ACTION_TELEPORT_119 = 236,   // Halls of Stone
    KITT_ACTION_TELEPORT_120 = 237,   // Utgarde Keep
    KITT_ACTION_TELEPORT_121 = 238,   // Utgarde Pinnacle
    // sub menu Raid Teleports 122
    KITT_ACTION_TELEPORT_122 = 250,   // Black Temple
    KITT_ACTION_TELEPORT_123 = 251,   // Blackwing Lair
    KITT_ACTION_TELEPORT_124 = 252,   // Hyjal Summit
    KITT_ACTION_TELEPORT_125 = 253,   // Serpentshrine Cavern
    KITT_ACTION_TELEPORT_126 = 254,   // Trial of the Crusader
    KITT_ACTION_TELEPORT_127 = 255,   // Gruul\'s Lair
    KITT_ACTION_TELEPORT_128 = 256,   // Magtheridon\'s Lair
    KITT_ACTION_TELEPORT_129 = 257,   // Icecrown Citadel
    KITT_ACTION_TELEPORT_130 = 258,   // Karazhan
    KITT_ACTION_TELEPORT_131 = 259,   // Molten Core
    KITT_ACTION_TELEPORT_132 = 260,   // Naxxramas
    KITT_ACTION_TELEPORT_133 = 261,   // Onyxia\'s Lair
    KITT_ACTION_TELEPORT_134 = 262,   // Ruins of Ahn\'Qiraj
    KITT_ACTION_TELEPORT_135 = 263,   // Sunwell Plateau
    KITT_ACTION_TELEPORT_136 = 264,   // The Eye
    KITT_ACTION_TELEPORT_137 = 265,   // Temple of Ahn\'Qiraj
    KITT_ACTION_TELEPORT_138 = 266,   // The Eye of Eternity
    KITT_ACTION_TELEPORT_139 = 267,   // The Obsidian Sanctum
    KITT_ACTION_TELEPORT_140 = 268,   // Ulduar
    KITT_ACTION_TELEPORT_141 = 269,   // Vault of Archavon
    KITT_ACTION_TELEPORT_142 = 270,   // Zul\'Gurub
    KITT_ACTION_TELEPORT_143 = 271,   // Zul\'Aman
    // sub menu fun zone
    KITT_ACTION_TELEPORT_13 = 290,   // tele pvp arena
    KITT_ACTION_TELEPORT_16 = 291,   // Nu Apasa
    KITT_ACTION_DUNGEONS_RESET = 292,   // Dungeons Heroic reset CD
    KITT_ACTION_RAID_10_RESET = 293,   // Raid reset 10 normal
    KITT_ACTION_RAID_25_RESET = 294,   // Raid reset 25 normal
    KITT_ACTION_RAID_10H_RESET = 295,   // Raid reset 10 Heroic
    KITT_ACTION_RAID_25H_RESET = 296,   // Raid reset 25 Heroic
    // main menu
    KITT_ACTION_AH_OPEN = 395,   // AH open
    KITT_ACTION_BANK_OPEN = 396,   // bank open
    KITT_ACTION_MAIL_OPEN = 397,   // mail open
    KITT_ACTION_TELE_ZONE = 398,  // Teleport Zone
    KITT_ACTION_VENDOR_OPEN = 399  //vendor open
};

enum Spells
{
    /*    SPELL_DEATH = 5, */
    SPELL_DEATH = 27255
};

class kitt_vendor_go : public CreatureScript
{
public:
    kitt_vendor_go() : CreatureScript("kitt_vendor_go") {}

    struct kitt_vendor_goAI : public ScriptedAI
    {
        kitt_vendor_goAI(Creature* creature) : ScriptedAI(creature) {}

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

        void JustDied(Unit* killer) override
        {
            // Aici adaugi logica ta de dupa moarte.

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
            // me->MonsterYell("Am fost invins!", LANG_UNIVERSAL, 0);
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


        void AddGossipItemCustom(Player* player, uint32 gossip_option_id, uint32 sender, uint32 action)
        {
            AddGossipItemFor(player, KITT_MENU_ID_TELEPORT, gossip_option_id, sender, static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + action);
        }
        // Main Menu
        bool OnGossipHello(Player* player) override
        {
            player->PlayerTalkClass->ClearMenus();

            AddGossipItemCustom(player, KITT_GOSSIP_TELE_ZONE, KITT_SENDER_MENU_TELE_ZONE, KITT_ACTION_TELE_ZONE);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_1, KITT_SENDER_MENU_NAV_HORDE, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_2, KITT_SENDER_MENU_NAV_ALLIANCE, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_17, KITT_SENDER_EKINGDOMS_NAV, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_18, KITT_SENDER_KALIMDOR_NAV, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_5, KITT_SENDER_MENU_OUTLAND, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_19, KITT_SENDER_NORTHREND_NAV, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_20, KITT_SENDER_C_DUNGEONS_NAV, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_21, KITT_SENDER_BC_DUNGEONS_NAV, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_22, KITT_SENDER_WRATH_DUNGEONS_NAV, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_23, KITT_SENDER_RAID_NAV, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_6, KITT_SENDER_MENU_FUN_ZONE, KITT_ACTION_OPEN_SUBMENU);
            AddGossipItemCustom(player, KITT_GOSSIP_AH_OPEN, KITT_SENDER_AH_NAV, KITT_ACTION_AH_OPEN);
            AddGossipItemCustom(player, KITT_GOSSIP_BANK_OPEN, KITT_SENDER_BANK_NAV, KITT_ACTION_BANK_OPEN);
            AddGossipItemCustom(player, KITT_GOSSIP_MAIL_OPEN, KITT_SENDER_MAIL_NAV, KITT_ACTION_MAIL_OPEN);
            AddGossipItemCustom(player, KITT_GOSSIP_OPTION_VENDOR, KITT_SENDER_VENDOR_NAV, KITT_ACTION_VENDOR_OPEN);
            SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
            // me->Whisper("Du hast nicht gen\303\274gend Votepunkte", LANG_UNIVERSAL, player);
            /*            me->Say("Cu ce te pot ajuta azi, " + player->GetName() + "?", LANG_UNIVERSAL); */
/*            switch (urand(0, 7))
            {
            case 0:
                // Random pentru text OnHello
                me->Say("Cu ce te pot ajuta azi, " + player->GetName() + "?", LANG_UNIVERSAL);
                break;
            case 1:
                me->Say("Salut, " + player->GetName() + "! Ce te aduce pe aici?", LANG_UNIVERSAL);
                break;
            case 2:
                me->Say(player->GetName() + ". Daca mai primim un singur email azi, cred ca o sa-mi iau un al doilea job ca sa le pot citi pe toate.", LANG_UNIVERSAL);
                break;
            case 3:
                me->Say("Bine ai venit, " + player->GetName() + ". Cauti ceva anume?", LANG_UNIVERSAL);
                break;
            case 4:
                me->Say("Cafeaua asta cred ca are nevoie de o cafea. Tu ce zici " + player->GetName() + " ?", LANG_UNIVERSAL);
                break;
            case 5:
                me->Say(player->GetName() + ". Energia mea de azi este sponsorizata de 'abia astept sa ajung acasa'.", LANG_UNIVERSAL);
                break;
            case 6:
                me->Say(player->GetName() + ". Agenda sedintei de azi: 1. Discutii despre sedinta de maine. 2. Cafea.", LANG_UNIVERSAL);
                break;
            case 7:
                me->Say("Bine ai venit, " + player->GetName() + ". Cauti ceva anume?", LANG_UNIVERSAL);
                break;
            }*/
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            uint32 const sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);

            player->PlayerTalkClass->ClearMenus();

            switch (sender)
            {
                // main menu teleport zone
            case KITT_SENDER_MENU_TELE_ZONE:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELE_ZONE)
                {
                    player->TeleportTo(0, -4854.04f, -1004.33f, 453.76f, 0.0f);
                    /*                    me->Say(player->GetName() + " a plecat la Teleport Zone", LANG_UNIVERSAL); */

                }
                break;
            }
            // menu horde
            case KITT_SENDER_MENU_NAV_HORDE:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_3, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_3);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_7, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_7);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_8, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_8);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_9, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_9);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                    me->Say(player->GetName() + ", ce oras doresti sa vizitezi?", LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu alliance
            case KITT_SENDER_MENU_NAV_ALLIANCE:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_4, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_4);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_10, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_10);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_11, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_11);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_12, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_12);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu Eastern Kingdoms
            case KITT_SENDER_EKINGDOMS_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_24, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_24);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_25, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_25);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_26, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_26);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_27, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_27);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_28, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_28);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_29, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_29);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_30, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_30);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_31, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_31);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_32, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_32);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_33, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_33);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_34, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_34);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_35, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_35);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_36, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_36);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_37, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_37);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_38, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_38);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_39, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_39);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_40, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_40);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_41, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_41);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_42, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_42);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_43, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_43);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_44, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_44);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_45, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_45);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_46, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_46);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_47, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_47);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // Kalimdor
            case KITT_SENDER_KALIMDOR_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_48, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_48);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_49, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_49);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_50, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_50);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_51, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_51);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_52, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_52);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_53, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_53);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_54, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_54);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_55, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_55);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_56, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_56);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_57, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_57);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_58, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_58);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_59, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_59);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_60, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_60);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_61, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_61);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_62, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_62);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_63, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_63);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_64, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_64);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_65, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_65);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_66, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_66);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu outland
            case KITT_SENDER_MENU_OUTLAND:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_14, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_14);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_67, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_67);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_68, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_68);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_69, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_69);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_70, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_70);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_71, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_71);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_72, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_72);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_73, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_73);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu Northrend
            case KITT_SENDER_NORTHREND_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_15, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_15);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_75, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_75);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_76, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_76);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_77, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_77);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_78, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_78);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_79, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_79);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_80, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_80);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_81, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_81);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_82, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_82);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_83, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_83);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_84, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_84);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu Classic Dungeons
            case KITT_SENDER_C_DUNGEONS_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_85, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_85);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_86, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_86);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_87, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_87);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_88, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_88);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_89, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_89);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_90, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_90);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_91, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_91);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_92, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_92);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_93, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_93);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_94, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_94);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_95, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_95);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_96, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_96);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_97, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_97);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_98, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_98);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_99, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_99);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_100, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_100);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_101, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_101);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_102, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_102);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_103, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_103);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu BC Dungeons
            case KITT_SENDER_BC_DUNGEONS_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_104, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_104);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_105, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_105);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_106, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_106);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_107, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_107);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_108, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_108);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_109, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_109);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu Wrath Dungeons
            case KITT_SENDER_WRATH_DUNGEONS_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_110, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_110);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_111, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_111);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_112, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_112);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_113, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_113);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_114, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_114);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_115, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_115);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_116, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_116);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_117, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_117);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_118, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_118);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_119, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_119);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_120, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_120);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_121, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_121);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu Raid Teleports
            case KITT_SENDER_RAID_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_122, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_122);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_123, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_123);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_124, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_124);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_125, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_125);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_126, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_126);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_127, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_127);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_128, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_128);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_129, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_129);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_130, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_130);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_131, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_131);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_132, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_132);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_133, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_133);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_134, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_134);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_135, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_135);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_136, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_136);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_137, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_137);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_138, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_138);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_139, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_139);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_140, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_140);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_141, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_141);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_142, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_142);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_143, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_143);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // menu fun zone
            case KITT_SENDER_MENU_FUN_ZONE:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_13, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_13);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_16, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_TELEPORT_16);
                    AddGossipItemCustom(player, KITT_GOSSIP_INSTANCE_RESET, KITT_SENDER_MENU_INSTANCE_RESET, KITT_ACTION_OPEN_SUBMENU);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_HELLO, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            // Instance Reset submenu in fun zone
            case KITT_SENDER_MENU_INSTANCE_RESET:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU)
                {
                    AddGossipItemCustom(player, KITT_GOSSIP_DUNGEONS_RESET, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_DUNGEONS_RESET);
                    AddGossipItemCustom(player, KITT_GOSSIP_RAID_10_RESET, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_RAID_10_RESET);
                    AddGossipItemCustom(player, KITT_GOSSIP_RAID_25_RESET, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_RAID_25_RESET);
                    AddGossipItemCustom(player, KITT_GOSSIP_RAID_10H_RESET, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_RAID_10H_RESET);
                    AddGossipItemCustom(player, KITT_GOSSIP_RAID_25H_RESET, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_RAID_25H_RESET);
                    AddGossipItemCustom(player, KITT_GOSSIP_OPTION_BACK, KITT_SENDER_MAIN_MENU_NAV, KITT_ACTION_OPEN_SUBMENU); // Inapoi la Hello
                    SendGossipMenuFor(player, KITT_NPC_TEXT_INSTANCE, me->GetGUID());
                    /*                   me->Say("Unde vrei sa mergi? " + player->GetName(), LANG_UNIVERSAL); */
                    return false;
                }
                break;
            }
            case KITT_SENDER_AH_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_AH_OPEN)
                {
                    player->GetSession()->SendAuctionHello(me->GetGUID(), me);
                    CloseGossipMenuFor(player);
                    return false;
                }
                break;
            }
            case KITT_SENDER_BANK_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_BANK_OPEN)
                {
                    player->GetSession()->SendShowBank(me->GetGUID());
                    CloseGossipMenuFor(player);
                    return false;
                }
                break;
            }
            case KITT_SENDER_MAIL_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_MAIL_OPEN)
                {
                    player->GetSession()->SendShowMailBox(me->GetGUID());

                    // Nu inchide meniul de gossip aici, deoarece interfata de mail va aparea deasupra
                    // CloseGossipMenuFor(player); 

                    return false; // Nu procesa mai departe in switch/case-ul principal daca ai return aici
                }
                break;
            }
            case KITT_SENDER_VENDOR_NAV:
            {
                if (action == static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_VENDOR_OPEN)
                {
                    player->GetSession()->SendListInventory(me->GetGUID());
                    CloseGossipMenuFor(player);
                    return false;
                }
                break;
            }
            case KITT_SENDER_MAIN_MENU_NAV:
            default:
            {
                switch (action)
                {
                    // teleport horde
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_3:
                    if (player->GetLevel() >= 1)
                    {
                        if (player->GetTeam() == ALLIANCE)
                        {
                            //Orgrimmar out //17609
                            player->TeleportTo(1, 1107.23f, -4360.45f, 26.2786f, 6.05629f);
                        }
                        else if (player->GetTeam() == HORDE)
                        {
                            //Orgrimmar in //17609
                            player->TeleportTo(1, 1493.17f, -4414.95f, 23.0400f, 0.0f);
                        }
                    }
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_7:
                    player->TeleportTo(0, 1572.00f, 213.637f, -43.1031f, 0.853f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_8:
                    player->TeleportTo(1, -1386.75f, 138.474f, 23.0348f, 0.0f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_9:
                    player->TeleportTo(530, 9380.18f, -7277.81f, 14.2404f, 0.0f);
                    break;
                    // teleport alliance
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_4:
                    if (player->GetLevel() >= 1)
                    {
                        if (player->GetTeam() == ALLIANCE)
                        {
                            //Stormwind in //17334
                            player->TeleportTo(0, -8837.57f, 637.394f, 94.8055f, 4.96371f);
                        }
                        else if (player->GetTeam() == HORDE)
                        {
                            //Stormwind out //17334
                            player->TeleportTo(0, -9121.74f, 286.842f, 100.404f, 1.33343f);
                        }
                    }
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_10:
                    player->TeleportTo(1, 9946.02f, 2588.77f, 1316.19f, 0.0f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_11:
                    player->TeleportTo(0, -5002.33f, -857.664f, 497.054f, 0.0f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_12:
                    player->TeleportTo(530, -4004.39f, -11878.5f, -1.01131f, 0.0f);
                    break;
                    //Eastern Kingdoms
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_24:
                    player->TeleportTo(0, -9449.06f, 64.8392f, 56.3581f, 3.07047f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_25:
                    player->TeleportTo(530, 9024.37f, -6682.55f, 16.8973f, 3.14131f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_26:
                    player->TeleportTo(0, -5603.76f, -482.704f, 396.98f, 5.23499f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_27:
                    player->TeleportTo(0, 2274.95f, 323.918f, 34.1137f, 4.24367f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_28:
                    player->TeleportTo(530, 7595.73f, -6819.6f, 84.3718f, 2.56561f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_29:
                    player->TeleportTo(0, -5405.85f, -2894.15f, 341.972f, 5.48238f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_30:
                    player->TeleportTo(0, 505.126f, 1504.63f, 124.808f, 1.77987f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_31:
                    player->TeleportTo(0, -10684.9f, 1033.63f, 32.5389f, 6.07384f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_32:
                    player->TeleportTo(0, -9447.8f, -2270.85f, 71.8224f, 0.283853f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_33:
                    player->TeleportTo(0, -10531.7f, -1281.91f, 38.8647f, 1.56959f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_34:
                    player->TeleportTo(0, -385.805f, -787.954f, 54.6655f, 1.03926f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_35:
                    player->TeleportTo(0, -3517.75f, -913.401f, 8.86625f, 2.60705f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_36:
                    player->TeleportTo(0, 275.049f, -652.044f, 130.296f, 0.502032f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_37:
                    player->TeleportTo(0, -1581.45f, -2704.06f, 35.4168f, 0.490373f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_38:
                    player->TeleportTo(0, -11921.7f, -59.544f, 39.7262f, 3.73574f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_39:
                    player->TeleportTo(0, -6782.56f, -3128.14f, 240.48f, 5.65912f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_40:
                    player->TeleportTo(0, -10368.6f, -2731.3f, 21.6537f, 5.29238f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_41:
                    player->TeleportTo(0, 112.406f, -3929.74f, 136.358f, 0.981903f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_42:
                    player->TeleportTo(0, -6686.33f, -1198.55f, 240.027f, 0.916887f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_43:
                    player->TeleportTo(0, -11184.7f, -3019.31f, 7.29238f, 3.20542f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_44:
                    player->TeleportTo(0, -7979.78f, -2105.72f, 127.919f, 5.10148f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_45:
                    player->TeleportTo(0, 1743.69f, -1723.86f, 59.6648f, 5.23722f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_46:
                    player->TeleportTo(0, 2280.64f, -5275.05f, 82.0166f, 4.7479f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_47:
                    player->TeleportTo(530, 12806.5f, -6911.11f, 41.1156f, 2.22935f);
                    break;
                    // Kalimdor
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_48:
                    player->TeleportTo(530, -4192.62f, -12576.7f, 36.7598f, 1.62813f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_49:
                    player->TeleportTo(1, 9889.03f, 915.869f, 1307.43f, 1.9336f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_50:
                    player->TeleportTo(1, 228.978f, -4741.87f, 10.1027f, 0.416883f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_51:
                    player->TeleportTo(1, -2473.87f, -501.225f, -9.42465f, 0.6525f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_52:
                    player->TeleportTo(530, -2095.7f, -11841.1f, 51.1557f, 6.19288f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_53:
                    player->TeleportTo(1, 6463.25f, 683.986f, 8.92792f, 4.33534f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_54:
                    player->TeleportTo(1, -575.772f, -2652.45f, 95.6384f, 0.006469f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_55:
                    player->TeleportTo(1, 1574.89f, 1031.57f, 137.442f, 3.8013f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_56:
                    player->TeleportTo(1, 1919.77f, -2169.68f, 94.6729f, 6.14177f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_57:
                    player->TeleportTo(1, -5375.53f, -2509.2f, -40.432f, 2.41885f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_58:
                    player->TeleportTo(1, -656.056f, 1510.12f, 88.3746f, 3.29553f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_59:
                    player->TeleportTo(1, -3350.12f, -3064.85f, 33.0364f, 5.12666f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_60:
                    player->TeleportTo(1, -4808.31f, 1040.51f, 103.769f, 2.90655f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_61:
                    player->TeleportTo(1, -6940.91f, -3725.7f, 48.9381f, 3.11174f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_62:
                    player->TeleportTo(1, 3117.12f, -4387.97f, 91.9059f, 5.49897f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_63:
                    player->TeleportTo(1, 3898.8f, -1283.33f, 220.519f, 6.24307f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_64:
                    player->TeleportTo(1, -6291.55f, -1158.62f, -258.138f, 0.457099f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_65:
                    player->TeleportTo(1, -6815.25f, 730.015f, 40.9483f, 2.39066f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_66:
                    player->TeleportTo(1, 6658.57f, -4553.48f, 718.019f, 5.18088f);
                    break;

                    // Outland
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_14:
                    player->TeleportTo(530, -1812.93f, 5452.93f, 2.68198f, 3.5534f);
/*                    if (player->GetLevel() >= 60)
                    {
                        if (player->GetTeam() == ALLIANCE)
                        {
                            player->TeleportTo(530, -1812.93f, 5452.93f, 2.68198f, 3.5534f);
                        }
                        else if (player->GetTeam() == HORDE)
                        {
                            player->TeleportTo(530, -1913.26f, 5404.86f, 2.74473f, 0.0f);
                        }
                    }
                    else
                    {
                        me->Say(player->GetName() + ", ai nevoie de minim level 60 pentru Shattrath", LANG_UNIVERSAL);
                    } */
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_67:
                    player->TeleportTo(530, -207.335f, 2035.92f, 96.464f, 1.59676f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_68:
                    player->TeleportTo(530, -220.297f, 5378.58f, 23.3223f, 1.61718f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_69:
                    player->TeleportTo(530, -2266.23f, 4244.73f, 1.47728f, 3.68426f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_70:
                    player->TeleportTo(530, -1610.85f, 7733.62f, -17.2773f, 1.33522f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_71:
                    player->TeleportTo(530, 2029.75f, 6232.07f, 133.495f, 1.30395f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_72:
                    player->TeleportTo(530, 3271.2f, 3811.61f, 143.153f, 3.44101f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_73:
                    player->TeleportTo(530, -3681.01f, 2350.76f, 76.587f, 4.25995f);
                    break;

                    // Northrend
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_15:
                    player->TeleportTo(571, 5804.15f, 624.771f, 647.767f, 0.0f);
/*                    if (player->GetLevel() >= 70)
                    {
                        player->TeleportTo(571, 5804.15f, 624.771f, 647.767f, 0.0f);
                    }
                    else
                    {
                        me->Say(player->GetName() + ", ai nevoie de minim level 70 pentru Dalaran", LANG_UNIVERSAL);
                    } */
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_75:
                    player->TeleportTo(571, 2954.24f, 5379.13f, 60.4538f, 2.55544f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_76:
                    player->TeleportTo(571, 682.848f, -3978.3f, 230.161f, 1.54207f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_77:
                    player->TeleportTo(571, 2678.17f, 891.826f, 4.37494f, 0.101121f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_78:
                    player->TeleportTo(571, 4017.35f, -3404.22f, 289.925f, 5.35431f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_79:
                    player->TeleportTo(571, 5560.23f, -3211.66f, 371.709f, 5.55055f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_80:
                    player->TeleportTo(571, 5614.67f, 5818.86f, -69.722f, 3.60807f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_81:
                    player->TeleportTo(571, 5411.17f, -966.37f, 167.082f, 1.57167f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_82:
                    player->TeleportTo(571, 6120.46f, -1013.89f, 408.39f, 5.12322f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_83:
                    player->TeleportTo(571, 8323.28f, 2763.5f, 655.093f, 2.87223f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_84:
                    player->TeleportTo(571, 4522.23f, 2828.01f, 389.975f, 0.215009f);
                    break;
                    // Classic Dungeons
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_85:
                    player->TeleportTo(0, -5163.54f, 925.423f, 257.181f, 1.57423f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_86:
                    player->TeleportTo(0, -11209.6f, 1666.54f, 24.6974f, 1.42053f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_87:
                    player->TeleportTo(0, -8799.15f, 832.718f, 97.6348f, 6.04085f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_88:
                    player->TeleportTo(1, 1811.78f, -4410.5f, -18.4704f, 5.20165f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_89:
                    player->TeleportTo(1, -4657.3f, -2519.35f, 81.0529f, 4.54808f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_90:
                    player->TeleportTo(1, -4470.28f, -1677.77f, 81.3925f, 1.16302f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_91:
                    player->TeleportTo(0, 2873.15f, -764.523f, 160.332f, 5.10447f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_92:
                    player->TeleportTo(0, -234.675f, 1561.63f, 76.8921f, 1.24031f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_93:
                    player->TeleportTo(1, -731.607f, -2218.39f, 17.0281f, 2.78486f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_94:
                    player->TeleportTo(1, 4249.99f, 740.102f, -25.671f, 1.34062f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_95:
                    player->TeleportTo(0, -7179.34f, -921.212f, 165.821f, 5.09599f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_96:
                    player->TeleportTo(0, -7527.05f, -1226.77f, 285.732f, 5.29626f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_97:
                    player->TeleportTo(1, -3520.14f, 1119.38f, 161.025f, 4.70454f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_98:
                    player->TeleportTo(1, -1421.42f, 2907.83f, 137.415f, 1.70718f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_99:
                    player->TeleportTo(0, 1269.64f, -2556.21f, 93.6088f, 0.620623f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_100:
                    player->TeleportTo(0, 3352.92f, -3379.03f, 144.782f, 6.25978f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_101:
                    player->TeleportTo(0, -10177.9f, -3994.9f, -111.239f, 6.01885f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_102:
                    player->TeleportTo(0, -6071.37f, -2955.16f, 209.782f, 0.015708f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_103:
                    player->TeleportTo(1, -6801.19f, -2893.02f, 9.00388f, 0.158639f);
                    break;
                    // BC Dungeons
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_104:
                    player->TeleportTo(530, -3324.49f, 4943.45f, -101.239f, 4.63901f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_105:
                    player->TeleportTo(1, -8369.65f, -4253.11f, -204.272f, -2.70526f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_106:
                    player->TeleportTo(530, 738.865f, 6865.77f, -69.4659f, 6.27655f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_107:
                    player->TeleportTo(530, -347.29f, 3089.82f, 21.394f, 5.68114f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_108:
                    player->TeleportTo(530, 12884.6f, -7317.69f, 65.5023f, 4.799f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_109:
                    player->TeleportTo(530, 3100.48f, 1536.49f, 190.3f, 4.62226f);
                    break;
                    // Wrath Dungeons
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_110:
                    player->TeleportTo(571, 3707.86f, 2150.23f, 36.76f, 3.22f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_111:
                    player->TeleportTo(1, -8756.39f, -4440.68f, -199.489f, 4.66289f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_112:
                    player->TeleportTo(571, 8590.95f, 791.792f, 558.235f, 3.13127f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_113:
                    player->TeleportTo(571, 4765.59f, -2038.24f, 229.363f, 0.887627f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_114:
                    player->TeleportTo(571, 6722.44f, -4640.67f, 450.632f, 3.91123f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_115:
                    player->TeleportTo(571, 5643.16f, 2028.81f, 798.274f, 4.60242f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_116:
                    player->TeleportTo(571, 3782.89f, 6965.23f, 105.088f, 6.14194f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_117:
                    player->TeleportTo(571, 5693.08f, 502.588f, 652.672f, 4.0229f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_118:
                    player->TeleportTo(571, 9136.52f, -1311.81f, 1066.29f, 5.19113f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_119:
                    player->TeleportTo(571, 8922.12f, -1009.16f, 1039.56f, 1.57044f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_120:
                    player->TeleportTo(571, 1203.41f, -4868.59f, 41.2486f, 0.283237f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_121:
                    player->TeleportTo(571, 1267.24f, -4857.3f, 215.764f, 3.22768f);
                    break;
                    // Raid
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_122:
                    player->TeleportTo(530, -3649.92f, 317.469f, 35.2827f, 2.94285f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_123:
                    player->TeleportTo(229, 152.451f, -474.881f, 116.84f, 0.001073f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_124:
                    player->TeleportTo(1, -8177.89f, -4181.23f, -167.552f, 0.913338f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_125:
                    player->TeleportTo(530, 797.855f, 6865.77f, -65.4165f, 0.005938f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_126:
                    player->TeleportTo(571, 8515.61f, 714.153f, 558.248f, 1.57753f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_127:
                    player->TeleportTo(530, 3530.06f, 5104.08f, 3.50861f, 5.51117f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_128:
                    player->TeleportTo(530, -336.411f, 3130.46f, -102.928f, 5.20322f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_129:
                    player->TeleportTo(571, 5855.22f, 2102.03f, 635.991f, 3.57899f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_130:
                    player->TeleportTo(0, -11118.9f, -2010.33f, 47.0819f, 0.649895f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_131:
                    player->TeleportTo(230, 1126.64f, -459.94f, -102.535f, 3.46095f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_132:
                    player->TeleportTo(571, 3668.72f, -1262.46f, 243.622f, 4.785f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_133:
                    player->TeleportTo(1, -4708.27f, -3727.64f, 54.5589f, 3.72786f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_134:
                    player->TeleportTo(1, -8409.82f, 1499.06f, 27.7179f, 2.51868f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_135:
                    player->TeleportTo(530, 12574.1f, -6774.81f, 15.0904f, 3.13788f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_136:
                    player->TeleportTo(530, 3088.49f, 1381.57f, 184.863f, 4.61973f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_137:
                    player->TeleportTo(1, -8240.09f, 1991.32f, 129.072f, 0.941603f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_138:
                    player->TeleportTo(571, 3784.17f, 7028.84f, 161.258f, 5.79993f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_139:
                    player->TeleportTo(571, 3472.43f, 264.923f, -120.146f, 3.27923f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_140:
                    player->TeleportTo(571, 9222.88f, -1113.59f, 1216.12f, 6.27549f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_141:
                    player->TeleportTo(571, 5453.72f, 2840.79f, 421.28f, 0.0f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_142:
                    player->TeleportTo(0, -11916.7f, -1215.72f, 92.289f, 4.72454f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_143:
                    player->TeleportTo(530, 6851.78f, -7972.57f, 179.242f, 4.64691f);
                    break;
                    // fun zone
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_13:
                    player->TeleportTo(0, -13234.2f, 215.180f, 31.4339f, 1.022f);
                    break;
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_TELEPORT_16:
                    if (!player->HasEnoughMoney(15000000))
                    {
                        player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                        me->Say(player->GetName() + ", ai nevoie de 1500g.", LANG_UNIVERSAL, 0);
                        CloseGossipMenuFor(player);
                    }
                    else
                    {
                        player->ModifyMoney(-15000000);
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
                        /*                        me->GetMotionMaster()->MoveChase(me->GetVictim());
                        me->SetFaction(14);*/

                    }
                    break;
                    // Instance Reset CD
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_DUNGEONS_RESET:
                {
                    // difficulty raid 0 = 10N  1 = 25N  2 = 10H  3 = 25H  dungeons 1 = 5hc
                    QueryResult result = CharacterDatabase.PQuery("SELECT `guid` FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 1)", player->GetGUID().GetCounter());

                    if (!result/* || !result->GetRowCount() == 0 */)
                    {
/*                        me->Whisper("Nu ai nicio instanta (Dungeons Heroic) blocata (cu cooldown) pe care sa o resetezi.", LANG_UNIVERSAL, player); */
                        std::string message = "|cffff0000!...|r Nu ai nicio instanta (Dungeons Heroic) blocata (cu cooldown) pe care sa o resetezi.";
                        ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        CloseGossipMenuFor(player);
                        break;
                    }
                    if (!player->HasEnoughMoney(300000))
                    {
                        player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                        me->Whisper(player->GetName() + ", ai nevoie de 30g pentru a reseta aceste instante (Dungeons Heroic)!", LANG_UNIVERSAL, player);
                        CloseGossipMenuFor(player);
                        break;
                    }
                    else
                    {
                        player->ModifyMoney(-300000);
                        // 10 N
                        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 1)", player->GetGUID().GetCounter());
                        CloseGossipMenuFor(player);
                        /*me->Whisper("Instance reset: Dungeons Heroic (Necesita re-log), leave party/raid and re-log.", LANG_UNIVERSAL, player); */
                        std::string message = "|cffff0000ATENTIE|r Instance reset: Dungeons Heroic (Necesita re-log), leave party/raid and re-log.";
                        ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                    }
                    break;
                }
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_RAID_10_RESET:
                {
                    // difficulty raid 0 = 10N  1 = 25N  2 = 10H  3 = 25H  dungeons 1 = 5hc
                    QueryResult result = CharacterDatabase.PQuery("SELECT `guid` FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 0)", player->GetGUID().GetCounter());

                    if (!result/* || !result->GetRowCount() == 0 */)
                    {
/*                        me->Whisper("Nu ai nicio instanta (10-N) blocata (cu cooldown) pe care sa o resetezi.", LANG_UNIVERSAL, player); */
                        std::string message = "|cffff0000!...|r Nu ai nicio instanta (10-N) blocata (cu cooldown) pe care sa o resetezi.";
                        ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        CloseGossipMenuFor(player);
                        break;
                    }
                        if (!player->HasEnoughMoney(1000000))
                        {
                            player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                            me->Whisper(player->GetName() + ", ai nevoie de 100g pentru a reseta aceste instante (10-N)!", LANG_UNIVERSAL, player);
                            CloseGossipMenuFor(player);
                            break;
                        }
                        else
                        {
                            player->ModifyMoney(-1000000);
                            // 10 N
                            CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 0)", player->GetGUID().GetCounter());
                            CloseGossipMenuFor(player);
/*                            me->Whisper("Instance reset: 10-N (Necesita re-log), leave party/raid and re-log.", LANG_UNIVERSAL, player); */
                            std::string message = "|cffff0000Atentie|r Instance reset: 10-N (Necesita re-log), leave party/raid and re-log.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }
                    break;
                }
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_RAID_25_RESET:
                {
                    // difficulty raid 0 = 10N  1 = 25N  2 = 10H  3 = 25H  dungeons 1 = 5hc
                    QueryResult result = CharacterDatabase.PQuery("SELECT `guid` FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 1)", player->GetGUID().GetCounter());

                    if (!result/* || !result->GetRowCount() == 0 */)
                    {
/*                        me->Whisper("Nu ai nicio instanta (25-N) blocata (cu cooldown) pe care sa o resetezi.", LANG_UNIVERSAL, player); */
                        std::string message = "|cffff0000!...|r Nu ai nicio instanta (25-N) blocata (cu cooldown) pe care sa o resetezi.";
                        ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        CloseGossipMenuFor(player);
                        break;
                    }
                        if (!player->HasEnoughMoney(3500000))
                        {
                            player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                            me->Whisper(player->GetName() + ", ai nevoie de 350g pentru a reseta aceste instante (25-N)!", LANG_UNIVERSAL, player);
                            CloseGossipMenuFor(player);
                            break;
                        }
                        else
                        {
                            player->ModifyMoney(-3500000);
                            // 25 N
                            CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 1)", player->GetGUID().GetCounter());
                            CloseGossipMenuFor(player);
/*                            me->Whisper("Instance reset: 25-N (Necesita re-log), leave party/raid and re-log.", LANG_UNIVERSAL, player); */
                            std::string message = "|cffff0000Atentie|r Instance reset: 25-N (Necesita re-log), leave party/raid and re-log.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }
                    break;
                }
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_RAID_10H_RESET:
                {
                    // difficulty raid 0 = 10N  1 = 25N  2 = 10H  3 = 25H  dungeons 1 = 5hc
                    QueryResult result = CharacterDatabase.PQuery("SELECT `guid` FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 2)", player->GetGUID().GetCounter());

                    if (!result/* || !result->GetRowCount() == 0 */)
                    {
/*                        me->Whisper("Nu ai nicio instanta (10-Heroic) blocata (cu cooldown) pe care sa o resetezi.", LANG_UNIVERSAL, player); */
                        std::string message = "|cffff0000!...|r Nu ai nicio instanta (10-Heroic) blocata (cu cooldown) pe care sa o resetezi.";
                        ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        CloseGossipMenuFor(player);
                        break;
                    }
                        if (!player->HasEnoughMoney(2000000))
                        {
                            player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                            me->Whisper(player->GetName() + ", ai nevoie de 200g pentru a reseta aceste instante (10-Heroic)!", LANG_UNIVERSAL, player);
                            CloseGossipMenuFor(player);
                            break;
                        }
                        else
                        {
                            player->ModifyMoney(-2000000);
                            // 10 H
                            CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 2)", player->GetGUID().GetCounter());
                            CloseGossipMenuFor(player);
/*                            me->Whisper("Instance reset: 10-Heroic (Necesita re-log), leave party/raid and re-log.", LANG_UNIVERSAL, player); */
                            std::string message = "|cffff0000Atentie|r Instance reset: 10-Heroic (Necesita re-log), leave party/raid and re-log.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }
                    break;
                }
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_RAID_25H_RESET:
                {
                    // difficulty raid 0 = 10N  1 = 25N  2 = 10H  3 = 25H  dungeons 1 = 5hc
                    QueryResult result = CharacterDatabase.PQuery("SELECT `guid` FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 3)", player->GetGUID().GetCounter());

                    if (!result/* || !result->GetRowCount() == 0 */)
                    {
/*                        me->Whisper("Nu ai nicio instanta (25-Heroic) blocata (cu cooldown) pe care sa o resetezi.", LANG_UNIVERSAL, player); */
                        std::string message = "|cffff0000!...|r Nu ai nicio instanta (25-Heroic) blocata (cu cooldown) pe care sa o resetezi.";
                        ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        CloseGossipMenuFor(player);
                        break;
                    }
                        if (!player->HasEnoughMoney(5000000))
                        {
                            player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, 0, 0, 0);
                            me->Whisper(player->GetName() + ", ai nevoie de 500g pentru a reseta aceste instante (25-Heroic)!", LANG_UNIVERSAL, player);
                            CloseGossipMenuFor(player);
                            break;
                        }
                        else
                        {
                            player->ModifyMoney(-5000000);
                            // 25 H
                            CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = {} AND `instance` IN (SELECT `id` FROM `instance` WHERE `difficulty` = 3)", player->GetGUID().GetCounter());
                            CloseGossipMenuFor(player);
/*                            me->Whisper("Instance reset: 25-Heroic (Necesita re-log), leave party/raid and re-log.", LANG_UNIVERSAL, player); */
                            std::string message = "|cffff0000Atentie|r Instance reset: 25-Heroic (Necesita re-log), leave party/raid and re-log.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }
                    break;
                }
                    // select back
                case static_cast<uint32>(GOSSIP_ACTION_INFO_DEF) + KITT_ACTION_OPEN_SUBMENU:
                    return OnGossipHello(player); // Merge la meniul principal
                default:
                    break;
                }
                CloseGossipMenuFor(player);
                return false;
            }
            }
            CloseGossipMenuFor(player);
            // Random pentru text OnHello
            switch (urand(0, 9))
            {
            case 0:
                me->Say(player->GetName() + " si'a luat talpasita.", LANG_UNIVERSAL);
                break;
            case 1:
                me->Say(player->GetName() + " a disparut ca magaru'n ceata.", LANG_UNIVERSAL);
                break;
            case 2:
                me->Say(player->GetName() + " a plecat sa se bata cu morile de vant.", LANG_UNIVERSAL);
                break;
            case 3:
                me->Say(player->GetName() + " s'a evaporat. Cred ca a fost un vapor de apa.", LANG_UNIVERSAL);
                break;
            case 4:
                me->Say(player->GetName() + " a iesit sa verifice daca e cerul suficient de albastru.", LANG_UNIVERSAL);
                break;
            case 5:
                me->Say("Momentan, " + player->GetName() + " este intr'o misiune ultra-secreta. Va reveni cand misiunea (sau cafeaua) este completa.", LANG_UNIVERSAL);
                break;
            case 6:
                me->Say("Ne pare rau, biroul " + player->GetName() + " este temporar un portal catre o alta dimensiune. Revenim cu detalii logistice.", LANG_UNIVERSAL);
                break;
            case 7:
                me->Say("Conform GPS-ului nostru, " + player->GetName() + " se afla la intersectia dintre 'nu'mi pasa' si 'nu sunt aici'. Va reveni curand!", LANG_UNIVERSAL);
                break;
            case 8:
                me->Say(player->GetName() + " s'a dus sa'si reincarce bateriile... la propriu, cu un incarcator de telefon.", LANG_UNIVERSAL);
                break;
            case 9:
                me->Say(player->GetName() + " a plecat la polul nord sa se bronzeze.", LANG_UNIVERSAL);
                break;
            }
            return false;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new kitt_vendor_goAI(creature);
    }
};

void AddSC_kitt_vendor_go()
{
    new kitt_vendor_go();
}
