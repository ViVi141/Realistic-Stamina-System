// SCR_StaminaHelpers.c — 体力系统数学辅助
// 从 SCR_RealisticStaminaSystem.c 拆分

class StaminaHelpers
{
    // 精确计算幂函数：base^exponent
    // 直接使用引擎的 Math.Pow 以保证数值准确性
    static float Pow(float base, float exponent)
    {
        if (base <= 0.0)
            return 0.0;
        if (exponent == 0.0)
            return 1.0;
        if (exponent == 1.0)
            return base;
        return Math.Pow(base, exponent);
    }
}
