#include "ScriptMgr.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "WorldSession.h"
#include "Item.h"
#include "Chat.h"
#include "Mail.h"
#include "CharacterDatabase.h"


namespace
{
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

        // arena mapID
        // 559 nagrad // 562 Blade's Edge // 572 Ruins of Lordaeron

        // BG mapID
        // 489 WG // 529 AB // 30 AV // 566 EOTS

        // gm item
        { 900901,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // tfc Thunderfury //rank 5
        { 900902,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Staff of Disintegration //rank 9
        { 919347,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Claw of Chromaggus //rank 9
        { 950412,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Bloodvenom Blade //rank 9
        { 954806,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // TFC Frostscythe of Lord Ahune //rank 5
        { 17,          { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Martin Fury


        // player item
        { 900903,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // BRK-4000
        { 900904,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Shadowmourne
        { 900905,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Cryptmaker
        { 900906,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Repeating bow
        { 900907,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Impaling Spike
        { 900908,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Seducer
        { 900909,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // High-Blade of the Silver
        { 900910,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Antonidas
        { 900911,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Kel'Thuzad's Blade
        { 900912,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Fal'inrush
        { 900913,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Royal Scepter
        { 900914,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Heaven's Fall
        { 900915,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Blade of Kings
        { 900916,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Winter shield
        { 900917,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Penumbra
        { 900918,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // BRK-4000 // expire
        { 900919,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Shadowmourne // expire
        { 900920,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Cryptmaker // expire
        { 900921,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Repeating bow // expire
        { 900922,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Impaling Spike // expire
        { 900923,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Seducer // expire
        { 900924,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // High-Blade of the Silver // expire
        { 900925,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Antonidas // expire
        { 900926,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Kel'Thuzad's Blade // expire
        { 900927,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Fal'inrush // expire
        { 900928,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Royal Scepter // expire
        { 900929,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Heaven's Fall // expire
        { 900930,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Blade of Kings // expire
        { 900931,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Winter shield // expire
        { 900932,      { 0,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Penumbra // expire

        // B0ts  // for bots only
        { 900950,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // BRK-4000 // expire
        { 900951,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Shadowmourne // expire
        { 900952,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Cryptmaker // expire
        { 900953,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Repeating bow // expire
        { 900954,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Impaling Spike // expire
        { 900955,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Seducer // expire
        { 900956,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // High-Blade of the Silver // expire
        { 900957,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Antonidas // expire
        { 900958,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Kel'Thuzad's Blade // expire
        { 900959,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Fal'inrush // expire
        { 900960,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Royal Scepter // expire
        { 900961,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Heaven's Fall // expire
        { 900962,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Blade of Kings // expire
        { 900963,      { 9,       { 559, 562, 572, 489, 529, 30, 566, 628, 607 } } }, // Winter shield // expire
    };

    // enchant restrict list
    static const std::unordered_set<uint32> RestrictedEnchants = {
        5012,
        5013,
        5014,
        5015,
        5016,
        5017,
        5018,
        5019,
        5020,
        5021,
        5022,
        5023
    };

    // Functie helper care verifica rapid daca un ID se afla in lista noastra
    bool IsEnchantRestricted(uint32 enchantId)
    {
        return RestrictedEnchants.find(enchantId) != RestrictedEnchants.end();
    }
}

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
            bool isInDuel = player->IsInDuel();

                // GetMapId()) == config lista permisa
                // GetMapId()) != config lista nepermisa
            if (isInDuel || player->InBattleground() || player->InArena() ||
                player->GetSession()->GetSecurity() < config.minSecurity ||
                config.mapWhitelist.find(player->GetMapId()) != config.mapWhitelist.end())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Security]:|r The item |cffffffff[%s]|r cannot be equipped in this zone!", item->GetTemplate()->Name1.c_str());

                return EQUIP_ERR_CANT_EQUIP_EVER;
            }
        }

        return EQUIP_ERR_OK;
    }

    void OnEquip(Player* player, Item* item, uint16 /*slot*/, bool /*update*/) override
    {
        if (!player || !item)
            return;

        if (player->InBattleground() || player->InArena())
        {
            for (uint8 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
            {
                uint32 enchantId = item->GetEnchantmentId(EnchantmentSlot(slot));

                if (IsEnchantRestricted(enchantId))
                {
                    player->ApplyEnchantment(item, EnchantmentSlot(slot), false);
                }
            }

            item->SetState(ITEM_CHANGED, player);
            item->SendUpdateToPlayer(player);
        }
    }

    void OnDuelStart(Player* player1, Player* player2) override
    {
        if (player1 && player1->GetSession() && player1->IsInWorld())
        {
            OnMapChanged(player1);
        }

        if (player2 && player2->GetSession() && player2->IsInWorld())
        {
            OnMapChanged(player2);
        }
    }

    void OnMapChanged(Player* player) override
    {
        if (!player || !player->IsInWorld())
            return;

        for (uint8 i = EQUIPMENT_SLOT_START; i <= EQUIPMENT_SLOT_TABARD; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!item)
                continue;

            // --- INCEPUT VERIFICARE ENCHANT-URI PENTRU BG/ARENA ---
            for (uint8 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
            {
                uint32 enchantId = item->GetEnchantmentId(EnchantmentSlot(slot));

                if (IsEnchantRestricted(enchantId))
                {
                    if (player->InBattleground() || player->InArena())
                    {
                        player->ApplyEnchantment(item, EnchantmentSlot(slot), false);
                    }
                    else
                    {
                        player->ApplyEnchantment(item, EnchantmentSlot(slot), false);
                        player->ApplyEnchantment(item, EnchantmentSlot(slot), true);
                    }
                }
            }
            // --- SFARSIT VERIFICARE ENCHANT-URI ---

            auto it = MultiSecurityMap.find(item->GetEntry());
            if (it != MultiSecurityMap.end())
            {
                const RestrictedConfig& config = it->second;
                bool isInDuel = player->IsInDuel();
                // GetMapId()) == config lista permisa
                // GetMapId()) != config lista nepermisa
                if (isInDuel || player->InBattleground() || player->InArena() ||
                    player->GetSession()->GetSecurity() < config.minSecurity ||
                    config.mapWhitelist.find(player->GetMapId()) != config.mapWhitelist.end())
                {
                    ItemPosCountVec dest;
                    InventoryResult msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);

                    if (msg == EQUIP_ERR_OK)
                    {
                        player->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
                        player->StoreItem(dest, item, true);
                        ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Security]:|r The item |cffffffff[%s]|r has been moved to your backpack (not allowed here).", item->GetTemplate()->Name1.c_str());
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

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_INVENTORY_BY_ITEM);
        stmt->setUInt32(0, itemGuidLow);
        trans->Append(stmt);

        item->SetOwnerGUID(ObjectGuid::Empty);
        item->SetState(ITEM_NEW, player);

        MailDraft draft("Security: Item Moved", "The item has been moved to your mailbox.\nYou entered a restricted zone while it was equipped.");
        draft.AddItem(item);

        draft.SendMailTo(trans, player, MailSender(MAIL_CREATURE, 34337));
        CharacterDatabase.CommitTransaction(trans);

        item->SetOwnerGUID(player->GetGUID());
        item->SetState(ITEM_UNCHANGED, player);

        ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Security]:|r The item |cffffffff[%s]|r has been moved to your mailbox (inventory full).", item->GetTemplate()->Name1.c_str());
    }
};


void AddSC_kitt_item_restrict()
{
    new kitt_item_restrict();
}
