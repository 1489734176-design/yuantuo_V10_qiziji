# 模式和掉电记忆 ARM 回归

在仓库根目录执行：

```powershell
& 'D:\Program Files\Python314\python.exe' 'tests/tool_mode/run_tests.py'
```

测试使用安装的 Arm Compiler 6 编译实际 `tool_mode.c`、`flash_data_save.c`，抽取实际用户初始化/目标处理和 `pwm_set` 函数，并在 Unicorn Cortex-M0 中运行。TM1650键盘应答和Flash物理写擦由模拟硬件替代，Flash模拟保持仅允许1→0编程的约束。构建输出位于 `.planning/2026-11-04-tm1650/control-tests/`。

覆盖双模式切换和独立档位、按下/释放三样本去抖、按住不重复、ACK失败保持锁定、未知/组合键、各档扳机端点与中间值、真实PWM斜坡、安全保存门控、半写/CRC损坏回退、双页51槽换页和擦写失败、版本/字段范围拒绝、序号回绕与旧三半字记录迁移。

`--baseline` 对任务开始时的 `original` 快照运行40%默认上限断言，预期失败，证明旧固件缺少新行为。此回归不能代替目标板验证。
