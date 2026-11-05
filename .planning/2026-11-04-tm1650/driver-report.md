# TM1650 驱动适配报告

## 产品修改

- `TM1650/tm1650.h`：按新图修正 SDA=PA11、SCL=PA12；SDA 开漏、外部 4.7K 上拉。保留现有宏/API，增加 `uint8_t TM1650_ReadKey(uint8_t *key)`。
- `TM1650/tm1650.c`：新增读取 0x49 原始键码，高位先读，末字节主机 NACK/STOP。成功返回1并写入键码；空指针或命令无 ACK 返回0且不覆盖输出。K2/K3原始0x47/0x46直接保留，不在驱动加入业务或去抖。
- ACK 采样调整到第9个时钟上升沿之前。规格书第3页文字明确第8个下降沿后芯片拉低 SDA、检测到第9个上升沿后释放 SDA；原代码在第9上升沿后延时采样有漏应答风险。命令无 ACK 时直接 STOP，不再发送数据字节。
- 显存保持用户 shadow/dirty 框架；Init 写零、Flush、FlushStep、Refresh 只有整个写事务得到 ACK 才清该位 dirty。
- 系统配置保留最新 shadow 和 dirty。SetDisplay 保持立即发送的兼容行为，失败由下一次 FlushStep/Flush 重试；新配置覆盖旧请求。Refresh 重发最新配置，保留上层请求的休眠/亮度状态。
- FlushStep 每次最多尝试一个事务，先重试系统配置，再轮询脏显存。返回1代表本次 ACK 成功，0代表失败或无待刷新。失败也占用本次服务，不能因返回0在同帧再读取按键。
- 两个产品文件保持 GBK 编码和 CRLF、原缩进格式，改过处中文说明。未修改厂商代码、未提交Git。

## TDD 与驱动验证

新增 `tests/tm1650/run_tests.py` 和 `test_support.c`。实际 ARM Cortex-M0 驱动与真实 GPIO/RCC HAL 用 Arm Compiler6 编译，Unicorn 执行。仅将物理时间替换为延时累加，TM1650从机模型按 GPIO 的真实边沿驱动 ACK/读码和解码命令，不替换驱动函数。

基线 RED：原驱动8组按预期失败，新IO、ACK采样、缺读键接口、FlushStep失败仍报成功、配置无重试、Init/Flush丢dirty均被复现；已有 shadow 操作回归通过。

GREEN：运行命令：

```powershell
& 'D:/Program Files/Python314/python.exe' 'tests/tm1650/run_tests.py'
```

9组全部 PASS，编译使用 `-Wall -Wextra -Werror`。覆盖：

1. 实际 GPIO CRH 配置 PA11 SDA开漏、PA12 SCL推挽。
2. 第9上升沿从机释放ACK时正确应答。
3. 读命令MSB 0x49，读取0x46/0x47/0x07/0x00/0xA5的8位原始数据、NACK、STOP。
4. 读命令无ACK或空指针不改写输出、不读取数据。
5. 显存地址或数据无ACK时单步返回失败，下一步重新发同显存内容。
6. 系统配置失败自动重试，并采用最新配置。
7. Init所有写零/配置失败后保留5个待写事务。
8. Flush/Refresh失败保留脏位；Refresh保留最新非默认系统配置。
9. shadow改位保留其他段、Clear、多个dirty单步一次仅一事务。

延时累加测量：系统/显存2字节事务显式延时240us；1字节键码读取显式延时234us。上述时间不包含真实函数指令成本和电机中断延长，最终以实板逻辑分析仪为准。现有60ms POR等待仅在初始化，运行服务没有毫秒延时循环或关闭中断。

## 跨模块验证现状

- `tests/tool_mode/run_tests.py`：PASS，7 cases/90 checks。
- `tests/battery_com/run_tests.py --keil D:/Users/mym02/AppData/Local/Keil_v5 --build-dir .planning/2026-11-04-tm1650/driver-suite-battery`：编译失败，提取测试TU未声明新增 `tool_mode_save_handle`/`tool_mode_init`。
- `tests/seven_led/run_tests.py`：编译失败，提取测试TU未声明新增 `tool_mode_save_handle`。

已向主代理反馈上述旧夹具问题，分工范围内未改动这两套测试。最终完整固件的warning-reviewed clean rebuild由主代理在整体集成后执行。

## 尚需实板确认

未进行烧录、供电、实际电机负载或抗干扰试验。需确认新板PA11/PA12波形、ACK释放时机、0x47/0x46按键、运行/停车及5s休眠显示、总线故障恢复和电机保护。规格书读键时序图末位泛称ACK，按本任务明确接口要求实现主机NACK；建议实板同时确认该末位与STOP可持续正确读取。
