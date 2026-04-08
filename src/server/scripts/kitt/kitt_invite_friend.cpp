//----- Kitt Arthur -----
// full config by kittArthur
// ----------- & -----------
// ----- Arthur_19` -----

#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "CharacterCache.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include <unordered_map>
#include "Log.h"
#include "GameTime.h"


using namespace Trinity::ChatCommands;

namespace
{
    static uint32 sKittInviteFriend = 0;

    static std::map<ObjectGuid, time_t> playtimeCooldownMap;
    static uint32 playtimeCooldownTime = 30; // in secunde anti-flood


    // Structura pentru premii
    struct RewardItem {
        uint32 ItemId;
        uint32 Count;
    };

    // --- CONFIGURARE PREMII AICI ---
    // puncte pe site VP / DP pentru cel ce invita
    static int KittInviteBySiteVP = 30;
    static int KittInviteBySiteDP = 50;
    // puncte site VP / DP pentru cel Nou
    static int KittInviteNewSiteVP = 15;
    static int KittInviteNewSiteDP = 0;


    // Premii pentru cel NOU (care a fost recomandat)
    const std::vector<RewardItem> newbieRewards = {
        { 49426, 150 }, // Embleme of Frost
        { 47241, 150 }, // Embleme of Triumph
        { 40753, 150 }  // Emblem of Valor
    };

    // Premii pentru RECRUTATOR (cel care a recomandat)
    const std::vector<RewardItem> referrerRewards = {
        { 44990, 100 }, // Champion's Seal
        { 33079, 1 },  // Murloc Costume
        { 38233, 100 },  // Path of Illidan
        { 33927, 1 },  // Brewfest Pony Keg
        { 13379, 1 }  // Piccolo of the Flaming Fire
    };

    void SendRewardToMail(Player* player, uint32 itemId, uint32 itemCount)
    {
        if (!player)
            return;

        ItemTemplate const* temp = sObjectMgr->GetItemTemplate(itemId);
        if (!temp)
            return;

        // Creem obiectul Item
        Item* item = Item::CreateItem(itemId, itemCount, player);
        if (!item)
            return;

        // Incepem tranzactia SQL pentru a salva item-ul inainte de trimitere
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        item->SaveToDB(trans);

        // Pregatim mail-ul
        MailDraft draft("Invitation Reward", "Your inventory was full. Please find your reward attached.");
        draft.AddItem(item);
        // Trimitem mail-ul (folosim un ID de creatura generic pentru expeditor, ex: 34337)
        draft.SendMailTo(trans, player, MailSender(MAIL_CREATURE, 34337));
        CharacterDatabase.CommitTransaction(trans);

        ChatHandler(player->GetSession()).PSendSysMessage("|cffFFFF00[Invite Reward]:|r Inventory full! Your reward |cff0070dd|Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h|r has been sent to your mailbox.",
            itemId, temp->Name1.c_str());
    }

