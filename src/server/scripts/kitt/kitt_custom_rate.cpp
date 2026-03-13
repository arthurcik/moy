#include "ScriptMgr.h"
#include "World.h"
#include "WorldSession.h"
#include "DatabaseEnv.h"
#include "Chat.h"
#include "GameTime.h"
#include "Config.h"
#include "Log.h"

//#include "Containers.h"
//#include <sstream>
//#include <string>


using namespace Trinity::ChatCommands;

namespace
{
    static uint32 sKittCustomRateXpEnabled = 0;
    static uint32 sKittCustomRateRepEnabled = 0;

    static std::unordered_map<ObjectGuid, uint32> CustomXPRates;
    static std::unordered_map<ObjectGuid, uint32> CustomRepRates;

    static std::map<ObjectGuid, time_t> zKittCustomRateMap; // anti-flood map guid
    static uint32 zKittCustomRateCommandCD = 300; // in secunde anti-flood time wait
}

class kitt_custom_rate : public PlayerScript
{
public:
    kitt_custom_rate() : PlayerScript("kitt_custom_rate") {}

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (sKittCustomRateXpEnabled == 0 && sKittCustomRateRepEnabled == 0)
            return;

        ObjectGuid playerGuid = player->GetGUID();

        if (CustomXPRates.find(playerGuid) == CustomXPRates.end())
        {
            CustomXPRates[playerGuid] = 0;
        }

        if (CustomRepRates.find(playerGuid) == CustomRepRates.end())
        {
            CustomRepRates[playerGuid] = 0;
        }

        player->m_Events.AddEventAtOffset([player]()
            {
                if (!player || !player->IsInWorld() || !player->GetSession())
                    return;

                Notify(player);
            }, 20s);
    }

    void OnLogout(Player* player) override
    {
        //CustomXPRates.erase(player->GetGUID());
        zKittCustomRateMap.erase(player->GetGUID());
    }

    void OnGiveXP(Player* player, uint32& amount, Unit* victim) override
    {
        if (sKittCustomRateXpEnabled == 0)
            return;

        auto it = CustomXPRates.find(player->GetGUID());
        if (it == CustomXPRates.end() || it->second == 0)
            return;

        uint32 customRate = it->second;
        float serverRate = sWorld->getRate(victim ? RATE_XP_KILL : RATE_XP_QUEST);

        if (serverRate > 0.0f)
        {
            amount = uint32((amount / serverRate) * customRate);
        }
    }

    void OnReputationChange(Player* player, uint32 /*factionId*/, int32& standing, bool /*incremental*/) override
    {
        if (sKittCustomRateRepEnabled == 0)
            return;

        auto it = CustomRepRates.find(player->GetGUID());
        if (it == CustomRepRates.end() || it->second == 0)
            return;

        if (standing > 0)
        {
            uint32 customRate = it->second;
            float serverRate = sWorld->getRate(RATE_REPUTATION_GAIN);

            if (serverRate > 0.0f)
            {
                standing = int32((standing / serverRate) * customRate);
            }
        }
    }

    static void Notify(Player* player, bool forceXp = false, bool forceRep = false)
    {
        if (!player || !player->IsInWorld() || !player->GetSession())
            return;

        bool canShowXp = (sKittCustomRateXpEnabled > 0 && player->GetLevel() < 90);
        bool canShowRep = (sKittCustomRateRepEnabled > 0);
        bool showXp = false;
        bool showRep = false;
        if (forceXp) {
            showXp = (sKittCustomRateXpEnabled > 0);
        }
        else if (forceRep) {
            showRep = (sKittCustomRateRepEnabled > 0);
        }
        else {
            showXp = canShowXp;
            showRep = canShowRep;
        }

        if (!showXp && !showRep)
            return;

        uint32 currentXpRate = CustomXPRates[player->GetGUID()];
        uint32 currentRepRate = CustomRepRates[player->GetGUID()];

        ChatHandler handler(player->GetSession());

        handler.PSendSysMessage("|cff00ccff--- [ Rate Status ] ---|r");

        if (showXp)
        {
            if (currentXpRate == 0)
            {
                handler.PSendSysMessage("|cffffffffXP:|r |cff00ff00x%u (Server)|r << >> |cffffffffChange anytime: |cff00ccff.zxprate <1-15>|r", (uint32)sWorld->getRate(RATE_XP_KILL));
                //handler.PSendSysMessage("|cffffffffYou can change this anytime using: |cff00ccff.zxprate <1-15>|r");
            }
            else
            {
                handler.PSendSysMessage("|cffffffffXP:|r |cff00ccffx%u|r (custom) << >> |cffffffffTo reset use: |cff00ccff.zxprate 0|r", currentXpRate);
                //handler.PSendSysMessage("|cffffffffReset: |cff00ccff.zxprate 0|r");
            }
        }

        if (showRep)
        {
            if (currentRepRate == 0)
            {
                handler.PSendSysMessage("|cffffffffRep:|r |cff00ff00x%u (Server)|r << >> |cffffffffChange anytime: |cff00ccff.zreprate <1-3>|r", (uint32)sWorld->getRate(RATE_REPUTATION_GAIN));
                //handler.PSendSysMessage("|cffffffffYou can change this anytime using: |cff00ccff.zreprate <1-3>|r");
            }
            else
            {
                handler.PSendSysMessage("|cffffffffRep:|r |cff00ccffx%u|r (custom) << >> |cffffffffTo reset use: |cff00ccff.zreprate 0|r", currentRepRate);
                //handler.PSendSysMessage("|cffffffffReset: |cff00ccff.zreprate 0|r");
            }
        }

        //handler.PSendSysMessage("|cff444444------------------------------|r");
        //handler.PSendSysMessage("|cffffffffYou can change this anytime using: |cff00ccff.zxprate <1-15>|r |cffffffffor|r |cff00ccff.zreprate <1-3>|r");
        //handler.PSendSysMessage("|cffffffffReset: |cff00ccff.zxprate 0|r |cffffffffsau|r |cff00ccff.zreprate 0|r");


        return;
    }

};

