//! 呼吸采样：RSS-CPCR Respiratory 轴 + 音量随驱动强弱渐变

class SCR_RSS_BreathSoundDriver
{
    protected static const string SOUND_BREATH_IN =
        "{48C8925347BEF7EB}Sounds/Character/Voice/Samples/Eng/Male1/Breath/Character_Voice_Breath_Regenerate_In_500_02.wav";
    protected static const string SOUND_BREATH_OUT =
        "{FCD113283AF2E71D}Sounds/Character/Voice/Samples/Eng/Male1/Breath/Character_Voice_Breath_Regenerate_Out_500_02.wav";

    protected static const float CLIP_DURATION_SEC = 0.5;

    protected int m_iPhase = 0;
    protected AudioHandle m_AudioHandle = AudioHandle.Invalid;
    protected float m_fNextTriggerWorldSec = -1.0;
    protected bool m_bCycleArmed = false;
    protected bool m_bWindingDown = false;
    protected float m_fDrive01 = 0.0;
    protected float m_fVolumeSmoothed = 0.0;
    protected int m_iJitterSeed = 1;

    void Update(float worldTimeSec, float respiratory01)
    {
        if (!SCR_RSS_Constants.V6_BREATH_SOUND_ENABLED)
        {
            DisarmCycle(true);
            return;
        }

        m_fDrive01 = Math.Clamp(respiratory01, 0.0, 1.0);
        float audible = SCR_RSS_Constants.V6_CARDIO_BREATH_AUDIBLE;

        float volTarget = 0.0;
        if (m_fDrive01 > audible)
        {
            volTarget = SCR_RSS_PresentationAudio.DriveToVolume(
                m_fDrive01,
                SCR_RSS_Constants.V6_BREATH_VOL_MIN,
                SCR_RSS_Constants.V6_BREATH_VOL_MAX);
        }

        m_fVolumeSmoothed = SCR_RSS_PresentationAudio.SmoothToward(
            m_fVolumeSmoothed, volTarget, SCR_RSS_Constants.V6_PRESENTATION_VOL_SMOOTH);

        // IsSoundPlayed==true 表示已结束
        if (m_AudioHandle != AudioHandle.Invalid)
        {
            if (AudioSystem.IsSoundPlayed(m_AudioHandle))
                m_AudioHandle = AudioHandle.Invalid;
            else
                SCR_RSS_PresentationAudio.ApplyVolume(m_AudioHandle, m_fVolumeSmoothed);
        }

        bool wantBreath = false;
        if (m_fDrive01 > audible)
            wantBreath = true;

        if (!wantBreath)
        {
            if (m_bCycleArmed)
            {
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
                float armDelay = 0.20 + 0.40 * (1.0 - m_fDrive01) + 0.20 * NextJitter01();
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
                + PauseAfterInhaleSec(m_fDrive01);
            return;
        }

        if (m_iPhase == 1)
        {
            PlayClip(false);
            m_iPhase = 2;
            if (m_bWindingDown)
            {
                m_fNextTriggerWorldSec = worldTimeSec + CLIP_DURATION_SEC + 0.05;
                return;
            }
            m_fNextTriggerWorldSec = worldTimeSec + CLIP_DURATION_SEC
                + RestAfterExhaleSec(m_fDrive01);
            return;
        }

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

    protected float PauseAfterInhaleSec(float drive01)
    {
        // 激进：轻喘间隙也更短，极累几乎贴采样
        float loose = 0.14;
        float tight = 0.01;
        float basePause = loose + (tight - loose) * drive01;
        float jitter = (NextJitter01() - 0.5) * 0.06;
        float p = basePause + jitter;
        if (p < 0.01)
            p = 0.01;
        return p;
    }

    protected float RestAfterExhaleSec(float drive01)
    {
        // 激进：轻喘约 1.2s 休息，极累贴近官方 ~1.1 Hz
        float loose = 1.20;
        float tight = 0.02;
        float baseRest = loose + (tight - loose) * drive01;
        float jitter = (NextJitter01() - 0.5) * (0.22 + 0.18 * (1.0 - drive01));
        float r = baseRest + jitter;
        if (r < 0.02)
            r = 0.02;
        return r;
    }

    protected float NextJitter01()
    {
        m_iJitterSeed = m_iJitterSeed * 1103515245 + 12345;
        if (m_iJitterSeed < 0)
            m_iJitterSeed = -m_iJitterSeed;
        int bucket = Math.Mod(m_iJitterSeed, 1000);
        return bucket / 1000.0;
    }

    protected void PlayClip(bool inhale)
    {
        if (m_AudioHandle != AudioHandle.Invalid)
        {
            if (!AudioSystem.IsSoundPlayed(m_AudioHandle))
                AudioSystem.TerminateSoundFadeOut(m_AudioHandle, true, 0.08);
            m_AudioHandle = AudioHandle.Invalid;
        }

        if (inhale)
            m_AudioHandle = AudioSystem.PlaySound(SOUND_BREATH_IN);
        else
            m_AudioHandle = AudioSystem.PlaySound(SOUND_BREATH_OUT);

        SCR_RSS_PresentationAudio.ApplyVolume(m_AudioHandle, m_fVolumeSmoothed);
    }

    protected void DisarmCycle(bool forceStop)
    {
        m_bCycleArmed = false;
        m_bWindingDown = false;
        m_iPhase = 0;
        m_fNextTriggerWorldSec = -1.0;

        if (!forceStop)
            return;

        m_fDrive01 = 0.0;
        m_fVolumeSmoothed = 0.0;
        if (m_AudioHandle != AudioHandle.Invalid)
        {
            if (!AudioSystem.IsSoundPlayed(m_AudioHandle))
                AudioSystem.TerminateSoundFadeOut(m_AudioHandle, true, 0.18);
            m_AudioHandle = AudioHandle.Invalid;
        }
    }
}
