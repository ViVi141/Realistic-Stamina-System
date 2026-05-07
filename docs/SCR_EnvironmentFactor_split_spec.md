# SCR_EnvironmentFactor.c 拆分规格

**日期**: 2026-05-07  
**当前行数**: 2258  
**目标**: 拆至 4 个子模块，主文件降至 ~650 行编排层

---

## 当前结构概览

```
SCR_EnvironmentFactor.c (2258 行)
├── 状态变量                           L5-100    (~95)
├── 公共 API + 初始化 + getter         L102-712  (~610)
├── 模拟温度                           L718-816  (~100)
├── 天文/太阳工具                      L831-1116 (~286)  ← 阶段 A
├── 温度物理步进                       L1152-1442(~290)  ← 阶段 C
├── 热应激                             L1442-1482(~40)
├── 室内检测                           L1482-1750(~270)  ← 阶段 B
├── 调试/强制更新                      L1780-1918(~140)
└── 高级环境因子更新 (9 个 Calculate*)  L1923-2258(~335)  ← 阶段 D
```

---

## 阶段 A：天文/太阳工具 → 充实 SCR_EnvironmentAstronomyMath.c

**风险**: 低（纯函数，只依赖输入参数，不访问类成员）  
**当前**: `SCR_EnvironmentAstronomyMath.c` 仅 64 行  
**目标**: ~350 行

### 迁移的函数

| 函数 | 行 | 说明 | 依赖 |
|------|-----|------|------|
| `DayOfYear(y,m,d)` | L834 | 儒略日计算 | 无 |
| `SolarDeclination(n)` | L840 | 太阳赤纬 | 无 |
| `SolarCosZenith(lat,n,hour)` | L846 | 天顶角余弦 | 无 |
| `AirMass(cosTheta)` | L1104 | 大气质量 | 无 |
| `ClearSkyTransmittance(m)` | L1110 | 晴空透过率 | 无 |
| `InferCloudFactor()` | L1116 | 云量推断 | `m_pCachedWeatherManager` |
| `EstimateLatLongFromSunriseSunset()` | L853 | 日出日落→经纬估计 | `m_pCachedWeatherManager`, `m_fTimeZoneOffsetHours` |
| `EstimateLatLongFromAstronomicalSearch()` | L951 | 天文搜索→经纬 | `m_pCachedWeatherManager`, `m_fTimeZoneOffsetHours` |

### 接口设计

```c
class SCR_EnvironmentAstronomyMath
{
    // 纯函数 — 无状态
    static int DayOfYear(int year, int month, int day);
    static float SolarDeclination(int n);
    static float SolarCosZenith(float latDeg, int n, float localHour);
    static float AirMass(float cosTheta);
    static float ClearSkyTransmittance(float m);
    
    // 需要传入 WeatherManager
    static float InferCloudFactor(TimeAndWeatherManagerEntity weatherManager);
    static float EstimateLatLongFromSunriseSunset(
        out float outLatDeg, out float outLonDeg,
        TimeAndWeatherManagerEntity weatherManager,
        float timeZoneOffsetHours,
        float nextLocationEstimateLogTime);  // 节流用
    static float EstimateLatLongFromAstronomicalSearch(
        out float outLatDeg, out float outLonDeg,
        TimeAndWeatherManagerEntity weatherManager,
        float timeZoneOffsetHours,
        float nextLocationEstimateLogTime);
};
```

### 主文件中的变更

将直接调用改为委托：
```c
// 旧
float cloud = InferCloudFactor();
// 新
float cloud = SCR_EnvironmentAstronomyMath.InferCloudFactor(m_pCachedWeatherManager);
```

---

## 阶段 B：室内检测 → 新建 SCR_EnvironmentIndoorDetection.c

**风险**: 中（依赖 `m_pTraceParam*`、`m_pCachedBuildings`、owner 引用）  
**目标**: ~280 行

### 迁移的函数

| 函数 | 行 | 说明 |
|------|-----|------|
| `EvaluateRoofedBuildingInterior()` | L1482 | 室内判定主逻辑 |
| `IsUnderCover()` | L1592 | 是否有遮挡 |
| `RaycastHasRoof()` | L1604 | 屋顶射线检测 |
| `WorldToLocal()` | L1673 | 世界坐标→局部坐标 |
| `IsHorizontallyEnclosed()` | L1689 | 水平封闭检测 |
| `QueryBuildingCallback()` | L1750 | 建筑查询回调 |