    void GiveSitePoints(Player* player, uint32 amount, bool isDonation, CharacterDatabaseTransaction trans)
    {
        //Player* p = handler->GetSession()->GetPlayer();

        if (amount == 0 || !player)
            return;

        uint32 accountId = player->GetSession()->GetAccountId();
        std::string typeName = isDonation ? "Donation Points (DP)" : "Vote Points (VP)";
        std::string column = isDonation ? "dp" : "vp";

        // Actualizam tabelul FEDERATED care va modifica automat baza de date a site-ului
        trans->PAppend("UPDATE zkitt_site_points_linked SET {} = {} + {} WHERE id = {}",
            column.c_str(), column.c_str(), amount, accountId);

        TC_LOG_INFO("server.loading", ">> Invite System: Awarded {} {} to Account ID {}",
            amount, (isDonation ? "DP" : "VP"), accountId);
        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[Invite System]:|r You have received |cffffffff%u %s|r on our website store!",
            amount, typeName.c_str());
    }

    // Functia principala de acordare premii
    void GiveRewards(Player* player, const std::vector<RewardItem>& rewards)
    {
        if (!player)
            return;

        for (auto const& reward : rewards)
        {
            ItemTemplate const* temp = sObjectMgr->GetItemTemplate(reward.ItemId);
            if (!temp)
                continue;

            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, reward.ItemId, reward.Count);

            if (msg == EQUIP_ERR_OK)
            {
                player->StoreNewItem(dest, reward.ItemId, reward.Count, true);

                ChatHandler(player->GetSession()).PSendSysMessage("Received: |cff0070dd|Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h|r x%u",
                    reward.ItemId, temp->Name1.c_str(), reward.Count);
            }
            else
            {
                SendRewardToMail(player, reward.ItemId, reward.Count);
            }
        }
    }

    // Structura datelor stocate in RAM
    struct RecommendData
    {
        uint32 referrerAcc = 0;  // Cine a recomandat (Recrutatorul)
        std::string recomName = ""; // Nume cine a invitat
        bool rewardedNew = false;    // Premiu ridicat de cel nou
        bool rewardedRecom = false;  // Premiu ridicat de recrutator
        //RecommendData() : referrerAcc(0), recomName(""), rewardedNew(false), rewardedRecom(false) {}
    };

    // Cache principal: Key = acc_new (Contul invitat)
    std::unordered_map<uint32, RecommendData> RecommendCache;

    // Sincronizare rapid? RAM -> DB pentru premii
    void UpdateRewardInDB(uint32 accNew, bool isNewbie, CharacterDatabaseTransaction trans)
    {
        trans->PAppend("UPDATE character_kitt_invite_friend SET {} = 1 WHERE inv_accID = {}",
            isNewbie ? "inv_rewarded" : "inv_by_rewarded", accNew);
    }
}

// 1. Incarcarea datelor la pornirea serverului
class kitt_invite_friend_startup : public WorldScript
{
public:
    kitt_invite_friend_startup() : WorldScript("kitt_invite_friend_startup") {}

    void OnStartup() override
    {
        RecommendCache.clear();
        QueryResult result = CharacterDatabase.Query("SELECT inv_accID, inv_by_accID, inv_by_name, inv_rewarded, inv_by_rewarded FROM character_kitt_invite_friend");

        if (!result)
            return;

        uint32 count = 0;
        do {
            Field* fields = result->Fetch();
            uint32 accNew = fields[0].GetUInt32();
            RecommendData& data = RecommendCache[accNew];
            data.referrerAcc = fields[1].GetUInt32();
            data.recomName = fields[2].GetString();
            data.rewardedNew = fields[3].GetBool();
            data.rewardedRecom = fields[4].GetBool();
            count++;
        } while (result->NextRow());

        TC_LOG_INFO("server.loading", ">> KITT [Invite System]: Loaded: {} invites in RAM Cache.", count);
    }
};

class kitt_invite_friend_config : public WorldScript
{
public:
    kitt_invite_friend_config() : WorldScript("kitt_invite_friend_config") {}

    void OnConfigLoad(bool /*reload*/) override
    {
        sKittInviteFriend = sConfigMgr->GetIntDefault("Kitt.Invite.Friend", 0);
    }
};

class kitt_invite_friend : public PlayerScript
{
public:
    kitt_invite_friend() : PlayerScript("kitt_invite_friend") {}

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (sKittInviteFriend == 0)
            return;

        if (!player || !player->IsInWorld() || !player->GetSession())
            return;

        ObjectGuid playerGuid = player->GetGUID();

        player->m_Events.AddEventAtOffset([playerGuid]()
            {
                Player* p = ObjectAccessor::FindPlayer(playerGuid);
                if (!p || !p->IsInWorld() || !p->GetSession())
                    return;

                uint32 myAcc = p->GetSession()->GetAccountId();

                // Trimitem un mesaj informativ discret
                ChatHandler(p->GetSession()).PSendSysMessage("|cff00ff00[Invite System]:|r Invite your friends and earn rewards at level 80!");

                // Verificam daca cel care s-a logat are deja setat un "invitator"
                auto it = RecommendCache.find(myAcc);
                if (it == RecommendCache.end() && p->GetTotalPlayedTime() < 36000)
                {
                    // Daca e nou si nu a setat pe nimeni, ii reamintim
                    ChatHandler(p->GetSession()).PSendSysMessage("|cffFFFF00Tip:|r Were you invited by a friend? Use |cffffffff.zinvite by|r |cff00ff00CharacterName|r to register!");
                }
                else if (it != RecommendCache.end() && p->GetLevel() >= 80 && !it->second.rewardedNew)
                {
                    // Daca are 80 si nu si-a luat premiul de "Nou Venit"
                    ChatHandler(p->GetSession()).PSendSysMessage("|cff00ff00[Reward]:|r Your Welcome Reward is ready! Use |cffffffff.zinvite claim me|r to claim it.");
                }

                ChatHandler(p->GetSession()).PSendSysMessage("Type |cffffffff.zinvite list|r to see your invited friends status.");
                ChatHandler(p->GetSession()).PSendSysMessage("To see the list of rewards, type |cffffffff.zinvite rewards|r.");

            }, 30s);
    }

    void OnLogout(Player* player) override
    {
        if (!player)
            return;

        playtimeCooldownMap.erase(player->GetGUID());
    }
};

