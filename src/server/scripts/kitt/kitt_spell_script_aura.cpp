#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ReputationMgr.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "SpellAuraDefines.h"
#include "SpellHistory.h"
#include "DBCStores.h"
#include "Item.h"
#include "Creature.h"
#include "Containers.h"
#include "WorldSession.h"


// INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
// (42683, 'kitt_spell_fly_mount_aura')
// spell ID      = 47977
// aura map 0,1  = 42683
class kitt_spell_fly_mount_aura : public AuraScript
{
    PrepareAuraScript(kitt_spell_fly_mount_aura);

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
        {
            // VERIFICARE: Verificam daca aura a fost creata de item-ul tau custom
            // Cautam item-ul in baza ID-ului tau (900899)
            bool hasCustomItem = false;

            // facem o verificare secundar?: are jucatorul item-ul custom in inventar?
            if (!hasCustomItem && player->HasItemCount(6948, 1))
                hasCustomItem = true;

            if (hasCustomItem)
            {
                player->SetCanFly(true);
                player->SetSpeedRate(MOVE_RUN, 2.0f);
                player->SetSpeedRate(MOVE_FLIGHT, 2.0f);
            }
        }
    }

    // Folosim semnatura confirmata de tine cu AuraEffectHandleModes
    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
        {
            player->SetCanFly(false);
            player->SetSpeedRate(MOVE_FLIGHT, 1.0f);
            player->SetSpeedRate(MOVE_RUN, 1.0f);

            // Reset hover vizual
            //player->SetByteValue(UNIT_FIELD_BYTES_1, 3, 0);
        }
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(kitt_spell_fly_mount_aura::OnApply, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(kitt_spell_fly_mount_aura::OnRemove, EFFECT_0, SPELL_AURA_ANY, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_kitt_spell_script_aura()
{
    RegisterSpellScript(kitt_spell_fly_mount_aura);
}
