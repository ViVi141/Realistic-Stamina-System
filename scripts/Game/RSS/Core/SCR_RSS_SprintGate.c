//! Sprint 门禁 + W′→引擎条表现（晃动/呼吸/Exhaustion）
//! 仅改 GetStamina() transient，不改 m_fTargetStamina（有氧权威）

class SCR_RSS_SprintGate
{
    static void ClearEnginePoke(
        SCR_CharacterStaminaComponent staminaComponent,
        inout bool pokeActive)
    {
        if (!pokeActive || !staminaComponent)
            return;
        staminaComponent.RestoreEngineStaminaFromTarget();
        pokeActive = false;
    }

    //! W′/W′max → 引擎条伪装读数（1=不压条）
    static float MapWPrimePoolToEngineDisplay(float wPrime01)
    {
        if (!SCR_RSS_Constants.V6_WPRIME_ENGINE_FX_ENABLED)
            return 1.0;

        float start = SCR_RSS_Constants.V6_WPRIME_ENGINE_FX_START;
        float floorVal = SCR_RSS_Constants.V6_WPRIME_ENGINE_FX_FLOOR;
        if (start <= 0.001)
            return 1.0;

        wPrime01 = Math.Clamp(wPrime01, 0.0, 1.0);
        if (wPrime01 >= start)
            return 1.0;

        float t = wPrime01 / start;
        return floorVal + (1.0 - floorVal) * t;
    }

    //! min(有氧, W′映射, 冲刺门禁压条)
    static float ComputeEnginePresentationDisplay(
        float aerobic01,
        float wPrime01,
        bool sprintAllowed,
        bool sprintIntent)
    {
        aerobic01 = Math.Clamp(aerobic01, 0.0, 1.0);
        float display = aerobic01;

        float wPrimeMapped = MapWPrimePoolToEngineDisplay(wPrime01);
        if (wPrimeMapped < display)
            display = wPrimeMapped;

        if (!sprintAllowed && sprintIntent)
        {
            float blockStamina = SCR_RSS_ConfigBridge.GetSprintEnableThreshold() - 0.01;
            if (blockStamina < 0.0)
                blockStamina = 0.0;
            if (blockStamina < display)
                display = blockStamina;
        }

        return display;
    }

    //! 统一写入引擎条：W′ 表现 + 冲刺门禁 poke
    static void ApplyEngineStaminaPresentation(
        bool sprintAllowed,
        bool sprintIntent,
        SCR_CharacterStaminaComponent staminaComponent,
        float aerobic01,
        float wPrime01,
        inout bool pokeActive)
    {
        if (!staminaComponent)
            return;

        float aerobicClamped = Math.Clamp(aerobic01, 0.0, 1.0);
        float display = ComputeEnginePresentationDisplay(
            aerobicClamped, wPrime01, sprintAllowed, sprintIntent);

        bool needTransient = false;
        if (display + 0.0005 < aerobicClamped)
            needTransient = true;

        if (!needTransient)
        {
            // 即使 poke 已清，仍每帧 Ease 回有氧，避免平滑停在半路
            if (pokeActive)
                ClearEnginePoke(staminaComponent, pokeActive);
            else
                staminaComponent.RestoreEngineStaminaFromTarget();
            return;
        }

        staminaComponent.ApplyTransientEngineStamina(display);
        pokeActive = true;
    }

    static void PokeEngineStaminaForSprintBlock(
        bool sprintAllowed,
        bool sprintIntent,
        SCR_CharacterStaminaComponent staminaComponent,
        inout bool pokeActive,
        float aerobic01,
        float wPrime01)
    {
        ApplyEngineStaminaPresentation(
            sprintAllowed,
            sprintIntent,
            staminaComponent,
            aerobic01,
            wPrime01,
            pokeActive);
    }

    static void ApplyOnPrepareControls(
        ActionManager am,
        bool sprintAllowed,
        bool isSprinting,
        int movementPhase,
        bool sprintToggle,
        SCR_CharacterStaminaComponent staminaComponent,
        inout bool pokeActive,
        float aerobic01,
        float wPrime01)
    {
        if (sprintAllowed)
        {
            // 仍可因 W′ 低而保持引擎条压低（晃动/呼吸）
            ApplyEngineStaminaPresentation(
                true,
                false,
                staminaComponent,
                aerobic01,
                wPrime01,
                pokeActive);
            return;
        }

        bool sprintIntent = false;
        if (isSprinting || movementPhase == 3)
            sprintIntent = true;
        if (sprintToggle)
            sprintIntent = true;
        if (am.GetActionValue("CharacterSprint") > 0.5)
            sprintIntent = true;

        am.SetActionValue("CharacterSprint", 0.0);
        am.SetActionValue("CharacterSprintToggle", 0.0);

        ApplyEngineStaminaPresentation(
            sprintAllowed,
            sprintIntent,
            staminaComponent,
            aerobic01,
            wPrime01,
            pokeActive);
    }
}
