#include "ScriptMgr.h"
#include "Player.h"
#include "GuildMgr.h"
#include "Config.h"
#include "Guild.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Chat.h"
#include "Item.h"
#include "Mail.h"


class kitt_first_lvl80 : public PlayerScript
{
    public:
        kitt_first_lvl80() : PlayerScript("kitt_first_lvl80") { }

        void OnLevelChanged(Player* player, uint8 /*oldLevel*/) override
        {
            uint8 newLevel = player->GetLevel();

            uint32 lvl21Config = sConfigMgr->GetIntDefault("kitt.Reward.Gold.21", 0);
            uint32 item21Config = sConfigMgr->GetIntDefault("kitt.Reward.Item.21", 0);
            uint32 iCount21Config = sConfigMgr->GetIntDefault("kitt.Reward.iCount.21", 1);
            uint32 lvl21Amount = lvl21Config * 100 * 100;

            uint32 lvl30Config = sConfigMgr->GetIntDefault("kitt.Reward.Gold.30", 0);
            uint32 item30Config = sConfigMgr->GetIntDefault("kitt.Reward.Item.30", 0);
            uint32 iCount30Config = sConfigMgr->GetIntDefault("kitt.Reward.iCount.30", 1);
            uint32 lvl30Amount = lvl30Config * 100 * 100;

            uint32 lvl40Config = sConfigMgr->GetIntDefault("kitt.Reward.Gold.40", 0);
            uint32 item40Config = sConfigMgr->GetIntDefault("kitt.Reward.Item.40", 0);
            uint32 iCount40Config = sConfigMgr->GetIntDefault("kitt.Reward.iCount.40", 1);
            uint32 lvl40Amount = lvl40Config * 100 * 100;

            uint32 lvl50Config = sConfigMgr->GetIntDefault("kitt.Reward.Gold.50", 0);
            uint32 item50Config = sConfigMgr->GetIntDefault("kitt.Reward.Item.50", 0);
            uint32 iCount50Config = sConfigMgr->GetIntDefault("kitt.Reward.iCount.50", 1);
            uint32 lvl50Amount = lvl50Config * 100 * 100;

            uint32 lvl60Config = sConfigMgr->GetIntDefault("kitt.Reward.Gold.60", 0);
            uint32 item60Config = sConfigMgr->GetIntDefault("kitt.Reward.Item.60", 0);
            uint32 iCount60Config = sConfigMgr->GetIntDefault("kitt.Reward.iCount.60", 1);
            uint32 lvl60Amount = lvl60Config * 100 * 100;

            uint32 lvl70Config = sConfigMgr->GetIntDefault("kitt.Reward.Gold.70", 0);
            uint32 item70Config = sConfigMgr->GetIntDefault("kitt.Reward.Item.70", 0);
            uint32 iCount70Config = sConfigMgr->GetIntDefault("kitt.Reward.iCount.70", 1);
            uint32 lvl70Amount = lvl70Config * 100 * 100;

            uint32 lvl80Config = sConfigMgr->GetIntDefault("kitt.Reward.Gold.80", 0);
            uint32 item80Config = sConfigMgr->GetIntDefault("kitt.Reward.Item.80", 0);
            uint32 iCount80Config = sConfigMgr->GetIntDefault("kitt.Reward.iCount.80", 1);
            uint32 lvl80Amount = lvl80Config * 100 * 100;

            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(0, 0, dest, item70Config, iCount70Config);

            //uint32 itemId = 1977; // Exemplu: ID-ul itemului pe care vrei sa il adaugi
            //uint32 itemCount = 1;  // Cantitatea

            if (newLevel == 21)
            {
                if (item21Config > 0)
                {
                    if (msg == EQUIP_ERR_OK)
                    {
                        player->AddItem(item21Config, iCount21Config);

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in sac.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                    else
                    {
                        for (uint32 i = 0; i < iCount21Config; ++i)
                        {
                            player->SendItemRetrievalMail(item21Config, 1);
                        }

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in mail.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                }

                if (lvl21Config == 0)
                {
                    return;
                }
                else
                {
                    player->ModifyMoney(lvl21Amount);

                    player->m_Events.AddEventAtOffset([player, newLevel, lvl21Config]()
                        {
                            // verificare daca mai este in lume
                            if (!player || !player->IsInWorld())
                                return;
                            std::string message = "|cffff0000!... Ai atins nivelul ";
                            message += std::to_string(newLevel);
                            message += "!|r Ai fost recompensat cu ";
                            message += std::to_string(lvl21Config);
                            message += " galbeni.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }, 3s);
                }
            }

            if (newLevel == 30)
            {
                if (item30Config > 0)
                {
                    if (msg == EQUIP_ERR_OK)
                    {
                        player->AddItem(item30Config, iCount30Config);

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in sac.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                    else
                    {
                        for (uint32 i = 0; i < iCount30Config; ++i)
                        {
                            player->SendItemRetrievalMail(item30Config, 1);
                        }

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in mail.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                }

                if (lvl30Config == 0)
                {
                    return;
                }
                else
                {
                    player->ModifyMoney(lvl30Amount);

                    player->m_Events.AddEventAtOffset([player, newLevel, lvl30Config]()
                        {
                            // verificare daca mai este in lume
                            if (!player || !player->IsInWorld())
                                return;
                            std::string message = "|cffff0000!... Ai atins nivelul ";
                            message += std::to_string(newLevel);
                            message += "!|r Ai fost recompensat cu ";
                            message += std::to_string(lvl30Config);
                            message += " galbeni.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }, 3s);
                }
            }

            if (newLevel == 40)
            {
                if (item40Config > 0)
                {
                    if (msg == EQUIP_ERR_OK)
                    {
                        player->AddItem(item40Config, iCount40Config);

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in sac.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                    else
                    {
                        for (uint32 i = 0; i < iCount40Config; ++i)
                        {
                            player->SendItemRetrievalMail(item40Config, 1);
                        }

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in mail.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                }

                if (lvl40Config == 0)
                {
                    return;
                }
                else
                {
                    player->ModifyMoney(lvl40Amount);

                    player->m_Events.AddEventAtOffset([player, newLevel, lvl40Config]()
                        {
                            // verificare daca mai este in lume
                            if (!player || !player->IsInWorld())
                                return;
                            std::string message = "|cffff0000!... Ai atins nivelul ";
                            message += std::to_string(newLevel);
                            message += "!|r Ai fost recompensat cu ";
                            message += std::to_string(lvl40Config);
                            message += " galbeni.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }, 3s);
                }
            }

            if (newLevel == 50)
            {
                if (item50Config > 0)
                {
                    if (msg == EQUIP_ERR_OK)
                    {
                        player->AddItem(item50Config, iCount50Config);

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in sac.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                    else
                    {
                        for (uint32 i = 0; i < iCount50Config; ++i)
                        {
                            player->SendItemRetrievalMail(item50Config, 1);
                        }

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in mail.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                }

                if (lvl50Config == 0)
                {
                    return;
                }
                else
                {
                    player->ModifyMoney(lvl50Amount);

                    player->m_Events.AddEventAtOffset([player, newLevel, lvl50Config]()
                        {
                            // verificare daca mai este in lume
                            if (!player || !player->IsInWorld())
                                return;
                            std::string message = "|cffff0000!... Ai atins nivelul ";
                            message += std::to_string(newLevel);
                            message += "!|r Ai fost recompensat cu ";
                            message += std::to_string(lvl50Config);
                            message += " galbeni.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }, 3s);
                }
            }

            if (newLevel == 60)
            {
                if (item60Config > 0)
                {
                    if (msg == EQUIP_ERR_OK)
                    {
                        player->AddItem(item60Config, iCount60Config);

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in sac.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                    else
                    {
                        for (uint32 i = 0; i < iCount60Config; ++i)
                        {
                            player->SendItemRetrievalMail(item60Config, 1);
                        }

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in mail.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                }

                if (lvl60Config == 0)
                {
                    return;
                }
                else
                {
                    player->ModifyMoney(lvl60Amount);

                    player->m_Events.AddEventAtOffset([player, newLevel, lvl60Config]()
                        {
                            // verificare daca mai este in lume
                            if (!player || !player->IsInWorld())
                                return;
                            std::string message = "|cffff0000!... Ai atins nivelul ";
                            message += std::to_string(newLevel);
                            message += "!|r Ai fost recompensat cu ";
                            message += std::to_string(lvl60Config);
                            message += " galbeni.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }, 3s);
                }
            }

            if (newLevel == 70)
            {
                if (item70Config > 0)
                {
                    if (msg == EQUIP_ERR_OK)
                    {
                        player->AddItem(item70Config, iCount70Config);

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in sac.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                    else
                    {
                        for (uint32 i = 0; i < iCount70Config; ++i)
                        {
                            player->SendItemRetrievalMail(item70Config, 1);
                        }

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in mail.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                }

                if (lvl70Config == 0)
                {
                    return;
                }
                else
                {
                    player->ModifyMoney(lvl70Amount);

                    player->m_Events.AddEventAtOffset([player, newLevel, lvl70Config]()
                        {
                            // verificare daca mai este in lume
                            if (!player || !player->IsInWorld())
                                return;
                            std::string message = "|cffff0000!... Ai atins nivelul ";
                            message += std::to_string(newLevel);
                            message += "!|r Ai fost recompensat cu ";
                            message += std::to_string(lvl70Config);
                            message += " galbeni.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }, 3s);
                }
            }

            if (newLevel == 80)
            {
                if (item80Config > 0)
                {
                    if (msg == EQUIP_ERR_OK)
                    {
                        player->AddItem(item80Config, iCount80Config);

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in sac.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                    else
                    {
                        for (uint32 i = 0; i < iCount80Config; ++i)
                        {
                            player->SendItemRetrievalMail(item80Config, 1);
                        }

                        player->m_Events.AddEventAtOffset([player, newLevel]()
                            {
                                // verificare daca mai este in lume
                                if (!player || !player->IsInWorld())
                                    return;
                                std::string message = "|cffff0000!... Ai atins nivelul ";
                                message += std::to_string(newLevel);
                                message += "!|r Ai primit un cadou! |cffff0000Verifica in mail.|r";
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                            }, 3s);
                    }
                }

                if (lvl80Config == 0)
                {
                    return;
                }
                else
                {
                    player->ModifyMoney(lvl80Amount);
                    // msg intarziat 3 sec // [=] captureaza automat tot ce e nevoie
                    player->m_Events.AddEventAtOffset([player, newLevel, lvl80Config]()
                        {
                            // verificare daca mai este in lume
                            if (!player || !player->IsInWorld())
                                return;

                            std::string message = "|cffff0000!... Ai atins nivelul ";
                            message += std::to_string(newLevel);
                            message += "!|r Ai fost recompensat cu ";
                            message += std::to_string(lvl80Config);
                            message += " galbeni.";
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", message.c_str());
                        }, 3s);
                }
            }
        }
};

void AddSC_kitt_first_lvl80()
{
    new kitt_first_lvl80();
}
