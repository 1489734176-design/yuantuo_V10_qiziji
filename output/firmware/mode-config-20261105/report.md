# 双模式灯号及占空比参数化 — 2026-11-05

配置集中在USER/inc/parameter.h的“双模式挡位配置”段。修改宏数值后重新编译即可。

| 模式/挡位 | 灯号宏 | 默认LED | 占空比宏 | 默认百分比 |
|---|---|---|---|---|
| 调速1 | TOOL_SPEED_GEAR1_LED | 8 | TOOL_SPEED_GEAR1_DUTY_PERCENT | 38 |
| 调速2 | TOOL_SPEED_GEAR2_LED | 7 | TOOL_SPEED_GEAR2_DUTY_PERCENT | 58 |
| 调速3 | TOOL_SPEED_GEAR3_LED | 6 | TOOL_SPEED_GEAR3_DUTY_PERCENT | 78 |
| 调速4 | TOOL_SPEED_GEAR4_LED | 5 | TOOL_SPEED_GEAR4_DUTY_PERCENT | 100 |
| 情景1 | TOOL_SCENE_GEAR1_LED | 8 | TOOL_SCENE_GEAR1_DUTY_PERCENT | 18 |
| 情景2 | TOOL_SCENE_GEAR2_LED | 6 | TOOL_SCENE_GEAR2_DUTY_PERCENT | 100 |
| 情景3 | TOOL_SCENE_GEAR3_LED | 5 | TOOL_SCENE_GEAR3_DUTY_PERCENT | 100 |

灯号填写原理图LED5～LED8编号，自动转换为TM1650显存位。调速为绿色DIG1，情景为红色DIG2。占空比填写15～100的整数百分比：调速是扳机连续调节的挡位上限，情景是每挡独立固定目标；实际输出仍受最低占空比、软启动及各项保护约束。错误配置范围会在编译时报错。

本轮保留用户当前18%的情景1挡设置。扳机按住时K2/K3屏蔽、松扳机后按键释放重按、电量5s窗口、松手15s休眠及各项电机保护均保留。

原始C/H已直接编辑，模式、灯光、Flash模块及修改过的用户控制代码使用汉语注释、函数头说明，已清理注释前缀。Flash模块仅修改注释。后续编辑规范已记录AGENTS.md。

## 验证

- 默认/覆盖宏配置两组真实ARM回归均通过，每组7挡25断言；覆盖配置改变所有灯号，调速为25/45/65/85%，情景为35/55/75%，确认七挡宏均实际生效。
- seven_led、tool_mode、TM1650、mode_integration及battery_com全部既有回归通过。
- 独立只读审查未发现可操作正确性问题。
- Keil Arm Compiler 6.22全量重编译：0 Error(s), 0 Warning(s)。Code21648、RO224、RW72、ZI1552；BIN21944字节。
- HEX行校验、HEX/BIN数据一致、Flash参数区域边界及交付副本SHA256一致均验证通过。

未烧录或实板测试。实板需记录板版本、供电、负载及参数集，验证灯序、各挡输出、按键屏蔽、5s/15s计时及保护行为。

交付包括HEX/BIN/AXF、rebuild.log和manifest.json。此前较长故障报码仍受5s电量灯窗口限制，保护处理及锁存不变。
