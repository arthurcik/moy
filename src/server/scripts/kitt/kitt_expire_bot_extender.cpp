/**#include "ScriptMgr.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "WorldSession.h"


class kitt_expire_bot_extender : public PlayerScript
{
public:
    kitt_expire_bot_extender() : PlayerScript("kitt_expire_bot_extender") {}

    void OnLogout(Player* player) override
    {
        if (!player)
            return;
        uint32 accountId = player->GetSession()->GetAccountId();
        uint32 playerGuid = player->GetGUID().GetCounter();

        if (accountId == 2 || // test1
            accountId == 5 || // titel
            accountId == 8 || // gutza
            accountId == 26 || // test0
            accountId == 31) // PALAKISS
        {
            uint32 extraDay = 10; // zile extra
            uint32 extraTime = extraDay * 24 * 60 * 60;
            uint32 currentTime = GameTime::GetGameTime();

            uint32 futureLogoutTime = currentTime + extraTime;

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

            trans->PAppend("UPDATE characters SET logout_time = {} WHERE guid = {}",
                futureLogoutTime, playerGuid);

            CharacterDatabase.AsyncCommitTransaction(trans);
        }
    }
};

void AddSC_kitt_expire_bot_extender()
{
    new kitt_expire_bot_extender();
}
**/
