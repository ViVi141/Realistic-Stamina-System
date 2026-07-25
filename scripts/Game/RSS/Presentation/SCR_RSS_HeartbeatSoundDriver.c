//! 手动心跳采样：Slow 0.6s / Mid 0.9s / Fast 0.9s，按表现疲劳分档，滞回防跳档

class SCR_RSS_HeartbeatSoundDriver
{
    protected static const string SOUND_SLOW =
        "{9A36B88CB586AA8C}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Slow_01.wav";
    protected static const string SOUND_MID =
        "{7795DA13D207A2B1}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Mid_01.wav";
    protected static const string SOUND_FAST =
        "{93F9F242DC3E0E1C}Sounds/Character/Voice/Samples/Heartbeat/Character_Voice_Heartbeat_Fast_01.wav";

    protected static const float CLIP_SLOW_SEC = 0.6;
    protected static const float CLIP_MID_SEC = 0.9;
    protected static const float CLIP_FAST_SEC = 0.9;

    //! 0=slow 1=mid 2=fast
    protected int m_iTier = 0;
    protected AudioHandle m_AudioHandle = AudioHandle.Invalid;
    protected float m_fNextTriggerWorldSec = -1.0;
    protected bool m_bCycleArmed = false;
    protected bool m_bWindingDown = false;
    protected float m_fSmoothedFatigue = 0.0;
    protected int m_iJitterSeed = 17;

    void Update(float worldTimeSec, float presentation01)
    {
        if (!SCR_RSS_Constants.V6_HEARTBEAT_SOUND_ENABLED)
        {
            DisarmCycle(true);
            return;
        }

        float fatigueTarget = ComputeFatigue01(presentation01);
        m_fSmoothedFatigue = m_fSmoothedFatigue
            + (fatigueTarget - m_fSmoothedFatigue) * 0.10;

        bool wantBeat = false;
        if (m_fSmoothedFatigue > 0.05)
            wantBeat = true;
        if (fatigueTarget > 0.10)
            wantBeat = true;

        if (!wantBeat)
        {
            if (m_bCycleArmed)
                m_bWindingDown = true;
            if (!m_bCycleArmed)
                return;
        }
        else
        {
            m_bWindingDown = false;
            if (!m_bCycleArmed)
            {
                m_bCycleArmed = true;
                m_iTier = ResolveTier(m_fSmoothedFatigue, 0);
                float armDelay = 0.25 + 0.45 * NextJitter01();
                m_fNextTriggerWorldSec = worldTimeSec + armDelay;
            }
        }

        if (m_fNextTriggerWorldSec < 0.0)
            return;
        if (worldTimeSec + 0.0001 < m_fNextTriggerWorldSec)
            return;

        if (m_bWindingDown)
        {
            DisarmCycle(false);
            return;
        }

        m_iTier = ResolveTier(m_fSmoothedFatigue, m_iTier);
        float clipSec = PlayTierClip(m_iTier);
        float gap = GapAfterBeatSec(m_fSmoothedFatigue, m_iTier);
        m_fNextTriggerWorldSec = worldTimeSec + clipSec + gap;
    }

    void Cleanup()
    {
        DisarmCycle(true);
    }

    protected float ComputeFatigue01(float presentation01)
    {
        float start = SCR_RSS_Constants.V6_HEARTBEAT_SOUND_PRESENTATION_START;
        float hard = SCR_RSS_Constants.V6_HEARTBEAT_SOUND_PRESENTATION_HARD;
        if (start <= hard + 0.001)
            return 0.0;

        presentation01 = Math.Clamp(presentation01, 0.0, 1.0);
        if (presentation01 >= start)
            return 0.0;
        if (presentation01 <= hard)
            return 1.0;

        float t = (start - presentation01) / (start - hard);
        return t * t;
    }

    //! 滞回：升档易、降档难，避免 Fast/Mid 来回切
    protected int ResolveTier(float fatigue01, int currentTier)
    {
        float midEnter = SCR_RSS_Constants.V6_HEARTBEAT_TIER_MID_ENTER;
        float midExit = SCR_RSS_Constants.V6_HEARTBEAT_TIER_MID_EXIT;
        float fastEnter = SCR_RSS_Constants.V6_HEARTBEAT_TIER_FAST_ENTER;
        float fastExit = SCR_RSS_Constants.V6_HEARTBEAT_TIER_FAST_EXIT;

        if (currentTier >= 2)
        {
            if (fatigue01 < fastExit)
            {
                if (fatigue01 < midExit)
                    return 0;
                return 1;
            }
            return 2;
        }

        if (currentTier == 1)
        {
            if (fatigue01 >= fastEnter)
                return 2;
            if (fatigue01 < midExit)
                return 0;
            return 1;
        }

        // current slow
        if (fatigue01 >= fastEnter)
            return 2;
        if (fatigue01 >= midEnter)
            return 1;
        return 0;
    }

    protected float PlayTierClip(int tier)
    {
        if (m_AudioHandle != AudioHandle.Invalid)
        {
            if (AudioSystem.IsSoundPlayed(m_AudioHandle))
                AudioSystem.TerminateSoundFadeOut(m_AudioHandle, true, 0.06);
            m_AudioHandle = AudioHandle.Invalid;
        }

        if (tier >= 2)
        {
            m_AudioHandle = AudioSystem.PlaySound(SOUND_FAST);
            return CLIP_FAST_SEC;
        }
        if (tier == 1)
        {
            m_AudioHandle = AudioSystem.PlaySound(SOUND_MID);
            return CLIP_MID_SEC;
        }

        m_AudioHandle = AudioSystem.PlaySound(SOUND_SLOW);
        return CLIP_SLOW_SEC;
    }

    //! 档内微间隙：慢档略疏，快档几乎贴采样时长
    protected float GapAfterBeatSec(float fatigue01, int tier)
    {
        float gap = 0.08;
        if (tier <= 0)
            gap = 0.18 + (0.22 * (1.0 - fatigue01));
        else if (tier == 1)
            gap = 0.06 + (0.10 * (1.0 - fatigue01));
        else
            gap = 0.02 + (0.05 * (1.0 - fatigue01));

        float jitter = (NextJitter01() - 0.5) * 0.06;
        gap = gap + jitter;
        if (gap < 0.01)
            gap = 0.01;
        return gap;
    }

    protected float NextJitter01()
    {
        m_iJitterSeed = m_iJitterSeed * 1103515245 + 12345;
        if (m_iJitterSeed < 0)
            m_iJitterSeed = -m_iJitterSeed;
        int bucket = Math.Mod(m_iJitterSeed, 1000);
        return bucket / 1000.0;
    }

    protected void DisarmCycle(bool forceStop)
    {
        m_bCycleArmed = false;
        m_bWindingDown = false;
        m_fNextTriggerWorldSec = -1.0;
        m_iTier = 0;

        if (!forceStop)
        {
            if (m_fSmoothedFatigue > 0.0)
                m_fSmoothedFatigue = m_fSmoothedFatigue * 0.5;
            return;
        }

        m_fSmoothedFatigue = 0.0;
        if (m_AudioHandle != AudioHandle.Invalid)
        {
            if (AudioSystem.IsSoundPlayed(m_AudioHandle))
                AudioSystem.TerminateSoundFadeOut(m_AudioHandle, true, 0.2);
            m_AudioHandle = AudioHandle.Invalid;
        }
    }
}
