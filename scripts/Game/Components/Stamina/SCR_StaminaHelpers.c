// 体力系统辅助函数模块
// 存放所有体力系统相关的辅助函数和工具函数
// 模块化拆分：从 SCR_RealisticStaminaSystem.c 提取的辅助函数

class StaminaHelpers
{
    // ==================== 数学工具函数 ====================

    // 精确计算幂函数：base^exponent
    // [修复 fix2.md #3] 直接使用引擎的 Math.Pow 以保证数值准确性
    // 之前的自定义近似方法误差较大（例如计算 10^0.6 误差可达14%）
    //
    // @param base 底数（必须 > 0）
    // @param exponent 指数
    // @return base^exponent（精确值）
    static float Pow(float base, float exponent)
    {
        // 处理特殊情况
        if (base <= 0.0)
            return 0.0;

        if (exponent == 0.0)
            return 1.0;

        if (exponent == 1.0)
            return base;

        // [修复] 直接使用 Math.Pow，保证数值准确性
        // 既然代码中对 exponent >= 1.0 的情况已经使用了 Math.Pow，说明性能不是绝对瓶颈
        return Math.Pow(base, exponent);
    }
}
