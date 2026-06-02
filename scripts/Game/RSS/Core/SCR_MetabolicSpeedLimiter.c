/*
 * SCR_MetabolicSpeedLimiter.c
 * 
 * v5新增: 强制降速机制
 * 功能: 当代谢功率超过sustainable_watts时,自动降低移动速度
 * 
 * 注意: 这是一个独立的逻辑类,不是ScriptComponent
 * 应从PlayerBase.c中引用或作为全局单例
 */

class SCR_MetabolicSpeedLimiter
{
	// =========================================================================
	// 状态变量
	// =========================================================================
	
	protected float m_fCurrentSpeedRatio;         // 当前速度比例(1.0=无限制)
	protected float m_fOverrideTimer;             // 硬撑计时器
	protected bool m_bPlayerOverriding;           // 玩家是否在硬撑
	protected float m_fLastOverloadRatio;         // 上次过载比例
	protected bool m_bLastSlowdownTriggered;      // 上次是否触发过降速反馈
	
	// =========================================================================
	// 配置参数
	// =========================================================================
	
	protected float m_fSustainableWatts;
	protected float m_fForcedSlowdownDecayRate;
	protected float m_fForcedSlowdownMinRatio;
	protected bool m_bEnablePlayerOverride;
	protected float m_fOverrideMaxDuration;
	
	// =========================================================================
	// 构造函数
	// =========================================================================
	
	void SCR_MetabolicSpeedLimiter()
	{
		m_fCurrentSpeedRatio = 1.0;
		m_fOverrideTimer = 0;
		m_bPlayerOverriding = false;
		m_fLastOverloadRatio = 0;
		m_bLastSlowdownTriggered = false;
		
		LoadParameters();
	}
	
	// =========================================================================
	// 公开接口
	// =========================================================================
	
	float GetCurrentSpeedRatio()
	{
		return m_fCurrentSpeedRatio;
	}
	
	bool IsPlayerOverriding()
	{
		return m_bPlayerOverriding;
	}
	
	float GetOverrideTimeRemaining()
	{
		if (!m_bPlayerOverriding)
			return 0;
		
		float remaining = m_fOverrideMaxDuration - m_fOverrideTimer;
		if (remaining < 0)
			remaining = 0;
		return remaining;
	}
	
	void OnPlayerInitiateOverride()
	{
		if (!m_bEnablePlayerOverride)
			return;
		
		if (m_fCurrentSpeedRatio >= 0.95)
			return;
		
		m_bPlayerOverriding = true;
		m_fOverrideTimer = 0;
	}
	
	void OnPlayerCancelOverride()
	{
		if (m_bPlayerOverriding)
		{
			m_bPlayerOverriding = false;
		}
	}
	
	// =========================================================================
	// 更新逻辑
	// =========================================================================
	
	void TickUpdate(float tickDeltaTime, float pandolfWatts)
	{
		float sustainableWatts = CalculateSustainableWatts();
		
		if (pandolfWatts > sustainableWatts)
		{
			HandleOverload(tickDeltaTime, pandolfWatts, sustainableWatts);
		}
		else
		{
			HandleNormalLoad(tickDeltaTime);
		}
	}
	
	protected void HandleOverload(float tickDeltaTime, float pandolfWatts, float sustainableWatts)
	{
		float overloadRatio = pandolfWatts / sustainableWatts;
		m_fLastOverloadRatio = overloadRatio;
		
		if (m_bPlayerOverriding)
		{
			m_fOverrideTimer += tickDeltaTime;
			
			if (m_fOverrideTimer > m_fOverrideMaxDuration)
			{
				OnPlayerCancelOverride();
			}
		}
		else
		{
			float decayRate = m_fForcedSlowdownDecayRate * tickDeltaTime;
			float overloadFactor = Math.Pow(overloadRatio - 1.0, 0.5);
			decayRate = decayRate * overloadFactor;
			
			m_fCurrentSpeedRatio -= decayRate;
			if (m_fCurrentSpeedRatio < m_fForcedSlowdownMinRatio)
				m_fCurrentSpeedRatio = m_fForcedSlowdownMinRatio;
		}
		
		if (m_fCurrentSpeedRatio < 0.90)
		{
			if (!m_bLastSlowdownTriggered)
			{
				m_bLastSlowdownTriggered = true;
				
				// 触发UI事件
				UISignalBridge bridge = UISignalBridge.GetInstance();
				if (bridge)
				{
					bool severe;
					if (m_fCurrentSpeedRatio < 0.70)
					{
						severe = true;
					}
					else
					{
						severe = false;
					}
					bridge.OnMetabolicOverload(severe);
				}
			}
		}
	}
	
	protected void HandleNormalLoad(float tickDeltaTime)
	{
		if (m_fCurrentSpeedRatio < 1.0)
		{
			m_fCurrentSpeedRatio += 0.05 * tickDeltaTime;
			if (m_fCurrentSpeedRatio > 1.0)
				m_fCurrentSpeedRatio = 1.0;
		}
		
		m_fOverrideTimer = 0;
		m_bLastSlowdownTriggered = false;
	}
	
	protected float CalculateSustainableWatts()
	{
		return m_fSustainableWatts;
	}
	
	protected void LoadParameters()
	{
		m_fSustainableWatts = 400.0;
		m_fForcedSlowdownDecayRate = 0.02;
		m_fForcedSlowdownMinRatio = 0.5;
		m_bEnablePlayerOverride = true;
		m_fOverrideMaxDuration = 45.0;
	}
	
	void DebugPrint()
	{
		string overrideStr;
		if (m_bPlayerOverriding)
		{
			overrideStr = "true";
		}
		else
		{
			overrideStr = "false";
		}
		
		SCR_RSS_Logger.Debug(string.Format("[MetabolicLimiter] SpeedRatio=%.2f Override=%s",
			m_fCurrentSpeedRatio, overrideStr));
	}
};