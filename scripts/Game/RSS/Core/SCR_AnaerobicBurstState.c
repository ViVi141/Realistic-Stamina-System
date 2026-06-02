/*
 * SCR_AnaerobicBurstState.c
 * 
 * v5新增: 无氧冲刺爆发状态管理
 * 
 * 注意: 这是一个独立的逻辑类
 * 应由PlayerBase通过组件或全局状态持有
 */

class SCR_AnaerobicBurstState
{
	// =========================================================================
	// 网络同步变量
	// =========================================================================
	
	protected float m_fAnaerobicEnergy;      // 0-1
	protected float m_fCooldownRemaining;    // 秒
	protected bool m_bCanSprint;             // 是否可冲刺
	
	// =========================================================================
	// 本地变量
	// =========================================================================
	
	protected bool m_bIsSprintActive;
	protected float m_fSyncHeartbeatTimer;
	
	// =========================================================================
	// 配置参数
	// =========================================================================
	
	protected float m_fAnaerobicDrainRate;
	protected float m_fAnaerobicRecoveryBase;
	protected float m_fSprintEnableThreshold;
	protected float m_fCooldownFull;
	protected float m_fCooldownShort;
	protected float m_fEarlyReleaseBonusCoeff;
	protected float m_fAerobicGateForRecovery;
	
	// =========================================================================
	// 构造函数
	// =========================================================================
	
	void SCR_AnaerobicBurstState()
	{
		m_fAnaerobicEnergy = 1.0;
		m_fCooldownRemaining = 0;
		m_bCanSprint = true;
		
		m_bIsSprintActive = false;
		m_fSyncHeartbeatTimer = 0;
		
		LoadParametersFromPreset();
	}
	
	// =========================================================================
	// 公开查询接口
	// =========================================================================
	
	float GetAnaerobicEnergy()
	{
		return m_fAnaerobicEnergy;
	}
	
	bool CanSprint()
	{
		return m_bCanSprint && (m_fCooldownRemaining <= 0);
	}
	
	float GetCooldownRemaining()
	{
		return m_fCooldownRemaining;
	}
	
	float GetCooldownProgress()
	{
		if (m_fCooldownFull <= 0)
			return 1.0;
		
		return 1.0 - (m_fCooldownRemaining / m_fCooldownFull);
	}
	
	bool IsSprintActive()
	{
		return m_bIsSprintActive;
	}
	
	// =========================================================================
	// 状态变更接口
	// =========================================================================
	
	void OnSprintStart(float currentVelocity, float loadWeight)
	{
		m_bIsSprintActive = true;
	}
	
	void OnSprintEnd(bool forcedByDepletion)
	{
		m_bIsSprintActive = false;
		
		float cooldownTime = 0;
		
		if (forcedByDepletion)
		{
			cooldownTime = m_fCooldownFull;
		}
		else
		{
			float reserveRatio = m_fAnaerobicEnergy;
			float bonus = m_fEarlyReleaseBonusCoeff * reserveRatio;
			
			cooldownTime = m_fCooldownFull * (1.0 - bonus);
			if (cooldownTime < m_fCooldownShort * 0.8)
				cooldownTime = m_fCooldownShort * 0.8;
		}
		
		m_fCooldownRemaining = cooldownTime;
		
		// 触发UI事件
		UISignalBridge bridge = UISignalBridge.GetInstance();
		if (bridge)
		{
			bridge.OnSprintCooldownStarted(cooldownTime, forcedByDepletion);
		}
	}
	
	// =========================================================================
	// 每帧更新
	// =========================================================================
	
	void TickUpdate(float deltaTime, float aerobicStamina, float drainVelocity, float loadWeight)
	{
		if (m_bIsSprintActive)
		{
			UpdateAnaerobicDrain(deltaTime, drainVelocity, loadWeight);
		}
		else
		{
			UpdateAnaerobicRecovery(deltaTime, aerobicStamina);
			UpdateCooldown(deltaTime);
		}
	}
	
	protected void UpdateAnaerobicDrain(float deltaTime, float drainVelocity, float loadWeight)
	{
		float refSpeed = 4.0;
		float speedRatio = drainVelocity / refSpeed;
		if (speedRatio < 0.5)
			speedRatio = 0.5;
		
		float loadFactor = 1.0 + (loadWeight - 35.0) * 0.01;
		if (loadFactor < 0.5)
			loadFactor = 0.5;
		
		float drainPerSec = m_fAnaerobicDrainRate;
		drainPerSec = drainPerSec * speedRatio * loadFactor;
		
		m_fAnaerobicEnergy -= drainPerSec * deltaTime;
		if (m_fAnaerobicEnergy < 0.0)
			m_fAnaerobicEnergy = 0.0;
		
		if (m_fAnaerobicEnergy < 0.01)
		{
			OnSprintEnd(true);
		}
	}
	
	protected void UpdateAnaerobicRecovery(float deltaTime, float aerobicStamina)
	{
		if (m_fCooldownRemaining > 0)
			return;
		
		if (aerobicStamina < m_fAerobicGateForRecovery)
			return;
		
		float recoveryRate = m_fAnaerobicRecoveryBase * deltaTime;
		
		m_fAnaerobicEnergy += recoveryRate;
		if (m_fAnaerobicEnergy > 1.0)
			m_fAnaerobicEnergy = 1.0;
	}
	
	protected void UpdateCooldown(float deltaTime)
	{
		if (m_fCooldownRemaining > 0)
		{
			m_fCooldownRemaining -= deltaTime;
			if (m_fCooldownRemaining < 0)
				m_fCooldownRemaining = 0;
		}
	}
	
	// =========================================================================
	// 配置加载
	// =========================================================================
	
	protected void LoadParametersFromPreset()
	{
		m_fAnaerobicDrainRate = 0.08;
		m_fAnaerobicRecoveryBase = 0.01;
		m_fSprintEnableThreshold = 0.20;
		m_fCooldownFull = 180.0;
		m_fCooldownShort = 75.0;
		m_fEarlyReleaseBonusCoeff = 0.4;
		m_fAerobicGateForRecovery = 0.15;
	}
	
	void DebugPrint()
	{
		string canSprintStr;
		if (m_bCanSprint)
		{
			canSprintStr = "true";
		}
		else
		{
			canSprintStr = "false";
		}
		
		SCR_RSS_Logger.Debug(string.Format("[AnaerobicState] Energy=%.2f Cooldown=%.1f CanSprint=%s",
			m_fAnaerobicEnergy, m_fCooldownRemaining, canSprintStr));
	}
};