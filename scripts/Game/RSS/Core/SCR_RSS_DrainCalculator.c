//! v5 消耗测速：v_drain = clamp(v_measured, 0, v_theoretical_max)

class SCR_RSS_DrainCalculator
{
    //! 统一陆地/游泳消耗速度输入，禁止 Run/Sprint 固定 3.8/5.5。
    //! @param measuredSpeedMs GetVelocity 水平模长（m/s）
    //! @param theoreticalMaxMs 当前档位理论上限（m/s）
    //! @return 用于 Pandolf 的速度（m/s）
    static float GetDrainVelocityMs(float measuredSpeedMs, float theoreticalMaxMs)
    {
        if (measuredSpeedMs < 0.0)
            measuredSpeedMs = 0.0;
        if (theoreticalMaxMs < 0.05)
            theoreticalMaxMs = 0.05;
        if (measuredSpeedMs > theoreticalMaxMs)
            return theoreticalMaxMs;
        return measuredSpeedMs;
    }

    //! 按移动相位返回理论速度上限（v5 行军档）
    static float GetTheoreticalMaxSpeedMs(int movementPhase, float encumbranceSpeedPenalty)
    {
        float walk = SCR_RSS_ConfigBridge.GetV5WalkSpeedMs();
        float run = SCR_RSS_ConfigBridge.GetV5RunSpeedMs();
        float sprint = SCR_RSS_ConfigBridge.GetV5SprintSpeedMs();

        float encMult = 1.0 - encumbranceSpeedPenalty;
        if (encMult < 0.5)
            encMult = 0.5;

        walk = walk * encMult;
        run = run * encMult;
        sprint = sprint * encMult;

        if (movementPhase == 3)
            return sprint;
        if (movementPhase == 2)
            return run;
        if (movementPhase == 1)
            return walk;
        return walk;
    }

    //! Pandolf 功率（W）是否超过可持续阈值 → 触发渐进降速系数（0-1）
    static float GetMetabolicOverspeedFactor(float pandolfWatts)
    {
        float sustainable = SCR_RSS_ConfigBridge.GetSustainableWatts();
        if (sustainable <= 1.0)
            sustainable = SCR_RSS_Constants.V5_SUSTAINABLE_WATTS_DEFAULT;
        if (pandolfWatts <= sustainable)
            return 1.0;

        float ratio = sustainable / pandolfWatts;
        if (ratio < SCR_RSS_Constants.V5_MIN_METABOLIC_SPEED_FACTOR)
            ratio = SCR_RSS_Constants.V5_MIN_METABOLIC_SPEED_FACTOR;
        return ratio;
    }

    //! 代谢超速时对已应用速度倍率做渐进缩放（v5 速度—消耗闭环）
    //! @param appliedSpeedMultiplier 当前 RSS 速度倍率
    //! @param currentSpeedMs 实测水平速度（m/s）
    //! @param movementPhase 移动相位（0 idle … 3 sprint）
    //! @param encumbranceSpeedPenalty 负重速度惩罚 [0,1]
    //! @param totalWeightKg 湿重+身体总重（kg）
    //! @param gradePercent 坡度百分比
    //! @param terrainFactor 地形系数
    //! @param isExhausted 精疲力尽时跳过降速
    //! @return 修正后倍率；未超速或与入参相同时不变
    static float GetMetabolicCorrectedSpeedMultiplier(
        float appliedSpeedMultiplier,
        float currentSpeedMs,
        int movementPhase,
        float encumbranceSpeedPenalty,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        bool isExhausted)
    {
        if (isExhausted)
            return appliedSpeedMultiplier;

        float theoreticalMaxMs = GetTheoreticalMaxSpeedMs(movementPhase, encumbranceSpeedPenalty);
        float vDrainMs = GetDrainVelocityMs(currentSpeedMs, theoreticalMaxMs);
        float pandolfWatts = SCR_RSS_MetabolismMath.CalculatePandolfEnergyExpenditure(
            vDrainMs, totalWeightKg, gradePercent, terrainFactor, true);
        float metaFactor = GetMetabolicOverspeedFactor(pandolfWatts);
        if (metaFactor >= 0.999)
            return appliedSpeedMultiplier;

        return Math.Clamp(appliedSpeedMultiplier * metaFactor, 0.01, 3.0);
    }
}
