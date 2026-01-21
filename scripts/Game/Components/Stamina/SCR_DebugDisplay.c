// 调试信息显示模块
// 负责格式化并显示调试信息
// 模块化拆分：从 PlayerBase.c 提取的调试显示逻辑

// 调试信息参数结构体
class DebugInfoParams
{
    IEntity owner;
    string movementTypeStr;
    float staminaPercent;
    float baseSpeedMultiplier;
    float encumbranceSpeedPenalty;
    float finalSpeedMultiplier;
    float gradePercent;
    float slopeAngleDegrees;
    bool isSprinting;
    int currentMovementPhase;
    float debugCurrentWeight;
    float combatEncumbrancePercent;
    TerrainDetector terrainDetector;
    EnvironmentFactor environmentFactor;
    float heatStressMultiplier;
    float rainWeight;
    float swimmingWetWeight;
    float currentSpeed;  // 当前实际速度（m/s）
    bool isSwimming;     // 是否在游泳
}

class DebugDisplay
{
    // ==================== 静态变量 ====================
    protected static float m_fNextDebugLogTime = 0.0; // 下次调试日志时间
    protected static float m_fNextStatusLogTime = 0.0; // 下次状态日志时间
    protected static float m_fNextHintTime = 0.0; // 下次 Hint 显示时间
    protected static string m_sLastStaminaStatus = ""; // 上次体力状态，用于检测状态变化
    
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
    
    // 格式化坡度信息字符串（中英双语）
    // @param slopeAngleDegrees 坡度角度（度）
    // @return 坡度信息字符串
    static string FormatSlopeInfo(float slopeAngleDegrees)
    {
        if (Math.AbsFloat(slopeAngleDegrees) <= 0.1)
            return "";
        
        string slopeDirection = "";
        string slopeDirectionEn = "";
        if (slopeAngleDegrees > 0)
        {
            slopeDirection = "上坡";
            slopeDirectionEn = "Uphill";
        }
        else
        {
            slopeDirection = "下坡";
            slopeDirectionEn = "Downhill";
        }
        
        return string.Format(" | 坡度: %1° (%2) | Grade: %1° (%3)", 
            Math.Round(Math.AbsFloat(slopeAngleDegrees) * 10.0) / 10.0,
            slopeDirection,
            slopeDirectionEn);
    }
    
