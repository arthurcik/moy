#include "ScriptMgr.h"
#include "Player.h"
#include "GuildMgr.h"
#include "Config.h"
#include "Guild.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "WorldSession.h"



class kitt_p_start_guild : public PlayerScript
{
    public:
        kitt_p_start_guild() : PlayerScript("kitt_p_start_guild") { }

   void OnLogin(Player* player, bool firstLogin)
   {
       if (firstLogin)
       {
           bool excludeGMs = sConfigMgr->GetBoolDefault("StartGuild.ExcludeGMs", false);

           if (excludeGMs && player->GetSession()->GetSecurity() > SEC_PLAYER)
           {
               return;
           }

           uint32 GUILD_ID_ALLIANCE = sConfigMgr->GetIntDefault("StartGuild.Alliance", 0);
           uint32 GUILD_ID_HORDE = sConfigMgr->GetIntDefault("StartGuild.Horde", 0);
           uint32 desiredGuildId = (player->GetTeam() == ALLIANCE) ? GUILD_ID_ALLIANCE : GUILD_ID_HORDE;

           if (desiredGuildId == 0)
               return;

           Guild* guild = sGuildMgr->GetGuildById(desiredGuildId);

           /*if (guild)
           {
               CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
               guild->AddMember(trans, player->GetGUID(), GUILD_RANK_NONE);
               CharacterDatabase.CommitTransaction(trans);
           }*/
           if (guild)
           {
               ObjectGuid playerGuid = player->GetGUID();
               uint32 gId = desiredGuildId;

               player->m_Events.AddEvent(new LambdaBasicEvent([playerGuid, gId]()
                   {
                       Player* p = ObjectAccessor::FindConnectedPlayer(playerGuid);
                       if (!p)
                           return;

                       if (Guild* targetGuild = sGuildMgr->GetGuildById(gId))
                       {
                           if (!p->GetGuildId())
                           {
                               CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                               targetGuild->AddMember(trans, p->GetGUID(), GUILD_RANK_NONE);
                               CharacterDatabase.CommitTransaction(trans);
                           }
                       }
                   }), player->m_Events.CalculateTime(10s));
           }
           else
           {
               TC_LOG_ERROR("sql.sql", "Start guild {} not found in GuildMgr cache!", desiredGuildId);
           }
       }
   }
};

void AddSC_kitt_p_start_guild()
{
    new kitt_p_start_guild();
}
