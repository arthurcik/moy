//----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Channel.h"
#include "ChatCommand.h"
#include "ChannelMgr.h"
#include "GameTime.h"
#include "WorldSession.h"
#include <string>
#include <thread>
#include <algorithm>
#include <cctype>
#include "Log.h"
#include "Config.h"
#include <regex>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "World.h"


namespace
{
    std::string TelegramLogPath = "";

    bool TelegramEnable = false;
    std::string TelegramToken = "";
    std::string TelegramChatId = "";
    std::string TelegramChannel = "global";

    uint32 KittNightStart = 23; // Ora 23:00
    uint32 KittNightEnd = 10;   // Ora 10:00


    bool workerRunning = true;
    std::queue<std::string> messageQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;

    std::string KittCleanWoWLinks(std::string text)
    {
        //static const std::regex linkRegex("\\|c[0-9a-fA-F]{8}\\|H[a-zA-Z0-9:]+\\|h\\[([^\\]]+)\\]\\|h\\|r");
        //static const std::regex linkRegex("\\|c[0-9a-fA-F]{8}\\|H[a-zA-Z0-9:/\\+ ]+\\|h\\[([^\\]]+)\\]\\|h\\|r");
        static const std::regex linkRegex("\\|c[a-fA-F0-9]{8}\\|H.*?\\|h\\[([^\\]]+)\\]\\|h\\|r");


        return std::regex_replace(text, linkRegex, "[$1]");
    }

    static void SendToTelegram(std::string message)
    {
        std::string encodedMsg = "";
        for (char c : message) {
            if (isalnum((unsigned char)c))
                encodedMsg += c;
            else if (c == ' ')
                encodedMsg += "%20";
            else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
                encodedMsg += buf;
            }
        }

        std::string command = "curl -s -S -X POST \"https://api.telegram.org/bot" + TelegramToken + "/sendMessage\" -d \"chat_id=" + TelegramChatId + "&text=" + encodedMsg + "\"";

#ifdef _WIN32
        //command += " > NUL";
        //command += " 2>> \"" + TelegramLogPath + "\" > NUL";
        //command += " >> \"" + TelegramLogPath + "\" 2>&1";
        command += " | findstr /C:\"\\\"ok\\\":false\" >> \"" + TelegramLogPath + "\"";
#else
        //command += " > /dev/null 2>&1";
        //command += " 2>> \"" + TelegramLogPath + "\" > /dev/null";
        //command += " >> \"" + TelegramLogPath + "\" 2>&1";
        command += " | grep '\"ok\":false' >> \"" + TelegramLogPath + "\"";
#endif

        system(command.c_str());
        //TC_LOG_ERROR("telegram", "send comand: {}", command.c_str());
    }

    void TelegramWorker()
    {
        while (workerRunning)
        {
            //std::string msg;
            std::string batchMessage = "";
            bool bufferWasFull = false;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCondition.wait(lock, [] { return !messageQueue.empty() || !workerRunning; });

                if (!workerRunning && messageQueue.empty())
                    break;

                batchMessage = "~\n\n";


                //msg = messageQueue.front();
                //messageQueue.pop();
                while (!messageQueue.empty())
                {
                    std::string nextMsg = messageQueue.front();

                    if (batchMessage.length() + nextMsg.length() > 3800)
                    {
                        bufferWasFull = true;
                        break;
                    }

                    batchMessage += nextMsg + "\n\n";
                    messageQueue.pop();
                }
            }

            if (!batchMessage.empty())
            {
                SendToTelegram(batchMessage);
            }

            if (bufferWasFull)
            {
                // 100ms = 10 mesaje pe secunda
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::seconds(30));
            }

            //SendToTelegram(msg);

            // 100ms pauza = max 10 mesaje pe secunda (siguranta totala pentru Telegram)
            //std::this_thread::sleep_for(std::chrono::milliseconds(100));
            //std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }

    struct PlayerSpamInfo
    {
        uint32 count;
        uint32 lastResetTime;
    };
    std::map<ObjectGuid, PlayerSpamInfo> KittTelegramAntiSpamMap;
    std::mutex spamMutex;
    // Valorile pentru Anti-Spam
    uint32 KittSpamTimeMsg = 5;
    uint32 KittSpamContMsg = 2;
    uint32 KittSpamMuteTime = 60;

    bool IsNightTime()
    {
        if (KittNightStart == KittNightEnd)
            return false;

        time_t t = time(nullptr);
        tm* now = localtime(&t);
        uint32 hour = now->tm_hour;

        if (KittNightStart > KittNightEnd)
        {
            return (hour >= KittNightStart || hour < KittNightEnd);
        }
        else
        {
            return (hour >= KittNightStart && hour < KittNightEnd);
        }
    }

}


class KittTelegramChatScript : public PlayerScript
{
public:
    KittTelegramChatScript() : PlayerScript("KittTelegramChatScript") {}

