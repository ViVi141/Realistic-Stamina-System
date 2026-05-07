// 载具体力恢复辅助模块
// 从 PlayerBase.c 拆分，处理载具内的体力恢复逻辑
// 拆分原因：PlayerBase.c 超过 EnforceScript 65535 字节编译限制
// 日期：2026-05-08

class SCR_PlayerBaseVehicleHelper
{
    //! 在载具中时处理体力恢复（不消耗体力，仅恢复）
    //! @return true 如果当前在载具中并已处理
    static bool HandleVehicleStaminaUpdate(
        SCR_CharacterControllerComponent ctrl,
        IEntity owner,
        CompartmentAccessComponent compartmentAccess,
        SCR_CharacterStaminaComponent staminaComponent,
        ExerciseTracker exerciseTracker,
        FatigueSystem fatigueSystem,
        EpocState epocState,
        EncumbranceCache encumbranceCache,
        EnvironmentFactor environmentFactor,
        TerrainDetector terrainDetector,
        StanceTransitionManager stanceTransitionManager,
        inout float lastStaminaUpdateTime,
        float currentWetWeight,
        int speedUpdateIntervalMs,
        bool isDebugEnabled)
    {
        bool isInVehicle = SCR_PlayerBaseMovementHelper.IsInVehicle(compartmentAccess);
        if (!isInVehicle)
            return false;

        static int vehicleDebugCounter = 0;
        vehicleDebugCounter++;
        if (vehicleDebugCounter >= 25)
        {
            vehicleDebugCounter = 0;
            if (staminaComponent && isDebugEnabled)
            {
                float currentStamina = staminaComponent.GetTargetStamina();
                PrintFormat("[RSS] 载具中 / In Vehicle: 体力=%1%% | Stamina=%1%%",
                    Math.Round(currentStamina * 100.0).ToString());
            }
        }

        if (exerciseTracker)
        {
            float vehicleCurrentTimeMs = GetGame().GetWorld().GetWorldTime();
            exerciseTracker.Update(vehicleCurrentTimeMs, false, true);
        }

        float vehicleStaminaPercent = 1.0;
        if (staminaComponent)
            vehicleStaminaPercent = Math.Clamp(staminaComponent.GetTargetStamina(), 0.0, 1.0);

        float vehicleNetRatePerSec = 0.0;
        if (vehicleStaminaPercent < 1.0)
        {
            vehicleNetRatePerSec = StaminaUpdateCoordinator.GetNetStaminaRatePerSecond(
                vehicleStaminaPercent, false, 0.0, -StaminaConstants.REST_RECOVERY_PER_TICK, 0.0, 0.0, 1.0,
                epocState, encumbranceCache, exerciseTracker, ctrl, null, true);
            float vehicleRecoveryRate = vehicleNetRatePerSec / 5.0;

            float currentWorldTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
            if (fatigueSystem)
                fatigueSystem.ProcessFatigueDecay(currentWorldTime, 0.0);
            float maxStaminaCap = 1.0;
            if (fatigueSystem)
                maxStaminaCap = fatigueSystem.GetMaxStaminaCap();
            float timeDeltaSec;
            if (lastStaminaUpdateTime >= 0.0)
                timeDeltaSec = currentWorldTime - lastStaminaUpdateTime;
            else
                timeDeltaSec = speedUpdateIntervalMs / 1000.0;
            timeDeltaSec = Math.Clamp(timeDeltaSec, 0.01, 0.5);
            float tickScale = Math.Clamp(timeDeltaSec / 0.2, 0.01, 2.0);
            float oldStamina = vehicleStaminaPercent;
            float newStamina = Math.Clamp(oldStamina + vehicleRecoveryRate * tickScale, 0.0, maxStaminaCap);
            if (vehicleStaminaPercent > maxStaminaCap)
                newStamina = maxStaminaCap;
            staminaComponent.SetTargetStamina(newStamina);
            lastStaminaUpdateTime = currentWorldTime;
            vehicleStaminaPercent = newStamina;

            if (vehicleDebugCounter == 0 && isDebugEnabled)
            {
                PrintFormat("[RSS] 载具中恢复 / Vehicle Recovery: %1%% -> %2%% (净率: %3/s)",
                    Math.Round(oldStamina * 100.0).ToString(),
                    Math.Round(newStamina * 100.0).ToString(),
                    vehicleNetRatePerSec.ToString());
            }
        }

        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (settings && settings.m_bHintDisplayEnabled)
        {
            float vehicleTimeToFullSec = -1.0;
            float targetStamina = 1.0;
            if (fatigueSystem)
                targetStamina = fatigueSystem.GetMaxStaminaCap();
            if (vehicleStaminaPercent < targetStamina && vehicleNetRatePerSec > 0.00001)
            {
                vehicleTimeToFullSec = (targetStamina - vehicleStaminaPercent) / vehicleNetRatePerSec;
                if (vehicleTimeToFullSec < 0.5)
                    vehicleTimeToFullSec = 0.5;
                if (vehicleTimeToFullSec > 7200.0)
                    vehicleTimeToFullSec = 7200.0;
            }

            float vehicleDebugWeight = 0.0;
            if (encumbranceCache && encumbranceCache.IsCacheValid())
                vehicleDebugWeight = encumbranceCache.GetCurrentWeight();

            DebugInfoParams vehicleParams = SCR_PlayerBaseNetworkHelper.BuildVehicleDebugInfoParams(
                owner,
                vehicleStaminaPercent,
                vehicleDebugWeight,
                currentWetWeight,
                -1.0,
                vehicleTimeToFullSec,
                terrainDetector,
                environmentFactor,
                stanceTransitionManager);
            DebugDisplay.OutputHintInfo(vehicleParams);
        }

        ctrl.RSS_SetMudSlipCameraShake01(0.0);
        return true;
    }
}
