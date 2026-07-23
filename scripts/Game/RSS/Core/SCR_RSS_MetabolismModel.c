//! v6 代谢功率模型：Walk=LCDA 背包式；Run/Sprint=Pandolf+ACSM；功率反解
//! 与 Python rss_digital_twin_fix.py 保持同形

class SCR_RSS_MetabolismModel
{
    //! LCDA backpacking + graded walking（W）；body-mass specific × CHARACTER_WEIGHT
    //! 适用范围：站立 / Walk，v ≤ LCDA_MAX_SPEED_MS；坡度项已含缓/陡下坡，不再叠 Santee
    static float CalculateLcdaBackpackPowerWatts(
        float velocityMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor)
    {
        float bodyKg = SCR_RSS_Constants.CHARACTER_WEIGHT;
        bodyKg = Math.Max(bodyKg, 1.0);
        totalWeightKg = Math.Max(totalWeightKg, 0.0);
        terrainFactor = Math.Clamp(terrainFactor, 0.5, 3.0);

        float loadKg = Math.Max(totalWeightKg - bodyKg, 0.0);
        float loadRatio = loadKg / bodyKg;
        float loadMult = 1.0 + SCR_RSS_Constants.LCDA_LOAD_COEFF
            * Math.Pow(loadRatio, SCR_RSS_Constants.LCDA_LOAD_EXP);

        float speedMs = Math.Max(velocityMs, 0.0);
        float walkNet = 0.0;
        if (speedMs >= 0.1)
        {
            float fracTerm = SCR_RSS_Constants.LCDA_SPEED_FRAC_COEFF
                * Math.Pow(speedMs, SCR_RSS_Constants.LCDA_SPEED_FRAC_EXP);
            float speedSq = speedMs * speedMs;
            float quarticTerm = SCR_RSS_Constants.LCDA_SPEED_QUARTIC_COEFF
                * speedSq * speedSq;
            walkNet = terrainFactor * (fracTerm + quarticTerm);
        }

        float gradeDecimal = gradePercent * 0.01;
        float gradeTerm = 0.0;
        if (speedMs >= 0.1 && Math.AbsFloat(gradeDecimal) > 0.0001)
        {
            float gradeExp = 100.0 * gradeDecimal + SCR_RSS_Constants.LCDA_GRADE_OFFSET;
            float inner = Math.Pow(SCR_RSS_Constants.LCDA_GRADE_BASE_B, gradeExp);
            float outer = Math.Pow(SCR_RSS_Constants.LCDA_GRADE_BASE_A, 1.0 - inner);
            gradeTerm = SCR_RSS_Constants.LCDA_GRADE_COEFF * speedMs * gradeDecimal
                * (1.0 - outer);
        }

        float mPerKg = SCR_RSS_Constants.LCDA_REST_W_PER_KG
            + (SCR_RSS_Constants.LCDA_STAND_NET_W_PER_KG + walkNet + gradeTerm) * loadMult;
        return Math.Max(mPerKg * bodyKg, 0.0);
    }

    //! 纯 Pandolf 功率（W），不含 ACSM 混合
    static float CalculatePandolfPowerWatts(
        float velocityMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        bool useSanteeCorrection)
    {
        velocityMs = Math.Max(velocityMs, 0.0);
        totalWeightKg = Math.Max(totalWeightKg, 0.0);
        terrainFactor = Math.Clamp(terrainFactor, 0.5, 3.0);

        if (velocityMs < 0.1)
        {
            float bodyWeight = SCR_RSS_Constants.CHARACTER_WEIGHT;
            float loadWeight = Math.Max(totalWeightKg - bodyWeight, 0.0);
            return CalculateStaticStandingPowerWatts(bodyWeight, loadWeight);
        }

        float velocityTerm = velocityMs - SCR_RSS_Constants.PANDOLF_VELOCITY_OFFSET;
        float velocitySquaredTerm = velocityTerm * velocityTerm;
        float fitnessBonus = SCR_RSS_Constants.FIXED_PANDOLF_FITNESS_BONUS;
        float baseTerm = (SCR_RSS_Constants.PANDOLF_BASE_COEFF * fitnessBonus)
            + (SCR_RSS_Constants.PANDOLF_VELOCITY_COEFF * velocitySquaredTerm);

        float gradeDecimal = gradePercent * 0.01;
        float velocitySquared = velocityMs * velocityMs;
        float gradeTerm = gradeDecimal * (SCR_RSS_Constants.PANDOLF_GRADE_BASE_COEFF
            + (SCR_RSS_Constants.PANDOLF_GRADE_VELOCITY_COEFF * velocitySquared));

        float maxGradeTerm = baseTerm * 3.0;
        gradeTerm = Math.Min(gradeTerm, maxGradeTerm);

        if (gradePercent < 0.0 && gradePercent > -SCR_RSS_Constants.GENTLE_DOWNHILL_GRADE_MAX)
            gradeTerm = gradeTerm * SCR_RSS_Constants.GENTLE_DOWNHILL_SAVINGS_MULTIPLIER;

        float steepDownhillPenalty = 0.0;
        if (useSanteeCorrection && gradePercent < -SCR_RSS_Constants.STEEP_DOWNHILL_GRADE_THRESHOLD)
        {
            float absGrade = Math.AbsFloat(gradePercent);
            float ramp = (absGrade - SCR_RSS_Constants.STEEP_DOWNHILL_GRADE_THRESHOLD) / 15.0;
            if (ramp > 1.0)
                ramp = 1.0;
            steepDownhillPenalty = baseTerm * ramp * SCR_RSS_Constants.STEEP_DOWNHILL_PENALTY_MAX_FRACTION;
        }

        float weightMultiplier = totalWeightKg / SCR_RSS_Constants.REFERENCE_WEIGHT;
        weightMultiplier = Math.Max(weightMultiplier, 0.1);

        float powerWatts = weightMultiplier * (baseTerm + gradeTerm + steepDownhillPenalty)
            * terrainFactor * SCR_RSS_Constants.REFERENCE_WEIGHT;
        return Math.Max(powerWatts, 0.0);
    }