class kitt_invite_friend_command : public CommandScript
{
public:
    kitt_invite_friend_command() : CommandScript("kitt_invite_friend_command") {}

    std::vector<ChatCommandBuilder> GetCommands() const override
    {
        static std::vector<ChatCommandBuilder> KittInviteClaimSubcommandTable =
        {
            { "me",     HandleClaimNewbieReward,      rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            { "friend", HandleClaimReferrerReward,    rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
        };

        static std::vector<ChatCommandBuilder> KittInviteSubcommandTable =
        {
            { "by",           HandleZRecomandatCommand,     rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            { "claim",        KittInviteClaimSubcommandTable },
            { "list",         HandleZShowStatisticsCommand, rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
            { "rewards",      HandleZShowRewardsCommand,    rbac::RBAC_PERM_JOIN_NORMAL_BG, Console::No },
        };

        static std::vector<ChatCommandBuilder> KittInviteCommandTable =
        {
            { "zinvite", KittInviteSubcommandTable },
        };

        return KittInviteCommandTable;
    }

    // --- 2. INREGISTRARE RECOMANDARE ---
    static bool HandleZRecomandatCommand(ChatHandler* handler, Optional<std::string_view> args)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me) return true;

        if (sKittInviteFriend == 0)
        {
            handler->SendSysMessage("|cffff0000Error:|r This system is currently disabled in server config.");
            return true;
        }

        uint32 myAcc = me->GetSession()->GetAccountId();

        // 1. Verificam daca a setat deja (Informatii din RAM)
        auto it = RecommendCache.find(myAcc);
        if (it != RecommendCache.end())
        {
            // In structura actuala stocam numele original in DB, 
            // Altfel, afisam un mesaj generic sau cautam in DB. 
            // Presupunand ca am adaugat recomName in struct:
            handler->PSendSysMessage("You have already set |cffffffff%s|r as the person who invited you.", it->second.recomName.c_str());
            return true;
        }

        // 2. Mesaj de ajutor daca nu a scris numele
        if (!args)
        {
            handler->SendSysMessage("Usage: |cffffffff.zinvite by|r |cff00ff00CharacterName|r");
            handler->SendSysMessage("Note: You can only set your referrer within the first 10 hours of play time.");
            return true;
        }

        // 3. Verificare timp (10 ore)
        if (me->GetTotalPlayedTime() > 36000)
        {
            handler->SendSysMessage("You have exceeded the 10-hour limit to set a referrer.");
            return true;
        }

        // 4. Normalizare Nume
        std::string targetName(args.value());
        if (!targetName.empty())
        {
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            targetName[0] = std::toupper(targetName[0]);
        }

        // 5. Verificare tinta
        CharacterCacheEntry const* targetCache = sCharacterCache->GetCharacterCacheByName(targetName);
        if (!targetCache)
        {
            handler->PSendSysMessage("Character |cffffffff'%s'|r does not exist.", targetName.c_str());
            return true;
        }

        if (targetCache->AccountId == myAcc)
        {
            handler->SendSysMessage("You cannot recommend yourself or characters on your own account!");
            return true;
        }

        // --- PROTECTIE FLOOD (Cooldown 10 secunde) ---
        time_t currentTime = GameTime::GetGameTime();

        if (playtimeCooldownMap.count(me->GetGUID()) && currentTime < playtimeCooldownMap[me->GetGUID()])
        {
            uint32 waitTime = playtimeCooldownMap[me->GetGUID()] - currentTime;
            handler->PSendSysMessage("|cffff0000[Anti-Flood]|r Please wait %u seconds before using .zinvite again.", waitTime);
            return true;
        }

        playtimeCooldownMap[me->GetGUID()] = currentTime + playtimeCooldownTime;
        // ----------------------------------------------

        // --- PROTECTIE IP (Anti-Self-Invite) ---
        std::string myIP = me->GetSession()->GetRemoteAddress();
        std::string targetIP = "";

        // Pasul A: Verificam daca tinta este online (Cea mai rapida metoda
        if (Player* targetPlayer = ObjectAccessor::FindConnectedPlayer(targetCache->Guid))
        {
            targetIP = targetPlayer->GetSession()->GetRemoteAddress();
        }
        else
        {
            // Pasul B: Daca e offline, verificam ultimul IP din baza de date 'auth'
            // Luam last_ip din tabelul account folosind AccountId-ul tintei
            QueryResult resultIP = LoginDatabase.PQuery("SELECT last_ip FROM account WHERE id = {}", targetCache->AccountId);
            if (resultIP)
            {
                targetIP = (*resultIP)[0].GetCString();
            }
        }

        if (!targetIP.empty() && myIP == targetIP)
        {
            handler->SendSysMessage("|cffff0000Error:|r You cannot recommend someone who shares the same IP address!");
            return true;
        }

        // --- PROTECRIE VECHIME CONT (SQL) ---
        // Verificam daca contul a fost creat in ultimele 24 de ore
        QueryResult resultJoin = LoginDatabase.PQuery("SELECT id FROM account WHERE id = {} AND joindate > (NOW() - INTERVAL 5 DAY)", myAcc);

        if (!resultJoin)
        {
            //handler->SendSysMessage("|cffff0000Error:|r Your account is too old to participate in this system.");
            handler->SendSysMessage("|cffff0000Error:|r Only accounts created in the last 5 days can set a zinvite by CharacterName.");
            return true;
        }

        // --- PROTECTIE REFERINTA INCRUCISATA (Cross-Invite) ---
        // Verificam daca cel pe care vrei si-l setezi ca invitator (targetCache->AccountId)
        // nu a fost deja invitat de tine (myAcc).

        auto itCheck = RecommendCache.find(targetCache->AccountId);
        if (itCheck != RecommendCache.end())
        {
            if (itCheck->second.referrerAcc == myAcc)
            {
                handler->SendSysMessage("|cffff0000Error:|r Cross-referencing is not allowed!");
                handler->SendSysMessage("This player has already registered you as the person who invited him.");
                return true;
            }
        }
        // ------------------------------------------------------


        // 6. Salvare RAM & DB
        RecommendData& data = RecommendCache[myAcc];
        data.referrerAcc = targetCache->AccountId;
        data.recomName = targetName;
        data.rewardedNew = false;
        data.rewardedRecom = false;

        CharacterDatabase.PExecute("INSERT IGNORE INTO character_kitt_invite_friend (inv_accID, inv_by_accID, inv_by_name) VALUES ({}, {}, '{}')",
            myAcc, targetCache->AccountId, targetName.c_str());

        handler->PSendSysMessage("|cff00ff00Success!|r |cffffffff%s|r has been recorded as the person who invited you.", targetName.c_str());
        return true;
    }

    // --- 3. PREMIU JUCATOR NOU ---
    static bool HandleClaimNewbieReward(ChatHandler* handler)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me) return true;

        if (sKittInviteFriend == 0)
        {
            handler->SendSysMessage("|cffff0000Error:|r This system is currently disabled in server config.");
            return true;
        }

        uint32 myAcc = me->GetSession()->GetAccountId();

        // 1. Verificam daca contul exista in sistemul de invitatii
        auto it = RecommendCache.find(myAcc);
        if (it == RecommendCache.end())
        {
            handler->SendSysMessage("You are not registered in the recommendation system.");
            handler->SendSysMessage("Use |cffffffff.zinvite by|r |cff00ff00CharacterName|r first (only available in the first 10 hours).");
            return true;
        }

        // 2. Verificam daca premiul a fost deja ridicat
        if (it->second.rewardedNew)
        {
            handler->PSendSysMessage("You have already claimed your welcome reward for being invited by |cffffffff%s|r.", it->second.recomName.c_str());
            return true;
        }

        // 3. Verificam nivelul (80)
        if (me->GetLevel() < 80)
        {
            handler->PSendSysMessage("You must reach level 80 to claim this reward. Current level: |cffFF0000%u|r/80.", me->GetLevel());
            return true;
        }

        // 4. Totul este OK: Update RAM & DB
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        it->second.rewardedNew = true;
        UpdateRewardInDB(myAcc, true, trans);

        // 5. Acordarea premiilor (functia GiveRewards se ocupa de mesaje)
        GiveRewards(me, newbieRewards);
        GiveSitePoints(me, KittInviteNewSiteVP, false, trans);
        GiveSitePoints(me, KittInviteNewSiteDP, true, trans);

        me->SaveInventoryAndGoldToDB(trans);

        // 5. Trimitem totul la baza de date deodata
        CharacterDatabase.CommitTransaction(trans);

        handler->SendSysMessage("|cff00ff00Success!|r Your welcome reward has been claimed and saved.");

        return true;
    }

