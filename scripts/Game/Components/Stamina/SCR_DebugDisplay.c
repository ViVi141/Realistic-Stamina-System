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
}
