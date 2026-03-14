#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ReputationMgr.h"
//#include "QuestMgr.h"
//#include "QuestDef.h"
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

    static void CompleteObjectiveForKitt(Player* player, QuestObjective const& obj, uint32 questId, bool enableRep, const std::vector<uint32>& repQuests)
    {
        switch (obj.Type)
        {
        case QUEST_OBJECTIVE_ITEM:
        {
            uint32 curItemCount = player->GetItemCount(obj.ObjectID, true);
            if (curItemCount < (uint32)obj.Amount)
            {
                uint32 needCount = obj.Amount - curItemCount;
                ItemPosCountVec dest;
                if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, obj.ObjectID, needCount) == EQUIP_ERR_OK)
                    if (Item* item = player->StoreNewItem(dest, obj.ObjectID, true))
                        player->SendNewItem(item, needCount, true, false);
            }
            break;
        }
        case QUEST_OBJECTIVE_CURRENCY:
            player->ModifyCurrency(obj.ObjectID, obj.Amount, CurrencyGainSource::Cheat);
            break;

        case QUEST_OBJECTIVE_MIN_REPUTATION:
        case QUEST_OBJECTIVE_MAX_REPUTATION:
        case QUEST_OBJECTIVE_INCREASE_REPUTATION:
        {
            bool isRepQuestInList = std::find(repQuests.begin(), repQuests.end(), questId) != repQuests.end();

            if (enableRep && isRepQuestInList)
            {
                if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(obj.ObjectID))
                    player->GetReputationMgr().SetReputation(factionEntry, obj.Amount);
            }
            break;
        }

        case QUEST_OBJECTIVE_MONEY:
            player->ModifyMoney(obj.Amount);
            break;

        default:
            player->UpdateQuestObjectiveProgress(static_cast<QuestObjectiveType>(obj.Type), obj.ObjectID, obj.Amount);
            break;
        }
    }

    void OnQuestStatusChange(Player* player, uint32 questId) override
    {
        std::vector<uint32> autoQuests = GetIdListFromConfig("kitt.Quests.AutoCompleteList");
        bool enableRep = sConfigMgr->GetBoolDefault("kitt.Quests.AutoReputation.Enable", false);
        std::vector<uint32> repQuests = GetIdListFromConfig("kitt.Quests.AutoReputation.List");

        if (std::find(autoQuests.begin(), autoQuests.end(), questId) == autoQuests.end())
            return;

        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest || player->GetQuestStatus(questId) != QUEST_STATUS_INCOMPLETE)
            return;

        for (QuestObjective const& obj : quest->Objectives)
            CompleteObjectiveForKitt(player, obj, questId, enableRep, repQuests);

        player->CompleteQuest(questId);
        player->SendQuestComplete(questId);

        player->RewardQuest(quest, LootItemType::Item, 0, player, false);

        TC_LOG_INFO("server.loading", "Quest: {} auto-completat (Reputatie Config: {})", questId, enableRep ? "ON" : "OFF");
    }
};

void AddSC_kitt_quest_complet()
{
    new kitt_quest_complet();
}
