# 调速累计灯显和双调速换向回归结果

日期：2026-11-05。

## 范围

- `tests/tool_mode/test_mode_config.c`：默认调速掩码改为 `0x40/0x60/0x70/0x78`，原覆盖灯号配置的期望改为 `0x08/0x48/0x58/0x78`；情景单灯、占空比和反转保持时间回归保留。
- `tests/seven_led/test_seven_led.c`：增加真实方向门控的调速3档正转到反转交接、反转4档到正转交接及累计灯显检查；运行中方向未提交时保留原显示和目标记忆。
- `tests/tm1650/run_mode_integration.py`：更新累计灯显及原换向用例，并增加双调速交接和涉及情景的三种换向组合；每次显示同时检查真实TM1650显存和边沿模型接收到的数据。
- `tests/tool_mode/README.md`：同步当前26组主模式回归、累计灯显、首次方向恢复及5组TM1650集成回归说明。

## 红测

生产LED代码修改前分别运行：

```powershell
& '.planning/2026-11-05-reverse-scene-memory/run_mode_config_tests.ps1' -TaskVariants default
& '.planning/2026-11-05-reverse-scene-memory/run_mode_config_tests.ps1' -TaskVariants override
```

两组均成功编译，并以退出码1在 `case 2, test_mode_config.c:61, after 5 checks` 失败。失败断言为2档累计灯显检查：默认期望 `0x60`，覆盖配置期望 `0x48`。与原代码仅亮本档单灯的行为一致。

## 最终绿测

生产修改完成后运行：

```powershell
& '.planning/2026-11-05-reverse-scene-memory/run_mode_config_tests.ps1'
& 'D:/Program Files/Python314/python.exe' 'tests/seven_led/run_tests.py'
& 'D:/Program Files/Python314/python.exe' 'tests/tm1650/run_mode_integration.py'
```

全部退出码0，编译输出无警告。

| 回归 | 结果 |
| --- | --- |
| 默认参数 | 8 cases，30 checks |
| 覆盖参数 | 8 cases，30 checks |
| 七灯主场景（含新增case9） | 11928 checks |
| 4组上电电量 | 1、1、1、2 checks |
| SWD上电配置 | 6 checks |
| 电量5秒保持/重扣 | 203004 checks |
| 松扳机15秒休眠/唤醒 | 45009 checks |
| 故障5秒显示/15秒休眠 | 145203 checks |
| 七灯总计 | 405155 checks |
| TM1650真实代码/边沿模型 | 5/5用例通过 |

TM1650通过用例：

- `test_physical_k3_to_scene_display`
- `test_physical_k2_speed_order_and_mode_memory`
- `test_physical_keys_inhibited_by_trigger`
- `test_physical_reverse_k3_led5_and_direction_memory`
- `test_physical_speed_direction_handoff_and_scene_memory`

配置及TM1650测试使用 `-Wall -Wextra -Werror`。本分工未执行正式Keil工程全量重建，未连接目标板；以上为实际ARM C代码的模拟硬件验证。
