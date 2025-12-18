#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ReputationMgr.h"
#include "DBCStores.h"
//#include <vector>
//#include <string>
//#include <sstream>
//#include <algorithm>

class kitt_quest_complet : public PlayerScript
{
public:
    kitt_quest_complet() : PlayerScript("kitt_quest_complet") {}

    std::vector<uint32> GetIdListFromConfig(std::string configPath)
    {
        std::vector<uint32> list;
        std::string configStr = sConfigMgr->GetStringDefault(configPath.c_str(), "");
        if (configStr.empty()) return list;

        std::stringstream ss(configStr);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item.erase(std::remove(item.begin(), item.end(), ' '), item.end());
            if (!item.empty())
                list.push_back(std::stoul(item));
        }
        return list;
    }

    void OnQuestStatusChange(Player* player, uint32 questId) override
    {
        std::vector<uint32> autoQuests = GetIdListFromConfig("kitt.Quests.AutoCompleteList");

        if (std::find(autoQuests.begin(), autoQuests.end(), questId) == autoQuests.end())
            return;

        if (player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
        {
            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest) return;

            for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
            {
                uint32 id = quest->RequiredItemId[x];
                uint32 count = quest->RequiredItemCount[x];
                if (!id || !count) continue;
                uint32 curItemCount = player->GetItemCount(id, true);
                if (curItemCount < count)
                {
                    ItemPosCountVec dest;
                    uint32 needCount = count - curItemCount;
                    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, id, needCount) == EQUIP_ERR_OK)
                    {
                        Item* item = player->StoreNewItem(dest, id, true);
                        player->SendNewItem(item, needCount, true, false);
                    }
                }
            }

            for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            {
                int32 creature = quest->RequiredNpcOrGo[i];
                uint32 creatureCount = quest->RequiredNpcOrGoCount[i];
                if (creature > 0)
                {
                    if (CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(creature))
                        for (uint16 z = 0; z < creatureCount; ++z)
                            player->KilledMonster(cInfo, ObjectGuid::Empty);
                }
                else if (creature < 0)
                    for (uint16 z = 0; z < creatureCount; ++z)
                        player->KillCreditGO(creature);
            }

            bool enableRep = sConfigMgr->GetBoolDefault("kitt.Quests.AutoReputation.Enable", false);
            if (enableRep)
            {
                std::vector<uint32> repQuests = GetIdListFromConfig("kitt.Quests.AutoReputation.List");
                if (std::find(repQuests.begin(), repQuests.end(), questId) != repQuests.end())
                {
                    for (uint8 i = 0; i < 2; ++i)
                    {
                        uint32 repFaction = (i == 0) ? quest->GetRepObjectiveFaction() : quest->GetRepObjectiveFaction2();
                        int32 repValue = (int32)((i == 0) ? quest->GetRepObjectiveValue() : quest->GetRepObjectiveValue2());

                        if (repFaction && repValue)
                        {
                            if (player->GetReputationMgr().GetReputation(repFaction) < repValue)
                            {
                                if (FactionEntry const* fEntry = sFactionStore.LookupEntry(repFaction))
                                    player->GetReputationMgr().SetReputation(fEntry, repValue);
                            }
                        }
                    }
                }
            }

            player->CompleteQuest(questId);
            player->SendQuestComplete(questId);
            player->SendQuestUpdate(questId);

            TC_LOG_ERROR("kitt", "Quest: {} completat (Reputation: {})", questId, enableRep ? "ON" : "OFF");
        }
    }
};

void AddSC_kitt_quest_complet()
{
    new kitt_quest_complet();
}
