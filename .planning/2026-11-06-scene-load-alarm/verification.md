# 情景1档负载升速与报警窗口修复验证

> 本文记录上一阶段构建。后续报警改为明确的持续循环，新的报警说明及配套HEX/BIN见continuous-alarm.md；情景1档负载参数保持不变。

2026-11-06；相对基线 `20b0946d594849989f44374d314304de8b92a549`。

## 实现

直接编辑生产源 `USER/tool_mode.c`、`USER/inc/parameter.h`、`USER/led.c`，添加/更新中文函数头和参数注释，未使用Python脚本重写生产源。

- 正转情景1档正常目标18%；启动进入MOTOR_RUN后屏蔽100ms，校准平均电流达到3.5A阈值连续20ms后，目标升为100%。
- 用户测得空载约1.3A；卸载阈值设为2.0A，持续100ms后恢复18%。升降继续采用原pwm_set斜坡，目标与扣动深度无关。
- 运行许可、扳机、电机状态或任一实时/锁存故障不满足时，复位本次负载状态。切模式、挡位或方向也复位，不写入Flash运行进度。
- 报警去除battery_display_en门控；普通电量仍在5s熄灭。原报警优先级、节奏、轮数、持续故障重装10轮、松扳机及休眠条件均保留。
- 原反转情景100%保持850ms再降15%、独立模式记忆、双调速换向沿用当前档位、调速累计LED均通过原回归。

## 可调参数

所有配置位于 `USER/inc/parameter.h`。

| 宏 | 默认值 | 含义 |
| --- | --- | --- |
| TOOL_SCENE_GEAR1_DUTY_PERCENT | 18 | 正常目标占空比，% |
| TOOL_SCENE_GEAR1_BOOST_DUTY_PERCENT | 100 | 负载确认后目标占空比，% |
| TOOL_SCENE_GEAR1_LOAD_ON_MA | 3500 | 负载进入阈值，mA |
| TOOL_SCENE_GEAR1_LOAD_OFF_MA | 2000 | 卸载恢复阈值，mA |
| TOOL_SCENE_GEAR1_STARTUP_MASK_MS | 100 | 进入运行后的启动屏蔽，ms |
| TOOL_SCENE_GEAR1_LOAD_ON_MS | 20 | 连续负载确认，ms |
| TOOL_SCENE_GEAR1_LOAD_OFF_MS | 100 | 连续卸载确认，ms |

使用当前实际电流保护链：CH4原始值扣零偏，再乘VCC的Q15校准增益，得到IBusAvg_Adc。默认硬件参数为8.91891 ADC码/A，进入阈值向上取整为32码（约3.59A），退出阈值向下取整为17码（约1.91A）。实测1.3A约12码。模拟阈值精度受到ADC分辨率、CH4与用于零偏校准的CH5差异及模拟增益误差影响，需实板校准。

## 软件验证

1. 负载升速红测在实现前失败于用例27、当时test_tool_mode.c第742行，完成6项检查，捕获固定18%未升速。
2. `tests/tool_mode/run_tests.py`：真实ARM C仿真，31 cases / 41559 checks通过。涵盖阈值、迟滞、确认边界、启动屏蔽、重复调用不累计时间、8项故障/运行许可复位、模式方向范围，以及实际user_oc_handle扣零偏和增益校准。
3. `tests/seven_led/run_tests.py`：完整9个运行场景合计541261检查通过。覆盖跨5s保持原闪烁、5s后新故障启动、10轮计数耗尽及原重装语义、松扳机、15s休眠；详细红绿测证据见alarm-test-results.md。
4. `run_mode_config_tests.ps1`：默认12 cases / 522 checks，覆盖12 cases / 246 checks通过。覆盖参数为82%、4700/2600mA、37/11/43ms，独立期望为1536 PWM、进入42码/退出23码；保留LED/调速/反转覆盖测试。两套ARM编译均启用-Wall -Wextra -Werror，无警告输出。
5. 独立只读源码审查：未发现阻塞问题；确认整合仍经user_gears_handle→pwm_set及原保护链。
6. `git diff --check`通过；只出现Git行尾转换提示。

## 完整构建与固件

命令：`& '.planning\2026-11-06-scene-load-alarm\build.ps1'`。

Keil uVision使用AC6 6.22，对MM32SPIN0230执行 `-r` 全量重编译，退出码0，**0 Error(s), 0 Warning(s)**。

- Code=22584，RO=224，RW=72，ZI=1584。
- ROM合计22880字节，位于0x08000000开始、0x7800大小的项目代码区域内。
- RW+ZI为1656字节，位于0x1000大小的RAM区域内。
- 构建日志：`build/rebuild.log`。
- HEX：`build/Objects/SPIN0230_DEMO.hex`。
- BIN：`build/SPIN0230_DEMO.bin`。
- HEX SHA256：`5D9A5C7716AC3980F26C923A2905A9908C52D199FF6F38773D8F61225242641F`。
- BIN SHA256：`D69B90EA1690501EB93970D6E782A796F5C25B84729BAA3531393598985FF4F0`。

临时验证工程沿用原项目编译配置，将生成物隔离到当前任务目录，构建后删除临时uvprojx，没有覆盖原KEIL_PRJ/Objects及Listngs输出。

## 实板验证状态

本次没有连接实板，未烧录，不能宣称实测效果已与样机完全一致。已知信息仅为用户反馈空载1.3A；板号/硬件版本、供电电压和负载条件尚未记录。

建议后续按本节默认参数记录：硬件版本、供电电压、螺丝/钻头及材料负载、平均电流与实际PWM波形。确认启动不误升速、持续加载升至100%、保持扳机卸载后恢复18%、重复加载正常，以及松扳机、换向和原过流/温度/欠压/堵转保护按预期生效。报警持续超过5s和故障延迟出现也需目视核对。
