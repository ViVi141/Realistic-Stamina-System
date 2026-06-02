/*
 * SCR_SpeedRecalibration_V5.c
 * 
 * v5新增: 速度重标定模块
 * Phase 1: 将35kg理想环境下的目标速度从v4降低
 */

class SCR_SpeedRecalibration_V5
{
	// =========================================================================
	// 获取v5目标速度(35kg理想环境)
	// =========================================================================
	
	static float GetWalkSpeed_35kg_IdealEnv(string preset)
	{
		if (preset == "Elite")
		{
			return 1.4;
		}
		else if (preset == "Standard")
		{
			return 1.5;
		}
		else if (preset == "Tactical")
		{
			return 1.7;
		}
		else
		{
			return 1.4;
		}
	}
	
	static float GetRunSpeed_35kg_IdealEnv(string preset)
	{
		if (preset == "Elite")
		{
			return 2.8;
		}
		else if (preset == "Standard")
		{
			return 3.0;
		}
		else if (preset == "Tactical")
		{
			return 3.2;
		}
		else
		{
			return 2.8;
		}
	}
	
	static float GetSprintSpeed_35kg_IdealEnv(string preset)
	{
		if (preset == "Elite")
		{
			return 4.0;
		}
		else if (preset == "Standard")
		{
			return 4.2;
		}
		else if (preset == "Tactical")
		{
			return 4.5;
		}
		else
		{
			return 4.0;
		}
	}
	
	// =========================================================================
	// 负重修正
	// =========================================================================
	
	static float ApplyLoadEncumbrance(float speed_35kg, float currentWeight)
	{
		if (currentWeight <= 35.0)
		{
			float lightness = 35.0 - currentWeight;
			return speed_35kg * (1.0 + lightness * 0.01);
		}
		else if (currentWeight <= 50.0)
		{
			float excess = currentWeight - 35.0;
			return speed_35kg * (1.0 - excess * 0.008);
		}
		else
		{
			float excess = currentWeight - 50.0;
			float ratio = 1.0 - excess * 0.015;
			if (ratio < 0.5)
				ratio = 0.5;
			return speed_35kg * ratio;
		}
	}
	
	// =========================================================================
	// 坡度修正: Tobler行走函数
	// =========================================================================
	
	static float GetTobler(float gradePct)
	{
		float gradeDecimal = gradePct / 100.0;
		float S = gradeDecimal;
		
		// W(S) = 6 * exp(-3.5 * |S + 0.05|)
		float absVal = S + 0.05;
		if (absVal < 0)
			absVal = -absVal;
		
		float exponent = -3.5 * absVal;
		float W = 6.0 * Math.Pow(2.71828, exponent);
		float mult = W / 5.039;
		
		if (mult < 0.15)
			mult = 0.15;
		if (mult > 1.25)
			mult = 1.25;
		
		return mult;
	}
	
	// =========================================================================
	// 集成方法
	// =========================================================================
	
	static float CalculateFinalWalkSpeed(
		string preset,
		float currentWeight,
		float gradePct,
		float terrainFactor)
	{
		float baseSpeed = GetWalkSpeed_35kg_IdealEnv(preset);
		float withLoad = ApplyLoadEncumbrance(baseSpeed, currentWeight);
		float withGrade = withLoad * GetTobler(gradePct);
		float final = withGrade * terrainFactor;
		
		if (final < 0.5)
			final = 0.5;
		
		return final;
	}
	
	static float CalculateFinalRunSpeed(
		string preset,
		float currentWeight,
		float gradePct,
		float terrainFactor)
	{
		float baseSpeed = GetRunSpeed_35kg_IdealEnv(preset);
		float withLoad = ApplyLoadEncumbrance(baseSpeed, currentWeight);
		float withGrade = withLoad * GetTobler(gradePct);
		float final = withGrade * terrainFactor;
		
		if (final < 0.8)
			final = 0.8;
		
		return final;
	}
	
	static float CalculateFinalSprintSpeed(
		string preset,
		float currentWeight,
		float gradePct,
		float terrainFactor)
	{
		float baseSpeed = GetSprintSpeed_35kg_IdealEnv(preset);
		float withLoad = ApplyLoadEncumbrance(baseSpeed, currentWeight);
		float withGrade = withLoad * GetTobler(gradePct);
		float final = withGrade * terrainFactor;
		
		if (final < 1.0)
			final = 1.0;
		
		return final;
	}
	
	static void DebugPrintSpeeds(string preset, float weight)
	{
		float walk = CalculateFinalWalkSpeed(preset, weight, 0, 1.0);
		float run = CalculateFinalRunSpeed(preset, weight, 0, 1.0);
		float sprint = CalculateFinalSprintSpeed(preset, weight, 0, 1.0);
		
		SCR_RSS_Logger.Debug(string.Format("[SpeedRecal] %s @ %.1fkg: Walk=%.2f Run=%.2f Sprint=%.2f",
			preset, weight, walk, run, sprint));
	}
};