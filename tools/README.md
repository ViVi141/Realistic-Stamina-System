# 开发工具

本目录包含用于参数优化、系统模拟和趋势图生成的 Python 脚本。

## 脚本说明

### 参数优化脚本

- **`optimize_2miles_params.py`** - 2英里参数优化脚本
  - 自动搜索最佳参数组合，使2英里能在15分27秒内完成
  - 输出优化后的参数值

- **`test_2miles.py`** - 2英里测试脚本
  - 验证参数优化结果
  - 测试完成时间和平均速度

### 趋势图生成脚本

- **`simulate_stamina_system.py`** - 基础趋势图生成脚本
  - 生成体力、速度、速度倍数随时间变化的趋势图
  - 输出 `stamina_system_trends.png`

- **`generate_comprehensive_trends.py`** - 综合趋势图生成脚本
  - 生成包含2英里测试、不同负重对比、恢复速度分析的综合图表
  - 输出 `comprehensive_stamina_trends.png`

### 其他工具脚本

- **`calculate_recovery_time.py`** - 恢复时间计算脚本
  - 计算体力恢复时间
  - 验证恢复速度是否符合预期

## 使用方法

### 运行参数优化

```bash
python optimize_2miles_params.py
```

### 生成趋势图

```bash
# 基础趋势图
python simulate_stamina_system.py

# 综合趋势图
python generate_comprehensive_trends.py
```

### 测试2英里

```bash
python test_2miles.py
```

## 依赖

- Python 3.x
- numpy
- matplotlib

安装依赖：

```bash
pip install numpy matplotlib
```

## 注意事项

- 脚本中的参数应与 `scripts/Game/` 目录中的 C 代码保持一致
- 生成的图表保存在当前目录
- 参数优化可能需要几分钟时间
