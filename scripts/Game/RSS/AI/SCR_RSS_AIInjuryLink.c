//! RSS AI Injury Link — 伤害-体力联动
//!
//! 受伤 AI 体力消耗更快、恢复更慢。
//! 在 SCR_StaminaConsumption / SCR_StaminaRecovery 中调用修正消耗/恢复系数。

class SCR_RSS_AIInjuryLink
{
    //------------------------------------------------------------------------------------------------
    //! 获取 AI 实体的伤害系数（消耗加速 + 恢复惩罚）
    //! \param owner  实体
    //! \param outDrainMul  输出的消耗倍率（≥1.0）
    //! \param outRecoveryMul  输出的恢复倍率（≤1.0）
    //! \return 是否成功获取伤害信息
    static bool GetInjuryMultipliers(
        IEntity owner,
        out float outDrainMul,
        out float outRecoveryMul)
    {
        outDrainMul = 1.0;
        outRecoveryMul = 1.0;

        if (!StaminaConfigBridge.IsAIInjuryLinkEnabled())
            return false;
        if (!owner)
            return false;
        if (!Replication.IsServer())
            return false;

        SCR_CharacterDamageManagerComponent dmgMgr = SCR_CharacterDamageManagerComponent.Cast(
            owner.FindComponent(SCR_CharacterDamageManagerComponent));
        if (!dmgMgr)
            return false;

        SCR_CharacterBloodHitZone blood = dmgMgr.GetBloodHitZone();
        if (!blood)
            return false;

        float health = blood.GetHealth();
        float maxHealth = blood.GetMaxHealth();
        if (maxHealth <= 0.0)
            return false;

        float ratio = Math.Clamp(health / maxHealth, 0.0, 1.0);

        if (ratio > 0.9)
        {
            outDrainMul = 1.0;
            outRecoveryMul = 1.0;
        }
        else if (ratio > 0.7)
        {
            outDrainMul = StaminaConstants.RSS_AI_INJURY_DRAIN_MILD;
            outRecoveryMul = StaminaConstants.RSS_AI_INJURY_RECOVERY_MILD;
        }
        else if (ratio > 0.5)
        {
            outDrainMul = StaminaConstants.RSS_AI_INJURY_DRAIN_MODERATE;
            outRecoveryMul = StaminaConstants.RSS_AI_INJURY_RECOVERY_MODERATE;
        }
        else if (ratio > 0.3)
        {
            outDrainMul = StaminaConstants.RSS_AI_INJURY_DRAIN_SEVERE;
            outRecoveryMul = StaminaConstants.RSS_AI_INJURY_RECOVERY_SEVERE;
        }
        else
        {
            outDrainMul = StaminaConstants.RSS_AI_INJURY_DRAIN_CRITICAL;
            outRecoveryMul = StaminaConstants.RSS_AI_INJURY_RECOVERY_CRITICAL;
        }

        return true;
    }
}
