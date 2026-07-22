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
    SCR_RSS_TerrainDetector terrainDetector;
    SCR_RSS_EnvironmentFactor environmentFactor;
    float heatStressMultiplier;
    float rainWeight;
    float swimmingWetWeight;
    float currentSpeed;  // 当前实际速度（m/s）
    bool isSwimming;     // 是否在游泳
    SCR_RSS_StanceTransitionManager stanceTransitionManager; // 姿态转换管理器（新增）
    float timeToDepleteSec;  // 按当前净消耗，体力耗尽所需秒数（-1 表示净恢复中）
    float timeToFullSec;     // 按当前净恢复，体力回满所需秒数（-1 表示净消耗中）
    string speedSource;      // 速度计算来源：Server=服务器权威 Client=客户端本地 Local=AI/非网络
    float anaerobicPercent;       // v5 无氧池
    float sprintCooldownSec;      // v5 冲刺冷却剩余
    float burstCooldownFullSec;   // v5 满冷却时长
    float maxStaminaCap;          // 疲劳积分体力上限（1−疲劳惩罚）
    float fatigueIntegralNorm;    // v6 积分疲劳 I 归一化
    float metabolismPowerW;       // STA 计费功率 P_bill = min(P_met, CP)
    float metabolismPowerMetW;  // 代谢模型全功率 P_met（测速后）
    float metabolismPowerRawW;    // 未限速瞬时功率 P_raw (W)，仅诊断对照
    float effectiveCpW;           // 有效临界功率 CP (W)
    float aerobicPowerW;          // 与 P_bill 相同，保留兼容
    float totalDrainPerTick;      // 粗消耗 /tick（姿态/环境前）
    float finalDrainPerTick;      // 实际扣减 /tick（含 EPOC、静止代谢分流）
    float metabolicNetPerTick;    // 代谢净值 /tick（恢复−finalDrain）
    float capRatchetPerTick;      // 疲劳 cap 下压 /tick
    float netStaminaPerTick;      // 实际 STA 变化 /tick
}

class SCR_RSS_DebugDisplay
{
    // ==================== 静态变量 ====================
    protected static float m_fNextDebugLogTime = 0.0; // 下次调试日志时间
    protected static float m_fNextStatusLogTime = 0.0; // 下次状态日志时间
    protected static float m_fNextHintTime = 0.0; // 下次 Hint 显示时间
    protected static string m_sLastStaminaStatus = ""; // 上次体力状态，用于检测状态变化
    protected static float m_fLastTerrainForceUpdateTime = 0.0; // 上次地形 ForceUpdate 时间（调试节流）
    protected static const float TERRAIN_DEBUG_UPDATE_INTERVAL = 0.5; // 地形 ForceUpdate 节流间隔（秒）
    protected static float s_fDrainDiagLastStamina = -1.0; // 上次 [RSS][Drain] 批次体力（用于观测速率）
    protected static float s_fDrainDiagLastWorldTime = -1.0;

    // ==================== 公共方法 ====================
    
