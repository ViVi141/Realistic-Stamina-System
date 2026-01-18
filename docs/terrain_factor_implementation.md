# 地形系数 (Terrain Factor) 实现指南

本文档基于 `SCR_RecoilForceAimModifier` 代码示例，说明如何在 Arma Reforger 中实现地形系数功能。

---

## 📚 代码示例分析

### 关键 API 调用

```enforce
// 1. 创建射线追踪参数
TraceParam paramGround = new TraceParam();
paramGround.Start = owner.GetOrigin() + (vector.Up * 0.1);
paramGround.End = paramGround.Start - vector.Up;
paramGround.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
paramGround.Exclude = owner;
paramGround.LayerMask = EPhysicsLayerPresets.Projectile;

// 2. 执行射线追踪
owner.GetWorld().TraceMove(paramGround, FilterCallback);

// 3. 获取表面材质
GameMaterial material = paramGround.SurfaceProps;

// 4. 获取弹道信息（包含密度等物理属性）
BallisticInfo ballisticInfo;
if (material)
    ballisticInfo = material.GetBallisticInfo();

// 5. 使用密度值计算系数
if (ballisticInfo)
    surfaceDensityMultiplier = Math.AbsFloat(ballisticInfo.GetDensity() - 1) * 0.2 + 1;
```

### API 说明

| API | 说明 | 返回值 |
|-----|------|--------|
| `TraceParam.SurfaceProps` | 射线追踪命中的表面材质 | `GameMaterial` |
| `GameMaterial.GetBallisticInfo()` | 获取材质的弹道/物理信息 | `BallisticInfo` |
| `BallisticInfo.GetDensity()` | 获取材质密度（用于计算地形系数） | `float` (通常 0.5-3.0) |

---

## 🎯 实现方案

### 方案一：基于密度值的映射

**优点**: 简单直接，利用现有 API  
**缺点**: 密度值可能不完全对应地形系数

```enforce
// 在 SCR_RealisticStaminaSystem.c 中添加

class TerrainFactorSystem
{
    // 地形系数常量（基于 Pandolf 模型研究）
    static const float TERRAIN_FACTOR_PAVED = 1.0;        // 铺装路面
    static const float TERRAIN_FACTOR_DIRT = 1.1;         // 碎石路
    static const float TERRAIN_FACTOR_GRASS = 1.2;        // 高草丛
    static const float TERRAIN_FACTOR_BRUSH = 1.5;        // 重度灌木丛
    static const float TERRAIN_FACTOR_SAND = 1.8;         // 软沙地
    static const float TERRAIN_FACTOR_SNOW = 2.1;         // 深雪
    static const float TERRAIN_FACTOR_MUD = 2.5;          // 极粘稠泥地
    
    // 密度到地形系数的映射函数
    static float GetTerrainFactorFromDensity(float density)
    {
        // 密度值范围通常在 0.5 (空气/软物) 到 3.0 (硬物/金属) 之间
        // 根据实际测试调整映射曲线
        
        if (density <= 0.0)
            return TERRAIN_FACTOR_PAVED; // 默认值
        
        // 简化线性映射（需要根据实际测试调整）
        // 假设：密度 1.0 = 铺装路面，密度 3.0 = 深雪/泥地
        float terrainFactor = 1.0 + (density - 1.0) * 0.6; // 线性映射
        
        // 限制在合理范围内
        return Math.Clamp(terrainFactor, TERRAIN_FACTOR_PAVED, TERRAIN_FACTOR_MUD);
    }
    
    // 获取角色脚下的地形系数
    static float GetTerrainFactorUnderCharacter(IEntity character)
    {
        if (!character)
            return TERRAIN_FACTOR_PAVED; // 默认值
        
        // 执行射线追踪检测地面
        TraceParam paramGround = new TraceParam();
        paramGround.Start = character.GetOrigin() + (vector.Up * 0.1);
        paramGround.End = paramGround.Start - (vector.Up * 0.5); // 向下追踪 0.5 米
        paramGround.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        paramGround.Exclude = character;
        paramGround.LayerMask = EPhysicsLayerPresets.Projectile;
        
        character.GetWorld().TraceMove(paramGround, FilterCallback);
        
        // 获取表面材质
        GameMaterial material = paramGround.SurfaceProps;
        if (!material)
            return TERRAIN_FACTOR_PAVED; // 默认值
        
        // 获取弹道信息（包含密度）
        BallisticInfo ballisticInfo = material.GetBallisticInfo();
        if (!ballisticInfo)
            return TERRAIN_FACTOR_PAVED; // 默认值
        
        // 使用密度值计算地形系数
        float density = ballisticInfo.GetDensity();
        return GetTerrainFactorFromDensity(density);
    }
    
    // 过滤回调（排除角色实体）
    static bool FilterCallback(IEntity e)
    {
        if (ChimeraCharacter.Cast(e))
            return false;
        return true;
    }
}
```