class kitt_custom_rate_command : public CommandScript
{
public:
    kitt_custom_rate_command() : CommandScript("kitt_custom_rate_command") {}

    std::vector<ChatCommandBuilder> GetCommands() const override
    {
        return
        {
            {"zxprate", HandleSetXPRateCommand, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No},
            {"zreprate", HandleSetRepRateCommand, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No}

        };
    }

    static bool HandleSetXPRateCommand(ChatHandler* handler, Tail args)
    {
        if (sKittCustomRateXpEnabled == 0)
        {
            handler->SendSysMessage("|cffff0000Error:|r This system is currently disabled in server config.");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        std::string_view argsStr = args;
        if (args.empty())
        {
            kitt_custom_rate::Notify(player, true, false);
            return true;
        }

        std::string input(argsStr);
        for (char const& c : input)
        {
            if (!isdigit(c))
            {
                handler->SendSysMessage("|cffff0000Error:|r |cffffffffPlease enter a whole number (1, 2, ...).|r");
                return true;
            }
        }

        uint32 newRate = (uint32)atoi(input.c_str());

        if (newRate > 15)
        {
            handler->SendSysMessage("|cffff0000Error:|r |cffffffffMaximum allowed rate is 15.|r");
            return true;
        }

        // --- PROTECTIE FLOOD (Cooldown 300 secunde) ---
        time_t currentTime = GameTime::GetGameTime();

        if (zKittCustomRateMap.count(player->GetGUID()) && currentTime < zKittCustomRateMap[player->GetGUID()])
        {
            uint32 waitTime = zKittCustomRateMap[player->GetGUID()] - currentTime;
            handler->PSendSysMessage("|cffff0000[Anti-Flood]|r Please wait |cffffffff%u|r seconds before changing your rate again.", (uint32)waitTime);
            return true;
        }

        zKittCustomRateMap[player->GetGUID()] = currentTime + zKittCustomRateCommandCD;
        // ----------------------------------------------

        CustomXPRates[player->GetGUID()] = newRate;

        //CharacterDatabase.PExecute("REPLACE INTO character_kitt_custom_rate (player_guid, player_name, xp_rate) VALUES ({}, '{}', {})",
        //    player->GetGUID().GetCounter(), player->GetName().c_str(), newRate);
        CharacterDatabase.PExecute("INSERT INTO character_kitt_custom_rate (player_guid, player_name, xp_rate) VALUES ({}, '{}', {})"
            " ON DUPLICATE KEY UPDATE xp_rate = VALUES(xp_rate), player_name = VALUES(player_name)",
            player->GetGUID().GetCounter(), player->GetName().c_str(), newRate);


        if (newRate == 0)
        {
            float sRateKill = sWorld->getRate(RATE_XP_KILL);
            //float sRateQuest = sWorld->getRate(RATE_XP_QUEST);

            handler->PSendSysMessage("|cff00ccffXP Rate reset.|r |cffffffffUsing server settings:|r");
            handler->PSendSysMessage("- XP: x%u", (uint32)sRateKill);

            //handler->PSendSysMessage("- Kill XP: x%u", (uint32)sRateKill);
            //handler->PSendSysMessage("- Quest XP: x%u", (uint32)sRateQuest);
        }
        else
        {
            handler->PSendSysMessage("|cff00ccffSuccess!|r |cffffffffYour XP rate is now set to: |cff00ff00x%u|r.", (uint32)newRate);
        }

        return true;
    }

    static bool HandleSetRepRateCommand(ChatHandler* handler, Tail args)
    {
        if (sKittCustomRateRepEnabled == 0)
        {
            handler->SendSysMessage("|cffff0000Error:|r This system is currently disabled in server config.");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        std::string_view argsStr = args;
        if (args.empty())
        {
            kitt_custom_rate::Notify(player, false, true);
            return true;
        }

        std::string input(argsStr);
        for (char const& c : input)
        {
            if (!isdigit(c))
            {
                handler->SendSysMessage("|cffff0000Error:|r |cffffffffPlease enter a whole number (1, 2, ...).|r");
                return true;
            }
        }

        uint32 newRate = (uint32)atoi(input.c_str());

        if (newRate > 3)
        {
            handler->SendSysMessage("|cffff0000Error:|r |cffffffffMaximum allowed rate is 3.|r");
            return true;
        }

        // --- PROTECTIE FLOOD (Cooldown 300 secunde) ---
        time_t currentTime = GameTime::GetGameTime();

        if (zKittCustomRateMap.count(player->GetGUID()) && currentTime < zKittCustomRateMap[player->GetGUID()])
        {
            uint32 waitTime = zKittCustomRateMap[player->GetGUID()] - currentTime;
            handler->PSendSysMessage("|cffff0000[Anti-Flood]|r Please wait |cffffffff%u|r seconds before changing your rate again.", (uint32)waitTime);
            return true;
        }

        zKittCustomRateMap[player->GetGUID()] = currentTime + zKittCustomRateCommandCD;
        // ----------------------------------------------

        CustomRepRates[player->GetGUID()] = newRate;

        //CharacterDatabase.PExecute("REPLACE INTO character_kitt_custom_rate (player_guid, player_name, rep_rate) VALUES ({}, '{}', {})",
        //    player->GetGUID().GetCounter(), player->GetName().c_str(), newRate);
        CharacterDatabase.PExecute("INSERT INTO character_kitt_custom_rate (player_guid, player_name, rep_rate) VALUES ({}, '{}', {})"
            " ON DUPLICATE KEY UPDATE rep_rate = VALUES(rep_rate), player_name = VALUES(player_name)",
            player->GetGUID().GetCounter(), player->GetName().c_str(), newRate);


        if (newRate == 0)
        {
            float sRateKill = sWorld->getRate(RATE_REPUTATION_GAIN);
            //float sRateQuest = sWorld->getRate(RATE_XP_QUEST);

            handler->PSendSysMessage("|cff00ccffRep Rate reset.|r |cffffffffUsing server settings:|r");
            handler->PSendSysMessage("- Rep: x%u", (uint32)sRateKill);

            //handler->PSendSysMessage("- Kill XP: x%u", (uint32)sRateKill);
            //handler->PSendSysMessage("- Quest XP: x%u", (uint32)sRateQuest);
        }
        else
        {
            handler->PSendSysMessage("|cff00ccffSuccess!|r |cffffffffYour Rep rate is now set to: |cff00ff00x%u|r.", (uint32)newRate);
        }

        return true;
    }
};

class kitt_custom_rate_config : public WorldScript
{
public:
    kitt_custom_rate_config() : WorldScript("kitt_custom_rate_config") {}

    void OnConfigLoad(bool /*reload*/) override
    {
        sKittCustomRateXpEnabled = sConfigMgr->GetIntDefault("Kitt.Custom.Rate.Xp", 0);
        sKittCustomRateRepEnabled = sConfigMgr->GetIntDefault("Kitt.Custom.Rate.Rep", 0);
    }
};

class kitt_custom_rate_startup : public WorldScript
{
public:
    kitt_custom_rate_startup() : WorldScript("kitt_custom_rate_startup") {}

    void OnStartupOFF() /*override*/
    {
        CustomXPRates.clear();

        //QueryResult result = CharacterDatabase.PQuery("SELECT player_guid, xp_rate FROM character_kitt_custom_rate WHERE xp_rate > 0");
        QueryResult result = CharacterDatabase.PQuery(
            "SELECT c.guid, k.xp_rate "
            "FROM character_kitt_custom_rate k "
            "JOIN characters c ON k.player_guid = c.guid "
            "WHERE k.xp_rate > 0 AND c.level < 80");

        if (!result)
            return;

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt32());
            uint32 newRate = fields[1].GetUInt32();

            CustomXPRates[guid] = newRate;
            count++;
        } while (result->NextRow());

        TC_LOG_INFO("server.loading", ">> KITT [Custom Xp Rate] Loaded {} with custom rate.", count);
    }

