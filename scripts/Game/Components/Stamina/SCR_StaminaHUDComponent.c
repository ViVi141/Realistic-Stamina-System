// Stamina HUD Component - 在屏幕右上角显示体力状态信息
// 使用简单的 Widget 系统直接创建和管理 HUD 元素
// 显示：体力、速度、负重、移动类型、坡度、温度、风、室内外、地面、湿重

class SCR_StaminaHUDComponent
{
    // 单例实例
    protected static ref SCR_StaminaHUDComponent s_Instance;
    
    // Widget 引用
    protected Widget m_wRoot;
    protected TextWidget m_wTextStamina;      // 体力
    protected TextWidget m_wTextSpeed;        // 速度倍数
    protected TextWidget m_wTextWeight;       // 负重
    protected TextWidget m_wTextMove;         // 移动类型
    protected TextWidget m_wTextSlope;        // 坡度
    protected TextWidget m_wTextHeat;         // 温度（虚拟气温）
    protected TextWidget m_wTextWind;         // 风速风向
    protected TextWidget m_wTextLocation;     // 室内/室外
    protected TextWidget m_wTextGround;       // 地面类型
    protected TextWidget m_wTextWet;          // 湿重
    
    // 缓存的数据
    protected static float s_fCachedStaminaPercent = 1.0;
    protected static float s_fCachedSpeedMultiplier = 1.0;
    protected static float s_fCachedCurrentSpeed = 0.0;  // 当前实际速度（m/s）
    protected static float s_fCachedWeight = 0.0;
    protected static string s_sCachedMoveType = "Idle";
    protected static float s_fCachedSlopeAngle = 0.0;
    protected static float s_fCachedTemperature = 20.0;  // 虚拟气温（°C）
    protected static float s_fCachedWindSpeed = 0.0;     // 风速（m/s）
    protected static float s_fCachedWindDirection = 0.0; // 风向（度）
    protected static bool s_bCachedIsIndoor = false;     // 是否室内
    protected static float s_fCachedTerrainDensity = -1.0; // 地面密度
    protected static float s_fCachedWetWeight = 0.0;
    protected static bool s_bCachedIsSwimming = false; // 是否在游泳
    
    // 上一次显示的值（用于减少不必要的更新）
    protected string m_sLastDisplayedText = "";
    
    // ==================== 公共静态方法 ====================
    
    // 更新所有数据（从外部调用）
    static void UpdateAllValues(
        float staminaPercent,
        float speedMultiplier,
        float currentSpeed,
        float weight,
        string moveType,
        float slopeAngle,
        float temperature,
        float windSpeed,
        float windDirection,
        bool isIndoor,
        float terrainDensity,
        float wetWeight,
        bool isSwimming)
    {
        s_fCachedStaminaPercent = staminaPercent;
        s_fCachedSpeedMultiplier = speedMultiplier;
        s_fCachedCurrentSpeed = currentSpeed;
        s_fCachedWeight = weight;
        s_sCachedMoveType = moveType;
        s_fCachedSlopeAngle = slopeAngle;
        s_fCachedTemperature = temperature;
        s_fCachedWindSpeed = windSpeed;
        s_fCachedWindDirection = windDirection;
        s_bCachedIsIndoor = isIndoor;
        s_fCachedTerrainDensity = terrainDensity;
        s_fCachedWetWeight = wetWeight;
        s_bCachedIsSwimming = isSwimming;
        
        // 如果实例存在，更新显示
        if (s_Instance)
            s_Instance.UpdateDisplay();
    }
    
    // 简化版：只更新体力值（向后兼容）
    static void UpdateStaminaValue(float staminaPercent)
    {
        s_fCachedStaminaPercent = staminaPercent;
        
        // 如果实例存在，更新显示
        if (s_Instance)
            s_Instance.UpdateDisplay();
    }
    
    // 获取当前缓存的体力值
    static float GetCachedStaminaPercent()
    {
        return s_fCachedStaminaPercent;
    }
    
