//! 手动触发再生呼吸：In/Out 各 0.5s；节奏随疲劳加喘，轻喘稀疏、重喘连密，带抖动与软起停

class SCR_RSS_BreathSoundDriver
{
    protected static const string SOUND_BREATH_IN =
        "{48C8925347BEF7EB}Sounds/Character/Voice/Samples/Eng/Male1/Breath/Character_Voice_Breath_Regenerate_In_500_02.wav";
    protected static const string SOUND_BREATH_OUT =
        "{FCD113283AF2E71D}Sounds/Character/Voice/Samples/Eng/Male1/Breath/Character_Voice_Breath_Regenerate_Out_500_02.wav";

    protected static const float CLIP_DURATION_SEC = 0.5;

    //! 0=待吸气 1=吸气后等呼气 2=呼气后休息
    protected int m_iPhase = 0;
    protected AudioHandle m_AudioHandle = AudioHandle.Invalid;
    protected float m_fNextTriggerWorldSec = -1.0;
    protected bool m_bCycleArmed = false;
    //! 正在收尾：再完成一对 In–Out 后停
    protected bool m_bWindingDown = false;
    protected float m_fSmoothedFatigue = 0.0;
    protected int m_iJitterSeed = 1;

    void Update(float worldTimeSec, float presentation01)
    {
        if (!SCR_RSS_Constants.V6_BREATH_SOUND_ENABLED)
        {
            DisarmCycle(true);
            return;
        }

        float fatigueTarget = ComputeFatigue01(presentation01);
        m_fSmoothedFatigue = m_fSmoothedFatigue
            + (fatigueTarget - m_fSmoothedFatigue) * 0.12;

        bool wantBreath = false;
        if (m_fSmoothedFatigue > 0.04)
            wantBreath = true;
        if (fatigueTarget > 0.08)
            wantBreath = true;

        if (!wantBreath)
        {
            if (m_bCycleArmed)
            {
                // 软停：若正待吸气则直接停；若已吸则做完呼气再停
                if (m_iPhase == 0)
                    DisarmCycle(false);
                else
                    m_bWindingDown = true;
            }
            if (!m_bCycleArmed)
                return;
        }
        else
        {
            m_bWindingDown = false;
            if (!m_bCycleArmed)
            {
                m_bCycleArmed = true;
                m_iPhase = 0;
                // 软起：不要贴门限立刻开喘
                float armDelay = 0.35 + 0.55 * NextJitter01();
                m_fNextTriggerWorldSec = worldTimeSec + armDelay;
            }
        }

        if (m_fNextTriggerWorldSec < 0.0)
            return;
        if (worldTimeSec + 0.0001 < m_fNextTriggerWorldSec)
            return;

        if (m_iPhase == 0)
        {
            if (m_bWindingDown)
            {
                DisarmCycle(false);
                return;
            }
            PlayClip(true);
            m_iPhase = 1;
            m_fNextTriggerWorldSec = worldTimeSec + CLIP_DURATION_SEC
                + PauseAfterInhaleSec(m_fSmoothedFatigue);
            return;
        }

        if (m_iPhase == 1)
        {
            PlayClip(false);
            m_iPhase = 2;
            if (m_bWindingDown)
            {
                // 呼完收尾
                m_fNextTriggerWorldSec = worldTimeSec + CLIP_DURATION_SEC + 0.05;
                return;
            }
            m_fNextTriggerWorldSec = worldTimeSec + CLIP_DURATION_SEC
                + RestAfterExhaleSec(m_fSmoothedFatigue);
            return;
        }

        // phase 2：休息结束
        if (m_bWindingDown)
        {
            DisarmCycle(false);
            return;
        }
        m_iPhase = 0;
        m_fNextTriggerWorldSec = worldTimeSec;
    }

    void Cleanup()
    {
        DisarmCycle(true);
    }

    protected float ComputeFatigue01(float presentation01)
    {
        float start = SCR_RSS_Constants.V6_BREATH_SOUND_PRESENTATION_START;
        float hard = SCR_RSS_Constants.V6_BREATH_SOUND_PRESENTATION_HARD;
        if (start <= hard + 0.001)
            return 0.0;

        presentation01 = Math.Clamp(presentation01, 0.0, 1.0);
        if (presentation01 >= start)
            return 0.0;
        if (presentation01 <= hard)
            return 1.0;

        float t = (start - presentation01) / (start - hard);
        // 缓入：轻喘更轻，重喘才明显
        return t * t;
    }

    //! 吸→呼间隙：轻喘略拉开，重喘几乎贴着
    protected float PauseAfterInhaleSec(float fatigue01)
    {
        float loose = 0.22;
        float tight = 0.03;
        float basePause = loose + (tight - loose) * fatigue01;
        float jitter = (NextJitter01() - 0.5) * 0.08;
        float p = basePause + jitter;
        if (p < 0.02)
            p = 0.02;
        return p;
    }

    //! 一整口后的休息：轻喘可隔 1.5–2.5s，重喘几乎连续
    protected float RestAfterExhaleSec(float fatigue01)
    {
        float loose = 2.10;
        float tight = 0.06;
        float baseRest = loose + (tight - loose) * fatigue01;
        float jitter = (NextJitter01() - 0.5) * (0.35 + 0.25 * (1.0 - fatigue01));
        float r = baseRest + jitter;
        if (r < 0.04)
            r = 0.04;
        return r;
    }

    protected float NextJitter01()
    {
        // 简单 LCG，避免每帧同相位（Enforce 无 % / 位与）
        m_iJitterSeed = m_iJitterSeed * 1103515245 + 12345;
        if (m_iJitterSeed < 0)
            m_iJitterSeed = -m_iJitterSeed;
        int bucket = Math.Mod(m_iJitterSeed, 1000);
        return bucket / 1000.0;
    }

    protected void PlayClip(bool inhale)
    {
        // 不硬切上一声：正常按时间表应已播完；仅异常重叠时淡出
        if (m_AudioHandle != AudioHandle.Invalid)
        {
            if (AudioSystem.IsSoundPlayed(m_AudioHandle))
                AudioSystem.TerminateSoundFadeOut(m_AudioHandle, true, 0.08);
            m_AudioHandle = AudioHandle.Invalid;
        }

        if (inhale)
            m_AudioHandle = AudioSystem.PlaySound(SOUND_BREATH_IN);
        else
            m_AudioHandle = AudioSystem.PlaySound(SOUND_BREATH_OUT);
    }

    protected void DisarmCycle(bool forceStop)
    {
        m_bCycleArmed = false;
        m_bWindingDown = false;
        m_iPhase = 0;
        m_fNextTriggerWorldSec = -1.0;

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
                AudioSystem.TerminateSoundFadeOut(m_AudioHandle, true, 0.18);
            m_AudioHandle = AudioHandle.Invalid;
        }
    }
}
