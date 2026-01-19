// EPOC延迟状态管理类
// 用于封装EPOC延迟相关的状态变量（因为EnforceScript不支持基本类型的ref参数）

class EpocState
{
    float m_fEpocDelayStartTime = -1.0; // EPOC延迟开始时间（世界时间），-1表示未启动
    bool m_bIsInEpocDelay = false; // 是否处于EPOC延迟期间
    float m_fLastSpeedForEpoc = 0.0; // 上一帧的速度（用于检测速度变化）
    float m_fSpeedBeforeStop = 0.0; // 停止前的速度（用于计算EPOC消耗）
    
    // 获取EPOC延迟开始时间
    float GetEpocDelayStartTime()
    {
        return m_fEpocDelayStartTime;
    }
    
    // 设置EPOC延迟开始时间
    void SetEpocDelayStartTime(float time)
    {
        m_fEpocDelayStartTime = time;
    }
    
    // 获取是否处于EPOC延迟期间
    bool IsInEpocDelay()
    {
        return m_bIsInEpocDelay;
    }
    
    // 设置是否处于EPOC延迟期间
    void SetIsInEpocDelay(bool value)
    {
        m_bIsInEpocDelay = value;
    }
    
    // 获取上一帧的速度
    float GetLastSpeedForEpoc()
    {
        return m_fLastSpeedForEpoc;
    }
    
    // 设置上一帧的速度
    void SetLastSpeedForEpoc(float speed)
    {
        m_fLastSpeedForEpoc = speed;
    }
    
    // 获取停止前的速度
    float GetSpeedBeforeStop()
    {
        return m_fSpeedBeforeStop;
    }
    
    // 设置停止前的速度
    void SetSpeedBeforeStop(float speed)
    {
        m_fSpeedBeforeStop = speed;
    }
    
    // 重置EPOC状态
    void Reset()
    {
        m_fEpocDelayStartTime = -1.0;
        m_bIsInEpocDelay = false;
        m_fSpeedBeforeStop = 0.0;
    }
}