    // 初始化 HUD（从 PlayerBase 调用）
    static void Init()
    {
        if (s_Instance)
            return;
        
        // 检查配置是否启用
        SCR_RSS_Settings settings = SCR_RSS_ConfigManager.GetSettings();
        if (!settings || !settings.m_bHintDisplayEnabled)
            return;
        
        s_Instance = new SCR_StaminaHUDComponent();
        s_Instance.CreateHUD();
    }
    
    // 销毁 HUD
    static void Destroy()
    {
        if (s_Instance)
        {
            s_Instance.DestroyHUD();
            s_Instance = null;
        }
    }
    
    // 检查是否已初始化
    static bool IsInitialized()
    {
        return s_Instance != null;
    }
    
    // ==================== 私有方法 ====================
    
    // 创建 HUD
    protected void CreateHUD()
    {
        WorkspaceWidget workspace = GetGame().GetWorkspace();
        if (!workspace)
        {
            Print("[RSS_StaminaHUD] Workspace not found");
            return;
        }
        
        // 直接在 workspace 上创建布局
        m_wRoot = workspace.CreateWidgets("{CD4F57077E64ECE5}UI/layouts/HUD/StatsPanel/StaminaHUD.layout");
        
        if (!m_wRoot)
        {
            // 如果布局加载失败，打印日志
            Print("[RSS_StaminaHUD] Layout not found or failed to load");
            return;
        }
        
        // 查找所有 Text widget
        m_wTextStamina = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Stamina"));
        m_wTextSpeed = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Speed"));
        m_wTextWeight = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Weight"));
        m_wTextMove = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Move"));
        m_wTextSlope = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Slope"));
        m_wTextHeat = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Heat"));
        m_wTextWind = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Wind"));
        m_wTextLocation = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Location"));
        m_wTextGround = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Ground"));
        m_wTextWet = TextWidget.Cast(m_wRoot.FindAnyWidget("Text-Wet"));
        
        // 向后兼容：如果没有找到新的 widget，使用旧的 "Text"
        if (!m_wTextStamina)
        {
            Widget staminaSlot = m_wRoot.FindAnyWidget("Slot-Stamina");
            if (staminaSlot)
                m_wTextStamina = TextWidget.Cast(staminaSlot.FindAnyWidget("Text"));
            
            if (!m_wTextStamina)
                m_wTextStamina = TextWidget.Cast(m_wRoot.FindAnyWidget("Text"));
        }
        
        int widgetCount = 0;
        if (m_wTextStamina) widgetCount++;
        if (m_wTextSpeed) widgetCount++;
        if (m_wTextWeight) widgetCount++;
        if (m_wTextMove) widgetCount++;
        if (m_wTextSlope) widgetCount++;
        if (m_wTextHeat) widgetCount++;
        if (m_wTextWind) widgetCount++;
        if (m_wTextLocation) widgetCount++;
        if (m_wTextGround) widgetCount++;
        if (m_wTextWet) widgetCount++;
        
        Print("[RSS_StaminaHUD] HUD created with " + widgetCount.ToString() + " text widgets");
    }
    
    
    // 销毁 HUD
    protected void DestroyHUD()
    {
        if (m_wRoot)
        {
            m_wRoot.RemoveFromHierarchy();
            m_wRoot = null;
        }
        m_wTextStamina = null;
        m_wTextSpeed = null;
        m_wTextWeight = null;
        m_wTextMove = null;
        m_wTextSlope = null;
        m_wTextHeat = null;
        m_wTextWind = null;
        m_wTextLocation = null;
        m_wTextGround = null;
        m_wTextWet = null;
        m_sLastDisplayedText = "";
    }
    
