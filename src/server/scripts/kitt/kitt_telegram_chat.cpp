//----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Channel.h"
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


namespace
{
    std::string TelegramLogPath = "";

    bool TelegramEnable = false;
    std::string TelegramToken = "";
    std::string TelegramChatId = "";
    std::string TelegramChannel = "global";

    bool workerRunning = true;
    std::queue<std::string> messageQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;

    std::string KittCleanWoWLinks(std::string text)
    {
        static const std::regex linkRegex("\\|c[0-9a-fA-F]{8}\\|H[a-zA-Z0-9:]+\\|h\\[([^\\]]+)\\]\\|h\\|r");

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
            std::string msg;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                queueCondition.wait(lock, [] { return !messageQueue.empty() || !workerRunning; });

                if (!workerRunning && messageQueue.empty())
                    break;

                msg = messageQueue.front();
                messageQueue.pop();
            }

            SendToTelegram(msg);

            // 100ms pauza = max 10 mesaje pe secunda (siguranta totala pentru Telegram)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
}


class KittTelegramChatScript : public PlayerScript
{
public:
    KittTelegramChatScript() : PlayerScript("KittTelegramChatScript") {}

    void OnChat(Player* player, uint32 type, uint32 /*lang*/, std::string& msg, Channel* channel) override
    {
        if (!TelegramEnable || !channel)
            return;

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
            std::string fullMessage = "<" + KittchannelName + "> [" + playerName + "]: " + KittcleanedMsg;

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

class KittKittTelegramChatScript_config : public WorldScript
{
public:
    KittKittTelegramChatScript_config() : WorldScript("KittKittTelegramChatScript_config") {}

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

void AddSC_kitt_telegram_chat()
{
    new KittKittTelegramChatScript_config();
    new KittTelegramChatScript();
}
