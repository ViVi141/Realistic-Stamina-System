// Swimming Stamina Model - 3D physics-based swimming energy expenditure
// Extracted from SCR_RealisticStaminaSystem.c (v3.22.6 split)
//
// Physics model: F_d = 0.5 * rho * v^2 * C_d * A
// References:
//   Holmer, I. (1974). Energy cost of arm stroke, leg kick, and whole crawl stroke.
//   Pendergast, D. R., et al. (1977). Energy cost of swimming.
//

class SCR_RSS_SwimmingStaminaModel
{
    // ==================== 游泳体力消耗计算（物理阻力模型 - 优化版）====================
    // 基于物理阻力模型：F_d = 0.5 * ρ * v² * C_d * A
    // 生理学依据：游泳的能量消耗远高于陆地移动（约3-5倍）
    // 参考：Holmér, I. (1974). Energy cost of arm stroke, leg kick, and whole crawl stroke.
    //       Pendergast, D. R., et al. (1977). Energy cost of swimming.
    //
    // 优化内容：
    // 1. 降低基础功率：从50W降至25W（更高效的生存踩水）
    // 2. 提高负重门槛：从20kg提高到25kg（20kg是标准战斗装具）
    // 3. 使用幂函数惩罚曲线：让轻度负重时更轻松，极端负重时才迅速崩溃
    // 4. 低强度踩水折扣：对于极低强度的踩水，引入"有氧代谢效率"折扣
    //
    // @param velocity 当前游泳速度（m/s）
    // @param currentWeight 当前总重量（kg），包括身体重量和负重
    // @return 游泳体力消耗率（%/s，每0.2秒的消耗率需要乘以 0.2）
    static float CalculateSwimmingStaminaDrain(float velocity, float currentWeight)
    {
        // 确保速度和重量有效
        velocity = Math.Max(velocity, 0.0);
        currentWeight = Math.Max(currentWeight, 0.0);
        
        float bodyWeight = SCR_RSS_Constants.CHARACTER_WEIGHT; // 90kg
        float effectiveWeight = Math.Max(currentWeight - bodyWeight, 0.0); // 有效负重（去除身体重量）
        
        // ==================== 1. 静态消耗优化（踩水维持）====================
        // 将基础功率降低，模拟更高效的生存踩水
        float baseMaintenancePower = SCR_RSS_Constants.SWIMMING_BASE_POWER; // 25W（从50W降至25W）
        
        float staticMultiplier = 1.0;
        // 使用 25kg 作为硬核负重的起点
        float threshold = SCR_RSS_Constants.SWIMMING_ENCUMBRANCE_THRESHOLD; // 25kg
        
        if (effectiveWeight > threshold)
        {
            // 只有超过 threshold 之后才开始指数级增加
            // 模拟：负重越大，为了维持头部在水面上所需的功率呈非线性上升
            float loadFactor = (effectiveWeight - threshold) / (SCR_RSS_Constants.SWIMMING_FULL_PENALTY_WEIGHT - threshold);
            loadFactor = Math.Clamp(loadFactor, 0.0, 1.0);
            
            // 使用平方律：让 25-30kg 之间依然相对温和，35kg之后才真正痛苦
            const float powerExponent = 2.0; // 平方律
            staticMultiplier = 1.0 + (Math.Pow(loadFactor, powerExponent) * (SCR_RSS_Constants.SWIMMING_STATIC_DRAIN_MULTIPLIER - 1.0));
        }
        
        float staticPower = baseMaintenancePower * staticMultiplier;
        
        // ==================== 2. 动态消耗（v³ 阻尼）====================
        // 修正：物理公式计算的是机械功，人体转化效率极低（约 3-5%）
        // 实际上游泳 1.5m/s 的代谢消耗大约在 800W-1000W 左右
        float dynamicPower = 0.0;
        if (velocity > SCR_RSS_Constants.SWIMMING_MIN_SPEED)
        {
            float vClamped = Math.Clamp(velocity, 0.0, 7.0);
            float velocityCubed = vClamped * vClamped * vClamped; // v³
            dynamicPower = 0.5 * SCR_RSS_Constants.SWIMMING_WATER_DENSITY * velocityCubed * 
                          SCR_RSS_Constants.SWIMMING_DRAG_COEFFICIENT * SCR_RSS_Constants.SWIMMING_FRONTAL_AREA;
            
            // 增加效率因子：人体游泳效率大约只有 0.05，所以消耗要除以 0.05 (即乘以 20)
            // 但为了游戏性平衡，我们取一个折中值
            dynamicPower = dynamicPower * SCR_RSS_Constants.SWIMMING_DYNAMIC_POWER_EFFICIENCY;
            
            // 动态阻力随负重的修正（士兵体积变大）
            float dragWeightBonus = 1.0 + (effectiveWeight / bodyWeight * 0.3);
            dragWeightBonus = Math.Clamp(dragWeightBonus, 1.0, 1.3);
            dynamicPower = dynamicPower * dragWeightBonus;
        }
        
        // ==================== 3. 转换与保护 ====================
        // 增加“生存压力”常数：避免水中静止被恢复逻辑覆盖
        float totalPower = staticPower + dynamicPower + SCR_RSS_Constants.SWIMMING_SURVIVAL_STRESS_POWER;
        
        // 生理功率上限：防止异常速度/模组冲突导致 v³ 产生天文数字并在后续乘法中溢出
        totalPower = Math.Clamp(totalPower, 0.0, SCR_RSS_Constants.SWIMMING_MAX_TOTAL_POWER);
        
        // ==================== 转换为体力消耗率 ====================
        // 直接使用小数系数，不要乘以 100（这是关键修复）
        // ENERGY_TO_STAMINA_COEFF 本身就是为了将功率转换为 0.0-1.0 这种小数比例而设计的
        float swimmingDrainRate = totalPower * SCR_RSS_Constants.SWIMMING_ENERGY_TO_STAMINA_COEFF;
        
        // ==================== 4. 低强度折扣（survival floating）====================
        // 关键修正：对于极低强度的踩水（静态功率），引入"有氧代谢效率"
        // 模拟士兵在水中通过深呼吸调整，不完全依赖消耗体力槽
        if (velocity < SCR_RSS_Constants.SWIMMING_LOW_INTENSITY_VELOCITY && effectiveWeight < threshold)
        {
            swimmingDrainRate = swimmingDrainRate * SCR_RSS_Constants.SWIMMING_LOW_INTENSITY_DISCOUNT; // 浮水且负重轻时，额外给予30%的消耗折扣
        }
        
        // 限制每秒最大消耗：12% 每秒是非常恐怖的（8秒淹死），适合极限冲刺
        swimmingDrainRate = Math.Clamp(swimmingDrainRate, 0.0, 0.12);
        
        return swimmingDrainRate;
    }

