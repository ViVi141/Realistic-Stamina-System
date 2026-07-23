//! Sprint 门禁：引擎条临时 poke / PrepareControls 拦截（从 PlayerBase.c 拆分）

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

    static void PokeEngineStaminaForSprintBlock(
        bool sprintAllowed,
        bool sprintIntent,
        SCR_CharacterStaminaComponent staminaComponent,
        inout bool pokeActive)
    {
        if (sprintAllowed || !sprintIntent)
        {
            ClearEnginePoke(staminaComponent, pokeActive);
            return;
        }

        if (!staminaComponent)
            return;

        float blockStamina = SCR_RSS_ConfigBridge.GetSprintEnableThreshold() - 0.01;
        if (blockStamina < 0.0)
            blockStamina = 0.0;
        staminaComponent.ApplyTransientEngineStamina(blockStamina);
        pokeActive = true;
    }

    static void ApplyOnPrepareControls(
        ActionManager am,
        bool sprintAllowed,
        bool isSprinting,
        int movementPhase,
        bool sprintToggle,
        SCR_CharacterStaminaComponent staminaComponent,
        inout bool pokeActive)
    {
        if (sprintAllowed)
        {
            ClearEnginePoke(staminaComponent, pokeActive);
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

        PokeEngineStaminaForSprintBlock(sprintAllowed, sprintIntent, staminaComponent, pokeActive);
    }
}
