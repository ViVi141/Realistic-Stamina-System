//! RSS 表现音效工具：PlaySound 句柄音量（经 SoundManagerModule.SetVolume）

class SCR_RSS_PresentationAudio
{
    static void ApplyVolume(AudioHandle handle, float volume01)
    {
        if (handle == AudioHandle.Invalid)
            return;
        if (!GetGame() || !GetGame().GetWorld())
            return;

        SCR_SoundManagerModule soundManager = SCR_SoundManagerModule.GetInstance(
            GetGame().GetWorld());
        if (!soundManager)
            return;

        volume01 = Math.Clamp(volume01, 0.0, 1.0);
        soundManager.SetVolume(handle, volume01);
    }

    //! 驱动轴 → 目标音量（带底与顶）
    static float DriveToVolume(float drive01, float volMin, float volMax)
    {
        drive01 = Math.Clamp(drive01, 0.0, 1.0);
        if (volMax < volMin)
            volMax = volMin;
        return volMin + (volMax - volMin) * drive01;
    }

    static float SmoothToward(float current, float target, float alpha)
    {
        if (alpha < 0.0)
            alpha = 0.0;
        if (alpha > 1.0)
            alpha = 1.0;
        return current + (target - current) * alpha;
    }
}