---

### 方案二：基于材质名称的精确映射（推荐）

**优点**: 精确控制，可针对特定材质设置系数  
**缺点**: 需要知道材质名称或类型枚举

```enforce
// 在 SCR_RealisticStaminaSystem.c 中添加

class TerrainFactorSystem
{
    // 地形系数映射表（基于材质类型）
    static const float TERRAIN_FACTOR_PAVED = 1.0;
    static const float TERRAIN_FACTOR_DIRT = 1.1;
    static const float TERRAIN_FACTOR_GRASS = 1.2;
    static const float TERRAIN_FACTOR_BRUSH = 1.5;
    static const float TERRAIN_FACTOR_SAND = 1.8;
    static const float TERRAIN_FACTOR_SNOW = 2.1;
    static const float TERRAIN_FACTOR_MUD = 2.5;
    
    // 获取地形系数（基于材质）
    static float GetTerrainFactor(IEntity character)
    {
        if (!character)
            return TERRAIN_FACTOR_PAVED;
        
        // 执行射线追踪
        TraceParam paramGround = new TraceParam();
        paramGround.Start = character.GetOrigin() + (vector.Up * 0.1);
        paramGround.End = paramGround.Start - (vector.Up * 0.5);
        paramGround.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
        paramGround.Exclude = character;
        paramGround.LayerMask = EPhysicsLayerPresets.Projectile;
        
        character.GetWorld().TraceMove(paramGround, FilterCallback);
        
        GameMaterial material = paramGround.SurfaceProps;
        if (!material)
            return TERRAIN_FACTOR_PAVED;
        
        // 方法1：尝试获取材质名称/类型（如果 API 支持）
        // string materialName = material.GetName(); // 假设 API 存在
        // return GetTerrainFactorFromMaterialName(materialName);
        
        // 方法2：使用密度值映射（当前可用）
        BallisticInfo ballisticInfo = material.GetBallisticInfo();
        if (ballisticInfo)
        {
            float density = ballisticInfo.GetDensity();
            return GetTerrainFactorFromDensity(density);
        }
        
        return TERRAIN_FACTOR_PAVED;
    }
    
     // 基于密度的地形系数映射（基于实际测试数据的插值映射）
     // 测试数据点：(0.65,1.0), (1.13,1.0), (1.2,1.2), (1.33,1.1), (1.55,1.3), 
     //            (1.6,1.4), (2.24,1.0), (2.3,1.0), (2.7,1.5), (2.94,1.8)
     // 见 docs/terrain_density_mapping.md 了解详细映射关系
     static float GetTerrainFactorFromDensity(float density)
     {
         if (density <= 0.0)
             return TERRAIN_FACTOR_PAVED; // 默认值
         
         // 特殊情况：高密度平整表面（沥青、混凝土）
         // 这些材质密度高但表面平整，应为基准值
         if (density >= 2.2 && density <= 2.4)
             return TERRAIN_FACTOR_PAVED; // 沥青(2.24)、混凝土(2.3) → 1.0
         
         // 特殊情况：低密度平整表面（木箱）
         if (density <= 0.7)
             return TERRAIN_FACTOR_PAVED; // 木箱(0.65) → 1.0
         
         // 区间 1: 0.7 < density <= 1.2
         // 插值：室内地板(1.13, 1.0) → 草地(1.2, 1.2)
         if (density <= 1.2)
         {
             if (density <= 1.13)
                 return TERRAIN_FACTOR_PAVED; // 室内地板区间
             // 线性插值
             float t = (density - 1.13) / (1.2 - 1.13);
             return 1.0 + t * 0.2; // 1.0 → 1.2
         }
         
         // 区间 2: 1.2 < density <= 1.33
         // 插值：草地(1.2, 1.2) → 土质(1.33, 1.1)
         if (density <= 1.33)
         {
             float t = (density - 1.2) / (1.33 - 1.2);
             return 1.2 - t * 0.1; // 1.2 → 1.1
         }
         
         // 区间 3: 1.33 < density <= 1.6
         // 插值：土质(1.33, 1.1) → 床垫(1.55, 1.3) → 海岸鹅卵石(1.6, 1.4)
         if (density <= 1.55)
         {
             // 子区间：土质 → 床垫
             float t = (density - 1.33) / (1.55 - 1.33);
             return 1.1 + t * 0.2; // 1.1 → 1.3
         }
         else // density <= 1.6
         {
             // 子区间：床垫 → 海岸鹅卵石
             float t = (density - 1.55) / (1.6 - 1.55);
             return 1.3 + t * 0.1; // 1.3 → 1.4
         }
         
         // 区间 4: 1.6 < density < 2.2
         // 插值：海岸鹅卵石(1.6, 1.4) → 平滑过渡到沥青区间(2.2, 1.0)
         if (density < 2.2)
         {
             float t = (density - 1.6) / (2.2 - 1.6);
             return 1.4 - t * 0.4; // 1.4 → 1.0
         }
         
         // 区间 5: 2.4 < density <= 2.7
         // 插值：混凝土区间结束(2.4, 1.0) → 铁棚(2.7, 1.5)
         if (density <= 2.7)
         {
             float t = (density - 2.4) / (2.7 - 2.4);
             return 1.0 + t * 0.5; // 1.0 → 1.5
         }
         
         // 区间 6: 2.7 < density
         // 插值：铁棚(2.7, 1.5) → 陶片屋顶(2.94, 1.8)
         if (density <= 2.94)
         {
             float t = (density - 2.7) / (2.94 - 2.7);
             return 1.5 + t * 0.3; // 1.5 → 1.8
         }
         
         // 外推：超出已知范围，限制在合理范围内
         return Math.Clamp(1.8 + (density - 2.94) * 0.1, 1.8, 2.5);
     }
    
    static bool FilterCallback(IEntity e)
    {
        if (ChimeraCharacter.Cast(e))
            return false;
        return true;
    }
}
```

