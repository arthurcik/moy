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
#include <vector>
#include <algorithm>
#include "Player.h"*/

#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Group.h"
#include "Guild.h"
#include "Channel.h"
#include "Log.h"
//#include <set>
//#include <map>

#include "kitt_chatlog/ChatLog.h"

//static std::set<ObjectGuid> KittWarnedPlayers;
static std::map<ObjectGuid, uint8> KittWarnedCount;

/*static const std::vector<std::string> KittbannedChannels = {
    "crb",
    "ch1",
    "ch2",
    "recount"
};*/

static const std::map<std::string, std::string> KittBannedAddons = {
    {"crb", "Carbonite"},
    {"ch1", "Carbonite Module 1"},
    {"ch2", "Carbonite Module 2"},
    {"recount", "Recount"}
};


class kitt_chat_logs : public PlayerScript
{
public:
    kitt_chat_logs() : PlayerScript("kitt_chat_logs") {}

    const uint32 KITT_LANG_ADDON_WOW = 4294967295;

    void OnLogout(Player* player) override
    {
        //KittWarnedPlayers.erase(player->GetGUID());
        KittWarnedCount.erase(player->GetGUID());
    }

    // pe say /s
    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg) override
    {
        if (!player)
            return;
        if (lang == KITT_LANG_ADDON_WOW)
        {
            ProcessKittAddonBlock(player, msg, "Say");
            return;
        }

        TC_LOG_ERROR("kitt", "player: {} type: {} lang: {} msg: {}", player->GetName(), type, lang, msg);
        sChatLog->ChatMsg(player, msg, type);
    }

    // pe whisp /w
    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* receiver) override
    {
        if (!player || !receiver)
            return;
        if (lang == KITT_LANG_ADDON_WOW)
        {
            ProcessKittAddonBlock(player, msg, "Whisp");
            return;
        }


        TC_LOG_ERROR("kitt", "player: {} type: {} lang: {} msg: {} receiver: {}", player->GetName(), type, lang, msg, receiver->GetName());

        std::string receiverName = receiver->GetName();
        sChatLog->WhisperMsg(player, receiverName, msg);
    }

    // pe grupuri /ra /p /bg
    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Group* group) override
    {
        if (!player || !group)
            return;

        if (lang == KITT_LANG_ADDON_WOW)
        {
            ProcessKittAddonBlock(player, msg, "Group");
            return;
        }

        //TC_LOG_ERROR("kitt", "player: {} type: {} lang: {} msg: {} group: {}", player->GetName(), type, lang, msg, group->GetGroupType());

        if (group->isBGGroup())
        {
            sChatLog->BattleGroundMsg(player, msg, type);
        }
        else if (group->isRaidGroup())
        {
            sChatLog->RaidMsg(player, msg, type);
        }
        else
        {
            sChatLog->PartyMsg(player, msg);
        }
    }

    // pe guilda /g /o
    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Guild* guild) override
    {
        if (!player || !guild)
            return;

        TC_LOG_ERROR("kitt", "[all data] player: {} type: {} lang: {} msg: {} guild: {}", player->GetName(), type, lang, msg, guild->GetName());

        if (lang == KITT_LANG_ADDON_WOW)
        {
            ProcessKittAddonBlock(player, msg, "Guild");
            return;
        }

        TC_LOG_ERROR("kitt", "player: {} type: {} lang: {} msg: {} guild: {}", player->GetName(), type, lang, msg, guild->GetName());

        bool isMaster = (player->GetGuildRank() == 0);
        bool isOfficerChannel = (type == CHAT_MSG_OFFICER);
        sChatLog->GuildMsg(player, msg, isOfficerChannel, isMaster);
    }

    // pe channel
    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Channel* channel) override
    {

        if (!player || !channel)
            return;

        TC_LOG_ERROR("kitt", "[view all data]player: {} type: {} lang: {} msg: {} channel: {}", player->GetName(), type, lang, msg, channel->GetName());

        /*ProcessKittAddonBlock(player, msg, channel->GetName());
        if (msg == "-.-")
        {
            return;
        }*/

        if (lang == KITT_LANG_ADDON_WOW)
        {
            //ProcessKittAddonBlock(player, msg, channel->GetName());
            return;
        }

        std::string KittchannelName = channel->GetName();
        std::string lowerChannelName = KittchannelName;
        std::transform(lowerChannelName.begin(), lowerChannelName.end(), lowerChannelName.begin(), ::tolower);

        //for (const std::string& banned : KittbannedChannels)
        for (auto it = KittBannedAddons.begin(); it != KittBannedAddons.end(); ++it)
        {
            std::string const& prefix = it->first;
            std::string const& fullName = it->second;
            // Verificam daca prefixul se afla la pozitia 0 (inceputul numelui)
            // aici cauta doar de la inceputul frazei
            if (lowerChannelName.compare(0, prefix.length(), prefix) == 0)
            {
                // Verificam daca jucatorul este deja in lista de avertizati
                ObjectGuid guid = player->GetGUID();
                //if (KittWarnedPlayers.find(guid) == KittWarnedPlayers.end())
                uint8& warnCount = KittWarnedCount[guid];

                if (warnCount < 3)
                {
                    warnCount++; // Incrementam numarul de avertismente
                    // Taiem mesajul daca este prea lung (ex: peste 40 caractere) pentru a evita spam-ul vizual
                    std::string shortMsg = msg;
                    if (shortMsg.length() > 100)   // 150 caractere maxim
                        shortMsg = shortMsg.substr(0, 100) + "...";

                    // Trimitem mesajul complet: Nume Addon + Continut blocat
                    ChatHandler(player->GetSession()).PSendSysMessage("SilenceMsgSendToChannel: |cff00ff00%s|r data: |cff888888[%s]|r", KittchannelName.c_str(), shortMsg.c_str());

                    // Trimitem mesajul o singura data
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Sistem]|r Comunicarea addon-ului tau (|cff00ff00%s|r) a fost oprita pentru a preveni lag-ul.", fullName.c_str());

                    // Il adaugam in lista
                    //KittWarnedPlayers.insert(guid);
                }
                msg = "-.-";
                //CheckAndBlock(lang, msg);
                return; // Este addon, nu logam
            }
            // aici cauta cuvintele oriunde in text
            //if (lowerChannelName.find(banned) != std::string::npos)
              //  return; // Daca am gasit un nume interzis, iesim si nu logam
        }
        /*if (lang == KITT_LANG_ADDON_WOW)
            return;*/

        TC_LOG_ERROR("kitt", "[log start]player: {} type: {} lang: {} msg: {} channel: {}", player->GetName(), type, lang, msg, channel->GetName());

        std::string channelName = channel->GetName();
        sChatLog->ChannelMsg(player, channelName, msg);
    }

    // Functie Helper pentru procesarea si blocarea addon-urilor
    void ProcessKittAddonBlock(Player* player, std::string& msg, std::string const& contextName)
    {
        if (msg.empty() || !player)
            return;

        // Pregatim prefixul mesajului pentru verificare (primele 20 caractere, litere mici)
        std::string lowerMsg = msg.substr(0, 20);
        std::transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(), ::tolower);

        // Parcurgem lista de addon-uri interzise (KittBannedAddons trebuie sa fie un std::map)
        for (auto const& [prefix, fullName] : KittBannedAddons)
        {
            if (lowerMsg.compare(0, prefix.length(), prefix) == 0)
            {
                ObjectGuid guid = player->GetGUID();

                // Notificam jucatorul o singura data pe sesiune
                //if (KittWarnedPlayers.find(guid) == KittWarnedPlayers.end())
                uint8& warnCount = KittWarnedCount[guid];

                if (warnCount < 3)
                {
                    warnCount++; // Incrementam numarul de avertismente
                    std::string shortMsg = (msg.length() > 100) ? msg.substr(0, 100) + "..." : msg;

                    ChatHandler(player->GetSession()).PSendSysMessage("SilenceMsg: |cff00ff00%s|r data: |cff888888[%s]|r", contextName.c_str(), shortMsg.c_str());
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Sistem]|r Addon-ul |cff00ff00%s|r a fost restrictionat pe acest server.", fullName.c_str());

                    //KittWarnedPlayers.insert(guid);
                }

                // BLOCAM MESAJUL: Inlocuim continutul cu un cod neutru
                msg = "-.-";
                return;
            }
        }
    }

};

void AddSC_kitt_chat_logs()
{
    new kitt_chat_logs();
}
