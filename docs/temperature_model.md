# 温度物理模型说明（P1） 🔧🌤️

## 概要
- 目的：基于太阳辐照与简化地气相互作用，按离散时间步（默认每 5 s）更新近地表温度（°C），并作为系统的“当前气温”输出。模型设计在游戏引擎不可用或不可用温度数据时提供物理合理的替代值。
- 实现文件：`scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c`

---

## 游戏/引擎提供的值（输入） 🕹️
- 时间与日期：
  - `GetTimeOfTheDay()` -> 小时（0.0 - 24.0）
  - `GetDate(year, month, day)` -> 用于年积日计算
- 地理：
  - `GetCurrentLatitude()` -> 纬度（度）
  - （经度暂未用于本模型的局地真太阳时修正）
  - 时区/夏令时与天文数据：引擎提供多个接口用于返回经纬度/时区/DST、日出/日落和月相，模组会优先使用这些接口（若可用），在引擎不可用或被设置覆盖时回退到内部计算。  
    示例接口：
    ```cpp
    SCR_MoonPhaseUIInfo moonPhaseInfo = m_TimeAndWeatherManager.GetMoonPhaseInfoForDate(year, month, day, daytime, timeZoneOffset, dstOffset, latitude);
    bool hasSR = m_TimeAndWeatherManager.GetSunriseHourForDate(year, month, day, latitude, longitude, timezone, dstOffset, out float hour24);
    bool hasSS = m_TimeAndWeatherManager.GetSunsetHourForDate(year, month, day, latitude, longitude, timezone, dstOffset, out float hour24);
    ```
    说明：
    - `GetSunriseHourForDate` / `GetSunsetHourForDate` 返回布尔值指示在给定经纬度与日期下是否存在有效的日出或日落（极地季节性无日出/日落时返回 false）。
    - `GetMoonPhaseInfoForDate` / `GetMoonPhaseForDate` 可用于获取月相并用于 UI/夜间辐照的修正。  
    这些接口可让模组直接使用引擎的经度/时区/DST和日出日落判定来校正本地太阳时并判定昼夜，从而在计算太阳天顶角与短波辐照时与地图经纬度和引擎保持一致。  
    注意：某些平台或地图可能无法返回经度/经纬信息（例如 `GetCurrentLongitude()` 不可用）。在这种情况下，模组将优先使用引擎提供的日出/日落/月相接口（`GetSunriseHour` / `GetSunsetHour` / `GetMoonPhase`）作为权威来源来判定昼夜与天文参数；只有在用户显式在 `SCR_RSS_Settings` 中配置了覆盖值时才会使用这些覆盖值。
- 天气相关：
  - `GetRainIntensity()` -> 降雨强度（0.0-1.0）
  - `GetCurrentWetness()` -> 地表湿度（0.0-1.0）
  - `GetWindSpeed()` -> 风速（m/s）
  - `GetTemperatureAirMinOverride() / GetTemperatureAirMaxOverride()` -> 引擎给出的日间 min/max（仅当启用引擎温度时作为边界）
  - `GetOverrideTemperature()` -> 是否被覆盖
- 使用位置：当 `m_bUseEngineTemperature && m_bUseEngineWeather` 时，会参考上面部分 API；否则完全通过模组内计算。

---

## 关键参数与计算公式 ⚙️
注：角度除非说明，均以度为单位并在公式中转换为弧度。

### 常数
- 太阳常数：`S = 1361 W/m^2`（变量名 `m_fSolarConstant`）
- 斯特藩-玻尔兹曼常数：`σ = 5.670374419e-8 W/m^2/K^4`（`STEFAN_BOLTZMANN`）
- 空气密度 `ρ ≈ 1.225 kg/m^3`、比热 `Cp ≈ 1004 J/(kg·K)`（经验取值）


### 太阳几何
- 年积日 n = DayOfYear(year, month, day)
- 太阳偏角（弧度） δ ≈ 23.44° × sin(2π (284 + n) / 365)
- 太阳天顶余弦 cosθ = sin(lat)*sin(δ) + cos(lat)*cos(δ)*cos(hourAngle)
  - hourAngle = 15° * (localHour - 12)
