/*#include "ScriptMgr.h"
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
#include "Player.h"*/

#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Group.h"
#include "Guild.h"
#include "Channel.h"
#include "Log.h"

class kitt_anti_addons : public PlayerScript
{
public:
    kitt_anti_addons() : PlayerScript("kitt_anti_addons") {}

    const uint32 LANG_ADDON_WOW = 4294967295;

    void CheckAndBlock(uint32 lang, std::string& msg)
    {
        if (lang == LANG_ADDON_WOW)
        {
            TC_LOG_ERROR("kitt", "cautare addon: {} .. {}", msg, lang);

            if (msg.find("Crb") != std::string::npos)
            {
                TC_LOG_ERROR("kitt", "blocare addon: {} .. {}", msg, lang);

                msg = "";
            }
        }
    }

    /*void CheckAndBlock(std::string& msg)
    {
        if (msg.find("QuestHelper") != std::string::npos ||
            msg.find("GearScore") != std::string::npos ||
            msg.find("Carbonite") != std::string::npos)
        {
            msg = "";
        }
    }*/

    bool IsBannedAddon(std::string const& msg)
    {
        if (msg.find("Crb") != std::string::npos)
        {
            return true;
        }
        return false;
    }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg) override
    {
        TC_LOG_ERROR("kitt", "messaj chat: {} .. {}", msg, lang);
        CheckAndBlock(lang, msg);
    }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver) override
    {
        TC_LOG_ERROR("kitt", "messaj whisp: {} .. {}", msg, lang);

        CheckAndBlock(lang, msg);
    }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Group* group) override
    {
        TC_LOG_ERROR("kitt", "messaj grup: {} .. {}", msg, lang);

        CheckAndBlock(lang, msg);
    }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Guild* guild) override
    {
        TC_LOG_ERROR("kitt", "messaj guild: {} .. {}", msg, lang);

        CheckAndBlock(lang, msg);
    }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* channel) override
    {
        TC_LOG_ERROR("kitt", "messaj channel: {} .. {}", msg, lang);

        CheckAndBlock(lang, msg);
    }
};

void AddSC_kitt_anti_addons()
{
    new kitt_anti_addons();
}
