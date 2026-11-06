# 调速累加灯与换向沿用当前档位

## Requirements
- 调速1～4档灯分别为LED8、LED8+7、LED8+7+6、LED8+7+6+5。继续从parameter.h读取灯号，覆盖配置按定义顺序累加。
- 真实换向时若来源/目标方向模式都为调速，将来源当前speed_gear赋给目标。涉及情景不复制档位。
- 目标speed_gear有改变才标记待保存，沿用现有停机Flash保存。源方向的设置保持其当前值。
- 首次上电是恢复开关所在方向的记忆，不能将默认active_direction=正转误当作换向；通过独立restore接口选择记忆，真实换向接口应用新规则。

## Implementation
- USER/led.c：调速使用位或累加前gear个灯号；情景继续单灯。
- USER/inc/parameter.h：配置说明改为调速累加灯。
- USER/tool_mode.c、USER/inc/tool_mode.h：增加首次上电restore接口；set_direction在双调速时复制档位，并在改变时标记save_pending。
- USER/user_control.c：首次方向提交调用restore，后续方向提交调用set_direction；停机/松扳机门控沿用。
- tests：验证默认/覆盖LED映射、正反双向和四种模式组合、换向后PWM档位目标/保存、上电恢复、真实方向门控与TM1650显示。

## Execution
- root写生产源码、主模式测试、计划和构建；speed_led_tests只写分配的显示/集成测试。
- 本轮开始源码快照保存在speed-led-original，保留前一轮所有源码改动及工作区用户Keil状态。
- 每次生产改变后完整重编译，并记录未执行实板验证。