    //! ACSM 跑步功率（W），totalWeightKg 为身体+装备
    static float CalculateAcsmPowerWatts(float velocityMs, float totalWeightKg)
    {
        velocityMs = Math.Max(velocityMs, 0.0);
        totalWeightKg = Math.Max(totalWeightKg, SCR_RSS_Constants.CHARACTER_WEIGHT * 0.5);
        float massScale = totalWeightKg / SCR_RSS_Constants.REFERENCE_WEIGHT;

        float v = velocityMs;
        float powerRef = SCR_RSS_Constants.V6_ACSM_REST_W
            + SCR_RSS_Constants.V6_ACSM_LINEAR_W_PER_MS * v
            + SCR_RSS_Constants.V6_ACSM_QUAD_W_PER_MS2 * v * v;
        return Math.Max(powerRef * massScale, 0.0);
    }

    //! Pandolf/ACSM 在 blend 区间平滑混合
    static float GetAcsmBlendWeight(float velocityMs)
    {
        float startMs = SCR_RSS_Constants.V6_ACSM_BLEND_START_MS;
        float endMs = SCR_RSS_Constants.V6_ACSM_BLEND_END_MS;
        if (velocityMs <= startMs)
            return 0.0;
        if (velocityMs >= endMs)
            return 1.0;
        return (velocityMs - startMs) / (endMs - startMs);
    }

    //! 综合代谢功率（W），含负重代谢阻尼（与 Rust twin 对齐）
    static float MetabolismPowerWatts(
        float velocityMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        bool useSanteeCorrection,
        int movementPhase)
    {
        float blended = MetabolismPowerWattsBlended(
            velocityMs, totalWeightKg, gradePercent, terrainFactor, useSanteeCorrection, movementPhase);
        return ApplyLoadMetabolicDampeningToPower(
            blended, velocityMs, totalWeightKg, gradePercent, terrainFactor, useSanteeCorrection, movementPhase);
    }

    static float MetabolismPowerWattsBlended(
        float velocityMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        bool useSanteeCorrection,
        int movementPhase)
    {
        bool preferAcsm = false;
        if (movementPhase == 2 || movementPhase == 3)
            preferAcsm = true;
        if (velocityMs >= SCR_RSS_Constants.V6_ACSM_BLEND_END_MS)
            preferAcsm = true;

        // Walk / Idle：LCDA 背包式（含坡度）；不叠 Santee
        if (!preferAcsm && velocityMs <= SCR_RSS_Constants.LCDA_MAX_SPEED_MS)
        {
            return CalculateLcdaBackpackPowerWatts(
                velocityMs, totalWeightKg, gradePercent, terrainFactor);
        }

        float pandolfW = CalculatePandolfPowerWatts(
            velocityMs, totalWeightKg, gradePercent, terrainFactor, useSanteeCorrection);

        if (!preferAcsm && velocityMs < SCR_RSS_Constants.V6_ACSM_BLEND_START_MS)
            return pandolfW;

        float acsmW = CalculateAcsmPowerWatts(velocityMs, totalWeightKg);
        float blend = GetAcsmBlendWeight(velocityMs);
        if (movementPhase == 3)
            blend = Math.Max(blend, 0.85);

        return pandolfW * (1.0 - blend) + acsmW * blend;
    }

    static float ApplyLoadMetabolicDampeningToPower(
        float loadedPowerW,
        float velocityMs,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        bool useSanteeCorrection,
        int movementPhase)
    {
        float bodyWeight = SCR_RSS_Constants.CHARACTER_WEIGHT;
        if (totalWeightKg <= bodyWeight + 0.5)
            return loadedPowerW;

        float dampening = SCR_RSS_ConfigBridge.GetLoadMetabolicDampening();
        if (dampening >= 1.0)
            return loadedPowerW;

        float unloadedPowerW = MetabolismPowerWattsBlended(
            velocityMs, bodyWeight, gradePercent, terrainFactor, useSanteeCorrection, movementPhase);
        float loadExtra = loadedPowerW - unloadedPowerW;
        if (loadExtra < 0.0)
            loadExtra = 0.0;
        return unloadedPowerW + loadExtra * dampening;
    }