### 接口设计

```c
class SCR_EnvironmentIndoorDetection
{
    // 复用对象（调用方传入）
    private ref TraceParam m_pTraceParamRoof;
    private ref TraceParam m_pTraceParamEnclosed;
    private ref array<IEntity> m_pCachedBuildings;
    private bool m_bIndoorDebug;
    
    void InitIndoorDetection(bool indoorDebug);
    bool EvaluateRoofedBuildingInterior(IEntity owner, float roofCheckHeightM, bool requireHorizontalEnclosure);
    bool IsUnderCover(IEntity owner);
    bool IsIndoorForEntity(IEntity owner, float currentTime, float checkInterval, 
                           out float lastCheckTime, ref bool cachedState);
    bool ShouldSuppressTerrainSlopeForEntity(IEntity owner, float currentTime, 
                                              float checkInterval,
                                              out float lastCheckTime, 
                                              ref bool cachedRoofedState);
    
    private bool RaycastHasRoof(IEntity owner, IEntity building, float roofCheckHeightM);
    private bool IsHorizontallyEnclosed(IEntity owner);
    private vector WorldToLocal(vector worldMat[4], vector worldPos);
    private bool QueryBuildingCallback(IEntity e);
};
```

---

## 阶段 C：温度物理步进 → 新建 SCR_EnvironmentTemperatureMath.c

**风险**: 高（与 15+ 成员变量交互 — 反照率、发射率、混合层高度、地表温度缓存等）  
**目标**: ~300 行

### 迁移的函数

| 函数 | 行 | 说明 |
|------|-----|------|
| `CalculateSimulatedTemperature()` | L718 | 简化模拟温度 |
| `CalculateUniversalTemperature()` | L792 | 通用温度模型 |
| `StepTemperature()` | L1152 | 温度物理步进 |
| `EstimateSeasonalAvgTemp()` | L1308 | 季节均值估计 |
| `NetRadiationAtSurface()` | L1320 | 地表净辐射 |
| `CalculateEquilibriumTemperatureFromPhysics()` | L1396 | 物理平衡求解 |

### 接口设计

```c
class SCR_EnvironmentTemperatureMath
{
    // --- 配置参数 ---
    float m_fAlbedo;
    float m_fAerosolOpticalDepth;
    float m_fSurfaceEmissivity;
    float m_fCloudBlockingCoeff;
    float m_fLECoef;
    float m_fTemperatureMixingHeight;
    float m_fAltitudeMeters;
    float m_fFogDensity;
    
    // --- 运行时状态 ---
    float m_fCachedSurfaceTemperature;   // 近地面温度
    float m_fCachedTemperature;          // 对外暴露的温度
    float m_fCachedWindSpeed;            // 风速（用于混合层修正）
    float m_fLastTemperatureUpdateTime;
    
    // --- 公共方法 ---
    float CalculateUniversalTemperature(float latitude, int dayOfYear, float hourOfDay,
                                         float altitudeMeters, float overcast, 
                                         float rainIntensity, float fogDensity);
    void StepTemperature(float dt, TimeAndWeatherManagerEntity weatherManager,
                         float longitude, float timeZoneOffsetHours,
                         float lat, int n, float tod, float cloudFactor);
    float GetTemperature();              // 返回 m_fCachedTemperature
    float CalculateEquilibriumTemperatureFromPhysics(TimeAndWeatherManagerEntity wm, 
                                                      float lat, int n, float tod, 
                                                      float cloudFactor);
    float EstimateSeasonalAvgTemp(float lat, int dayOfYear);
    float NetRadiationAtSurface(float T_surface, float lat, int n, float tod, 
                                 float cloudFactor);
    float CalculateSimulatedTemperature(TimeAndWeatherManagerEntity wm);
};
```

---

## 阶段 D：惩罚/因子计算 → 充实 SCR_EnvironmentPenaltyMath.c

**风险**: 高（依赖温度/湿度/室内缓存值）  
**当前**: `SCR_EnvironmentPenaltyMath.c` 仅 95 行  
**目标**: ~430 行

### 迁移的函数

