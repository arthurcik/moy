//----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "kitt_npcbot_ai.h"
#include "ScriptMgr.h"




class KittBotExpireException_config : public WorldScript
{
public:
    KittBotExpireException_config() : WorldScript("KittBotExpireException_config") {}

    void OnStartup() override
    {
        KittBotExpireException::LoadKittBotExceptions();
    }

    void OnConfigLoad(bool /*reload*/) override
    {
        KittBotExpireException::LoadKittBotExceptions();
        //TelegramEnable = sConfigMgr->GetBoolDefault("Kitt.Telegram.Enable", false);
        //TelegramToken = sConfigMgr->GetStringDefault("Kitt.Telegram.Token", "");
    }
};

void AddSC_kitt_npcbot_ai_loader()
{
    new KittBotExpireException_config();
}