    // --- 4. PREMIU RECRUTATOR (PE NUME PRIETEN) ---
    static bool HandleClaimReferrerReward(ChatHandler* handler, Optional<std::string_view> args)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me) return true;

        if (sKittInviteFriend == 0)
        {
            handler->SendSysMessage("|cffff0000Error:|r This system is currently disabled in server config.");
            return true;
        }

        // 1. Verificam daca argumentul (numele) lipseste
        if (!args)
        {
            handler->SendSysMessage("Usage: |cffffffff.zinvite claim friend|r CharacterName");
            return true;
        }

        uint32 myAcc = me->GetSession()->GetAccountId();

        // 2. Normalizare Nume
        std::string targetName(args.value());
        if (!targetName.empty()) {
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            targetName[0] = std::toupper(targetName[0]);
        }

        // 3. Verificam daca personajul tinta exista in Cache
        CharacterCacheEntry const* targetCache = sCharacterCache->GetCharacterCacheByName(targetName);
        if (!targetCache)
        {
            handler->PSendSysMessage("Character '|cffffffff%s|r' was not found.", targetName.c_str());
            return true;
        }

        uint32 invitedAcc = targetCache->AccountId;

        // 4. Verific?m leg?tura ?n RAM
        auto it = RecommendCache.find(invitedAcc);

        // Situatia A: Nu a fost invitat de nimeni SAU a fost invitat de altcineva
        if (it == RecommendCache.end() || it->second.referrerAcc != myAcc)
        {
            handler->PSendSysMessage("Character |cffffffff%s|r was not invited by you or is not in the system.", targetName.c_str());
            return true;
        }

        // Situatia B: Premiul a fost deja incasat pentru acest prieten
        if (it->second.rewardedRecom)
        {
            handler->PSendSysMessage("You have already claimed the reward for inviting |cffffffff%s|r.", targetName.c_str());
            return true;
        }

        // --- PROTECTIE FLOOD (Cooldown 10 secunde) ---
        time_t currentTime = GameTime::GetGameTime();

        if (playtimeCooldownMap.count(me->GetGUID()) && currentTime < playtimeCooldownMap[me->GetGUID()])
        {
            uint32 waitTime = playtimeCooldownMap[me->GetGUID()] - currentTime;
            handler->PSendSysMessage("|cffff0000[Anti-Flood]|r Please wait %u seconds before using .zinvite again.", waitTime);
            return true;
        }

        playtimeCooldownMap[me->GetGUID()] = currentTime + playtimeCooldownTime;
        // ----------------------------------------------


        // 5. Verificam nivelul (Interogare DB pentru cel mai mare caracter de pe cont)
        QueryResult qLvl = CharacterDatabase.PQuery("SELECT name, level FROM characters WHERE account = {} ORDER BY level DESC LIMIT 1", invitedAcc);
        if (qLvl)
        {
            Field* f = qLvl->Fetch();
            std::string topName = f[0].GetString();
            uint8 topLevel = f[1].GetUInt8();

            if (topLevel >= 80)
            {
                // Totul OK: Update RAM & DB
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                it->second.rewardedRecom = true;
                UpdateRewardInDB(invitedAcc, false, trans);

                // Acordare premii
                GiveRewards(me, referrerRewards);
                GiveSitePoints(me, KittInviteBySiteVP, false, trans);
                GiveSitePoints(me, KittInviteBySiteDP, true, trans);
                me->SaveInventoryAndGoldToDB(trans);

                // 5. Trimitem totul la baza de date deodat?
                CharacterDatabase.CommitTransaction(trans);

                handler->PSendSysMessage("|cff00ff00Success!|r You claimed the reward for inviting |cffffffff %s|r (Main: %s).", targetName.c_str(), topName.c_str());
            }
            else
            {
                // Mesaj clar: Prietenul nu are inca nivelul necesar
                handler->PSendSysMessage("Reward unavailable: |cffffffff %s|r has not reached level 80 yet (Current: %u/80).", topName.c_str(), topLevel);
            }
        }

        return true;
    }

    // --- 5. STATISTICI (Ultra Rapid) ---
    static bool HandleZShowStatisticsCommand(ChatHandler* handler)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me) return true;

        if (sKittInviteFriend == 0)
        {
            handler->SendSysMessage("|cffff0000Error:|r This system is currently disabled in server config.");
            return true;
        }

        //uint32 myAcc = handler->GetSession()->GetAccountId();
        //bool found = false;

        // --- PROTECTIE FLOOD (Cooldown 10 secunde) ---
        time_t currentTime = GameTime::GetGameTime();

        if (playtimeCooldownMap.count(me->GetGUID()) && currentTime < playtimeCooldownMap[me->GetGUID()])
        {
            uint32 waitTime = playtimeCooldownMap[me->GetGUID()] - currentTime;
            handler->PSendSysMessage("|cffff0000[Anti-Flood]|r Please wait %u seconds before using .zinvite again.", waitTime);
            return true;
        }

        playtimeCooldownMap[me->GetGUID()] = currentTime + playtimeCooldownTime;
        // ----------------------------------------------

        uint32 myAcc = handler->GetSession()->GetAccountId();
        bool found = false;
        uint32 inactiveDays = 10; // Pragul de inactivitate zile

        for (auto const& [accNew, data] : RecommendCache)
        {
            if (data.referrerAcc != myAcc) continue;

            // FILTRU 1: Nu afisam daca premiul a fost deja ridicat (pentru a pastra lista scurta)
            if (data.rewardedRecom) continue;

            // Interogare pentru cel mai mare caracter si ultima data de login
            // Adaugam 'logout_time' din tabelul characters (stocat in format Unix timestamp)
            QueryResult q = CharacterDatabase.PQuery(
                "SELECT name, level, logout_time FROM characters WHERE account = {} ORDER BY level DESC LIMIT 1", accNew);

            if (q)
            {
                Field* f = q->Fetch();
                std::string fName = f[0].GetCString();
                uint8 fLvl = f[1].GetUInt8();
                uint32 lastLogout = f[2].GetUInt32();

                // FILTRU 2: Daca nu e level 80 si nu a mai intrat de X zile, il ignoram
                //time_t currentTime = GameTime::GetGameTime();
                if (fLvl < 80 && (currentTime - lastLogout) >(static_cast<long long>(inactiveDays) * DAY))
                    continue;

                if (!found)
                {
                    handler->SendSysMessage("|cff00ff00=== [ Your Active Invitations ] ===|r");
                    handler->SendSysMessage("|cff00ccff(Showing only players in progress or ready for reward)|r");
                    handler->SendSysMessage("---------------------------------------------");
                    found = true;
                }

                std::string status = (fLvl >= 80) ? "|cff00ff00[REWARD READY]|r" : "|cffFF0000[IN PROGRESS]|r";
                handler->PSendSysMessage("- |cff00ff00%s|r | Level: %u | Status: %s", fName.c_str(), fLvl, status.c_str());
            }
        }

        if (!found)
        {
            handler->SendSysMessage("|cff00ccffYou have no active invitations to display.|r");
            handler->SendSysMessage("|cffffffffInactive friends or completed rewards are hidden.|r");
        }
        else
        {
            handler->SendSysMessage("---------------------------------------------");
            handler->SendSysMessage("To claim a reward, use: |cffffffff.zinvite claim friend|r Name");
        }

        return true;
    }

    // --- 6 Show items Reward ---
    static bool HandleZShowRewardsCommand(ChatHandler* handler)
    {
        Player* me = handler->GetSession()->GetPlayer();
        if (!me) return true;

        if (sKittInviteFriend == 0)
        {
            handler->SendSysMessage("|cffff0000Error:|r This system is currently disabled in server config.");
            return true;
        }

        // --- PROTECTIE FLOOD (Cooldown 10 secunde) ---
        time_t currentTime = GameTime::GetGameTime();

        if (playtimeCooldownMap.count(me->GetGUID()) && currentTime < playtimeCooldownMap[me->GetGUID()])
        {
            uint32 waitTime = playtimeCooldownMap[me->GetGUID()] - currentTime;
            handler->PSendSysMessage("|cffff0000[Anti-Flood]|r Please wait %u seconds before using .zinvite again.", waitTime);
            return true;
        }

        playtimeCooldownMap[me->GetGUID()] = currentTime + playtimeCooldownTime;
        // ----------------------------------------------

        handler->SendSysMessage("|cff00ff00=== [ Invitation System Rewards ] ===|r");

        // 1. Afisam premiile pentru cel NOU (Invitat)
        handler->SendSysMessage("|cffFFFF00Welcome Rewards (for the invited player):|r");
        if (newbieRewards.empty())
            handler->SendSysMessage("- No rewards configured yet.");
        else
        {
            for (auto const& reward : newbieRewards)
            {
                if (ItemTemplate const* temp = sObjectMgr->GetItemTemplate(reward.ItemId))
                    handler->PSendSysMessage("- |cff0070dd|Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h|r x%u",
                        reward.ItemId, temp->Name1.c_str(), reward.Count);
            }
        }

        handler->SendSysMessage(" "); // Linie goala pentru separare

        // 2. Afisam premiile pentru RECRUTATOR
        handler->SendSysMessage("|cffFFFF00Recruiter Rewards (for inviting a friend):|r");
        bool hasRewards = false;

        // Afisam Iteme
        if (!referrerRewards.empty())
        {
            for (auto const& reward : referrerRewards)
            {
                if (ItemTemplate const* temp = sObjectMgr->GetItemTemplate(reward.ItemId))
                {
                    handler->PSendSysMessage("- |cff0070dd|Hitem:%u:0:0:0:0:0:0:0:0|h[%s]|h|r x%u",
                        reward.ItemId, temp->Name1.c_str(), reward.Count);
                    hasRewards = true;
                }
            }
        }

        // Afisam Puncte de Site (VP/DP) daca sunt setate
        if (KittInviteBySiteVP > 0)
        {
            handler->PSendSysMessage("- |cffffffff%u Vote Points (VP)|r on our Website Store", KittInviteBySiteVP);
            hasRewards = true;
        }

        if (KittInviteBySiteDP > 0)
        {
            handler->PSendSysMessage("- |cffffffff%u Donation Points (DP)|r on our Website Store", KittInviteBySiteDP);
            hasRewards = true;
        }

        if (!hasRewards)
            handler->SendSysMessage("- No recruiter rewards configured yet.");

        //handler->SendSysMessage(" ");
        handler->SendSysMessage("---------------------------------------------");
        handler->SendSysMessage("|cff00ff00Requirements:|r The invited player must reach |cffFFFF00Level 80|r.");
        handler->SendSysMessage("|cffFFFF00To claim your rewards, use:|r");
        handler->SendSysMessage("- |cffffffff.zinvite claim me|r (If you were invited)");
        handler->SendSysMessage("- |cffffffff.zinvite claim friend Name|r (If you invited a friend)");
        handler->SendSysMessage("---------------------------------------------");
        return true;
    }
};

void AddSC_kitt_invite_friend()
{
    new kitt_invite_friend_startup();
    new kitt_invite_friend_config();
    new kitt_invite_friend_command();
    new kitt_invite_friend();
}
