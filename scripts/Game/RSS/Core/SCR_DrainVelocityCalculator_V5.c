/*
 * SCR_DrainVelocityCalculator_V5.c
 * 
 * v5新增: 速度-消耗闭环修正
 * Phase 1核心: 解决"消耗速度与理论速度不一致"问题
 * 
 * 问题陈述:
 * v4中Sprint/Run消耗使用固定的5.5/3.8 m/s,与实际限速(通过SetSpeedLimit)解耦
 * 导致"看起来没那么快,代谢按更快档位扣"现象
 * 
 * 解决方案:
 * v_drain = min(GetVelocity, theoretical_speed)
 * 确保消耗速度与实际移动一致
 */

class SCR_DrainVelocityCalculator_V5
{
	// =========================================================================
	// 关键方法: 计算用于消耗计算的速度
	// =========================================================================
	
	// 获取应用于消耗计算的速度(v_drain)
	// 输入: 当前测量速度(m/s), 移动状态(Walk/Run/Sprint), 当前无氧能量(0-1), 理论速度(动态计算)
	// 输出: 用于Pandolf计算的速度(m/s)
	static float GetDrainVelocity(
		float measuredVelocity,
		int movementPhase,  // 0=idle, 1=walk, 2=run, 3=sprint
		float anaerobicEnergy,
		float anaerobicThreshold,
		float theoreticalWalkSpeed = 1.4,
		float theoreticalRunSpeed = 3.0,
		float theoreticalSprintSpeed = 4.2
	)
	{
		float vDrain = 0;
		float vTheoretical = 0;
		
		switch (movementPhase)
		{
			case 0:  // IDLE
				vDrain = 0;
				break;
				
			case 1:  // WALK
				// v_drain = min(measured, theoretical_walk)
				// 理论Walk速度已通过SpeedRecalibration_V5计算(考虑负重/坡度/地形)
				vTheoretical = theoreticalWalkSpeed;
				vDrain = Math.Min(measuredVelocity, vTheoretical);
				break;
				
			case 2:  // RUN
				// v5修正: 使用动态计算的理论速度(考虑负重/坡度/地形)
				// v_drain = min(measured, theoretical_run)
				vTheoretical = theoreticalRunSpeed;
				vDrain = Math.Min(measuredVelocity, vTheoretical);
				break;
				
			case 3:  // SPRINT
				// v5修正: 使用动态计算的理论速度
				vTheoretical = theoreticalSprintSpeed;
				vDrain = Math.Min(measuredVelocity, vTheoretical);
				
				// 特殊处理: Sprint无氧池衰减
				// 当无氧<阈值时,速度向下衰减
				if (anaerobicEnergy < anaerobicThreshold && anaerobicThreshold > 0)
				{
					float decayFactor = anaerobicEnergy / anaerobicThreshold;
					float vRun = theoreticalRunSpeed;
					// 线性插值: 当无氧从阈值(20%)→0时,速度从Sprint→Run
					vTheoretical = Math.Lerp(vRun, vTheoretical, decayFactor);
					vDrain = Math.Min(measuredVelocity, vTheoretical);
				}
				break;
		}
		
		return Math.Max(vDrain, 0);
	}
	
	// =========================================================================
	// 内部方法: 获取理论速度 (已废弃 - Phase 2.2中移除)
	// =========================================================================
	
	// 注意: GetTheoreticalRunSpeed 和 GetTheoreticalSprintSpeed 方法已在 Phase 2.2 中移除
	// 现在使用 SCR_SpeedRecalibration_V5 动态计算理论速度,并通过参数传递给 GetDrainVelocity
	
	// =========================================================================
	// 调试方法
	// =========================================================================
	
	static void DebugPrintDrainVelocity(
		float measured,
		float drained,
		int phase,
		float anaerobic
	)
	{
		string phaseName = "";
		switch (phase)
		{
			case 0: phaseName = "IDLE"; break;
			case 1: phaseName = "WALK"; break;
			case 2: phaseName = "RUN"; break;
			case 3: phaseName = "SPRINT"; break;
		}
		
		SCR_RSS_Logger.Debug(string.Format(
			"[DrainVelocity] %s: measured=%.2f drained=%.2f anaerobic=%.2f",
			phaseName, measured, drained, anaerobic
		));
	}
};