    // 更新显示
    protected void UpdateDisplay()
    {
        // 计算各项数值
        int staminaPct = Math.Clamp(Math.Round(s_fCachedStaminaPercent * 100.0), 0, 100);
        int speedPct = Math.Round(s_fCachedSpeedMultiplier * 100.0);
        int speedMs = Math.Round(s_fCachedCurrentSpeed * 10.0);  // 保留一位小数（乘10存储）
        int weightKg = Math.Round(s_fCachedWeight);
        int slopeAngle = Math.Round(s_fCachedSlopeAngle);
        int tempC = Math.Round(s_fCachedTemperature);  // 虚拟气温（°C）
        int windSpeedInt = Math.Round(s_fCachedWindSpeed);  // 风速（m/s）
        int wetKg = Math.Round(s_fCachedWetWeight * 10.0);  // 保留一位小数
        
        // 构建完整文本用于检测变化
        string indoorStr = "O";
        if (s_bCachedIsIndoor)
            indoorStr = "I";
        string fullText = staminaPct.ToString() + speedMs.ToString() + weightKg.ToString() + 
                          s_sCachedMoveType + slopeAngle.ToString() + tempC.ToString() + 
                          windSpeedInt.ToString() + indoorStr + 
                          s_fCachedTerrainDensity.ToString() + wetKg.ToString();
        
        // 如果没有变化，不更新
        if (fullText == m_sLastDisplayedText)
            return;
        
        m_sLastDisplayedText = fullText;
        
        // 根据体力值获取颜色
        Color staminaColor = GetStaminaColor(staminaPct);
        Color speedColor = GetSpeedColor(speedPct);
        Color tempColor = GetTempColor(tempC);
        
        // 更新体力
        if (m_wTextStamina)
        {
            m_wTextStamina.SetText("STA " + staminaPct.ToString() + "%");
            m_wTextStamina.SetColor(staminaColor);
        }
        
        // 更新速度（显示实际速度 m/s，颜色基于速度倍数）
        if (m_wTextSpeed)
        {
            float speedDisplay = speedMs / 10.0;  // 还原小数
            m_wTextSpeed.SetText("SPD " + speedDisplay.ToString() + "m/s");
            m_wTextSpeed.SetColor(speedColor);
        }
        
        // 更新负重（基于负重惩罚阈值变色）
        // 战斗负重 30kg，最大负重 40.5kg
        if (m_wTextWeight)
        {
            if (weightKg > 0)
                m_wTextWeight.SetText("WT " + weightKg.ToString() + "kg");
            else
                m_wTextWeight.SetText("WT 0kg");
            
            // 负重颜色：超过最大负重红色，超过战斗负重橙色
            if (weightKg >= 40)
                m_wTextWeight.SetColor(GUIColors.RED_BRIGHT2);
            else if (weightKg >= 30)
                m_wTextWeight.SetColor(GUIColors.ORANGE_BRIGHT2);
            else
                m_wTextWeight.SetColor(GUIColors.DEFAULT);
        }
        
        // 更新移动类型
        if (m_wTextMove)
        {
            string displayMoveType = s_sCachedMoveType;
            
            // 如果在游泳，显示Swim
            if (s_bCachedIsSwimming)
                displayMoveType = "Swim";
            
            m_wTextMove.SetText(displayMoveType);
            m_wTextMove.SetColor(GUIColors.DEFAULT);
        }
        
        // 更新坡度（陡坡变色）
        // 上坡消耗更多体力，下坡速度更快但有风险
        if (m_wTextSlope)
        {
            int absSlopeAngle = Math.AbsInt(slopeAngle);
            if (absSlopeAngle > 1)
            {
                string slopeDir = "";
                if (slopeAngle > 0)
                    slopeDir = "+";
                m_wTextSlope.SetText("SLOPE " + slopeDir + slopeAngle.ToString() + "deg");
            }
            else
            {
                m_wTextSlope.SetText("SLOPE 0deg");
            }
            
            // 坡度颜色：陡坡（>20度）红色，中等坡度（>10度）橙色
            if (absSlopeAngle >= 20)
                m_wTextSlope.SetColor(GUIColors.RED_BRIGHT2);
            else if (absSlopeAngle >= 10)
                m_wTextSlope.SetColor(GUIColors.ORANGE_BRIGHT2);
            else
                m_wTextSlope.SetColor(GUIColors.DEFAULT);
        }
        
        // 更新温度（直接使用虚拟气温）
        if (m_wTextHeat)
        {
            m_wTextHeat.SetText("TEMP " + tempC.ToString() + "C");
            m_wTextHeat.SetColor(tempColor);
        }
        
        // 更新风速风向
        if (m_wTextWind)
        {
            string windDir = GetWindDirectionStr(s_fCachedWindDirection);
            if (windSpeedInt > 0)
            {
                m_wTextWind.SetText("WIND " + windDir + " " + windSpeedInt.ToString() + "m/s");
                // 风速颜色：强风用橙色/红色
                if (windSpeedInt >= 15)
                    m_wTextWind.SetColor(GUIColors.RED_BRIGHT2);
                else if (windSpeedInt >= 8)
                    m_wTextWind.SetColor(GUIColors.ORANGE_BRIGHT2);
                else
                    m_wTextWind.SetColor(GUIColors.DEFAULT);
            }
            else
            {
                m_wTextWind.SetText("WIND Calm");
                m_wTextWind.SetColor(GUIColors.DEFAULT);
            }
        }
        
        // 更新室内/室外
        if (m_wTextLocation)
        {
            if (s_bCachedIsIndoor)
            {
                m_wTextLocation.SetText("Indoor");
                m_wTextLocation.SetColor(Color.FromRGBA(100, 200, 100, 255));  // 绿色
            }
            else
            {
                m_wTextLocation.SetText("Outdoor");
                m_wTextLocation.SetColor(GUIColors.DEFAULT);
            }
        }
        
        // 更新地面类型
        if (m_wTextGround)
        {
            string groundType;
            Color groundColor;
            
            // 如果在游泳，显示Water
            if (s_bCachedIsSwimming)
            {
                groundType = "Water";
                groundColor = Color.FromRGBA(0, 150, 255, 255); // 蓝色
            }
            else
            {
                groundType = GetGroundTypeStr(s_fCachedTerrainDensity);
                groundColor = GetGroundColor(s_fCachedTerrainDensity);
            }
            
            m_wTextGround.SetText(groundType);
            m_wTextGround.SetColor(groundColor);
        }
        
        // 更新湿重
        if (m_wTextWet)
        {
            float wetDisplay = wetKg / 10.0;  // 还原小数
            if (wetDisplay > 0.1)
            {
                m_wTextWet.SetText("WET " + wetDisplay.ToString() + "kg");
                // 青色 (自定义颜色，因为 GUIColors 没有 CYAN)
                m_wTextWet.SetColor(Color.FromRGBA(0, 200, 255, 255));
            }
            else
            {
                m_wTextWet.SetText("WET 0kg");
                m_wTextWet.SetColor(GUIColors.DEFAULT);
            }
        }
    }
    