---

## 🔧 集成到现有系统

### 1. 修改 `CalculatePandolfEnergyExpenditure` 函数

```enforce
// 在 SCR_RealisticStaminaSystem.c 中修改

static float CalculatePandolfEnergyExpenditure(
    float velocity, 
    float currentWeight, 
    float gradePercent = 0.0,
    float terrainFactor = 1.0)  // 新增参数：地形系数
{
    // ... 现有代码 ...
    
    // 修改基础项和坡度项，应用地形系数
    float baseTerm = PANDOLF_BASE_COEFF + (PANDOLF_VELOCITY_COEFF * velocitySquaredTerm);
    float baseTerm = baseTerm * terrainFactor; // 应用地形系数
    
    float gradeTerm = gradeDecimal * (PANDOLF_GRADE_BASE_COEFF + (PANDOLF_GRADE_VELOCITY_COEFF * velocitySquared));
    float gradeTerm = gradeTerm * terrainFactor; // 应用地形系数
    
    // ... 其余代码 ...
}
```

### 2. 在 `PlayerBase.c` 中调用地形系数

```enforce
// 在 UpdateSpeedBasedOnStamina() 中添加地形系数获取

void UpdateSpeedBasedOnStamina()
{
    // ... 现有代码 ...
    
    // 获取地形系数（性能优化：缓存，仅在必要时更新）
    float terrainFactor = 1.0; // 默认值
    if (currentSpeed > 0.05) // 只在移动时检测
    {
        // 缓存地形系数，避免每帧检测（每 0.5 秒更新一次）
        static float cachedTerrainFactor = 1.0;
        static float lastTerrainCheckTime = 0.0;
        
        float currentTime = GetGame().GetWorld().GetWorldTime();
        if (currentTime - lastTerrainCheckTime > 0.5) // 每 0.5 秒检测一次
        {
            cachedTerrainFactor = TerrainFactorSystem.GetTerrainFactor(owner);
            lastTerrainCheckTime = currentTime;
        }
        
        terrainFactor = cachedTerrainFactor;
    }
    
    // 使用地形系数计算 Pandolf 能量消耗
    float baseDrainRateByVelocity = RealisticStaminaSpeedSystem.CalculatePandolfEnergyExpenditure(
        currentSpeed, 
        currentWeight, 
        gradePercent,
        terrainFactor  // 传入地形系数
    );
    
    // ... 其余代码 ...
}
```

---

## ⚡ 性能优化建议

### 1. 缓存地形系数
- 每 0.5-1.0 秒检测一次，而不是每帧
- 仅在角色移动时检测
- 检测到材质变化时立即更新

### 2. 减少射线追踪开销
- 射线长度限制在 0.5 米（角色脚下）
- 使用 `TraceFlags.WORLD | TraceFlags.ENTS`，避免追踪不必要的实体
- 在角色静止时跳过检测