    // 格式化Sprint信息字符串（中英双语）
    // @param isSprinting 是否正在Sprint
    // @param currentMovementPhase 当前移动阶段
    // @return Sprint信息字符串
    static string FormatSprintInfo(bool isSprinting, int currentMovementPhase)
    {
        if (!isSprinting && currentMovementPhase != 3)
            return "";
        
        float sprintMultiplier = StaminaConstants.GetSprintStaminaDrainMultiplier();
        return string.Format(" | Sprint消耗倍数: %1x | Sprint Drain Multiplier: %1x", 
            sprintMultiplier.ToString());
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
            "[RealisticSystem] 调试 / Debug: 类型=%1 | 体力=%2%% | 基础速度倍数=%3 | 负重惩罚=%4 | 最终速度倍数=%5 | 坡度=%6%% | Type=%1 | Stamina=%2%% | Base Speed=%3 | Encumbrance Penalty=%4 | Final Speed=%5 | Grade=%6%%%7%8%9", 
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
        
        // 获取室内状态
        bool isIndoorDebug = environmentFactor.IsIndoor();
        string indoorStr = "";
        string transitionStr = "";
        
        // 检测室内外状态变化
        static bool s_wasLastIndoor = false;
        if (isIndoorDebug && !s_wasLastIndoor)
            transitionStr = " [进入室内]";
        else if (!isIndoorDebug && s_wasLastIndoor)
            transitionStr = " [离开室内]";
        s_wasLastIndoor = isIndoorDebug;
        
        if (isIndoorDebug)
            indoorStr = "室内";
        else
            indoorStr = "室外";
        
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
            
            // 显示室内/室外状态对降雨的影响
            string rainStatus = "";
            if (isIndoorDebug)
                rainStatus = " (室内)";
            else
                rainStatus = " (室外)";
            
            rainStr = string.Format("降雨: %1 (%2kg, 强度%3%%%4)", 
                rainLevel,
                Math.Round(rainWeight * 10.0) / 10.0,
                Math.Round(rainIntensity * 100.0),
                rainStatus);
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
        
        // 将转换状态添加到室内状态字符串中
        indoorStr += transitionStr;
        
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
    static void OutputDebugInfo(DebugInfoParams params)
    {
        // ==================== 配置门禁检查 ====================
        // 获取配置实例
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        
        // 如果开关没开，直接退出，性能消耗极低
        if (!settings || !settings.m_bDebugLogEnabled)
            return;
        
        // 检查时间间隔
        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
        if (currentTime < m_fNextDebugLogTime)
            return;
        
        // 只对本地控制的玩家输出
        if (params.owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 格式化各个信息字符串
        string slopeInfo = FormatSlopeInfo(params.slopeAngleDegrees);
        string sprintInfo = FormatSprintInfo(params.isSprinting, params.currentMovementPhase);
        string encumbranceInfo = FormatEncumbranceInfo(params.debugCurrentWeight, params.combatEncumbrancePercent);
        
        // 获取地形信息
        float currentTimeForDebug = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
        string terrainInfo = FormatTerrainInfo(params.terrainDetector, params.owner, currentTimeForDebug);
        
        // 获取环境因子信息
        string envInfo = FormatEnvironmentInfo(params.environmentFactor, params.heatStressMultiplier, params.rainWeight, params.swimmingWetWeight);
        
        // 构建并输出调试消息
        string debugMessage = BuildDebugMessage(
            params.movementTypeStr,
            params.staminaPercent,
            params.baseSpeedMultiplier,
            params.encumbranceSpeedPenalty,
            params.finalSpeedMultiplier,
            params.gradePercent,
            slopeInfo,
            sprintInfo,
            encumbranceInfo,
            terrainInfo);
        
        Print(debugMessage + envInfo);
        
        // 更新下次日志时间
        m_fNextDebugLogTime = currentTime + (settings.m_iDebugUpdateInterval / 1000.0);
    }
    
    // 输出状态信息（每秒一次）
    // @param owner 角色实体
    // @param lastSecondSpeed 上一秒的速度（m/s）
    // @param lastStaminaPercent 上一秒的体力百分比
    // @param lastSpeedMultiplier 上一秒的速度倍数
    // @param isSwimming 是否在游泳
    // @param isSprinting 是否正在Sprint
    // @param currentMovementPhase 当前移动阶段
    // @param controller 角色控制器组件
    static void OutputStatusInfo(
        IEntity owner,
        float lastSecondSpeed,
        float lastStaminaPercent,
        float lastSpeedMultiplier,
        bool isSwimming,
        bool isSprinting,
        int currentMovementPhase,
        SCR_CharacterControllerComponent controller)
    {
        // ==================== 配置门禁检查 ====================
        // 获取配置实例
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        
        // 如果开关没开，直接退出，性能消耗极低
        if (!settings || !settings.m_bDebugLogEnabled)
            return;
        
        // 检查时间间隔
        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0; // 转换为秒
        if (currentTime < m_fNextStatusLogTime)
            return;
        
        // 只对本地控制的玩家输出
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 格式化移动类型
        string movementTypeStr = FormatMovementType(isSprinting, currentMovementPhase);
        if (isSwimming)
            movementTypeStr = "Swim";
        
        // 构建状态消息（中英双语）
        string statusMessage = string.Format(
            "[状态 / Status] 速度: %1 m/s | 体力: %2%% | 速度倍数: %3x | 类型: %4 | Speed: %1 m/s | Stamina: %2%% | Speed Multiplier: %3x | Type: %4",
            Math.Round(lastSecondSpeed * 10.0) / 10.0,
            Math.Round(lastStaminaPercent * 100.0),
            Math.Round(lastSpeedMultiplier * 100.0) / 100.0,
            movementTypeStr);
        
        Print(statusMessage);
        
        // 更新下次日志时间（状态信息每秒输出一次）
        m_fNextStatusLogTime = currentTime + 1.0;
    }
    
    // ==================== Hint 显示系统 ====================
    
    // 格式化体力状态等级
    // @param staminaPercent 体力百分比（0.0-1.0）
    // @return 体力状态等级字符串
    static string GetStaminaStatusLevel(float staminaPercent)
    {
        if (staminaPercent >= 0.8)
            return "Excellent";
        else if (staminaPercent >= 0.6)
            return "Good";
        else if (staminaPercent >= 0.4)
            return "Normal";
        else if (staminaPercent >= 0.2)
            return "Tired";
        else
            return "Exhausted";
    }
    
    // 格式化负重状态
    // @param currentWeight 当前负重（kg）
    // @param combatEncumbrancePercent 战斗负重百分比
    // @return 负重状态字符串
    static string GetEncumbranceStatus(float currentWeight, float combatEncumbrancePercent)
    {
        if (currentWeight <= 0.0)
            return "";
        
        if (combatEncumbrancePercent > 1.0)
            return "Overloaded";
        else if (combatEncumbrancePercent >= 0.9)
            return "Heavy";
        else if (combatEncumbrancePercent >= 0.7)
            return "Medium";
        else
            return "Light";
    }
    
    // 构建简洁的 Hint 消息
    // @param movementTypeStr 移动类型字符串
    // @param staminaPercent 体力百分比
    // @param finalSpeedMultiplier 最终速度倍数
    // @param currentWeight 当前负重（kg）
    // @param combatEncumbrancePercent 战斗负重百分比
    // @return Hint 消息字符串
    static string BuildHintMessage(
        string movementTypeStr,
        float staminaPercent,
        float finalSpeedMultiplier,
        float currentWeight,
        float combatEncumbrancePercent)
    {
        string staminaStatus = GetStaminaStatusLevel(staminaPercent);
        string encumbranceStatus = GetEncumbranceStatus(currentWeight, combatEncumbrancePercent);
        
        // Build compact message - single line format
        string hintMsg = string.Format("[RSS] %1%% %2", 
            Math.Round(staminaPercent * 100.0).ToString(),
            staminaStatus);
        
        return hintMsg;
    }
    
    // 构建详细的 Hint 第二行消息
    // @param movementTypeStr 移动类型字符串
    // @param finalSpeedMultiplier 最终速度倍数
    // @param currentWeight 当前负重（kg）
    // @param combatEncumbrancePercent 战斗负重百分比
    // @return Hint 第二行消息字符串
    static string BuildHintMessage2(
        string movementTypeStr,
        float finalSpeedMultiplier,
        float currentWeight,
        float combatEncumbrancePercent)
    {
        string encumbranceStatus = GetEncumbranceStatus(currentWeight, combatEncumbrancePercent);
        
        // Build second line message - compact format
        string hintMsg2 = "";
        if (currentWeight > 0.0)
        {
            hintMsg2 = string.Format("Spd:%1x Load:%2kg", 
                Math.Round(finalSpeedMultiplier * 100.0) / 100.0,
                Math.Round(currentWeight * 10.0) / 10.0);
        }
        else
        {
            hintMsg2 = string.Format("Spd:%1x %2", 
                Math.Round(finalSpeedMultiplier * 100.0) / 100.0,
                movementTypeStr);
        }
        
        return hintMsg2;
    }
    
    // 输出屏幕 Hint 信息（更新 HUD 显示）
    // @param params 调试信息参数结构体
    static void OutputHintInfo(DebugInfoParams params)
    {
        // ==================== 配置门禁检查 ====================
        // 获取配置实例
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        
        // 如果 Hint 显示没开启，直接退出
        if (!settings || !settings.m_bHintDisplayEnabled)
            return;
        
        // 只对本地控制的玩家输出
        if (params.owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 计算总湿重（降雨 + 游泳）
        float totalWetWeight = params.rainWeight + params.swimmingWetWeight;
        
        // 获取环境数据（从环境因子模块）
        float temperature = 20.0;
        float windSpeed = 0.0;
        float windDirection = 0.0;
        bool isIndoor = false;
        
        if (params.environmentFactor)
        {
            temperature = params.environmentFactor.GetTemperature();
            windSpeed = params.environmentFactor.GetWindSpeed();
            windDirection = params.environmentFactor.GetWindDirection();
            isIndoor = params.environmentFactor.IsIndoor();
        }
        
        // 获取地形密度（从地形检测模块）
        float terrainDensity = -1.0;
        if (params.terrainDetector)
            terrainDensity = params.terrainDetector.GetCachedTerrainDensity();
        
        // 更新 HUD 的所有值（会自动在右上角显示）
        SCR_StaminaHUDComponent.UpdateAllValues(
            params.staminaPercent,
            params.finalSpeedMultiplier,
            params.currentSpeed,
            params.debugCurrentWeight,
            params.movementTypeStr,
            params.slopeAngleDegrees,
            temperature,
            windSpeed,
            windDirection,
            isIndoor,
            terrainDensity,
            totalWetWeight,
            params.isSwimming
        );
    }
    
    // 输出简洁的状态 Hint（更新 HUD 显示）
    // @param owner 角色实体
    // @param staminaPercent 体力百分比
    // @param speedMultiplier 速度倍数
    // @param movementTypeStr 移动类型字符串
    static void OutputQuickHint(
        IEntity owner,
        float staminaPercent,
        float speedMultiplier,
        string movementTypeStr)
    {
        // ==================== 配置门禁检查 ====================
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        
        if (!settings || !settings.m_bHintDisplayEnabled)
            return;
        
        // 只对本地控制的玩家输出
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        
        // 更新 HUD 的体力值
        SCR_StaminaHUDComponent.UpdateStaminaValue(staminaPercent);
    }
}
