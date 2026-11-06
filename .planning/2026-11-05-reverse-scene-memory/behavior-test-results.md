# 独立行为测试结果

所有生产C/H由主代理编辑；本代理仅修改tests和本任务下测试脚本及生成结果。使用安装的Arm Compiler 6和Unicorn执行实际ARM C代码，没有启动Keil正式构建，也没有执行实板测试。

## 红测

实现前执行：

```powershell
& 'D:\Program Files\Python314\python.exe' 'tests/tool_mode/run_tests.py'
```

编译通过，行为失败：`AssertionError: mode/storage case 13, test_tool_mode.c:86, after 1 checks`。夹具设置反转方向并按K3，期望反转情景开始目标1874计数；旧代码仍选正转情景1，实际337计数。首个新增断言因此可靠地捕捉缺失行为。红测曾使用旧runner输出目录，主代理已恢复其两份生成物；所有当前runner输出已迁至本任务目录。

## 绿测

`tests/tool_mode/run_tests.py`：最终主代理复跑PASS，23 cases，39549 checks。涵盖正转原行为、反转K3 toggle、850ms边界/前台丢帧/保护降占空比/运行许可和方向模式复位、实际PWM双斜坡、双方向记忆及v1/v2 Flash兼容、独立CRC与写回中断。case22/23先红后绿，确认PWM更新处首次满值捕获能处理前台延迟和短峰回落。

`.planning/2026-11-05-reverse-scene-memory/run_mode_config_tests.ps1`：默认和覆盖配置各PASS，8 cases，30 checks。覆盖配置改变全部挡位灯号和占空比，并把反转配置改为LED7、90%升目标、20%低目标及321ms保持时间。

`tests/seven_led/run_tests.py`：全部9个场景PASS。基础8 cases/11917 checks；SOC上电各1/1/1/2 checks；SWD 6 checks；电池显示/重扣203004 checks；松扳机15s休眠45009 checks；故障显示/休眠145203 checks。新增真实方向提交门控恢复双向记忆与反转LED5显示。

`tests/tm1650/run_mode_integration.py`：全部4个物理总线用例PASS。分别验证正转K3情景显示、K2调速顺序及模式记忆、扳机屏蔽按键、反转K3 toggle/LED5及双方向记忆。TM1650读键、去抖、模式和显示均用实际ARM C；只有物理总线从设备用边沿级模型代替。