    // 获取风向字符串（8方向）
    // 注意：API返回的是"风吹向的方向"，需要反转180度显示"风来自的方向"
    // 例如：API返回315度（NW方向），实际是SE风（从东南吹来）
    protected string GetWindDirectionStr(float degrees)
    {
        // 反转180度：将"风吹向"转换为"风来自"
        degrees = degrees + 180.0;
        
        // 将角度归一化到 0-360
        while (degrees < 0)
            degrees += 360;
        while (degrees >= 360)
            degrees -= 360;
        
        // 8方向划分（每45度一个方向）
        // 0=N, 45=NE, 90=E, 135=SE, 180=S, 225=SW, 270=W, 315=NW
        if (degrees >= 337.5 || degrees < 22.5)
            return "N";
        else if (degrees >= 22.5 && degrees < 67.5)
            return "NE";
        else if (degrees >= 67.5 && degrees < 112.5)
            return "E";
        else if (degrees >= 112.5 && degrees < 157.5)
            return "SE";
        else if (degrees >= 157.5 && degrees < 202.5)
            return "S";
        else if (degrees >= 202.5 && degrees < 247.5)
            return "SW";
        else if (degrees >= 247.5 && degrees < 292.5)
            return "W";
        else
            return "NW";
    }
    
    // 获取地面类型字符串（基于物理密度）
    // 密度值来自 BallisticInfo.GetDensity()，范围约 0.5-3.0
    // 参考：木箱(0.65), 室内地板(1.13), 草地(1.2), 土质(1.33), 
    //       床垫(1.55), 鹅卵石(1.6), 沥青(2.24), 混凝土(2.3), 
    //       沙地(2.7), 碎石(2.94)
    protected string GetGroundTypeStr(float density)
    {
        if (density < 0)
            return "Unknown";
        else if (density <= 0.7)
            return "Wood";       // 木质表面
        else if (density <= 1.15)
            return "Floor";      // 室内地板
        else if (density <= 1.25)
            return "Grass";      // 草地
        else if (density <= 1.4)
            return "Dirt";       // 土质
        else if (density <= 1.65)
            return "Gravel";     // 鹅卵石/碎石路
        else if (density <= 2.4)
            return "Paved";      // 沥青/混凝土
        else if (density <= 2.8)
            return "Sand";       // 沙地
        else
            return "Rock";       // 岩石/碎石
    }
    