- 若 cosθ ≤ 0 则视为夜间（无短波入射）

### 顶端外大气辐照（含年变化）
- I0 = S * (1 + 0.033 cos(2π n / 365)) * cosθ

### 空气质量（Air Mass） — Kasten & Young
- m = 1 / (cosθ + 0.50572 * (96.07995 - θ_deg)^-1.6364)（若 cosθ ≤ 0 返回大值）

### 清空透过率（经验）
- τ = exp(-AOD * m)，在代码中使用 `Math.Pow(M_E, -m_fAerosolOpticalDepth * m)`，并 clamp 到 [0,1]
- 参数：`m_fAerosolOpticalDepth`（默认 0.14，经验值）

### 云因子与云遮挡
- cloudFactor = InferCloudFactor()：基于降雨强度、地表湿度与天气状态名推断（0..1）
- cloudBlocking = 0.7 * cloudFactor（经验系数）

### 短波到达地表
- SW_down = I0 * τ * (1 - cloudBlocking)

### 长波下行（简化模型）
- T_atm ≈ T_surface + 2°C（近似）
- ε_atm = 0.78 + 0.14 * cloudFactor（经验）
- LW_down = ε_atm * σ * (T_atm + 273.15)^4

### 地表发射
- LW_up = ε_surface * σ * (T_surface + 273.15)^4（`m_fSurfaceEmissivity` 默认 0.98）

### 净辐射
- Q_rad = (1 - albedo) * SW_down + LW_down - LW_up
  - `m_fAlbedo` 默认 0.2（草地/混合地表）

### 潜热项（简化）
- LE ≈ 200 * surfaceWetness（W/m^2，经验关系，湿润时增加能量损失）

### 混合层与风速影响
- Hmix = `m_fTemperatureMixingHeight`（m，默认 1000 m）
- 风影响：mixing_height_eff = max(10.0, Hmix * (1 + wind / 10))，风越大 → 混合层越高 → 温度对辐射的响应越小

### 温度变化（显式时间积分）
- dT = (Q_net * dt) / (ρ * Cp * mixing_height_eff)
- newT = T_surface + dT（结果 clamp 在 [-80°C, +60°C] 以保证物理与数值稳定）

---

## 稳态平衡温度求解（回退/初始） 🔍
- 目的：当引擎提供的 tempMin/tempMax 不可信（几乎相等或被锁死）时，使用物理平衡估算合理的初始温度。
- 方法：二分法求解 T 使 NetRadiationAtSurface(T) ≈ 0（容差 1 W/m^2），左右界 [-80, +60]。
- 若二分法在两端函数符号相同（无法找到根），退回到 `CalculateSimulatedTemperature()`（昼夜余弦模型）。

---

## 模块中的可调参数与默认值（代码位置） 🧭
- `m_fTempUpdateInterval = 5.0`（秒）：温度步进间隔（`SCR_EnvironmentFactor.c` / `SCR_RSS_Settings` → `m_fTempUpdateInterval`）
- `m_fTemperatureMixingHeight = 1000.0`（m）：混合层高度（`SCR_EnvironmentFactor.c` / `SCR_RSS_Settings` → `m_fTemperatureMixingHeight`）
- `m_fAlbedo = 0.2`（无量纲）：地表反照率（`SCR_EnvironmentFactor.c` / `SCR_RSS_Settings` → `m_fAlbedo`）
- `m_fAerosolOpticalDepth = 0.14`（无量纲）：气溶胶光学厚度（`SCR_EnvironmentFactor.c` / `SCR_RSS_Settings` → `m_fAerosolOpticalDepth`）
- `m_fSurfaceEmissivity = 0.98`（无量纲）：地表发射率（`SCR_EnvironmentFactor.c` / `SCR_RSS_Settings` → `m_fSurfaceEmissivity`）
- `m_fCloudBlockingCoeff = 0.7`（无量纲）：云层遮挡短波的系数（`SCR_EnvironmentFactor.c` / `SCR_RSS_Settings` → `m_fCloudBlockingCoeff`）
- `m_fLECoef = 200.0`（W/m2 每单位湿度）：潜热系数（`SCR_EnvironmentFactor.c` / `SCR_RSS_Settings` → `m_fLECoef`）
- `m_fCachedSurfaceTemperature`：近地面温度缓存（°C）
- `m_bUseEngineTemperature`：是否使用引擎温度（bool，`SCR_RSS_Settings` → `m_bUseEngineTemperature`）
- `m_bUseEngineTimezone`：是否优先使用引擎时区（bool，`SCR_RSS_Settings` → `m_bUseEngineTimezone`）
- `m_fLongitude`：经度覆盖（度，`SCR_RSS_Settings` → `m_fLongitude`）
- `m_fTimeZoneOffsetHours`：时区偏移覆盖（小时，`SCR_RSS_Settings` → `m_fTimeZoneOffsetHours`）

