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
                           // logs cine intra
                           TC_LOG_ERROR("sql.sql", "DEBUG START: Jucator {} (GUID: {}) incearca intrare in Gilda ID {} (Nume in RAM: >>> {} <<<)",
                               p->GetName().c_str(), p->GetGUID().GetCounter(), gId, targetGuild->GetName().c_str());

                           if (!p->GetGuildId())
                           {
                               CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                               targetGuild->AddMember(trans, p->GetGUID(), GUILD_RANK_NONE);
                               CharacterDatabase.CommitTransaction(trans);

                               // logs cine a intrat
                               TC_LOG_ERROR("sql.sql", "DEBUG SUCCESS: Jucator {} adaugat. Nume Gilda in RAM dupa operatiune: >>> {} <<<",
                                   p->GetName().c_str(), targetGuild->GetName().c_str());
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
