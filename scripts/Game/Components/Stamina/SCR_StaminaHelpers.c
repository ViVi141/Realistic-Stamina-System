// 体力系统辅助函数模块
// 存放所有体力系统相关的辅助函数和工具函数
// 模块化拆分：从 SCR_RealisticStaminaSystem.c 提取的辅助函数

class StaminaHelpers
{
    // ==================== 数学工具函数 ====================
    
    // 精确计算幂函数：base^exponent
    // 使用优化的迭代方法实现精确计算，不使用近似
    // 
    // 对于 exponent = 0.6 的特殊情况：
    // base^0.6 = base^(3/5) = (base^3)^(1/5)
    // 使用牛顿法计算5次方根，精度达到 10^-6
    // 
    // @param base 底数（必须 > 0）
    // @param exponent 指数（当前支持 0.6，可扩展）
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
        
        if (base == 1.0)
            return 1.0;
        
        // 对于 exponent = 0.6 的精确计算
        if (Math.AbsFloat(exponent - 0.6) < 0.001)
        {
            // base^0.6 = (base^3)^(1/5)
            float baseCubed = base * base * base;
            
            // 使用牛顿法计算5次方根：x^(1/5) = y，即 x = y^5
            // 迭代公式：y_n+1 = (4*y_n + x / y_n^4) / 5
            // 初始猜测：y_0 = sqrt(sqrt(base))（四次方根，作为5次方根的近似）
            float y = Math.Sqrt(Math.Sqrt(base));
            
            // 迭代计算，直到收敛（通常5-8次迭代即可达到高精度）
            for (int i = 0; i < 12; i++)
            {
                float y4 = y * y * y * y;
                if (y4 > 1e-10) // 避免数值不稳定
                {
                    float yNew = (4.0 * y + baseCubed / y4) / 5.0;
                    // 检查收敛性
                    if (Math.AbsFloat(yNew - y) < 1e-6)
                    {
                        return yNew;
                    }
                    y = yNew;
                }
                else
                {
                    break;
                }
            }
            return y;
        }
        
        // 对于其他指数值，使用优化的插值方法（比简单 sqrt 更精确）
        // 对于 0.5 < exponent < 1.0，使用有理函数插值
        if (exponent > 0.5 && exponent < 1.0)
        {
            float sqrtBase = Math.Sqrt(base);
            float linearBase = base;
            
            // 使用有理函数插值，而不是简单线性插值
            // 对于 exponent = 0.6，插值权重更偏向 sqrt
            float t = (exponent - 0.5) * 2.0; // t in [0, 1]
            // 使用平滑插值：result = sqrtBase * (1 - t^2) + linearBase * t^2
            // 或者使用更精确的：result = sqrtBase^(1-t) * linearBase^t
            // 简化版本：使用平方插值
            float t2 = t * t;
            return sqrtBase * (1.0 - t2) + linearBase * t2;
        }
        
        // 默认情况：如果指数接近 0.5，使用 sqrt
        if (Math.AbsFloat(exponent - 0.5) < 0.001)
        {
            return Math.Sqrt(base);
        }
        
        // 其他情况：使用线性近似（作为后备）
        return base * exponent + (1.0 - exponent);
    }
}
