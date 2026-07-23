//! RSS AI Combat Decay — 战斗衰减矩阵
//!
//! 根据体力状态分维度衰减 AI 战斗力：
//!   感知（SetPerceptionFactor） / 射速（SetFireRateCoef） / 技能（SetAISkill）
//!
//! 全部通过 SCR_AICombatComponent 公开 API，需在威胁系统初始化后调用。

class SCR_RSS_AICombatDecay
{
    //------------------------------------------------------------------------------------------------
    //! 主入口
    static void Apply(IEntity owner, ERSS_AIStaminaState state)
    {
        if (!SCR_RSS_ConfigBridge.IsAIStaminaCombatEffectsEnabled())
            return;
        if (!owner)
            return;
        if (!Replication.IsServer())
            return;

        SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(owner.FindComponent(SCR_AICombatComponent));
        if (!combat)
            return;

        // 根据状态取衰减系数
        float perceptionMul, fireRateMul, skillMul;
        GetDecayParams(state, perceptionMul, fireRateMul, skillMul);

        // 感知
        if (IsAIThreatSystemReady(owner))
            combat.SetPerceptionFactor(perceptionMul);

        // 射速
        combat.SetFireRateCoef(fireRateMul, false);

        // 技能：以预制体默认技能为基准缩放
        EAISkill refSkill = combat.GetAISkillDefault();
        int vRef = AISkillToValue(refSkill);
        if (vRef < 0)
            vRef = 0;

        float diffPerception = perceptionMul - 1.0;
        if (diffPerception < 0.0)
            diffPerception = -diffPerception;
        float diffFireRate = fireRateMul - 1.0;
        if (diffFireRate < 0.0)
            diffFireRate = -diffFireRate;
        if (diffPerception < 0.001 && diffFireRate < 0.001)
        {
            combat.ResetAISkill();
            return;
        }

        int blendedInt = Math.Round(vRef * skillMul);
        if (blendedInt < 0)
            blendedInt = 0;
        EAISkill sk = ValueToNearestAISkill(blendedInt);
        combat.SetAISkill(sk);
    }

    //------------------------------------------------------------------------------------------------
    protected static void GetDecayParams(
        ERSS_AIStaminaState state,
        out float perceptionMul,
        out float fireRateMul,
        out float skillMul)
    {
        switch (state)
        {
        case ERSS_AIStaminaState.FRESH:
            perceptionMul = 1.0;
            fireRateMul = 1.0;
            skillMul = 1.0;
            break;

        case ERSS_AIStaminaState.WINDED:
            perceptionMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_PERCEPTION_WINDED;
            fireRateMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_FIRE_RATE_WINDED;
            skillMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_SKILL_WINDED;
            break;

        case ERSS_AIStaminaState.FATIGUED:
            perceptionMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_PERCEPTION_FATIGUED;
            fireRateMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_FIRE_RATE_FATIGUED;
            skillMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_SKILL_FATIGUED;
            break;

        case ERSS_AIStaminaState.EXHAUSTED:
            perceptionMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_PERCEPTION_EXHAUSTED;
            fireRateMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_FIRE_RATE_EXHAUSTED;
            skillMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_SKILL_EXHAUSTED;
            break;

        case ERSS_AIStaminaState.COLLAPSED:
            perceptionMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_PERCEPTION_COLLAPSED;
            fireRateMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_FIRE_RATE_COLLAPSED;
            skillMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_SKILL_COLLAPSED;
            break;

        case ERSS_AIStaminaState.RECOVERING:
            perceptionMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_PERCEPTION_RECOVERING;
            fireRateMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_FIRE_RATE_RECOVERING;
            skillMul = SCR_RSS_AIConstants.RSS_AI_COMBAT_SKILL_RECOVERING;
            break;

        default:
            perceptionMul = 1.0;
            fireRateMul = 1.0;
            skillMul = 1.0;
            break;
        }
    }

    //------------------------------------------------------------------------------------------------
    protected static bool IsAIThreatSystemReady(IEntity owner)
    {
        AIControlComponent aiControl = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
        if (!aiControl)
            return false;
        AIAgent agent = aiControl.GetAIAgent();
        if (!agent)
            return false;
        SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(agent.FindComponent(SCR_AIInfoComponent));
        if (!info)
            return false;
        return info.GetThreatSystem() != null;
    }

    //------------------------------------------------------------------------------------------------
    protected static int AISkillToValue(EAISkill skill)
    {
        switch (skill)
        {
        case EAISkill.NOOB:    return 10;
        case EAISkill.ROOKIE:  return 20;
        case EAISkill.REGULAR: return 50;
        case EAISkill.VETERAN: return 70;
        case EAISkill.EXPERT:  return 80;
        case EAISkill.CYLON:   return 100;
        }
        return 50;
    }

    //------------------------------------------------------------------------------------------------
    protected static EAISkill ValueToNearestAISkill(int v)
    {
        if (v <= 0)
        {
            return EAISkill.NONE;
        }
        if (v < 15)
        {
            return EAISkill.NOOB;
        }
        if (v < 35)
        {
            return EAISkill.ROOKIE;
        }
        if (v < 60)
        {
            return EAISkill.REGULAR;
        }
        if (v < 75)
        {
            return EAISkill.VETERAN;
        }
        if (v < 90)
        {
            return EAISkill.EXPERT;
        }
                              return EAISkill.CYLON;
    }
}
