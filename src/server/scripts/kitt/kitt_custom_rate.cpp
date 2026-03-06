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
    static uint32 sKittCustomRateEnabled = 0;
    static std::unordered_map<ObjectGuid, uint32> CustomXPRates;

    static std::map<ObjectGuid, time_t> zKittCustomRateMap;
    static uint32 zKittCustomRateCommandCD = 300; // in secunde anti-flood
}

class kitt_custom_rate : public PlayerScript
{
public:
    kitt_custom_rate() : PlayerScript("kitt_custom_rate") {}

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (sKittCustomRateEnabled == 0)
            return;

        if (player->GetLevel() == 80)
            return;

        ObjectGuid playerGuid = player->GetGUID();

        if (CustomXPRates.find(playerGuid) == CustomXPRates.end())
        {
            CustomXPRates[playerGuid] = 0;
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
        if (sKittCustomRateEnabled == 0)
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

    static void Notify(Player* player)
    {
        if (!player || !player->IsInWorld() || !player->GetSession())
            return;

        uint32 currentRate = CustomXPRates[player->GetGUID()];
        ChatHandler handler(player->GetSession());

        handler.PSendSysMessage("|cff00ccff--- [ XP Rate Status ] ---|r");
        if (currentRate == 0)
        {
            handler.PSendSysMessage("|cffffffffYou are using |cff00ff00Server Rates|r |cffffffff(Xp: x%u).|r",
                (uint32)sWorld->getRate(RATE_XP_KILL));

            //handler.PSendSysMessage("|cffffffffYou are using |cff00ff00Server Rates|r |cffffffff(Kill: x%u, Quest: x%u).|r",
            //    (uint32)sWorld->getRate(RATE_XP_KILL), (uint32)sWorld->getRate(RATE_XP_QUEST));
        }
        else
        {
            handler.PSendSysMessage("|cffffffffYour current |cff00ff00Custom Rate is: x%u|r.|r", (uint32)currentRate);
        }
        handler.PSendSysMessage("|cffffffffYou can change this anytime using |cff00ccff.zxprate <value>|r.");
        handler.PSendSysMessage("|cff00ccffReset:|r |cffffffff.zxprate 0 (to use server settings)|r");


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
            {"zxprate", HandleSetXPRateCommand, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No}
        };
    }

    static bool HandleSetXPRateCommand(ChatHandler* handler, Tail args)
    {
        if (sKittCustomRateEnabled == 0)
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
            kitt_custom_rate::Notify(player);
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

        CharacterDatabase.PExecute("REPLACE INTO character_kitt_custom_rate (player_guid, player_name, xp_rate) VALUES ({}, '{}', {})",
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
};

class kitt_custom_rate_config : public WorldScript
{
public:
    kitt_custom_rate_config() : WorldScript("kitt_custom_rate_config") {}

    void OnConfigLoad(bool /*reload*/) override
    {
        sKittCustomRateEnabled = sConfigMgr->GetIntDefault("Kitt.Custom.Rate", 0);
    }
};

class kitt_custom_rate_startup : public WorldScript
{
public:
    kitt_custom_rate_startup() : WorldScript("kitt_custom_rate_startup") {}

    void OnStartup() override
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
};


void AddSC_kitt_custom_rate()
{
    new kitt_custom_rate_config();
    new kitt_custom_rate_startup();
    new kitt_custom_rate();
    new kitt_custom_rate_command();
}
