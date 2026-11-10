# 情景3档测试记录

## 基线与RED

- 当前全控制基线：ARM编译通过，case 36 `test_seat_stop_gear2_boundary` 在原行1050失败，67238次检查。
- 基线原因：既存到位测试仍预期2档25A/100ms、3档30A/100ms；当前参数已2档20A/50ms、3档25A/100ms。当前生产处理严格 `>`，运行启动屏蔽150ms，低样本递减1。只适配测试，生产到位逻辑保持原样。
- 新增case 47 RED：ARM编译通过，`test_scene3_limit_current_boundary` 在行1332失败，3次检查。159码保持1874，160码期望562但现仍1874；失败来自缺少新需求行为。
- 所有构建输出隔离在本计划目录；测试执行器只生成编译夹具，不重写生产C/H。

独立基线的旧到位失败如下，35与43通过：

| 用例 | 测试 | 原失败行 |
| --- | --- | --- |
| 36 | test_seat_stop_gear2_boundary | 1050 |
| 37 | test_seat_stop_gear3_boundary | 1067 |
| 38 | test_seat_stop_peak_only_and_no_pwm_side_effect | 1087 |
| 39 | test_seat_stop_current_interruptions | 1114 |
| 40 | test_seat_stop_run_and_fault_resets | 1149 |
| 41 | test_seat_stop_selection_resets | 1173 |
| 42 | test_seat_stop_gear_change_resets | 1189 |
| 44 | test_seat_stop_state_machine_release | 1222 |
| 45 | test_seat_stop_gear3_existing_protection_priority | 1277 |
| 46 | test_seat_stop_gear2_real_peak_sampling | 1303 |

其中45还使用了旧30A/50ms过流配置。当前多级积分峰值参数为60A/800ms、70A/100ms、80A/10ms，已改用714码（最小严格超过80A），第11帧达到32768积分，与到位第100样本同帧时故障优先撤销电机许可。全程未修改生产到位/过流逻辑或参数。

## 全套镜像地址冲突诊断

- 现象：新增用例各自通过，但全套在Unicorn报 `UC_ERR_WRITE_UNMAPPED`。
- ELF证据：ER_RO从 `0x08000000` 开始，大小33124（结束 `0x08008164`），覆盖测试Flash夹具PAGE30=`0x08007800`、PAGE31=`0x08007c00`。
- 主任务诊断：首个case13、checks=0、PC=`0x080076ae`，向 `0x1f` 写入，支持erase_fixture擦掉代码/常量池。
- 唯独改测试链接地址为 `0x08010000` 后完整56例通过，确认根因；随后按主任务授权直接修改测试执行器。生产Flash地址和生产镜像保持原设置。

## 首轮即时降速GREEN结果

- `python -B tests/tool_mode/run_tests.py`：56 cases，86194 checks，退出0。真实Arm Compiler 6编译，`-Wall -Wextra -Werror`。
- 默认配置：14 cases，529 checks，退出0。
- 27A/45%覆盖配置：14 cases，253 checks，退出0；239码保持正常75%/1405，240码立即降为45%/843，松扳机恢复正常目标。
- 新增47～56分别覆盖18A边界、同次扣动电流回落锁存、许可/故障撤销保持、首次进入许可、松扳机重按、模式/方向/档位/初始化复位、其他模式、真实独立AVG零偏/VCC增益、实际PWM软坡和原主循环故障门控。
- `git diff --check -- tests/tool_mode`：退出0，无空白错误。
- 后续Python均用 `-B`，未触碰主任务负责清理的缓存。仅编辑测试、测试说明和本任务测试脚本记录，未编辑生产C/H、未提交Git。
- 主机模拟不构成实际电机、供电、负载或安全保护的台架证据。

## 用户追加200ms消抖要求

- 新需求：严格大于18A ADC门限160码，首次观察连续超限满200ms才锁为30%；等于/低于门限或首次确认期间任一许可、AVG校准、故障条件失效都清确认时间。已锁存行为保持，松扳机/选择改变/初始化清锁存与确认。
- 参数新增 `TOOL_SCENE_GEAR3_LIMIT_CONFIRM_MS`，默认200u，直接使用真实模式毫秒时钟；覆盖配置用73u，无CONUT换算。
- 新RED：`python -B tests/tool_mode/run_tests.py --case 47` 编译成功，case47原行1349，2次检查失败。160码应保持1874，旧即时降速返回562。主任务据此修改生产，然后case47 7次检查GREEN。
- cases47～56已适配161码首次观察后199/200ms边界，包括真实AVG零偏和VCC增益、PWM软坡及主循环热故障门控。
- 新case57：同一毫秒1000次目标处理、1000次查询均不累计，199ms再重复1000次也不提前触发，200ms才降速。
- 新case58：0、159、160码任一样本均打断199ms确认，连续多次断续高电流不能累计；重启完整199/200ms才触发。
- 新case59：10项运行、AVG校准、实时/锁存故障或已到位条件在199ms失效，恢复后必须重新计满200ms。
- 新case60：在199ms松扳机、换模式、离开3档、换向或初始化，重进后必须从首次观察重新确认。
- 最新完整GREEN：`python -B tests/tool_mode/run_tests.py`，60 cases / 89593 checks，退出0。
- 最新默认配置GREEN：15 cases / 1536 checks；27A/45%/73ms覆盖GREEN：15 cases / 625 checks。覆盖配置240码不累计，241码72ms正常、73ms降速，并验证等于门限打断后重新计时。
- 本次只直接编辑测试C/H、README及此记录，未修改生产文件、原到位/故障保护测试或主计划。
