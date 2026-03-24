//! 体力 → AI 战斗表现（无侵入原版）：仅调用 SCR_AICombatComponent 公开 API。
//! - 感知：SetPerceptionFactor（影响视觉发现速度，见官方组件注释）
//! - 反应/输出节奏：SetFireRateCoef
//! - 战斗技能：以 GetAISkillDefault() 对应数值为 100% 基准，有效值 = vRef × 体力比例，再映射回 EAISkill；
//!   满体力 ResetAISkill() 恢复预制体设定，不会把「已设为最弱」的单位抬成更强档位。
//!
//! 开关：JSON m_bEnableAIStaminaCombatEffects（StaminaConstants.IsAIStaminaCombatEffectsEnabled），默认关闭

class SCR_RSS_AIStaminaCombatEffects
{
    //------------------------------------------------------------------------------------------------
    protected static int AISkillToValue(EAISkill skill)
    {
        if (skill == EAISkill.NONE)
            return 0;
        if (skill == EAISkill.NOOB)
            return 10;
        if (skill == EAISkill.ROOKIE)
            return 20;
        if (skill == EAISkill.REGULAR)
            return 50;
        if (skill == EAISkill.VETERAN)
            return 70;
        if (skill == EAISkill.EXPERT)
            return 80;
        if (skill == EAISkill.CYLON)
            return 100;
        return 50;
    }

    //------------------------------------------------------------------------------------------------
    protected static EAISkill ValueToNearestAISkill(int v)
    {
        if (v <= 0)
            return EAISkill.NONE;
        if (v < 15)
            return EAISkill.NOOB;
        if (v < 35)
            return EAISkill.ROOKIE;
        if (v < 60)
            return EAISkill.REGULAR;
        if (v < 75)
            return EAISkill.VETERAN;
        if (v < 90)
            return EAISkill.EXPERT;
        return EAISkill.CYLON;
    }

    //------------------------------------------------------------------------------------------------
    //! 服务器 AI + 开关开启时，按体力缩放感知、射速；战斗技能按「预制体默认技能数值 × 体力比例」计算。
    static void ApplyStaminaToCombat(IEntity owner, float staminaPercent01)
    {
        if (!StaminaConstants.IsAIStaminaCombatEffectsEnabled())
            return;
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;

        SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(owner.FindComponent(SCR_AICombatComponent));
        if (!combat)
            return;

        float s = staminaPercent01;
        if (s < 0.0)
            s = 0.0;
        if (s > 1.0)
            s = 1.0;

        float pMin = StaminaConstants.RSS_AI_STAMINA_COMBAT_PERCEPTION_MIN;
        float pMul = pMin + (1.0 - pMin) * s;
        combat.SetPerceptionFactor(pMul);

        float fMin = StaminaConstants.RSS_AI_STAMINA_COMBAT_FIRE_RATE_MIN;
        float fireCoef = fMin + (1.0 - fMin) * s;
        combat.SetFireRateCoef(fireCoef, false);

        // 基准：预制体/服务器为该单位配置的默认战斗技能（不是向全局「最强」插值）
        EAISkill refSkill = combat.GetAISkillDefault();
        int vRef = AISkillToValue(refSkill);
        if (vRef < 0)
            vRef = 0;

        if (s >= 0.999)
        {
            combat.ResetAISkill();
            return;
        }

        float vScaled = vRef * s;
        int blendedInt = Math.Round(vScaled);
        if (blendedInt < 0)
            blendedInt = 0;

        EAISkill sk = ValueToNearestAISkill(blendedInt);
        combat.SetAISkill(sk);
    }
}
