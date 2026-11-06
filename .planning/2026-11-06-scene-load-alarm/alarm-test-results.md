# 报警不受电量 5 秒窗口限制：实现与验证

> 本文为上一阶段历史记录。用户后续明确要求按住扳机持续循环，现已取消10轮倒计数；最终实现、验证和固件见continuous-alarm.md。

## 根因与最小修改

- `USER/led.c` 中 `UO_Led_Handler()` 使用 `battery_display_en && LedError_Control() && trigger` 选择报警分支。电量窗口到期后不再调用故障控制，且进入普通电量分支清零 `LedDeal` / `LedRunTimes`，因此正在闪烁的报警被截断，后到故障也无法启动。
- 仅将条件恢复为 `LedError_Control() && trigger`，普通电量仍由 `LedSoc_Run(..., battery_display_en, ...)` 的 5 秒窗口控制。
- 更新 `UO_Led_Handler()` 和 `LedError_Run()` 中文函数头注释。故障优先级、报码灯数、闪烁节奏、轮数、扳机条件和休眠条件均未修改。
- 用 `git show 26d69ea:USER/led.c` 核对了添加 5 秒功能前的实现：旧版调用条件也是 `LedError_Control() && trigger`，两个故障函数的逻辑与本次修改前完全相同。

## 原轮数语义

- `LedError_Run()` 本身按 `LedRunTimes` 递减，单次装载的 10 轮耗尽后继续调用仍熄灭。
- 故障仍持续且扳机保持扣住时，原 `LedError_Control()` 会在轮数为 0 时重新装载 10 轮；本次原样保留这项既有行为，没有改变计数或添加无限循环。
- 测试验证单独的 `LedError_Run()` 完成 13 闪 × 10 轮：130 个亮起沿、总计 106140 次 1 ms 调用；耗尽后额外 5001 次调用均保持熄灭。

## 红测

两次均在修改生产代码前运行同一命令：

```powershell
& 'D:\Program Files\Python314\python.exe' tests/seven_led/run_tests.py
```

1. 先执行持续故障场景：`AssertionError: case 8, test_seven_led.c:384, after 20005 checks`。第 5001 ms 本应仍处在原闪烁亮灯段，实际被电量超时关闭。
2. 将新故障场景排到该场景组前再次运行：`AssertionError: case 8, test_seven_led.c:421, after 2 checks`。普通电量熄灭后置位硬件过流与 MOS 温度故障，原代码跳过故障选择，报码未启动。

上述行号为当时红测文件的行号。其他原有场景均通过。

## 最终绿测

最小修复后及补足亮灯时松扳机的检查后，完整重跑上述命令，退出码 0；ARM 子测试编译和链接无警告输出。

```text
PASS: 9 cases, 112455 checks (ARM C / simulated GPIO, not a bench test)
PASS: SOC boot profile 1, 1 checks (ARM C / simulated GPIO, not a bench test)
PASS: SOC boot profile 2, 1 checks (ARM C / simulated GPIO, not a bench test)
PASS: SOC boot profile 3, 1 checks (ARM C / simulated GPIO, not a bench test)
PASS: SOC boot profile 4, 2 checks (ARM C / simulated GPIO, not a bench test)
PASS: SWD startup profile, 6 checks (ARM C / simulated GPIO, not a bench test)
PASS: battery 5s hold/repress, 203004 checks (ARM C / simulated GPIO, not a bench test)
PASS: release 15s sleep/wake, 45009 checks (ARM C / simulated GPIO, not a bench test)
PASS: fault independent of 5s window/15s sleep, 180782 checks (ARM C / simulated GPIO, not a bench test)
```

新增/加强的行为验证：

- MOS 温度故障的第二轮第 2 闪在 5000、5001、5006 ms 保持亮，5007 ms 灭，5507 ms 第 3 闪亮；窗口边界不截断也不重置节奏。
- 3 闪 × 10 轮合计 36040 ms，30 个亮起沿，其中 25 个发生在 5 秒后；轮数正常递减至 0。
- 原控制函数随后重新装载 10 轮；在下一轮亮灯段松扳机立即停止报码，松手 15 秒休眠，故障保持位不被清除。
- 5 秒以后新出现的故障仍起闪；硬件过流与 MOS 温度故障同时出现时仍按硬件过流的 1 闪码优先显示。
- 故障清除后普通电量仍保持 5 秒到期状态，普通电量长按 66000 ms 不回亮，重新扣扳机可再次显示 5 秒。

## 文件与边界

- 修改生产源：`USER/led.c`（1 处条件和 2 处函数头注释）。
- 修改测试：`tests/seven_led/test_seven_led.c`。
- 修改运行器输出目录和场景描述：`tests/seven_led/run_tests.py`；输出在本任务 `board-tests/` 中。
- `git diff --check -- USER/led.c tests/seven_led/test_seven_led.c tests/seven_led/run_tests.py` 退出码 0。Git 仅提示运行器的 LF 将按仓库设置转换为 CRLF。
- 本子任务未运行正式 Keil clean rebuild，未烧录、未进行实板验证；整项目重建由主代理统一执行。
