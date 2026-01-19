// 调试信息显示模块
// 负责格式化并显示调试信息
// 模块化拆分：从 PlayerBase.c 提取的调试显示逻辑

class DebugDisplay
{
    // ==================== 公共方法 ====================
    
    // 格式化移动类型字符串
    // @param isSprinting 是否正在Sprint
    // @param currentMovementPhase 当前移动阶段
    // @return 移动类型字符串
    static string FormatMovementType(bool isSprinting, int currentMovementPhase)
    {
        if (isSprinting || currentMovementPhase == 3)
            return "Sprint";
        else if (currentMovementPhase == 2)
            return "Run";
        else if (currentMovementPhase == 1)
            return "Walk";
        else if (currentMovementPhase == 0)
            return "Idle";
        return "Unknown";
    }
    
    // 格式化坡度信息字符串
    // @param slopeAngleDegrees 坡度角度（度）
    // @return 坡度信息字符串
    static string FormatSlopeInfo(float slopeAngleDegrees)
    {
        if (Math.AbsFloat(slopeAngleDegrees) <= 0.1)
            return "";
        
        string slopeDirection = "";
        if (slopeAngleDegrees > 0)
            slopeDirection = "上坡";
        else
            slopeDirection = "下坡";
        
        return string.Format(" | 坡度: %1%° (%2)", 
            Math.Round(Math.AbsFloat(slopeAngleDegrees) * 10.0) / 10.0,
            slopeDirection);
    }
    
    // 格式化Sprint信息字符串
    // @param isSprinting 是否正在Sprint
    // @param currentMovementPhase 当前移动阶段
    // @return Sprint信息字符串
    static string FormatSprintInfo(bool isSprinting, int currentMovementPhase)
    {
        if (!isSprinting && currentMovementPhase != 3)
            return "";
        
        return string.Format(" | Sprint消耗倍数: %1x", 
            RealisticStaminaSpeedSystem.SPRINT_STAMINA_DRAIN_MULTIPLIER.ToString());
    }
    
    // 格式化负重信息字符串
    // @param currentWeight 当前重量 (kg)
    // @param combatEncumbrancePercent 战斗负重百分比
    // @return 负重信息字符串
    static string FormatEncumbranceInfo(float currentWeight, float combatEncumbrancePercent)
    {
        if (currentWeight <= 0.0)
            return "";
        
        string combatStatus = "";
        if (combatEncumbrancePercent > 1.0)
            combatStatus = " [超过战斗负重]";
        else if (combatEncumbrancePercent >= 0.9)
            combatStatus = " [接近战斗负重]";
        
        float maxWeight = 40.5;
        float combatWeight = 30.0;
        
        return string.Format(" | 负重: %1kg/%2kg (最大:%3kg, 战斗:%4kg%5)", 
            currentWeight.ToString(), 
            maxWeight.ToString(),
            maxWeight.ToString(),
            combatWeight.ToString(),
            combatStatus);
    }
    
    // 格式化地形信息字符串
    // @param terrainDetector 地形检测模块引用
    // @param owner 角色实体
    // @param currentTime 当前时间
    // @return 地形信息字符串
    static string FormatTerrainInfo(TerrainDetector terrainDetector, IEntity owner, float currentTime)
    {
        if (!terrainDetector)
            return " | 地面密度: 未检测";
        
        // 强制更新地形检测（用于调试显示）
        terrainDetector.ForceUpdate(owner, currentTime);
        
        float cachedDensity = terrainDetector.GetCachedTerrainDensity();
        if (cachedDensity >= 0.0)
        {
            return string.Format(" | 地面密度: %1", 
                Math.Round(cachedDensity * 100.0) / 100.0);
        }
        
        return " | 地面密度: 未检测";
    }
    
    // 构建调试消息
    // @param movementTypeStr 移动类型字符串
    // @param staminaPercent 体力百分比
    // @param baseSpeedMultiplier 基础速度倍数
    // @param encumbranceSpeedPenalty 负重速度惩罚
    // @param finalSpeedMultiplier 最终速度倍数
    // @param gradeDisplay 坡度百分比
    // @param slopeInfo 坡度信息字符串
    // @param sprintInfo Sprint信息字符串
    // @param encumbranceInfo 负重信息字符串
    // @param terrainInfo 地形信息字符串
    // @return 调试消息字符串
    static string BuildDebugMessage(
        string movementTypeStr,
        float staminaPercent,
        float baseSpeedMultiplier,
        float encumbranceSpeedPenalty,
        float finalSpeedMultiplier,
        float gradeDisplay,
        string slopeInfo,
        string sprintInfo,
        string encumbranceInfo,
        string terrainInfo)
    {
        string debugMessage = string.Format(
            "[RealisticSystem] 调试: 类型=%1 | 体力=%2%% | 基础速度倍数=%3 | 负重惩罚=%4 | 最终速度倍数=%5 | 坡度=%6%%%7%8%9", 
            movementTypeStr,
            Math.Round(staminaPercent * 100.0).ToString(),
            baseSpeedMultiplier.ToString(),
            encumbranceSpeedPenalty.ToString(),
            finalSpeedMultiplier.ToString(),
            Math.Round(gradeDisplay * 10.0) / 10.0,
            slopeInfo,
            sprintInfo,
            encumbranceInfo);
        
        return debugMessage + terrainInfo;
    }
    
    // ==================== 环境因子信息格式化 ====================
    