    void OnStartup() override
    {
        CustomXPRates.clear();
        CustomRepRates.clear();

        /*QueryResult result = CharacterDatabase.PQuery(
            "SELECT c.guid, k.xp_rate, k.rep_rate "
            "FROM character_kitt_custom_rate k "
            "JOIN characters c ON k.player_guid = c.guid "
            "WHERE (k.xp_rate > 0 AND c.level < 80) OR k.rep_rate > 0");*/
        QueryResult result = CharacterDatabase.PQuery(
            "SELECT c.guid, k.xp_rate, k.rep_rate "
            "FROM character_kitt_custom_rate k "
            "JOIN characters c ON k.player_guid = c.guid "
            "WHERE ((k.xp_rate > 0 AND c.level < 80) OR k.rep_rate > 0) "
            "AND c.logout_time > (UNIX_TIMESTAMP() - (10 * 24 * 60 * 60))");

        if (!result)
            return;

        uint32 countXp = 0;
        uint32 countRep = 0;

        do
        {
            Field* fields = result->Fetch();
            ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt32());

            uint32 xpRate = fields[1].GetUInt32();
            uint32 repRate = fields[2].GetUInt32();

            // Daca are XP setat si e sub lvl 80, punem in mapa de XP
            if (xpRate > 0)
            {
                CustomXPRates[guid] = xpRate;
                countXp++;
            }

            // Daca are Reputatie setata, punem in mapa de Reputatie (indiferent de level)
            if (repRate > 0)
            {
                CustomRepRates[guid] = repRate;
                countRep++;
            }

        } while (result->NextRow());

        TC_LOG_INFO("server.loading", ">> KITT [Custom Rate] Loaded: {} players with Custom XP and {} with Custom Rep (last 10 days).", countXp, countRep);
    }
};


void AddSC_kitt_custom_rate()
{
    new kitt_custom_rate_config();
    new kitt_custom_rate_startup();
    new kitt_custom_rate();
    new kitt_custom_rate_command();
}