    //! 反解：给定目标功率，求最大可持续速度（m/s）
    static float InvertSpeedForPowerWatts(
        float targetPowerWatts,
        float totalWeightKg,
        float gradePercent,
        float terrainFactor,
        int movementPhase)
    {
        if (targetPowerWatts <= 1.0)
            return 0.0;

        float vLow = 0.0;
        float vHigh = SCR_RSS_Constants.V6_INVERT_SPEED_MAX_MS;
        int i;
        for (i = 0; i < 24; i++)
        {
            float vMid = (vLow + vHigh) * 0.5;
            float pMid = MetabolismPowerWatts(
                vMid, totalWeightKg, gradePercent, terrainFactor, true, movementPhase);
            if (pMid > targetPowerWatts)
                vHigh = vMid;
            else
                vLow = vMid;
        }
        return (vLow + vHigh) * 0.5;
    }

    //! 静态站立功率（W）
    static float CalculateStaticStandingPowerWatts(float bodyWeight, float loadWeight)
    {
        bodyWeight = Math.Max(bodyWeight, 0.0);
        loadWeight = Math.Max(loadWeight, 0.0);

        if (loadWeight < 5.0)
            return SCR_RSS_Constants.V6_STANDING_REST_WATTS;

        float baseStaticTerm = SCR_RSS_Constants.PANDOLF_STATIC_COEFF_1 * bodyWeight;
        float loadRatio = 0.0;
        if (bodyWeight > 0.0)
            loadRatio = loadWeight / bodyWeight;

        float loadStaticTerm = 0.0;
        if (loadWeight > 0.0)
        {
            float loadRatioSquared = loadRatio * loadRatio;
            loadStaticTerm = SCR_RSS_Constants.PANDOLF_STATIC_COEFF_2
                * (bodyWeight + loadWeight) * loadRatioSquared;
        }

        return Math.Max(baseStaticTerm + loadStaticTerm, 0.0);
    }

    //! 有氧功率封顶：STA 仅扣 min(P, CP)；超出部分由 W′ 模型独占
    static float AerobicPowerWattsForStaminaDrain(float powerWatts, float criticalPowerCapWatts = -1.0)
    {
        if (criticalPowerCapWatts > 1.0 && powerWatts > criticalPowerCapWatts)
            return criticalPowerCapWatts;
        return powerWatts;
    }

    //! 负重 Run/Sprint 步态税（Walk/Idle=1）。负载从 START→REF 升到 MAX_MULT。
    static float GetLoadedGaitStaminaDrainMultiplier(float loadWeightKg, int movementPhase)
    {
        if (movementPhase < 2)
            return 1.0;
        if (loadWeightKg <= SCR_RSS_Constants.LOADED_RUN_DRAIN_START_KG)
            return 1.0;

        float span = SCR_RSS_Constants.LOADED_RUN_DRAIN_REF_KG
            - SCR_RSS_Constants.LOADED_RUN_DRAIN_START_KG;
        if (span < 0.1)
            span = 0.1;
        float t = (loadWeightKg - SCR_RSS_Constants.LOADED_RUN_DRAIN_START_KG) / span;
        t = Math.Clamp(t, 0.0, 1.0);
        float maxMult = SCR_RSS_Constants.LOADED_RUN_DRAIN_MAX_MULT;
        if (maxMult < 1.0)
            maxMult = 1.0;
        return 1.0 + (maxMult - 1.0) * t;
    }

    //! 功率 → 有氧消耗率（%/s）
    static float StaminaDrainRatePerSecondFromPowerWatts(float powerWatts, float criticalPowerCapWatts = -1.0)
    {
        float aerobicW = AerobicPowerWattsForStaminaDrain(powerWatts, criticalPowerCapWatts);
        float energyToStaminaCoeff = SCR_RSS_ConfigBridge.GetEnergyToStaminaCoeff();
        energyToStaminaCoeff = Math.Clamp(energyToStaminaCoeff, 0.0, 0.1);
        energyToStaminaCoeff = energyToStaminaCoeff * SCR_RSS_Constants.V6_STAMINA_DRAIN_CALIBRATION;
        return Math.Max(aerobicW * energyToStaminaCoeff, 0.0);
    }

    //! 兼容接口：返回 %/s（与旧 CalculatePandolfEnergyExpenditure 一致）
    static float CalculatePandolfEnergyExpenditure(
        float velocity,
        float currentWeight,
        float gradePercent,
        float terrainFactor,
        bool useSanteeCorrection,
        int movementPhase)
    {
        float powerW = MetabolismPowerWatts(
            velocity, currentWeight, gradePercent, terrainFactor, useSanteeCorrection, movementPhase);
        return StaminaDrainRatePerSecondFromPowerWatts(powerW);
    }

    //! 静态站立 %/s
    static float CalculateStaticStandingCost(float bodyWeight, float loadWeight)
    {
        if (loadWeight < 5.0)
            return -0.0025;
        if (loadWeight < 15.0)
            return -0.001;

        float powerW = CalculateStaticStandingPowerWatts(bodyWeight, loadWeight);
        return StaminaDrainRatePerSecondFromPowerWatts(powerW);
    }
}
