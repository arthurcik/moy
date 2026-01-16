#include "ScriptMgr.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "WorldSession.h"
#include "Item.h"
#include "Chat.h"
#include "Mail.h"
#include "CharacterDatabase.h"
//#include <unordered_set>



struct RestrictedConfig {
    uint8 minSecurity;
    std::unordered_set<uint32> mapWhitelist;
};

// Mapam ID-ul itemului la configuratia sa specifica
static inline const std::unordered_map<uint32, RestrictedConfig> MultiSecurityMap = {
    // rangGM = 0 permis pt toti. rang < NR = nepermis
    // mapID este in lista = nepermis
    // nepermis = mapID ori gm_rang
    // { ID_ITEM, {  RANG_GM, { LISTA_MAP_ID_NEPERMIS } } }

    // gm item
    { 900901,      { 9,       {  } } }, // tfc Thunderfury
    { 900902,      { 9,       {  } } }, // Staff of Disintegration
    { 919347,      { 9,       {  } } }, // Claw of Chromaggus
    { 950412,      { 9,       {  } } }, // Bloodvenom Blade
    { 954806,      { 5,       {  } } }, // TFC Frostscythe of Lord Ahune


    // player item
    { 900903,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // BRK-4000
    { 900904,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Shadowmourne
    { 900905,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Cryptmaker
    { 900906,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Repeating bow
    { 900907,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Impaling Spike
    { 900908,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Seducer
    { 900909,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // High-Blade of the Silver
    { 900910,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Antonidas
    { 900911,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Kel'Thuzad's Blade
    { 900912,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Fal'inrush
    { 900913,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Royal Scepter
    { 900914,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Heaven's Fall
    { 900915,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Blade of Kings
    { 900916,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Winter shield
    { 900917,      { 0,       { 489, 529, 30, 566, 628, 607 } } }, // Penumbra

};


class kitt_item_restrict : public PlayerScript
{
public:
    kitt_item_restrict() : PlayerScript("kitt_item_restrict") {}

    InventoryResult OnCanEquipItem(Player* player, uint8 /*slot*/, uint16& /*dest*/, Item* item, bool /*swap*/, bool /*not_loading*/) override
    {
        if (!player || !item)
            return EQUIP_ERR_OK;

        auto it = MultiSecurityMap.find(item->GetEntry());
        if (it != MultiSecurityMap.end())
        {
            const RestrictedConfig& config = it->second;

                // GetMapId()) == config lista permisa
                // GetMapId()) != config lista nepermisa
            if (player->GetSession()->GetSecurity() < config.minSecurity ||
                config.mapWhitelist.find(player->GetMapId()) != config.mapWhitelist.end())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Securitate]:|r Obiectul |cffffffff[%s]|r nu poate fi echipat in aceasta zona!", item->GetTemplate()->Name1.c_str());

                return EQUIP_ERR_CANT_EQUIP_EVER;
            }
        }

        return EQUIP_ERR_OK;
    }

    void OnMapChanged(Player* player)
    {
        if (!player || !player->IsInWorld())
            return;

        for (uint8 i = EQUIPMENT_SLOT_START; i <= EQUIPMENT_SLOT_TABARD; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!item)
                continue;

            auto it = MultiSecurityMap.find(item->GetEntry());
            if (it != MultiSecurityMap.end())
            {
                const RestrictedConfig& config = it->second;
                // GetMapId()) == config lista permisa
                // GetMapId()) != config lista nepermisa
                if (player->GetSession()->GetSecurity() < config.minSecurity ||
                    config.mapWhitelist.find(player->GetMapId()) != config.mapWhitelist.end())
                {
                    ItemPosCountVec dest;
                    InventoryResult msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);

                    if (msg == EQUIP_ERR_OK)
                    {
                        player->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
                        player->StoreItem(dest, item, true);
                        ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Securitate]:|r Obiectul |cffffffff[%s]|r a fost mutat in rucsac (nepermis aici).", item->GetTemplate()->Name1.c_str());
                    }
                    else
                    {
                        MoveItemToMail(player, item);
                    }
                }
            }
        }
    }

    void MoveItemToMail(Player* player, Item* item)
    {
        if (!player || !item)
            return;

        uint8 bag = item->GetBagSlot();
        uint8 slot = item->GetSlot();
        uint32 itemGuidLow = item->GetGUID().GetCounter();

        player->MoveItemFromInventory(bag, slot, true);

        SQLTransaction trans = CharacterDatabase.BeginTransaction();

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_ITEM);
        stmt->setUInt32(0, itemGuidLow);
        trans->Append(stmt);

        item->SetOwnerGUID(ObjectGuid::Empty);
        item->SetState(ITEM_NEW, player);

        MailDraft draft("Securitate: Obiect Mutat", "Obiectul a fost mutat la posta.\nAi patruns echipat intr-o zona nepermisa.");
        draft.AddItem(item);

        draft.SendMailTo(trans, player, MailSender(MAIL_CREATURE, 34337));
        CharacterDatabase.CommitTransaction(trans);

        item->SetOwnerGUID(player->GetGUID());
        item->SetState(ITEM_UNCHANGED, player);

        ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Securitate]:|r Obiectul |cffffffff[%s]|r a fost mutat la posta (inventar plin).", item->GetTemplate()->Name1.c_str());
    }
};


void AddSC_kitt_item_restrict()
{
    new kitt_item_restrict();
}
