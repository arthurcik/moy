#include "ScriptMgr.h"
#include "Player.h"
#include "GuildMgr.h"
#include "Config.h"
#include "Guild.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "World.h"
#include "Errors.h"

class kitt_p_start_guild : public PlayerScript
{
    public:
        kitt_p_start_guild() : PlayerScript("kitt_p_start_guild") { }

   void OnLogin(Player* player, bool firstLogin)
   {
       if (firstLogin)
       {
           uint32 GUILD_ID_ALLIANCE = sConfigMgr->GetIntDefault("StartGuild.Alliance", 0);
           uint32 GUILD_ID_HORDE = sConfigMgr->GetIntDefault("StartGuild.Horde", 0);
           uint32 desiredGuildId = (player->GetTeam() == ALLIANCE) ? GUILD_ID_ALLIANCE : GUILD_ID_HORDE;

           if (desiredGuildId == 0)
               return;

           Guild* guild = sGuildMgr->GetGuildById(desiredGuildId);

           if (guild)
           {
               guild->AddMember(CharacterDatabase.BeginTransaction(), player->GetGUID(), GUILD_RANK_NONE);
           }
           else
           {
               TC_LOG_ERROR("kitt", "Start guild {} not found in GuildMgr cache!", desiredGuildId);
           }
       }
   }
};

void AddSC_kitt_p_start_guild()
{
    new kitt_p_start_guild();
}