    // 格式化移动类型字符串
    // @param isSprinting 是否正在Sprint
    // @param currentMovementPhase 当前移动阶段
    // @return 移动类型字符串
    static string FormatMovementType(bool isSprinting, int currentMovementPhase)
    {
        return SCR_RSS_SpeedCalculator.FormatMovementPhaseLabel(isSprinting, currentMovementPhase);
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
        
        return " | Sprint（Pandolf） | Sprint (Pandolf)";
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
    static string FormatTerrainInfo(SCR_RSS_TerrainDetector terrainDetector, IEntity owner, float currentTime)
    {
        if (!terrainDetector)
            return " | 地面密度: 未检测";
        
        if (currentTime - m_fLastTerrainForceUpdateTime >= TERRAIN_DEBUG_UPDATE_INTERVAL)
        {
            terrainDetector.ForceUpdate(owner, currentTime);
            m_fLastTerrainForceUpdateTime = currentTime;
        }
        
        float cachedDensity = terrainDetector.GetCachedTerrainDensity();
        string matLabel = terrainDetector.GetCachedGroundMaterialLabel();
        if (cachedDensity >= 0.0)
        {
            if (matLabel != "")
            {
                return string.Format(" | 地面: %1 | ρ≈%2", 
                    matLabel,
                    Math.Round(cachedDensity * 100.0) / 100.0);
            }
            return string.Format(" | 地面密度: %1", 
                Math.Round(cachedDensity * 100.0) / 100.0);
        }
        if (matLabel != "")
            return string.Format(" | 地面: %1", matLabel);
        
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
    // @param stanceTransitionInfo 姿态转换信息字符串
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
        string terrainInfo,
        string stanceTransitionInfo)
    {
        string debugMessage = string.Format(
            "[RSS] 类型=%1 体力=%2%% 速度倍=%3 负重惩罚=%4 最终倍=%5 坡度=%6%%%7", 
            movementTypeStr,
            Math.Round(staminaPercent * 100.0).ToString(),
            baseSpeedMultiplier.ToString(),
            encumbranceSpeedPenalty.ToString(),
            finalSpeedMultiplier.ToString(),
            Math.Round(gradeDisplay * 10.0) / 10.0,
            slopeInfo);
        
        debugMessage += sprintInfo;
        debugMessage += encumbranceInfo;
        debugMessage += terrainInfo;
        debugMessage += stanceTransitionInfo;
        
        return debugMessage;
    }

    //! 代谢/疲劳诊断（排查 70% 平台、Sprint 过快）
    static string FormatMetabolismDiagnosticInfo(DebugInfoParams params)
    {
        if (params.metabolismPowerW <= 1.0)
            return "";

        int capPct = Math.Round(params.maxStaminaCap * 100.0);
        int anaPct = Math.Round(params.anaerobicPercent * 100.0);
        int pBillW = Math.Round(params.metabolismPowerW);
        int cpW = Math.Round(params.effectiveCpW);
        float drainTick = params.finalDrainPerTick;
        if (drainTick <= 0.0 && params.totalDrainPerTick > 0.0)
            drainTick = params.totalDrainPerTick;
        float metaNetTick = params.metabolicNetPerTick;
        float capRatchetTick = params.capRatchetPerTick;
        float netTick = params.netStaminaPerTick;
        int fatPct = Math.Round(params.fatigueIntegralNorm * 100.0);

        string line = string.Format(
            " | 代谢:P_bill=%1W CP=%2W finalDrain=%3 metaNet=%4 capΔ=%5 net=%6/t cap=%7%% W'=%8%% If=%9%%",
            pBillW.ToString(),
            cpW.ToString(),
            Math.Round(drainTick * 100000.0) / 100000.0,
            Math.Round(metaNetTick * 100000.0) / 100000.0,
            Math.Round(capRatchetTick * 100000.0) / 100000.0,
            Math.Round(netTick * 100000.0) / 100000.0,
            capPct.ToString(),
            anaPct.ToString(),
            fatPct.ToString());

        if (params.metabolismPowerMetW > params.metabolismPowerW + 50.0)
        {
            int pMetW = Math.Round(params.metabolismPowerMetW);
            line = line + string.Format(" P_met=%1W", pMetW.ToString());
        }
        if (params.metabolismPowerRawW > params.metabolismPowerMetW + 50.0)
        {
            int rawW = Math.Round(params.metabolismPowerRawW);
            line = line + string.Format(" P_raw=%1W", rawW.ToString());
        }
        return line;
    }
    
    // ==================== 环境因子信息格式化 ====================
    
    // 格式化环境因子信息字符串
    // @param environmentFactor 环境因子模块引用
    // @param heatStressMultiplier 热应激倍数
    // @param rainWeight 降雨湿重（kg）
    // @param swimmingWetWeight 游泳湿重（kg）
    // @return 环境因子信息字符串
    static string FormatEnvironmentInfo(
        SCR_RSS_EnvironmentFactor environmentFactor,
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
    
    // 格式化姿态转换信息字符串（v2.0 乳酸堆积模型）
    // @param stanceTransitionManager 姿态转换管理器
    // @return 姿态转换信息字符串
    static string FormatStanceTransitionInfo(SCR_RSS_StanceTransitionManager stanceTransitionManager)
    {
        if (!stanceTransitionManager)
            return "";
        
        // 获取疲劳堆积值
        float stanceFatigue = stanceTransitionManager.GetStanceFatigue();
        
        // 如果疲劳堆积为0，返回空字符串
        if (stanceFatigue <= 0.0)
            return "";
        
        // 构建疲劳堆积信息
        return string.Format(" | 姿态疲劳: %1", 
            Math.Round(stanceFatigue * 100.0) / 100.0);
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
        
        if (!settings || !settings.m_bDebugLogEnabled)
            return;
        
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
        
        // 获取姿态转换信息
        string stanceTransitionInfo = FormatStanceTransitionInfo(params.stanceTransitionManager);
        
        // 构建并加入统一批次（由 SCR_RSS_Constants 管理定时）
        // 统一 [RSS] 前缀，单行整合
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
            terrainInfo,
            stanceTransitionInfo);
        
        string speedSourceStr = "";
        if (params.speedSource && params.speedSource != "")
        {
            speedSourceStr = string.Format(" | 速度来源:%1 | Speed Source:%2", params.speedSource, params.speedSource);
        }
        string metabInfo = FormatMetabolismDiagnosticInfo(params);
        SCR_RSS_DebugBatchManager.AddDebugBatchLine(debugMessage + speedSourceStr + metabInfo + envInfo);
    }
    
    // 输出状态信息（每秒一次）
    // @param owner 角色实体
    // @param snapshotSpeed 与体力 tick 同步的水平速度（m/s）
    // @param snapshotStaminaPercent 与体力 tick 同步的体力
    // @param snapshotSpeedMultiplier 与体力 tick 同步的最终速度倍率
    // @param engineMovementPhase 引擎相位（输入意图）
    // @param effectiveMovementPhase 有效相位（含惯性 coasting）
    static void OutputStatusInfo(
        IEntity owner,
        float snapshotSpeed,
        float snapshotStaminaPercent,
        float snapshotSpeedMultiplier,
        bool isSwimming,
        bool isSprinting,
        int engineMovementPhase,
        int effectiveMovementPhase,
        SCR_CharacterControllerComponent controller)
    {
        // ==================== 配置门禁检查 ====================
        // 获取配置实例
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        
        // 如果开关没开，直接退出，性能消耗极低
        if (!settings || !settings.m_bDebugLogEnabled)
            return;
        
        if (owner != SCR_PlayerController.GetLocalControlledEntity())
            return;
        // 若本秒内已有批次输出，跳过避免重复
        if (SCR_RSS_DebugBatchManager.WasBatchJustFlushed())
            return;
        float currentTime = GetGame().GetWorld().GetWorldTime() / 1000.0;
        if (currentTime < m_fNextStatusLogTime)
            return;
        
        string movementTypeStr;
        if (isSwimming)
            movementTypeStr = "Swim";
        else
            movementTypeStr = SCR_RSS_SpeedCalculator.FormatMovementTypeForDisplay(
                isSprinting, engineMovementPhase, effectiveMovementPhase, snapshotSpeed);
        
        string statusMessage = string.Format(
            "[RSS] 状态: 速度%1m/s 体力%2%% 倍率%3x 类型%4",
            Math.Round(snapshotSpeed * 100.0) / 100.0,
            Math.Round(snapshotStaminaPercent * 100.0),
            Math.Round(snapshotSpeedMultiplier * 100.0) / 100.0,
            movementTypeStr);

        if (controller)
        {
            string statusMetab = controller.RSS_FormatStatusMetabolismDiagnostic();
            if (statusMetab != "")
                statusMessage = statusMessage + statusMetab;
        }
        
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

        // AI 无 HUD：仅玩家角色更新屏幕提示
        if (params.owner)
        {
            SCR_CharacterControllerComponent ownerCtrl = SCR_CharacterControllerComponent.Cast(params.owner.FindComponent(SCR_CharacterControllerComponent));
            if (ownerCtrl && !ownerCtrl.IsPlayerControlled())
                return;
        }
        
        // 只对本地控制的玩家输出（含载具内：角色在本地控制的载具中）
        IEntity controlled = SCR_PlayerController.GetLocalControlledEntity();
        if (params.owner != controlled)
        {
            bool isOwnerInControlledVehicle = false;
            if (controlled)
            {
                ChimeraCharacter character = ChimeraCharacter.Cast(params.owner);
                if (character)
                {
                    CompartmentAccessComponent compAccess = character.GetCompartmentAccessComponent();
                    if (compAccess)
                    {
                        BaseCompartmentSlot slot = compAccess.GetCompartment();
                        if (slot)
                        {
                            IEntity vehicle = slot.GetVehicle();
                            if (vehicle == controlled)
                                isOwnerInControlledVehicle = true;
                        }
                    }
                }
            }
            if (!isOwnerInControlledVehicle)
                return;
        }
        
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
        string groundMaterialLabel = "";
        if (params.terrainDetector)
        {
            terrainDensity = params.terrainDetector.GetCachedTerrainDensity();
            groundMaterialLabel = params.terrainDetector.GetCachedGroundMaterialLabel();
        }
        
        // 更新 HUD 的所有值（会自动在右上角显示）
        SCR_RSS_StaminaHUDComponent.UpdateAllValues(
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
            groundMaterialLabel,
            totalWetWeight,
            params.isSwimming);
        SCR_RSS_StaminaHUDComponent.UpdateTimeEtaHud(params.timeToDepleteSec, params.timeToFullSec);
        SCR_RSS_StaminaHUDComponent.UpdateWPrimeHud(
            params.anaerobicPercent,
            params.sprintCooldownSec,
            params.burstCooldownFullSec);
    }

    //! 体力消耗 / ETA 诊断行（Debug 或 HUD Hint 开启时，由批次管理器每秒输出）
    static void OutputStaminaDrainDiagnostics(
        RSS_StaminaDebugOutputParams tick,
        SCR_CharacterControllerComponent controller,
        SCR_RSS_EpocState epocState,
        SCR_RSS_EncumbranceCache encumbranceCache,
        SCR_RSS_ExerciseTracker exerciseTracker,
        SCR_RSS_EnvironmentFactor environmentFactor)
    {
        if (!tick || !controller)
            return;
        // 仅在本秒批次窗口内写入；避免每 17ms tick 直接 Print 刷屏
        if (!SCR_RSS_DebugBatchManager.IsDebugBatchActive())
            return;

        float recoveryPerTick = SCR_RSS_UpdateCoordinator.ComputeRecoveryRatePerTick(
            tick.staminaPercent,
            tick.currentSpeed,
            tick.baseDrainRateByVelocity,
            tick.baseDrainRateByVelocityForModule,
            tick.heatStressMultiplier,
            epocState,
            encumbranceCache,
            exerciseTracker,
            controller,
            environmentFactor,
            false);

        float finalDrainPerTick = SCR_RSS_UpdateCoordinator.ComputeFinalDrainRatePerTick(
            tick.useSwimmingModel,
            tick.currentSpeed,
            tick.totalDrainRate,
            epocState,
            false);

        float netPerTick = recoveryPerTick - finalDrainPerTick;
        float tickSec = SCR_RSS_Constants.RSS_STAMINA_TICK_SEC;
        float perSecMult = 1.0 / tickSec;
        float recoveryPerSec = recoveryPerTick * perSecMult;
        float drainPerSec = finalDrainPerTick * perSecMult;
        float netPerSec = SCR_RSS_UpdateCoordinator.GetNetStaminaRatePerSecond(
            tick.staminaPercent,
            tick.useSwimmingModel,
            tick.currentSpeed,
            tick.totalDrainRate,
            tick.baseDrainRateByVelocity,
            tick.baseDrainRateByVelocityForModule,
            tick.heatStressMultiplier,
            epocState,
            encumbranceCache,
            exerciseTracker,
            controller,
            environmentFactor,
            false);

        float tickScale = Math.Clamp(tick.timeDeltaSec / tickSec, 0.01, 2.0);
        float observedPctPerSec = 0.0;
        float worldTime = 0.0;
        if (GetGame())
        {
            World world = GetGame().GetWorld();
            if (world)
                worldTime = world.GetWorldTime() / 1000.0;
        }
        if (s_fDrainDiagLastStamina >= 0.0 && s_fDrainDiagLastWorldTime >= 0.0)
        {
            float obsDt = worldTime - s_fDrainDiagLastWorldTime;
            if (obsDt > 0.05)
            {
                observedPctPerSec = (tick.staminaPercent - s_fDrainDiagLastStamina) / obsDt * 100.0;
            }
        }
        s_fDrainDiagLastStamina = tick.staminaPercent;
        s_fDrainDiagLastWorldTime = worldTime;

        float linearDepleteSec = -1.0;
        if (drainPerSec > recoveryPerSec)
        {
            float netLoss = drainPerSec - recoveryPerSec;
            float effectiveLoss = netLoss;
            if (tick.capShrinkPerSec > 0.0)
            {
                float capGap = tick.staminaPercent - tick.targetStaminaCap;
                if (Math.AbsFloat(capGap) < 0.02 || tick.staminaPercent > tick.targetStaminaCap)
                    effectiveLoss = netLoss + tick.capShrinkPerSec;
            }
            if (effectiveLoss > 0.0)
                linearDepleteSec = tick.staminaPercent / effectiveLoss;
        }

        float linearFullSec = -1.0;
        if (recoveryPerSec > drainPerSec && tick.staminaPercent < tick.targetStaminaCap - 0.001)
        {
            float netGain = recoveryPerSec - drainPerSec;
            if (netGain > 0.000001)
            {
                float remaining = tick.targetStaminaCap - tick.staminaPercent;
                linearFullSec = remaining / netGain;
            }
        }

        bool walkRecoveryZone = false;
        float walkRecoveryThreshold = SCR_RSS_Constants.GetWalkRecoveryZoneThreshold();
        if (!tick.useSwimmingModel && !tick.isSprintActive && tick.staminaPercent < walkRecoveryThreshold)
        {
            if (tick.currentSpeed >= SCR_RSS_Constants.RSS_IDLE_SPEED_THRESHOLD_MPS)
                walkRecoveryZone = true;
        }

        string epocStr = "off";
        if (tick.epocActive)
            epocStr = "on";

        string walkRecStr = "off";
        if (walkRecoveryZone)
            walkRecStr = "on";

        string hudDepleteStr = "—";
        if (tick.timeToDepleteSec >= 0.0)
            hudDepleteStr = Math.Round(tick.timeToDepleteSec).ToString() + "s";

        string hudFullStr = "—";
        if (tick.timeToFullSec >= 0.0)
            hudFullStr = Math.Round(tick.timeToFullSec).ToString() + "s";

        string linearDepleteStr = "—";
        if (linearDepleteSec >= 0.0)
            linearDepleteStr = Math.Round(linearDepleteSec).ToString() + "s";

        string linearFullStr = "—";
        if (linearFullSec >= 0.0)
            linearFullStr = Math.Round(linearFullSec).ToString() + "s";

        float netPctPerSec = netPerSec * 100.0;
        float capRatePctPerSec = tick.capShrinkPerSec * 100.0;
        float effectiveLossPctPerSec = 0.0;
        if (drainPerSec > recoveryPerSec)
        {
            float netLoss = drainPerSec - recoveryPerSec;
            effectiveLossPctPerSec = netLoss * 100.0;
            if (tick.capShrinkPerSec > 0.0)
            {
                float capGap = tick.staminaPercent - tick.targetStaminaCap;
                if (Math.AbsFloat(capGap) < 0.02 || tick.staminaPercent > tick.targetStaminaCap)
                    effectiveLossPctPerSec = (netLoss + tick.capShrinkPerSec) * 100.0;
            }
        }

        string line1Head = string.Format(
            "[RSS][Drain] tick: 恢复=%1 消耗=%2 净=%3 | /s: 恢复=%4 消耗=%5 净=%6%%/s",
            Math.Round(recoveryPerTick * 1000000.0) / 1000000.0,
            Math.Round(finalDrainPerTick * 1000000.0) / 1000000.0,
            Math.Round(netPerTick * 1000000.0) / 1000000.0,
            Math.Round(recoveryPerSec * 1000000.0) / 1000000.0,
            Math.Round(drainPerSec * 1000000.0) / 1000000.0,
            Math.Round(netPctPerSec * 1000.0) / 1000.0);
        string line1Tail = string.Format(
            " capRate=%1%%/s 有效=%2%%/s | 观测=%3%%/s dt=%4s scale=%5",
            Math.Round(capRatePctPerSec * 1000.0) / 1000.0,
            Math.Round(effectiveLossPctPerSec * 1000.0) / 1000.0,
            Math.Round(observedPctPerSec * 1000.0) / 1000.0,
            Math.Round(tick.timeDeltaSec * 1000.0) / 1000.0,
            Math.Round(tickScale * 1000.0) / 1000.0);
        string line1 = line1Head + line1Tail;
        AppendDrainDebugLine(line1);

        string line2 = string.Format(
            "[RSS][ETA] HUD耗尽=%1 线性耗尽=%2 HUD回满=%3 线性回满=%4 | base=%5 total=%6",
            hudDepleteStr,
            linearDepleteStr,
            hudFullStr,
            linearFullStr,
            Math.Round(tick.baseDrainRateByVelocity * 1000000.0) / 1000000.0,
            Math.Round(tick.totalDrainRate * 1000000.0) / 1000000.0);
        AppendDrainDebugLine(line2);

        string cpStr = "—";
        if (tick.effectiveCriticalPowerWatts > 1.0)
            cpStr = Math.Round(tick.effectiveCriticalPowerWatts).ToString() + "W";

        string limitStr = "—";
        if (tick.appliedSpeedLimitMs > 0.05)
        {
            float limitRounded = Math.Round(tick.appliedSpeedLimitMs * 100.0) / 100.0;
            limitStr = limitRounded.ToString() + "m/s";
        }

        float drainVelMs = SCR_RSS_DrainCalculator.GetDrainVelocityMs(
            tick.currentSpeed, tick.appliedSpeedLimitMs);
        float acctVelMs = SCR_RSS_DrainCalculator.GetMetabolicAccountingVelocityMs(
            tick.currentSpeed, tick.appliedSpeedLimitMs, tick.wPrimePool01);

        string overspeedStr = "off";
        if (SCR_RSS_DrainCalculator.IsMetabolicOverspeedAccounting(
            tick.currentSpeed, tick.appliedSpeedLimitMs))
        {
            if (SCR_RSS_DrainCalculator.IsWPrimePoolAvailableForOverspeed(tick.wPrimePool01))
                overspeedStr = "on";
        }

        string line3Head = string.Format(
            "[RSS][P] 代谢=%1W CP=%2 v_drain=%3m/s v_acct=%4m/s v_meas=%5m/s v_pos=%6m/s v_limit=%7",
            Math.Round(tick.powerWatts),
            cpStr,
            Math.Round(drainVelMs * 100.0) / 100.0,
            Math.Round(acctVelMs * 100.0) / 100.0,
            Math.Round(tick.currentSpeed * 100.0) / 100.0,
            Math.Round(tick.landPositionDeltaSpeedMs * 100.0) / 100.0,
            limitStr);
        float fatiguePowerDbg = SCR_RSS_DrainCalculator.GetMetabolicFatiguePowerWatts(
            tick.currentSpeed,
            tick.appliedSpeedLimitMs,
            tick.totalWeightWithWetAndBody,
            tick.gradePercent,
            tick.terrainFactor,
            tick.effectiveMovementPhase);
        string line3Tail = string.Format(
            " | env=%1 步行恢复=%2 EPOC=%3 上限=%4%% P_fat=%5W 超速记账=%6 超速罚=%7%/s",
            Math.Round(tick.environmentMult * 1000.0) / 1000.0,
            walkRecStr,
            epocStr,
            Math.Round(tick.targetStaminaCap * 100.0),
            Math.Round(fatiguePowerDbg),
            overspeedStr,
            Math.Round(tick.overspeedExtraDrainPerSec * 10000.0) / 100.0);
        string line3 = line3Head + line3Tail;
        AppendDrainDebugLine(line3);
    }

    protected static void AppendDrainDebugLine(string line)
    {
        SCR_RSS_DebugBatchManager.AddDebugBatchLine(line);
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
        SCR_RSS_StaminaHUDComponent.UpdateStaminaValue(staminaPercent);
    }
}