| 函数 | 行 | 说明 |
|------|-----|------|
| `CalculateRainWetWeight()` | L2144 | 降雨湿重计算 |
| `CalculateRainBreathingPenalty()` | L2205 | 暴雨呼吸阻力 |
| `CalculateMudTerrainFactor()` | L2211 | 泥泞地形惩罚 |
| `CalculateMudSprintPenalty()` | L2217 | 泥泞冲刺惩罚 |
| `CalculateSlipRisk()` | L2223 | 滑倒风险 |
| `CalculateHeatStressPenalty()` | L2229 | 热应激惩罚 |
| `AdjustEnergyForTemperature()` | L2237 | 温度能耗补偿 |
| `CalculateColdStressPenalty()` | L2243 | 冷应激惩罚 |
| `CalculateSurfaceWetnessPenalty()` | L2251 | 地表湿度惩罚 |
| `CalculateHeatStressMultiplier()` | L1442 | 热应激倍数 |

### 接口设计

```c
class SCR_EnvironmentPenaltyMath
{
    // 所有方法接收当前环境缓存值作为参数，不持有状态
    
    static void CalculateRainWetWeight(float currentTime, float deltaTime,
                                        float rainIntensity, bool isIndoor,
                                        ref float cachedRainWeight,
                                        ref float rainStopTime, ref float rainPeakWeight);
    
    static float CalculateSlipRisk(float mudFactor, float surfaceWetness, 
                                    float velocity, bool isSprinting);
    
    static float AdjustEnergyForTemperature(float basePower, float temperature, 
                                             float windSpeed);
    
    static void CalculateHeatStressPenalty(float temperature, bool isIndoor,
                                            out float heatStressPenalty,
                                            out float heatStressMultiplier);
    
    static void CalculateColdStressPenalty(float temperature, float windSpeed,
                                            out float coldStressPenalty,
                                            out float coldStaticPenalty);
    
    static void CalculateMudPenalties(float mudFactor, bool isSprinting,
                                       out float mudTerrainFactor,
                                       out float mudSprintPenalty);
};
```

---

## 阶段 E（主文件瘦身后）

完成后 `SCR_EnvironmentFactor.c` 约 650 行：

```c
class EnvironmentFactor
{
    // --- 少数核心状态变量 ---
    protected float m_fLastEnvironmentCheckTime;
    protected float m_fLastUpdateTime;
    protected TimeAndWeatherManagerEntity m_pCachedWeatherManager;
    protected IEntity m_pCachedOwner;
    
    // --- 子模块引用 ---
    protected ref SCR_EnvironmentAstronomyMath m_pAstroMath;
    protected ref SCR_EnvironmentIndoorDetection m_pIndoorDetector;
    protected ref SCR_EnvironmentTemperatureMath m_pTempMath;
    
    // --- 编排方法 ---
    void Initialize(World world, IEntity owner);
    bool UpdateEnvironmentFactors(float currentTime, IEntity owner, ...);
    
    // --- 委派 getter ---
    float GetTemperature()       { return m_pTempMath.GetTemperature(); }
    float GetSlipRisk()          { return m_fSlipRisk; }
    float GetHeatStressPenalty() { return m_fHeatStressPenalty; }
    // ... 其余 getter 不变
};
```

---

## 执行顺序与验收

| 阶段 | 依赖 | 风险 | 验收标准 |
|------|------|------|---------|
| A | 无 | 低 | 编译通过 + 单机冲刺测试 |
| B | A | 中 | 编译通过 + 室内外切换 + 温度正常 |
| C | A | 高 | 编译通过 + 温度步进不漂移 + 辐射平衡 |
| D | B, C | 高 | 编译通过 + 降雨/泥泞/滑倒逻辑一致 |
| E | D | — | 全量回归：单机 + 联机 + 配置同步 |

### 每次阶段的回归检查单

- [ ] Workbench 编译零错误
- [ ] 单机 0 kg Run 3.5km / 15:27（体力不低于 20%）
- [ ] 冲刺/恢复周期正常
- [ ] 室内外切换温度/坡度正确
- [ ] 联机配置同步正常
- [ ] HUD 显示与环境一致

---

## 进度

| 阶段 | 状态 | 完成日期 |
|------|------|---------|
| A — 天文/太阳工具 | 待执行 | — |
| B — 室内检测 | 待执行 | — |
| C — 温度物理 | 待执行 | — |
| D — 惩罚/因子 | 待执行 | — |
| E — 主文件收尾 | 待执行 | — |
