#include "ScriptPCH.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Player.h"
#include "Define.h"

class kitt_go : public GameObjectScript
{
public:
    kitt_go() : GameObjectScript("kitt_go") {}
    struct kitt_goAI : public GameObjectAI
    {
        kitt_goAI(GameObject* go) : GameObjectAI(go) {}

        bool OnGossipHello(Player* player) override
        {
            if (me->GetGOInfo())
            {
                switch (me->GetGOInfo()->entry) {
                case 501001:
                    if (player->GetLevel() >= 1)
                    {
                        if (player->GetTeam() == ALLIANCE)
                        {
                            //Orgrimmar out //17609
                            player->TeleportTo(1, 1107.23f, -4360.45f, 26.2786f, 6.05629f);
                        }
                        else if (player->GetTeam() == HORDE)
                        {
                            //Orgrimmar in //17609
                            player->TeleportTo(1, 1493.17f, -4414.95f, 23.0400f, 0.0f);
                        }
                    }
                    break;
                case 501002: player->TeleportTo(0, 1827.60f, 325.311f, 72.4703f, 0.0f); break;	//Undercity//17611
                case 501003: player->TeleportTo(1, -1386.75f, 138.474f, 23.0348f, 0.0f); break;	//ThunderBluff//17610
                case 501004: player->TeleportTo(530, 9380.18f, -7277.81f, 14.2404f, 0.0f); break;	//Silvermoon//32270
                case 501005:
                    if (player->GetLevel() >= 1)
                    {
                        if (player->GetTeam() == ALLIANCE)
                        {
                            //Stormwind in //17334
                            player->TeleportTo(0, -8837.57f, 637.394f, 94.8055f, 4.96371f);
                        }
                        else if (player->GetTeam() == HORDE)
                        {
                            //Stormwind out //17334
                            player->TeleportTo(0, -9121.74f, 286.842f, 100.404f, 1.33343f);
                        }
                    }
                    break;

                case 501006: player->TeleportTo(1, 9946.02f, 2588.77f, 1316.19f, 0.0f); break;	//Darnassus//17608
                case 501007: player->TeleportTo(0, -5002.33f, -857.664f, 497.054f, 0.0f); break;	//Ironforge//44089
                case 501008: player->TeleportTo(530, -4004.39f, -11878.5f, -1.01131f, 0.0f); break;	//Exodar//32268
                case 501009: player->TeleportTo(0, -13234.2f, 215.180f, 31.4339f, 0.0f); break;	//PVP Arena
                case 501015: player->TeleportTo(1, 4674.88f, -3638.37f, 966.264f, 0.0f); break;	//Event Zone
                case 501016:
                    //teleport to Shattrath

                    if (player->GetLevel() >= 60)
                    {
                        if (player->GetTeam() == ALLIANCE)
                        {
                            player->TeleportTo(530, -1812.93f, 5452.93f, 2.68198f, 3.5534f);
                        }
                        else if (player->GetTeam() == HORDE)
                        {
                            player->TeleportTo(530, -1913.26f, 5404.86f, 2.74473f, 0.0f);
                        }
                    }
                    break;
                case 501017: if (player->GetLevel() >= 50) {
                    player->TeleportTo(0, -7809.91f, -1133.80f, 214.656f, 0.0f);
                } break;	//Burning Stepps
                case 501018: if (player->GetLevel() >= 80) {
                    player->TeleportTo(571, 3669.64f, -1257.94f, 243.548f, 4.86877f);
                } break;	//Naxxramas
                case 501019: if (player->GetLevel() >= 60) {
                    player->TeleportTo(530, 6880.97f, -7951.26f, 170.099f, 0.0f);
                } break;	//Zun'Aman
                case 501021: if (player->GetLevel() >= 70) {
                    player->TeleportTo(571, 5804.15f, 624.771f, 647.767f, 0.0f);
                } break;	//Dalaran
                case 501022: if (player->GetLevel() >= 55) {
                    player->TeleportTo(609, 2390.51, -5640.49, 377.954, 0.0f);
                } break;	//DK1 jos DK Hall of Command
                case 501023: if (player->GetLevel() >= 55) {
                    player->TeleportTo(609, 2365.47, -5656.12, 426.083, 0.0f);
                } break;	//DK2 sus DK start area
                case 501024: if (player->GetLevel() >= 70) {
                    player->TeleportTo(571, 2828.46, 6177.56, 121.982, 0.0f);
                } break;	//Durotar to Warsong Hold
                case 501025: if (player->GetLevel() >= 70) {
                    player->TeleportTo(1, 1179.81, -4147.89, 52.3335, 0.0f);
                } break;	//Warsong Hold to Durotar
                case 501026: if (player->GetLevel() >= 70) {
                    player->TeleportTo(571, 2231.51, 5132.28, 5.3438, 0.0f);
                } break;	//Stormwind to Valiance Keep
                case 501027: if (player->GetLevel() >= 70) {
                    player->TeleportTo(0, -8291.1, 1404.3, 4.72375, 0.0f);
                } break;		//Valiance Keep to Stormwind
                case 501020: player->TeleportTo(0, -4854.04, -1004.33, 453.76, 0.0f); break;	//Portal Zone
                    /*            case 501020:
                                    if (player->GetLevel() >= 1 )
                                    {
                                        if (player->GetTeam() == ALLIANCE)
                                        {
                                             //old ironforge //portal zone
                                            player->TeleportTo(0, -4854.04, -1004.33, 453.76, 0.0f);
                                        }
                                        else if (player->GetTeam() == HORDE)
                                        {
                                             //isle tanaris //portal zone
                                            player->TeleportTo(1, -11813.0, -4749.61, 6.18968, 4.81013f);
                                        }
                                    }
                                    break;*/

                case 501028: player->TeleportTo(0, -14890.9, 454.866, 6.38416, 0.0f); break;	//event 900
                case 501029: player->TeleportTo(1, -4995.06, -2961.62, 71.8169, 0.0f); break;	//event 901

                case 501100: player->TeleportTo(603, 924.092, -6.90067, 418.588, 0.0f); break;	//Ulduar A
                case 501101: player->TeleportTo(603, 1497.23, -28.6204, 420.967, 0.0f); break;	//Ulduar B
                case 501102: player->TeleportTo(603, 2356.05, 2567.9, 419.633, 0.0f); break;	//Ulduar D

                case 501103: player->TeleportTo(578, 1285.59, 1070.35, 439.515, 0.0f); break;	//Ocu boss 2
                case 501104: player->TeleportTo(578, 1120.02, 921.552, 526.811, 0.0f); break;	//Ocu boss 3
                case 501105: player->TeleportTo(578, 1022.03, 1045.00, 605.626, 0.0f); break;	//Ocu boss 4

                case 202243: player->TeleportTo(631, -548.908, 2206.96, 539.290, 0.0f); break;	//ICC B
                case 202245: player->TeleportTo(631, -503.285, 2210.41, 62.8242, 0.0f); break;	//ICC A
                case 501106: player->TeleportTo(631, 516.173, -2124.75, 1042.99, 0.0f); break;	//ICC LK
                }
            }
            return false;
        }
    };
    GameObjectAI* GetAI(GameObject* go) const override
    {
        return new kitt_goAI(go);
    }
};


void AddSC_kitt_go()
{
    new kitt_go();
}