调整这些参数会显著影响模型响应（例如减小 AOD 或增大 albedo 会影响白天短波吸收；改变混合层高度会影响温度步进的时间常数）。

> 提示：这些设置在 `SCR_RSS_Settings` 中已提供 UI 来编辑（Custom 模式或全局设置），可在运行时修改并通过重新初始化 `EnvironmentFactor` 生效。

---

## 代码函数一览（位置与作用）
- `CalculateSimulatedTemperature()`：旧的昼夜余弦模型（用于模拟或稳态回退）。
- `SolarDeclination()` / `SolarCosZenith()` / `AirMass()` / `ClearSkyTransmittance()`：太阳几何与辐照工具函数。
- `InferCloudFactor()`：从降雨/湿度/天气名推断云因子。
- `StepTemperature(dt)`：主步进器，按 dt（秒）计算 dT 并更新 `m_fCachedSurfaceTemperature`，输出 `[RealisticSystem][TempStep]` 日志。
- `NetRadiationAtSurface(T, lat, n, tod, cloud)`：净辐射计算器（用于稳态求解）。
- `CalculateEquilibriumTemperatureFromPhysics()`：稳态二分法求解器。
- `CalculateTemperatureFromAPI()`：当 `m_bUseEngineTemperature`=true 时，用引擎 min/max 做昼夜插值回传值。

---

## 日志与调试 🔎
- 关键日志：
  - `[RealisticSystem][WeatherDebug] ... | UseEngineTemp=true/false`（当前是否使用引擎温度）
  - `[RealisticSystem] Warning: Temperature min/max nearly equal (...)`（当引擎 min/max 失真时）
  - `[RealisticSystem][TempStep] dt=... | SW=... | NewT=... | Cloud=...`（每步进的摘要）
  - `[RealisticSystem][TempStepVerbose] ...`（更详细的能量项输出，需 Verbose 打开）

---

## 校准与改进建议 ✅
- 校准目标：`m_fAerosolOpticalDepth`、`m_fAlbedo`、`m_fTemperatureMixingHeight`、`cloudBlocking` 系数、`LE` 系数。可利用 `tools/temperature_model_analysis.py` 进行参数扫描或用 Optuna 自动优化。
- 改进方向：
  - 更准确的对流换热/感热模型（基于风速的换热系数），替代目前将感热并入混合层近似的做法。
  - 使用实际地表类型映射不同 albedo/emissivity。
  - 引入地形遮挡、地表热容和土壤水分的简单一维模型以改进夜间冷却表现。

---

## 参考（代码位置）
- 主实现：`scripts/Game/Components/Stamina/SCR_EnvironmentFactor.c`
- 离线仿真/校准工具：`tools/temperature_model_analysis.py`

---

如需，我可以：
- 把上面参数以表格写入 `SCR_RSS_Settings` 并加入 UI 控件，或
- 生成一页简易的参数调参指南（包含推荐搜索范围与示例 Optuna 配置）。

需要我接着把参数暴露到设置里，还是先生成参数调参指南？ ✨