    // 格式化环境因子信息字符串
    // @param environmentFactor 环境因子模块引用
    // @param heatStressMultiplier 热应激倍数
    // @param rainWeight 降雨湿重（kg）
    // @param swimmingWetWeight 游泳湿重（kg）
    // @return 环境因子信息字符串
    static string FormatEnvironmentInfo(
        EnvironmentFactor environmentFactor,
        float heatStressMultiplier,
        float rainWeight,
        float swimmingWetWeight)
    {
        if (!environmentFactor)
            return " | 环境因子: 未初始化";
        
        // 获取当前时间
        float currentHour = environmentFactor.GetCurrentHour();
        string timeStr = "";
        if (currentHour >= 0.0)
        {
            int hourInt = Math.Floor(currentHour);
            int minuteInt = Math.Floor((currentHour - hourInt) * 60.0);
            string minuteStr = minuteInt.ToString();
            if (minuteStr.Length() == 1)
                minuteStr = "0" + minuteStr;
            timeStr = string.Format("%1:%2", hourInt.ToString(), minuteStr);
        }
        else
        {
            timeStr = "未知";
        }
        
        // 获取降雨信息
        bool isRainingDebug = environmentFactor.IsRaining();
        float rainIntensity = environmentFactor.GetRainIntensity();
        string rainStr = "";
        if (isRainingDebug && rainWeight > 0.0)
        {
            string rainLevel = "";
            if (rainIntensity >= 0.8)
                rainLevel = "暴雨";
            else if (rainIntensity >= 0.5)
                rainLevel = "中雨";
            else
                rainLevel = "小雨";
            rainStr = string.Format("降雨: %1 (%2kg, 强度%3%%)", 
                rainLevel,
                Math.Round(rainWeight * 10.0) / 10.0,
                Math.Round(rainIntensity * 100.0));
        }
        else if (rainWeight > 0.0)
        {
            // 停止降雨但仍有湿重（衰减中）
            rainStr = string.Format("降雨: 已停止 (残留%1kg)", 
                Math.Round(rainWeight * 10.0) / 10.0);
        }
        else
        {
            rainStr = "降雨: 无";
        }
        
        // 获取室内状态
        bool isIndoorDebug = environmentFactor.IsIndoor();
        string indoorStr = isIndoorDebug ? "室内" : "室外";
        
        // 获取游泳湿重
        string swimWetStr = "";
        if (swimmingWetWeight > 0.0)
        {
            swimWetStr = string.Format("游泳湿重: %1kg", 
                Math.Round(swimmingWetWeight * 10.0) / 10.0);
        }
        else
        {
            swimWetStr = "游泳湿重: 0kg";
        }
        
        // 构建环境信息字符串
        return string.Format(" | 时间: %1 | 热应激: %2x | %3 | %4 | %5", 
            timeStr,
            Math.Round(heatStressMultiplier * 100.0) / 100.0,
            rainStr,
            indoorStr,
            swimWetStr);
    }
    
    // ==================== 统一调试信息输出 ====================
    
    // 输出完整调试信息（每5秒一次）
    // @param owner 角色实体
    // @param movementTypeStr 移动类型字符串
    // @param staminaPercent 体力百分比
    // @param baseSpeedMultiplier 基础速度倍数
    // @param encumbranceSpeedPenalty 负重速度惩罚
    // @param finalSpeedMultiplier 最终速度倍数
    // @param gradePercent 坡度百分比
    // @param slopeAngleDegrees 坡度角度（度）
    // @param isSprinting 是否正在Sprint
    // @param currentMovementPhase 当前移动阶段
    // @param debugCurrentWeight 当前重量（kg）
    // @param combatEncumbrancePercent 战斗负重百分比
    // @param terrainDetector 地形检测模块
    // @param environmentFactor 环境因子模块
    // @param heatStressMultiplier 热应激倍数
    // @param rainWeight 降雨湿重（kg）
    // @param swimmingWetWeight 游泳湿重（kg）
    static void OutputDebugInfo(
        IEntity owner,
        string movementTypeStr,
        float staminaPercent,
        float baseSpeedMultiplier,
        float encumbranceSpeedPenalty,
        float finalSpeedMultiplier,
        float gradePercent,
        float slopeAngleDegrees,
        bool isSprinting,
        int currentMovementPhase,
        float debugCurrentWeight,
        float combatEncumbrancePercent,
        TerrainDetector terrainDetector,
        EnvironmentFactor environmentFactor,
        float heatStressMultiplier,
        float rainWeight,
        float swimmingWetWeight)
    {
        // 只对本地控制的玩家输出
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 格式化各个信息字符串
        string slopeInfo = FormatSlopeInfo(slopeAngleDegrees);
        string sprintInfo = FormatSprintInfo(isSprinting, currentMovementPhase);
        string encumbranceInfo = FormatEncumbranceInfo(debugCurrentWeight, combatEncumbrancePercent);
        
        // 获取地形信息
        float currentTimeForDebug = GetGame().GetWorld().GetWorldTime();
        string terrainInfo = FormatTerrainInfo(terrainDetector, owner, currentTimeForDebug);
        
        // 获取环境因子信息
        string envInfo = FormatEnvironmentInfo(environmentFactor, heatStressMultiplier, rainWeight, swimmingWetWeight);
        
        // 构建并输出调试消息
        string debugMessage = BuildDebugMessage(
            movementTypeStr,
            staminaPercent,
            baseSpeedMultiplier,
            encumbranceSpeedPenalty,
            finalSpeedMultiplier,
            gradePercent,
            slopeInfo,
            sprintInfo,
            encumbranceInfo,
            terrainInfo);
        
        Print(debugMessage + envInfo);
    }
}
