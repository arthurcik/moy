#include "ScriptMgr.h"
#include "Player.h"
#include "Channel.h"
#include "Chat.h"

class kitt_global_reminder : public PlayerScript
{
public:
    kitt_global_reminder() : PlayerScript("kitt_global_reminder") {}

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        RegisterReminderEvent(player, player->GetGUID().GetCounter());
    }

private:
    void RegisterReminderEvent(Player* player, uint32 pLowGUID)
    {
        if (!player)
            return;

        player->m_Events.AddEventAtOffset([this, pLowGUID]()
            {
                Player* p = ObjectAccessor::FindPlayerByLowGUID(pLowGUID);

                if (!p || !p->GetSession() || !p->IsInWorld())
                    return;

                if (p->IsBeingTeleported())
                {
                    RegisterReminderEvent(p, pLowGUID);
                    return;
                }

                bool hasGlobal = false;

                auto const& joinedChannels = p->GetJoinedChannels();

                for (auto const& channel : joinedChannels)
                {
                    if (channel && (channel->GetName() == "Global" || channel->GetName() == "global"))
                    {
                        hasGlobal = true;
                        break;
                    }
                }

                if (!hasGlobal)
                {
                    ChatHandler(p->GetSession()).PSendSysMessage("|cffFFFF00[Server]:|r You are not on the Global channel. Type |cff00FF00/join Global|r");

                    RegisterReminderEvent(p, pLowGUID);
                }
            }, 30s);
    }
};

void AddSC_auto_join_world()
{
    new kitt_global_reminder();
}
