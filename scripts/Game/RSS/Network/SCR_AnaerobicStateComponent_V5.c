/*
 * SCR_AnaerobicStateComponent_V5.c
 * 
 * v5网络同步组件: 负责同步无氧冲刺状态到所有客户端
 * 
 * 设计:
 * - 服务端权威: 服务端计算并通过RplProp广播
 * - 客户端预测: 本地玩家使用本地计算值,收到服务端数据时进行校正
 * - 混合模型: 平衡响应性和公平性
 */

[ComponentEditorProps(category: "RSS/Network", description: "RSS v5 Anaerobic State Network Sync")]
class SCR_AnaerobicStateComponent_V5Class : ScriptComponentClass
{
}

class SCR_AnaerobicStateComponent_V5 : ScriptComponent
{
	// =========================================================================
	// 网络同步变量（RplProp标记）
	// =========================================================================
	
	[RplProp(onRplName: "OnAnaerobicEnergyChanged")]
	protected float m_fAnaerobicEnergy;
	
	[RplProp(onRplName: "OnCooldownChanged")]
	protected float m_fCooldownRemaining;
	
	[RplProp(onRplName: "OnSprintStateChanged")]
	protected bool m_bCanSprint;
	
	// =========================================================================
	// 同步事件回调
	// =========================================================================
	
	protected void OnAnaerobicEnergyChanged()
	{
		if (Replication.IsClient())
		{
			// 客户端收到服务端更新，通知UI刷新
		}
	}
	
	protected void OnCooldownChanged()
	{
		if (Replication.IsClient())
		{
			// 客户端收到冷却更新
		}
	}
	
	protected void OnSprintStateChanged()
	{
		if (Replication.IsClient())
		{
			// 客户端收到冲刺状态更新
		}
	}
	
	// =========================================================================
	// 公开接口
	// =========================================================================
	
	void SetAnaerobicEnergy(float value)
	{
		if (!Replication.IsServer())
			return;
		
		m_fAnaerobicEnergy = Math.Clamp(value, 0.0, 1.0);
	}
	
	float GetAnaerobicEnergy()
	{
		return m_fAnaerobicEnergy;
	}
	
	void SetCooldownRemaining(float value)
	{
		if (!Replication.IsServer())
			return;
		
		if (value < 0.0)
			value = 0.0;
		m_fCooldownRemaining = value;
	}
	
	float GetCooldownRemaining()
	{
		return m_fCooldownRemaining;
	}
	
	void SetCanSprint(bool value)
	{
		if (!Replication.IsServer())
			return;
		
		m_bCanSprint = value;
	}
	
	bool GetCanSprint()
	{
		return m_bCanSprint;
	}
	
	// =========================================================================
	// 服务端同步接口: 从PlayerBase的逻辑类同步到网络组件
	// =========================================================================
	
	void SyncFromLogicState(SCR_AnaerobicBurstState logicState)
	{
		if (!Replication.IsServer())
			return;
		
		if (!logicState)
			return;
		
		SetAnaerobicEnergy(logicState.GetAnaerobicEnergy());
		SetCooldownRemaining(logicState.GetCooldownRemaining());
		SetCanSprint(logicState.CanSprint());
	}
	
	// =========================================================================
	// 组件初始化
	// =========================================================================
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_fAnaerobicEnergy = 1.0;
		m_fCooldownRemaining = 0.0;
		m_bCanSprint = true;
		
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	override void EOnInit(IEntity owner)
	{
		// 组件初始化完成
	}
}
