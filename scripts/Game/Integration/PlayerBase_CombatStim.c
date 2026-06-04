// PlayerBase split: 战斗兴奋剂
modded class SCR_CharacterControllerComponent
{
    int RSS_GetCombatStimPhase()
    {
        return m_iCombatStimPhase;
    }

    bool RSS_IsCaffeineSodiumBenzoateActive()
    {
        return SCR_CombatStimStateMachine.IsActive(m_iCombatStimPhase);
    }

    bool RSS_IsCombatStimOverdosed()
    {
        return SCR_CombatStimStateMachine.IsOverdosed(m_iCombatStimPhase);
    }

    protected void RSS_CombatStim_OnTickTransitions()
    {
        float wt = GetGame().GetWorld().GetWorldTime() / 1000.0;
        int oldPhase;
        float oldEndsAt;
        int nextPhase;
        float nextEndsAt;
        int nextDelayInjectionCount;
        if (!SCR_PlayerBaseCombatStimHelper.TryBuildTickTransition(
                m_iCombatStimPhase,
                m_fCombatStimPhaseEndsAt,
                wt,
                oldPhase,
                oldEndsAt,
                nextPhase,
                nextEndsAt,
                nextDelayInjectionCount,
                m_iCombatStimDelayInjectionCount))
            return;

        m_iCombatStimPhase = nextPhase;
        m_fCombatStimPhaseEndsAt = nextEndsAt;
        m_iCombatStimDelayInjectionCount = nextDelayInjectionCount;

        if (IsRssDebugEnabled())
            SCR_PlayerBaseDebugHelper.LogCombatStimTickTransition(
                oldPhase,
                m_iCombatStimPhase,
                oldEndsAt,
                m_fCombatStimPhaseEndsAt,
                wt);

        if (Replication.IsServer() && IsPlayerControlled())
            Rpc(RPC_CombatStimSyncToOwner, m_iCombatStimPhase, m_fCombatStimPhaseEndsAt);

        ChimeraCharacter ch = SCR_PlayerBaseCombatStimHelper.GetOwnerCharacter(GetOwner());
        SCR_PlayerBaseCombatStimHelper.ApplyBleedingScaleForOwner(
            m_iCombatStimPhase,
            m_fRSS_CombatStimBleedingBaseline,
            ch);
    }

    void RSS_CombatStim_OnInjectServer()
    {
        if (!Replication.IsServer())
            return;
        IEntity ownerEnt = GetOwner();
        ChimeraCharacter ch = SCR_PlayerBaseCombatStimHelper.GetOwnerCharacter(ownerEnt);
        if (!ch)
            return;

        float wt = GetGame().GetWorld().GetWorldTime() / 1000.0;
        int nextPhase;
        float nextEndsAt;
        int nextDelayInjectionCount;
        bool shouldDie;
        if (!SCR_PlayerBaseCombatStimHelper.TryBuildInjectionTransition(
                m_iCombatStimPhase,
                m_fCombatStimPhaseEndsAt,
                wt,
                m_iCombatStimDelayInjectionCount,
                nextPhase,
                nextEndsAt,
                nextDelayInjectionCount,
                shouldDie))
            return;

        if (IsRssDebugEnabled())
            SCR_PlayerBaseDebugHelper.LogCombatStimInject(
                m_iCombatStimPhase,
                nextPhase,
                shouldDie,
                wt);

        if (shouldDie)
        {
            SCR_PlayerBaseCombatStimHelper.TryKillCharacterFromOverdose(
                ch,
                m_fRSS_CombatStimBleedingBaseline);
            return;
        }

        m_iCombatStimPhase = nextPhase;
        m_fCombatStimPhaseEndsAt = nextEndsAt;
        m_iCombatStimDelayInjectionCount = nextDelayInjectionCount;

        if (Replication.IsServer() && IsPlayerControlled())
            Rpc(RPC_CombatStimSyncToOwner, m_iCombatStimPhase, m_fCombatStimPhaseEndsAt);

        SCR_PlayerBaseCombatStimHelper.ApplyBleedingScaleForOwner(
            m_iCombatStimPhase,
            m_fRSS_CombatStimBleedingBaseline,
            ch);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void RPC_CombatStimSyncToOwner(int phase, float phaseEndsAt)
    {
        m_iCombatStimPhase = phase;
        m_fCombatStimPhaseEndsAt = phaseEndsAt;
    }
}