    // ==================== 游泳体力消耗计算（3D 模型：水平 + 垂直）====================
    // 目标：考虑方向/垂直运动（上浮/下潜）对体力的额外消耗
    //
    // P_total = P_static + P_horizontal + P_vertical (+ survival stress)
    // - P_static：维持呼吸/浮水的基础功率，随负重非线性增长
    // - P_horizontal：水平阻力功率 ~ v_xz^3
    // - P_vertical：垂直方向（上浮/下潜）更昂贵
    //
    // @param velocityVector 速度向量（m/s），包含垂直分量
    // @param currentWeight 当前总重量（kg）
    // @return 游泳体力消耗率（0.0-1.0 / s）
    static float CalculateSwimmingStaminaDrain3D(vector velocityVector, float currentWeight)
    {
        currentWeight = Math.Max(currentWeight, 0.0);
        
        // 分解速度
        float vH = Math.Sqrt((velocityVector[0] * velocityVector[0]) + (velocityVector[2] * velocityVector[2])); // 水平速度（XZ）
        float vY = velocityVector[1]; // 垂直速度（Y）
        vH = Math.Max(vH, 0.0);
        
        float bodyWeight = SCR_RSS_Constants.CHARACTER_WEIGHT; // 90kg
        float effectiveWeight = Math.Max(currentWeight - bodyWeight, 0.0); // 有效负重
        
        // ==================== A. 静态/生存功率 ====================
        float baseMaintenancePower = SCR_RSS_Constants.SWIMMING_BASE_POWER; // 25W
        float threshold = SCR_RSS_Constants.SWIMMING_ENCUMBRANCE_THRESHOLD; // 25kg
        
        float staticMultiplier = 1.0;
        if (effectiveWeight > threshold)
        {
            float loadFactor = (effectiveWeight - threshold) / (SCR_RSS_Constants.SWIMMING_FULL_PENALTY_WEIGHT - threshold);
            loadFactor = Math.Clamp(loadFactor, 0.0, 1.0);
            staticMultiplier = 1.0 + (Math.Pow(loadFactor, 2.0) * (SCR_RSS_Constants.SWIMMING_STATIC_DRAIN_MULTIPLIER - 1.0));
        }
        float staticPower = baseMaintenancePower * staticMultiplier;
        
        // ==================== B. 水平阻力功率（v^3）====================
        // [修复 fix2.md #2] 使用合速度计算阻力，避免分量立方的和 ≠ 合矢量模长的立方
        float horizontalPower = 0.0;
        if (vH > SCR_RSS_Constants.SWIMMING_MIN_SPEED)
        {
            // [修复] 计算合速度（包括垂直分量），然后应用阻力公式
            // 物理原理：P_total ∝ |v_total|^3 = (sqrt(v_x^2 + v_y^2))^3
            // 而不是：|v_x|^3 + |v_y|^3
            float vTotal = Math.Sqrt(vH * vH + vY * vY);
            // CRITICAL FIX: Clamp vTotal before cubing to prevent overflow.
            // Unclamped 7.0m/s gives 42,875W which exceeds SWIMMING_MAX_TOTAL_POWER (2000)
            // but the intermediate multiplication may overflow on some platforms.
            vTotal = Math.Clamp(vTotal, 0.0, 7.0);
            float vTotalCubed = vTotal * vTotal * vTotal; // 合速度的立方

            // 使用合速度计算阻力功率
            horizontalPower = 0.5 * SCR_RSS_Constants.SWIMMING_WATER_DENSITY * vTotalCubed *
                              SCR_RSS_Constants.SWIMMING_DRAG_COEFFICIENT * SCR_RSS_Constants.SWIMMING_FRONTAL_AREA;
            
            // 人体效率/游戏平衡折中
            horizontalPower = horizontalPower * SCR_RSS_Constants.SWIMMING_DYNAMIC_POWER_EFFICIENCY;
            
            // 负重增加阻力（迎水面积/阻力系数增加）
            float dragWeightBonus = 1.0 + ((effectiveWeight / bodyWeight) * 0.4);
            dragWeightBonus = Math.Clamp(dragWeightBonus, 1.0, 1.4);
            horizontalPower = horizontalPower * dragWeightBonus;
        }
        
        // ==================== C. 垂直功率（上浮/下潜）====================
        float verticalPower = 0.0;
        float absVY = Math.AbsFloat(vY);
        if (absVY > SCR_RSS_Constants.SWIMMING_VERTICAL_SPEED_THRESHOLD)
        {
            // 垂直阻力功率：P = 0.5 * rho * |v|^3 * Cd * A
            float vYCubedAbs = absVY * absVY * absVY;
            float verticalDragPower = 0.5 * SCR_RSS_Constants.SWIMMING_WATER_DENSITY * vYCubedAbs *
                                      SCR_RSS_Constants.SWIMMING_VERTICAL_DRAG_COEFFICIENT * SCR_RSS_Constants.SWIMMING_VERTICAL_FRONTAL_AREA;
            
            if (vY > 0.0)
            {
                // 上浮：负重越大越困难（负浮力/装备重力项更明显）
                // - baseline：不背负也需要一定功率做“上浮动作/姿态调整”
                // - loadTerm：随负重增加而非线性上升（这里用线性+系数，强度交给 multiplier 调整）
                float baseUpForce = bodyWeight * 9.81 * SCR_RSS_Constants.SWIMMING_VERTICAL_UP_BASE_BODY_FORCE_COEFF;
                float loadUpForce = effectiveWeight * 9.81 * SCR_RSS_Constants.SWIMMING_EFFECTIVE_GRAVITY_COEFF;
                float upPower = (baseUpForce + loadUpForce) * vY;
                verticalPower = (upPower + verticalDragPower) * SCR_RSS_Constants.SWIMMING_VERTICAL_UP_MULTIPLIER;
            }
            else
            {
                // 下潜：对抗浮力 + 垂直阻力
                // 负重会“抵消”一部分浮力需求：负重越大，下潜越容易
                float buoyancyForce = bodyWeight * 9.81 * SCR_RSS_Constants.SWIMMING_BUOYANCY_FORCE_COEFF;
                float loadReliefForce = effectiveWeight * 9.81 * SCR_RSS_Constants.SWIMMING_VERTICAL_DOWN_LOAD_RELIEF_COEFF;
                loadReliefForce = Math.Clamp(loadReliefForce, 0.0, buoyancyForce);
                
                float netBuoyancyForce = buoyancyForce - loadReliefForce;
                float downPower = netBuoyancyForce * absVY;
                verticalPower = (downPower + verticalDragPower) * SCR_RSS_Constants.SWIMMING_VERTICAL_DOWN_MULTIPLIER;
            }
        }
        
        // ==================== D. 总功率 + 保护 ====================
        float totalPower = staticPower + horizontalPower + verticalPower + SCR_RSS_Constants.SWIMMING_SURVIVAL_STRESS_POWER;
        totalPower = Math.Clamp(totalPower, 0.0, SCR_RSS_Constants.SWIMMING_MAX_TOTAL_POWER);
        
        // 转换为体力消耗率（0.0-1.0 / s）
        float drainRate = totalPower * SCR_RSS_Constants.SWIMMING_ENERGY_TO_STAMINA_COEFF;
        
        // 低强度折扣：几乎不动且负重轻时
        if (vH < SCR_RSS_Constants.SWIMMING_LOW_INTENSITY_VELOCITY && absVY < 0.1 && effectiveWeight < threshold)
            drainRate = drainRate * SCR_RSS_Constants.SWIMMING_LOW_INTENSITY_DISCOUNT;
        
        return Math.Clamp(drainRate, 0.0, SCR_RSS_Constants.SWIMMING_MAX_DRAIN_RATE);
    }
}