    // 获取地面颜色（基于物理密度）
    // 绿色=良好（铺装路面），白色=普通，橙色=困难
    protected Color GetGroundColor(float density)
    {
        if (density < 0)
            return GUIColors.DEFAULT;
        else if (density <= 0.7)
            return Color.FromRGBA(100, 200, 100, 255);  // 木质 - 绿色（良好）
        else if (density <= 1.15)
            return Color.FromRGBA(100, 200, 100, 255);  // 地板 - 绿色（良好）
        else if (density <= 1.25)
            return GUIColors.DEFAULT;                   // 草地 - 白色（普通）
        else if (density <= 1.4)
            return GUIColors.DEFAULT;                   // 土质 - 白色（普通）
        else if (density <= 1.65)
            return GUIColors.ORANGE_BRIGHT2;            // 鹅卵石 - 橙色（困难）
        else if (density <= 2.4)
            return Color.FromRGBA(100, 200, 100, 255);  // 铺装 - 绿色（良好）
        else if (density <= 2.8)
            return GUIColors.ORANGE_BRIGHT2;            // 沙地 - 橙色（困难）
        else
            return GUIColors.RED_BRIGHT2;               // 岩石 - 红色（非常困难）
    }
    
    // 获取体力颜色
    protected Color GetStaminaColor(int pct)
    {
        if (pct < 20)
            return GUIColors.RED_BRIGHT2;
        else if (pct < 40)
            return GUIColors.ORANGE_BRIGHT2;
        else
            return GUIColors.DEFAULT;
    }
    
    // 获取速度颜色（越接近最大速度越红，表示体力消耗越快）
    protected Color GetSpeedColor(int pct)
    {
        if (pct >= 95)
            return GUIColors.RED_BRIGHT2;
        else if (pct >= 80)
            return GUIColors.ORANGE_BRIGHT2;
        else
            return GUIColors.DEFAULT;
    }
    
    // 获取温度颜色（基于摄氏度）
    protected Color GetTempColor(int tempC)
    {
        if (tempC >= 35)
            return GUIColors.RED_BRIGHT2;      // 高温危险 - 红色
        else if (tempC >= 28)
            return GUIColors.ORANGE_BRIGHT2;   // 炎热 - 橙色
        else if (tempC >= 15)
            return GUIColors.DEFAULT;          // 舒适 - 白色
        else if (tempC >= 5)
            return Color.FromRGBA(100, 180, 255, 255);  // 凉爽 - 浅蓝色
        else
            return Color.FromRGBA(0, 150, 255, 255);    // 寒冷 - 蓝色
    }
}