    void OnChat(Player* player, uint32 type, uint32 /*lang*/, std::string& msg, Channel* channel) override
    {
        if (!TelegramEnable || !channel || IsNightTime())
            return;

        //if (msg.find("[T]") != std::string::npos)
        //    return;

        std::string KittchannelName = channel->GetName();
        std::string lowerChannelName = KittchannelName;
        std::transform(lowerChannelName.begin(), lowerChannelName.end(), lowerChannelName.begin(),
            [](unsigned char c) { return std::tolower(c); });


        if (type == CHAT_MSG_CHANNEL && lowerChannelName == TelegramChannel)
        {
            // Anti-spam start --------------------
            uint32 now = GameTime::GetGameTime();
            ObjectGuid playerGuid = player->GetGUID();
            {
                std::lock_guard<std::mutex> lock(spamMutex);
                PlayerSpamInfo& info = KittTelegramAntiSpamMap[playerGuid];

                if (now - info.lastResetTime > KittSpamTimeMsg)
                {
                    info.count = 0;
                    info.lastResetTime = now;
                }

                info.count++;

                if (info.count > KittSpamContMsg)
                {
                    player->GetSession()->m_muteTime = static_cast<time_t>(now) + KittSpamMuteTime;

                    ChatHandler(player->GetSession()).PSendSysMessage("Ai primit Mute %u secunde pentru spam pe canalul %s. (Limita: %u mesaje / %u secunde)",
                        KittSpamMuteTime, lowerChannelName.c_str(), KittSpamContMsg, KittSpamTimeMsg);

                    //TC_LOG_INFO("server", "AntiSpam: Jucatorul %s a primit mute 60s.", player->GetName().c_str());
                    return;
                }
            }
            // Anti-spam end --------------------

            std::string KittcleanedMsg = KittCleanWoWLinks(msg);
            std::string playerName = player->GetName();
            //std::string fullMessage = "<" + KittchannelName + "> [" + playerName + "]: " + KittcleanedMsg;
            std::string fullMessage = "[" + playerName + "]: " + KittcleanedMsg;


            {
                std::lock_guard<std::mutex> lock(queueMutex);
                messageQueue.push(fullMessage);
            }
            queueCondition.notify_one();

            /*std::thread([fullMessage]() {
                SendToTelegram(fullMessage);
                }).detach();*/
        }
    }
};

class KittTelegramChatScript_config : public WorldScript
{
public:
    KittTelegramChatScript_config() : WorldScript("KittTelegramChatScript_config") {}

    void OnStartup() override
    {
        if (TelegramEnable)
        {
            TC_LOG_INFO("server.loading", ">> KITT [Telegram Chat] ACTIVAT.");

            std::thread(TelegramWorker).detach();
        }
        else
        {
            TC_LOG_INFO("server.loading", ">> KITT [Telegram Chat] DEZACTIVAT.");
        }
    }

    void OnConfigLoad(bool /*reload*/) override
    {
        TelegramEnable = sConfigMgr->GetBoolDefault("Kitt.Telegram.Enable", false);
        TelegramToken = sConfigMgr->GetStringDefault("Kitt.Telegram.Token", "");
        TelegramChatId = sConfigMgr->GetStringDefault("Kitt.Telegram.ChatId", "");
        TelegramChannel = sConfigMgr->GetStringDefault("Kitt.Telegram.Channel", "");
        KittSpamTimeMsg = sConfigMgr->GetIntDefault("Kitt.Telegram.SpamTime", 5);
        KittSpamContMsg = sConfigMgr->GetIntDefault("Kitt.Telegram.SpamCount", 3);
        KittSpamMuteTime = sConfigMgr->GetIntDefault("Kitt.Telegram.MuteTime", 60);
        KittNightStart = sConfigMgr->GetIntDefault("Kitt.Telegram.NightStart", 23);
        KittNightEnd = sConfigMgr->GetIntDefault("Kitt.Telegram.NightEnd", 10);


        if (TelegramEnable)
            TC_LOG_INFO("server.loading", ">> KITT [Telegram Chat] config load. Option ACTIVAT.");
        else
            TC_LOG_INFO("server.loading", ">> KITT [Telegram Chat] config load. Option DEZACTIVAT.");

        std::string logsDir = sConfigMgr->GetStringDefault("LogsDir", "");
        if (!logsDir.empty() && logsDir.back() != '/' && logsDir.back() != '\\')
        {
#ifdef _WIN32
            logsDir += "\\";
#else
            logsDir += "/";
#endif
        }
        TelegramLogPath = logsDir + "telegram_errors.log";
    }
};

using namespace Trinity::ChatCommands;

class KittTelegram_Command : public CommandScript
{
public:
    KittTelegram_Command() : CommandScript("KittTelegram_Command") {}

    std::vector<ChatCommandBuilder> GetCommands() const override
    {
        static std::vector<ChatCommandBuilder> KittTelegramSubcommandTable =
        {
            // Adaugam un spatiu si _string pentru a accepta argumente multiple (mesajul complet)
            { "send", HandleTelegramSendInternal, rbac::RBAC_PERM_COMMAND_RELOAD_CONFIG, Console::Yes },
        };

        static std::vector<ChatCommandBuilder> KittTelegramCommandTable =
        {
            { "telegram", KittTelegramSubcommandTable },
        };

        return KittTelegramCommandTable;
    }

    static bool HandleTelegramSendInternal(ChatHandler* /*handler*/, Tail args)
    {
        if (args.empty())
            return false;

        std::string channelName = TelegramChannel;
        //std::string msgContent(args.value());
        //std::string msgText = "|cff00ccff[Telegram]|r |cffffffff" + std::string(args) + "|r";
        std::string msgText = "[T] " + std::string(args);

        //std::string msgText = std::string(args);
        //ObjectGuid senderGuid = ObjectGuid::Create<HighGuid::Player>(1);
        ObjectGuid senderGuid = ObjectGuid::Empty;


        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_CHANNEL, LANG_UNIVERSAL, senderGuid, senderGuid, msgText, 0, "", "", 0, false, channelName);

        sWorld->SendGlobalMessage(&data);

        return true;
    }




};

void AddSC_kitt_telegram_chat()
{
    new KittTelegramChatScript_config();
    new KittTelegramChatScript();
    new KittTelegram_Command();
}
