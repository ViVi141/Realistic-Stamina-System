//! UpdateCoordinator DTOs（从 SCR_RSS_UpdateCoordinator.c 拆分以控制体积）

class SpeedCalculationResult
{
    float currentSpeed;
    vector lastPositionSample;
    bool hasLastPositionSample;
    vector computedVelocity;
}

class BaseDrainRateResult
{
    float baseDrainRate;
    bool swimmingVelocityDebugPrinted;
}

class RecoveryContext
{
    float currentWeightForRecovery;
    float staticDrainForRecovery;
    int stanceInt;
    ref SCR_RSS_EnvironmentFactor envFactor;
    float speedForRecovery;
    float heatPenalty;
    float restDurationMinutes;
    float exerciseDurationMinutes;
}

class StaminaDrainTickResult
{
    float totalDrainRate;
    float baseDrainRateByVelocity;
    float baseDrainRateByVelocityForModule;
    bool swimmingVelocityDebugPrinted;
}

//! CalculateTotalDrainRate 入参包（EnforceScript 方法参数上限 16）
class StaminaDrainTickParams
{
    bool useSwimmingModel;
    float currentSpeed;
    float gearWeightKg;
    float encumbranceSpeedPenalty;
    float bodyPlusGearWeightKg;
    float totalWeightWithWetAndBody;
    float gradePercent;
    float terrainFactor;
    vector velocityForDrain;
    bool swimmingVelocityDebugPrinted;
    IEntity owner;
    SCR_CharacterControllerComponent controller;
    SCR_RSS_EnvironmentFactor environmentFactor;
    bool isSprinting;
    int currentMovementPhase;
    float speedRatio;
    float heatStressMultiplier;
    bool isSprintActive;
    float staminaPercent;
    bool combatStimActive;
    SCR_RSS_EncumbranceCache encumbranceCache;
    SCR_RSS_FatigueSystem fatigueSystem;
    SCR_RSS_ExerciseTracker exerciseTracker;
    SCR_RSS_EpocState epocState;
    float currentTimeSec;
    float currentTimeForExerciseMs;
    float appliedSpeedLimitMs;
    float effectiveCriticalPowerWatts;
    float wPrimePool01;
}

class StaminaEtaResult
{
    float timeToDepleteSec;
    float timeToFullSec;
}
