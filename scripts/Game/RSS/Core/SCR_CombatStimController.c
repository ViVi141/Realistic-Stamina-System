// 战斗 Stim 控制器模块
// 从 PlayerBase.c 拆分，管理 CombatStim 状态转换与流血倍率

class SCR_CombatStimController
{
    static float ComputeBleedingBaseRateForEffect(SCR_BleedingDamageEffect bleed)
    {
        if (!bleed)
            return 0.0;
        SCR_CharacterHitZone hz = SCR_CharacterHitZone.Cast(bleed.GetAffectedHitZone());
        if (!hz)
            return 0.0;
        return hz.GetMaxBleedingRate() - hz.GetMaxBleedingRate() * hz.GetHealthScaled();
    }

    static void RefreshBleedingEffectsToMatchScale(SCR_CharacterDamageManagerComponent dmgMgr)
    {
        if (!dmgMgr)
            return;
        float scale = dmgMgr.GetBleedingScale();
        array<ref SCR_PersistentDamageEffect> effects = dmgMgr.GetAllPersistentEffectsOfType(SCR_BleedingDamageEffect);
        foreach (SCR_PersistentDamageEffect pe : effects)
        {
            SCR_BleedingDamageEffect bleed = SCR_BleedingDamageEffect.Cast(pe);
            if (!bleed)
                continue;
            bleed.SetDPS(ComputeBleedingBaseRateForEffect(bleed) * scale);
        }
    }

    static void UpdateBleedingScale(
        int combatStimPhase,
        float bleedingBaseline,
        ChimeraCharacter ownerCharacter,
        out float newBleedingBaseline)
    {
        newBleedingBaseline = bleedingBaseline;
        SCR_CharacterDamageManagerComponent dmgMgr = SCR_CharacterDamageManagerComponent.Cast(ownerCharacter.GetDamageManager());
        if (!dmgMgr)
            return;

        bool wantBuff = false;
        float mult = 1.0;
        if (combatStimPhase == ERSS_CombatStimPhase.ACTIVE)
        {
            wantBuff = true;
            mult = SCR_CombatStimConstants.BLEEDING_SCALE_MULT_ACTIVE;
        }
        else if (combatStimPhase == ERSS_CombatStimPhase.OD)
        {
            wantBuff = true;
            mult = SCR_CombatStimConstants.BLEEDING_SCALE_MULT_ACTIVE * SCR_CombatStimConstants.BLEEDING_SCALE_MULT_OD_EXTRA;
        }

        if (!wantBuff)
        {
            if (bleedingBaseline >= 0.0)
            {
                dmgMgr.SetBleedingScale(bleedingBaseline, true);
                RefreshBleedingEffectsToMatchScale(dmgMgr);
                newBleedingBaseline = -1.0;
            }
            return;
        }

        if (bleedingBaseline < 0.0)
            newBleedingBaseline = dmgMgr.GetBleedingScale();

        dmgMgr.SetBleedingScale(newBleedingBaseline * mult, true);
        RefreshBleedingEffectsToMatchScale(dmgMgr);
    }

    static void ResetBleedingScaleBeforeKill(
        ChimeraCharacter ownerCharacter,
        float bleedingBaseline,
        out float newBleedingBaseline)
    {
        newBleedingBaseline = bleedingBaseline;
        if (bleedingBaseline < 0.0)
            return;

        SCR_CharacterDamageManagerComponent dmgMgr = SCR_CharacterDamageManagerComponent.Cast(ownerCharacter.GetDamageManager());
        if (dmgMgr)
        {
            dmgMgr.SetBleedingScale(bleedingBaseline, true);
            RefreshBleedingEffectsToMatchScale(dmgMgr);
        }
        newBleedingBaseline = -1.0;
    }
}
