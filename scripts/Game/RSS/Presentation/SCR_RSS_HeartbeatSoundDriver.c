//! 心跳采样：RSS-CPCR Cardiac 轴；稀疏提醒式爆发 + 音量渐变（不连播）

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

    protected int m_iTier = 0;
    protected AudioHandle m_AudioHandle = AudioHandle.Invalid;
    protected float m_fNextTriggerWorldSec = -1.0;
    protected bool m_bArmed = false;
    protected float m_fDrive01 = 0.0;
    protected float m_fVolumeSmoothed = 0.0;
    protected int m_iBeatsLeftInBurst = 0;
    protected int m_iJitterSeed = 17;

    void Update(float worldTimeSec, float cardiac01)
    {
        if (!SCR_RSS_Constants.V6_HEARTBEAT_SOUND_ENABLED)
        {
            Cleanup();
            return;
        }

        m_fDrive01 = Math.Clamp(cardiac01, 0.0, 1.0);
        float audible = SCR_RSS_Constants.V6_CARDIO_HEART_AUDIBLE;

        float volTarget = 0.0;
        if (m_fDrive01 > audible)
        {
            volTarget = SCR_RSS_PresentationAudio.DriveToVolume(
                m_fDrive01,
                SCR_RSS_Constants.V6_HEARTBEAT_VOL_MIN,
                SCR_RSS_Constants.V6_HEARTBEAT_VOL_MAX);
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

        if (m_fDrive01 <= audible)
        {
            m_bArmed = false;
            m_iBeatsLeftInBurst = 0;
            return;
        }

        if (!m_bArmed)
        {
            m_bArmed = true;
            m_iBeatsLeftInBurst = 0;
            // 软起：先听一段时间再第一次提醒
            float armDelay = SCR_RSS_Constants.V6_HEARTBEAT_ARM_DELAY_SEC
                + 1.2 * NextJitter01();
            m_fNextTriggerWorldSec = worldTimeSec + armDelay;
        }

        if (m_fNextTriggerWorldSec < 0.0)
            return;
        if (worldTimeSec + 0.0001 < m_fNextTriggerWorldSec)
            return;

        // 爆发中：连打剩余拍；否则开启新提醒爆发
        if (m_iBeatsLeftInBurst <= 0)
            m_iBeatsLeftInBurst = ResolveBurstCount(m_fDrive01);

        m_iTier = ResolveTier(m_fDrive01, m_iTier);
        float clipSec = PlayTierClip(m_iTier);
        SCR_RSS_PresentationAudio.ApplyVolume(m_AudioHandle, m_fVolumeSmoothed);

        m_iBeatsLeftInBurst = m_iBeatsLeftInBurst - 1;
        if (m_iBeatsLeftInBurst > 0)
        {
            float intra = SCR_RSS_Constants.V6_HEARTBEAT_INTRA_BURST_GAP_SEC
                + 0.08 * NextJitter01();
            m_fNextTriggerWorldSec = worldTimeSec + clipSec + intra;
            return;
        }

        // 爆发结束 → 长冷却（提醒而非持续 thrub）
        float cool = CooldownAfterBurstSec(m_fDrive01);
        m_fNextTriggerWorldSec = worldTimeSec + clipSec + cool;
    }

    void Cleanup()
    {
        m_bArmed = false;
        m_iBeatsLeftInBurst = 0;
        m_fNextTriggerWorldSec = -1.0;
        m_fDrive01 = 0.0;
        m_fVolumeSmoothed = 0.0;
        m_iTier = 0;
        if (m_AudioHandle != AudioHandle.Invalid)
        {
            if (!AudioSystem.IsSoundPlayed(m_AudioHandle))
                AudioSystem.TerminateSoundFadeOut(m_AudioHandle, true, 0.25);
            m_AudioHandle = AudioHandle.Invalid;
        }
    }

    protected int ResolveBurstCount(float drive01)
    {
        if (drive01 >= SCR_RSS_Constants.V6_HEARTBEAT_TIER_FAST_ENTER)
            return 2;
        if (drive01 >= SCR_RSS_Constants.V6_HEARTBEAT_TIER_MID_ENTER)
            return 2;
        return 1;
    }

    protected float CooldownAfterBurstSec(float drive01)
    {
        float loose = SCR_RSS_Constants.V6_HEARTBEAT_COOLDOWN_MAX_SEC;
        float tight = SCR_RSS_Constants.V6_HEARTBEAT_COOLDOWN_MIN_SEC;
        float cool = loose + (tight - loose) * drive01;
        float jitter = (NextJitter01() - 0.5) * 2.5;
        cool = cool + jitter;
        if (cool < tight * 0.85)
            cool = tight * 0.85;
        return cool;
    }

    protected int ResolveTier(float drive01, int currentTier)
    {
        float midEnter = SCR_RSS_Constants.V6_HEARTBEAT_TIER_MID_ENTER;
        float midExit = SCR_RSS_Constants.V6_HEARTBEAT_TIER_MID_EXIT;
        float fastEnter = SCR_RSS_Constants.V6_HEARTBEAT_TIER_FAST_ENTER;
        float fastExit = SCR_RSS_Constants.V6_HEARTBEAT_TIER_FAST_EXIT;

        if (currentTier >= 2)
        {
            if (drive01 < fastExit)
            {
                if (drive01 < midExit)
                    return 0;
                return 1;
            }
            return 2;
        }

        if (currentTier == 1)
        {
            if (drive01 >= fastEnter)
                return 2;
            if (drive01 < midExit)
                return 0;
            return 1;
        }

        if (drive01 >= fastEnter)
            return 2;
        if (drive01 >= midEnter)
            return 1;
        return 0;
    }

    protected float PlayTierClip(int tier)
    {
        if (m_AudioHandle != AudioHandle.Invalid)
        {
            if (!AudioSystem.IsSoundPlayed(m_AudioHandle))
                AudioSystem.TerminateSoundFadeOut(m_AudioHandle, true, 0.05);
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

    protected float NextJitter01()
    {
        m_iJitterSeed = m_iJitterSeed * 1103515245 + 12345;
        if (m_iJitterSeed < 0)
            m_iJitterSeed = -m_iJitterSeed;
        int bucket = Math.Mod(m_iJitterSeed, 1000);
        return bucket / 1000.0;
    }
}