### 3. 客户端/服务器端优化
- 地形系数可在客户端计算（不影响游戏性）
- 服务器端验证：仅验证消耗率，不验证地形系数

---

## 🧪 测试与校准

### 测试步骤

1. **基准测试**（铺装路面）
   - 测量在 `terrainFactor = 1.0` 时的消耗率
   - 作为其他地形的参考基准

2. **材质密度测试**
   - 在不同材质上行走，记录 `BallisticInfo.GetDensity()` 值
   - 建立密度值到地形类型的映射表

3. **消耗率验证**
   - 在相同速度、负重、坡度下，比较不同地形的消耗率
   - 验证是否符合预期（草地 +20%，沙地 +80% 等）

4. **游戏性平衡**
   - 测试玩家在不同地形上的体验
   - 调整系数，确保游戏性合理（不会过于困难）

---

## 📝 示例：完整的地形系数实现

```enforce
// 文件：scripts/Game/Components/Stamina/SCR_RealisticStaminaSystem.c

// 在 RealisticStaminaSpeedSystem 类中添加：

// 地形系数常量
static const float TERRAIN_FACTOR_PAVED = 1.0;
static const float TERRAIN_FACTOR_DIRT = 1.1;
static const float TERRAIN_FACTOR_GRASS = 1.2;
static const float TERRAIN_FACTOR_BRUSH = 1.5;
static const float TERRAIN_FACTOR_SAND = 1.8;
static const float TERRAIN_FACTOR_SNOW = 2.1;
static const float TERRAIN_FACTOR_MUD = 2.5;

// 获取地形系数（基于地面材质）
static float GetTerrainFactor(IEntity character)
{
    if (!character)
        return TERRAIN_FACTOR_PAVED;
    
    TraceParam paramGround = new TraceParam();
    paramGround.Start = character.GetOrigin() + (vector.Up * 0.1);
    paramGround.End = paramGround.Start - (vector.Up * 0.5);
    paramGround.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
    paramGround.Exclude = character;
    paramGround.LayerMask = EPhysicsLayerPresets.Projectile;
    
    character.GetWorld().TraceMove(paramGround, FilterTerrainCallback);
    
    GameMaterial material = paramGround.SurfaceProps;
    if (!material)
        return TERRAIN_FACTOR_PAVED;
    
    BallisticInfo ballisticInfo = material.GetBallisticInfo();
    if (!ballisticInfo)
        return TERRAIN_FACTOR_PAVED;
    
    float density = ballisticInfo.GetDensity();
    
    // 基于密度的映射（需要根据实际测试校准）
    if (density <= 0.5) return TERRAIN_FACTOR_PAVED;
    if (density <= 1.0) return TERRAIN_FACTOR_DIRT;
    if (density <= 1.2) return TERRAIN_FACTOR_GRASS;
    if (density <= 1.5) return TERRAIN_FACTOR_BRUSH;
    if (density <= 1.8) return TERRAIN_FACTOR_SAND;
    if (density <= 2.1) return TERRAIN_FACTOR_SNOW;
    return TERRAIN_FACTOR_MUD;
}

// 过滤回调
static bool FilterTerrainCallback(IEntity e)
{
    if (ChimeraCharacter.Cast(e))
        return false;
    return true;
}

// 修改 CalculatePandolfEnergyExpenditure，添加地形系数参数
static float CalculatePandolfEnergyExpenditure(
    float velocity, 
    float currentWeight, 
    float gradePercent = 0.0,
    float terrainFactor = 1.0)  // 新增
{
    // ... 现有代码 ...
    
    // 应用地形系数到移动项
    float baseTerm = PANDOLF_BASE_COEFF + (PANDOLF_VELOCITY_COEFF * velocitySquaredTerm);
    baseTerm = baseTerm * terrainFactor;
    
    float gradeTerm = gradeDecimal * (PANDOLF_GRADE_BASE_COEFF + (PANDOLF_GRADE_VELOCITY_COEFF * velocitySquared));
    gradeTerm = gradeTerm * terrainFactor;
    
    // ... 其余代码 ...
}
```

---

## 📚 参考资源

- **Pandolf 模型**: Pandolf et al. (1977) - Energy expenditure prediction models
- **地形系数研究**: Knapik et al. (1996) - Load carriage and terrain effects
- **代码示例**: `SCR_RecoilForceAimModifier` - Arma Reforger 官方代码

---

**最后更新**: 2026-01-19
**作者**: ViVi141
