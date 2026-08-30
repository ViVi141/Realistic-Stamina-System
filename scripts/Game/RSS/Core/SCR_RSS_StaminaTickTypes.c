//! Stamina tick DTOs（从 PlayerBase_UpdateLoop.c 拆出以控制体积）
//! EnforceScript 方法参数上限 16，故用 DTO 打包跨 Phase 状态。

//! 状态行代谢诊断
class RSS_StatusMetabLogSnapshot
{
    float metabolismPowerW;
    float metabolismPowerMetW;
    float metabolismPowerRawW;
    float effectiveCpW;
    float aerobicPowerW;
    float finalDrainPerTick;
    float metabolicNetPerTick;
    float capRatchetPerTick;
    float netStaminaPerTick;
}

//! 本地玩家 Debug/HUD 单 tick 快照
class RSS_StaminaDebugOutputParams
{
    float staminaPercent;
    bool useSwimmingModel;
    float currentSpeed;
    float totalDrainRate;
    float baseDrainRateByVelocity;
    float baseDrainRateByVelocityForModule;
    float heatStressMultiplier;
    float baseSpeedMultiplier;
    float encumbranceSpeedPenalty;
    float finalSpeedMultiplier;
    float gradePercent;
    float slopeAngleDegrees;
    bool isSwimming;
    bool isSprinting;
    bool isSprintActive;
    int currentMovementPhase;
    int effectiveMovementPhase;
    float rainWeight;
    float maxStaminaCap;
    float fatigueIntegralNorm;
    float metabolismPowerW;
    float metabolismPowerMetW;
    float metabolismPowerRawW;
    float effectiveCpW;
    float aerobicPowerW;
    float finalDrainRate;
    float metabolicNetPerTick;
    float capRatchetPerTick;
    float netStaminaPerTick;
    float terrainFactor;
    float appliedSpeedLimitMs;
    float effectiveCriticalPowerWatts;
    float timeDeltaSec;
    float totalWeightWithWetAndBody;
    float powerWatts;
    float environmentMult;
    float targetStaminaCap;
    float capShrinkPerSec;
    float timeToDepleteSec;
    float timeToFullSec;
    bool epocActive;
    float wPrimePool01;
    float landPositionDeltaSpeedMs;
    float overspeedExtraDrainPerSec;
}

//! Cross-phase scratch for stamina tick (ICE split of UpdateSpeedBasedOnStamina)
class RSS_StaminaTickLocals
{
    IEntity owner;
    World world;
    bool isPlayer;
    float staminaPercent;
    float encumbranceSpeedPenalty;
    bool isExhausted;
    bool isSwimmingForSpeed;
    vector velocity;
    float currentSpeed;
    bool isSprintingNow;
    int phaseNow;
    int effectivePhase;
    bool isSprintActive;
    bool sprintIntent;
    float currentTimeForExerciseMs;
    float currentTime;
    float terrainFactor;
    float finalSpeedMultiplier;
    float customSprintSpeedMult;
    float baseSpeedMultiplier;
    float currentWeight;
    float speedToApply;
    float finalSpeedToApply;
    float storedEngineBase;
    bool isCriticalData;
    bool isSwimming;
    float timeDeltaSec;
    float heatStressMultiplier;
    float rainWeight;
    float totalWetWeight;
    float currentWeightWithWet;
    float totalWeight;
    float totalWeightWithWetAndBody;
    bool useSwimmingModel;
    float speedRatio;
    vector velocityForDrain;
    float slopeAngleDegrees;
    ref RSS_GradeCalculationResult gradeResult;
    float gradePercent;
    bool isSprinting;
    int currentMovementPhase;
    int effectiveMovementPhase;
    float totalDrainRate;
    float baseDrainRateByVelocity;
    float baseDrainRateByVelocityForModule;
    bool combatStimActive;
    ref RSS_StaminaDrainTickParams drainParams;
    ref RSS_StaminaDrainTickResult drainTick;
    float effectiveCriticalPowerWattsDbg;
    float environmentMultDbg;
    float powerWattsDbg;
    float wPrimePool01Dbg;
    bool needLocalDebugBatch;
    float staminaBeforeUpdate;
    float maxStaCapDbg;
    float fatigueNormDbg;
    float metabPowerDbg;
    float metabPowerMetDbg;
    float metabPowerRawDbg;
    float metabCpDbg;
    float metabAerobicDbg;
    float finalDrainDbg;
    float metabolicNetDbg;
    float overspeedExtraPerSec;
    float netStaminaTickDbg;
    ref RSS_StatusMetabLogSnapshot metabSnap;
    ref RSS_StaminaDebugOutputParams debugTick;
    float targetStaCapDbg;
    float capShrinkDbg;
    bool epocActiveDbg;
}
