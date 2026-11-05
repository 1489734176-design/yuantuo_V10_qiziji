# 扳机按键与灯光计时修改计划

> 按用户已明确给出的三项要求在当前会话继续实施。

**Goal:** 情景灯按LED7、LED6、LED5显示；扣住扳机时按键无效；每次重新扣动亮电量灯5s，松手后15s休眠。

**Architecture:** 保留前台1ms任务。模式键沿用三次10ms去抖，并在扣住扳机期间锁住按压，待松开扳机且按键确认释放后解锁。LED模块独立记录扳机上升沿和饱和5s计数；休眠仍由工具状态机管理。

**Tech Stack:** C、Keil Arm Compiler 6、已有Unicorn ARM回归。

**Spec:** 用户2026-11-05本轮三项修改；休眠沿用既有松手后计时逻辑，按住扳机保持唤醒。

## Global Constraints
- 保留所有电流、温度、电压、看门狗、堵转及方向保护。
- 保留本轮开始时38/58/78/100%调速参数；不烧录硬件。
- 修改处保留中文注释，构建输出隔离。

## Review Focus
- 扳机只扣动1ms时也要阻断未完成的按键去抖，松手后必须按键释放重按。
- TM1650读失败或未知键不能解锁被阻断的按压。
- 正常运行及故障按住超过15s保持供电；电量5s到期必须关闭且不溢出重新点亮。
- 松手后按键操作不能重启电量5s计时；扣动必须重置5s与15s。
- 休眠清屏、电源保持撤销及再次扣动唤醒不改变保护锁存。

### Task 1: 情景灯与灯光计时
**Files:** USER/led.c、USER/user_control.c、USER/inc/parameter.h；tests/seven_led。
- [x] 更新场景掩码为0x20/0x10/0x08，增加5s满时、继续扣住、松手未重扣、重扣重置及15s休眠边界断言，运行现有run_tests.py观察失败。
- [x] 增加电量上升沿窗口与饱和计数，SLEEP_TIME改15000，扳机按住清零睡眠计数，松手满15000进入休眠。
- [x] 运行seven_led回归通过，确认模式及PB9到休眠才关闭、故障锁存保留。

### Task 2: 扳机期间模式键屏蔽
**Files:** USER/tool_mode.c；tests/tool_mode、tests/tm1650。
- [x] 增加K2/K3扣住无效、跨松扳机按住无效、释放重按有效及未完成去抖被扣动打断的测试，运行观察失败。
- [x] tool_mode_key_handle每1ms根据gToolTrigger阻断并锁住事件；事件函数也检查扳机及POWER_DOWN，释放三次稳定采样后才解锁。
- [x] 运行模式和真实TM1650集成测试通过；同步旧测试对当前38/58/78/100%参数的期望。

### Task 3: 验证与交付
- [x] 运行seven_led、tool_mode、tm1650、mode_integration、battery_com全部回归，审查本轮差异。
- [x] 独立目录Keil -r完整重编译，确认0错误0警告。
- [x] 输出HEX/BIN和实板验证说明到output/firmware/trigger-led-timing-20261105。
