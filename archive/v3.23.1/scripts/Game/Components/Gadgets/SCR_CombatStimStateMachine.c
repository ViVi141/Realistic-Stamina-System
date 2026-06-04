//! 战斗兴奋剂阶段状态机（从 PlayerBase.c 拆分）
class SCR_CombatStimStateMachine
{
    static bool IsActive(int phase)
    {
        if (phase == ERSS_CombatStimPhase.ACTIVE)
            return true;
        if (phase == ERSS_CombatStimPhase.OD)
            return true;

        return false;
    }

    static bool IsOverdosed(int phase)
    {
        if (phase == ERSS_CombatStimPhase.OD)
            return true;

        return false;
    }

    static bool AdvancePhase(int currentPhase, float currentPhaseEndsAt, float worldTimeSec, out int outPhase, out float outPhaseEndsAt)
    {
        outPhase = currentPhase;
        outPhaseEndsAt = currentPhaseEndsAt;

        if (currentPhaseEndsAt < 0.0)
            return false;

        if (worldTimeSec < currentPhaseEndsAt)
            return false;

        if (currentPhase == ERSS_CombatStimPhase.ACTIVE)
        {
            outPhase = ERSS_CombatStimPhase.NONE;
            outPhaseEndsAt = -1.0;
            return true;
        }

        if (currentPhase == ERSS_CombatStimPhase.DELAY)
        {
            outPhase = ERSS_CombatStimPhase.ACTIVE;
            outPhaseEndsAt = worldTimeSec + SCR_CombatStimConstants.ACTIVE_DURATION_SEC;
            return true;
        }

        if (currentPhase == ERSS_CombatStimPhase.OD)
        {
            outPhase = ERSS_CombatStimPhase.NONE;
            outPhaseEndsAt = -1.0;
            return true;
        }

        return false;
    }

    static bool TryStartFromInjection(
        int currentPhase,
        float currentPhaseEndsAt,
        float worldTimeSec,
        int delayInjectionCount,
        out int outPhase,
        out float outPhaseEndsAt,
        out int outDelayInjectionCount,
        out bool outShouldDie)
    {
        outPhase = currentPhase;
        outPhaseEndsAt = currentPhaseEndsAt;
        outDelayInjectionCount = delayInjectionCount;
        outShouldDie = false;

        int normalizedPhase = currentPhase;
        float normalizedEndsAt = currentPhaseEndsAt;
        int normalizedDelayInjectionCount = delayInjectionCount;
        if (normalizedPhase != ERSS_CombatStimPhase.NONE && normalizedEndsAt >= 0.0 && worldTimeSec >= normalizedEndsAt)
        {
            normalizedPhase = ERSS_CombatStimPhase.NONE;
            normalizedEndsAt = -1.0;
            normalizedDelayInjectionCount = 0;
        }

        if (normalizedPhase == ERSS_CombatStimPhase.NONE)
        {
            outPhase = ERSS_CombatStimPhase.DELAY;
            outPhaseEndsAt = worldTimeSec + SCR_CombatStimConstants.ABSORPTION_DELAY_SEC;
            outDelayInjectionCount = 1;
            return true;
        }

        if (normalizedPhase == ERSS_CombatStimPhase.DELAY)
        {
            if (normalizedEndsAt > worldTimeSec)
            {
                int nextCount = normalizedDelayInjectionCount + 1;
                outDelayInjectionCount = nextCount;

                if (nextCount >= 3)
                {
                    outShouldDie = true;
                    return true;
                }

                float remaining = normalizedEndsAt - worldTimeSec;
                if (remaining < 0.0)
                    remaining = 0.0;

                outPhase = ERSS_CombatStimPhase.OD;
                outPhaseEndsAt = worldTimeSec + remaining + SCR_CombatStimConstants.ACTIVE_DURATION_SEC * SCR_CombatStimConstants.OD_ADDITIVE_DURATION_MULTIPLIER;
                return true;
            }

            outPhase = ERSS_CombatStimPhase.ACTIVE;
            outPhaseEndsAt = worldTimeSec + SCR_CombatStimConstants.ACTIVE_DURATION_SEC;
            outDelayInjectionCount = 0;
            return true;
        }

        if (normalizedPhase == ERSS_CombatStimPhase.ACTIVE)
        {
            float remaining = normalizedEndsAt - worldTimeSec;
            if (remaining < 0.0)
                remaining = 0.0;

            outPhase = ERSS_CombatStimPhase.OD;
            outPhaseEndsAt = worldTimeSec + remaining + SCR_CombatStimConstants.ACTIVE_DURATION_SEC * SCR_CombatStimConstants.OD_ADDITIVE_DURATION_MULTIPLIER;
            outDelayInjectionCount = 0;
            return true;
        }

        if (normalizedPhase == ERSS_CombatStimPhase.OD)
        {
            outShouldDie = true;
            return true;
        }

        return false;
    }
}
