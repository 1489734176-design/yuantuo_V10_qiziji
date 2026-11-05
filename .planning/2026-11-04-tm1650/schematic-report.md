# 新旧原理图与 TM1650 资料核对

日期：2026-11-04。只核对资料和现有参数，不修改产品代码。原理图均为一页；规格书十页。使用 Poppler 220 dpi 渲染两张完整原理图，130 dpi 渲染规格书，再对 MCU、TM1650、IO、供电及电流放大电路放大视觉核对。文本提取仅作为检索辅助。

## 来源与视觉证据

- 旧图：`D:\mym\NXN\9.henbangtai\V25_50Adianzuan\资料\NX32F002-28T-方灯-7LED.pdf`，第 1 页。
- 新图：`D:\mym\NXN\3.yuantuo\V10_qizhiji\NX32F002TS-飞机板-绿林\NX32F002TS-飞机板-绿林.pdf`，第 1 页。
- TM1650：`D:\mym\NXN\3.yuantuo\V10_qizhiji\NX32F002TS-飞机板-绿林\TM-TM1650.pdf`，V1.0，第 2-5、7 页。
- 全图与放大图：本目录 `pdf-audit/old-1.png`、`new-1.png`、`new-mcu.png`、`new-tm.png`、`new-io.png`、`new-power.png`、`new-current.png`、`old-current.png`，以及 `tm1650-03.png`、`tm1650-04.png`、`tm1650-05.png`、`tm1650-07.png`。
- 代码参数核对：`USER/inc/parameter.h`、`USER/inc/board.h`、`USER/board.c`、`USER/mm32_it.c`、`MOTOR_CONTROL/motor_control.c`。代码可能由其他代理在本任务并行修改，下列“现有”指本次读取时的内容。

## 全部 MCU 引脚对照

| 封装脚 | IO/电源 | 旧图网络 | 新图网络 | 作用/变化 |
|---:|---|---|---|---|
| 1 | PB4 | HIN1 | HIN1 | U 相上桥 PWM，未变 |
| 2 | PB5 | LIN3 | LIN3 | W 相下桥 PWM，未变 |
| 3 | PB6 | LIN2 | LIN2 | V 相下桥 PWM，未变 |
| 4 | PB7 | LIN1 | LIN1 | U 相下桥 PWM，未变 |
| 5 | PB8 | LED1 | LED3 | 新电量 L3 |
| 6 | PB9 | COM4 | EN | 新电源保持 |
| 7 | VSS | GND | GND | 未变 |
| 8 | VDD | +5V | +5V | 未变 |
| 9 | PA0 | COM2 | KEY | 新独立 K1 输入，R69 1K 串联，按下接地 |
| 10 | PA1 | SPEED | SPEED | 扳机模拟量，未变 |
| 11 | PA2 | VBus | VBus | 新图为 VCC 的 1/11 分压 |
| 12 | PA3 | TBAT | TBAT | DS/电池温度预留，未变 |
| 13 | PA4 | TMOS | TMOS | MOS NTC，型号改变 |
| 14 | PA5 | I_AVG | I_AVG | 滤波平均电流，未变 |
| 15 | PA6 | OP1_P | OP1_P | 电流运放正输入，未变 |
| 16 | PA7 | OP1_N | OP1_N | 电流运放负输入，未变 |
| 17 | PB0 | OP1_O | OP1_O | 峰值电流运放输出，未变 |
| 18 | PB1 | Bef_COM | Bef_COM | 反电势虚拟中点，未变 |
| 19 | PB2 | Bef_W | Bef_W | W 反电势，未变 |
| 20 | PA8 | Bef_V | Bef_V | V 反电势，未变 |
| 21 | PA9 | Bef_U | Bef_U | U 反电势，未变 |
| 22 | PA12 | LED+ | SCL | 改 TM1650 时钟 |
| 23 | PA11 | EN | SDA | 改 TM1650 数据 |
| 24 | PA10/NRST | COM3 | LED2 | 新电量 L2 |
| 25 | PA13/SWDIO | DSD、COM1 | DSD、DR | 新方向输入，仍与 SWDIO 共脚 |
| 26 | PA14/SWCLK | DSCK、LED2 | DSCK、LED1 | 新电量 L1，仍与 SWCLK 共脚 |
| 27 | PA15 | HIN3 | HIN3 | W 相上桥 PWM，未变 |
| 28 | PB3 | HIN2 | HIN2 | V 相上桥 PWM，未变 |

PWM 六路 MCU 引脚、HIN/LIN 编号和相序完全一致。两图的外部功率驱动芯片和 MOS 数量不同，不应因 UI 移植改动 PWM 相序。

## TM1650 连接、地址、灯位和按键

### 总线

- 主板 P2：1=+5V，2=SA，3=SL，4=GND。SA 经 R64 51Ω到 PA11/SDA；SL 经 R25 51Ω到 PA12/SCL。
- 面板 P1 相同顺序；TM1650 pin3=SDA/SA，pin2=SCL/SL。R12/R13 各 4.7KΩ上拉 +5V；C12/C20 100pF 标 NC。
- TM1650 pin10=+5V，pin4=GND；C15 1µF、C21 100nF 电源滤波。
- SDA 必须在 ACK 与读取时释放，避免 MCU 高电平推挽输出和芯片拉低争用。利用现有上拉使其回高。

### 四只红绿面板灯

新图中的元件号是 LED5、LED6、LED7、LED8，区别于底部的电量 LED1-3 和照明 LED4。若产品把面板四灯另命名 LED1-4，软件须显式说明该逻辑编号与物理图号的对应。

| 物理元件 | TM1650 段 | TM1650 脚 | 数据 bit/掩码 | 红色地址 | 绿色地址 |
|---|---|---:|---|---|---|
| LED5 | D/KI4 | 12 | bit3 / 0x08 | 0x68 | 0x6A |
| LED6 | E/KI5 | 13 | bit4 / 0x10 | 0x68 | 0x6A |
| LED7 | F/KI6 | 14 | bit5 / 0x20 | 0x68 | 0x6A |
| LED8 | G/KI7 | 15 | bit6 / 0x40 | 0x68 | 0x6A |

- 图中标“红”的公共线连 DIG1(pin1)，显存 0x68；标“绿”的公共线连 DIG2(pin5)，显存 0x6A。
- 四灯的 pin1 都接红公共线，pin2 都接绿公共线；pin3/4 共用到对应段输出。
- DIG3=0x6C，DIG4=0x6E 在新图用于按键扫描，没有连接显示灯。驱动初始化可将其显示数据清零。
- A/KI1=bit0，B/KI2=bit1，C/KI3=bit2，DP/KP=bit7 没有连接这四灯。
- 全部四红灯亮：向 0x68 写 0x78；四绿灯亮：向 0x6A 写 0x78。这里只描述电路映射，实际亮灯组合由产品要求决定。

### 按键

| 元件 | 电气连接 | 规格书键码 | 用户最终指定功能 |
|---|---|---|---|
| K2 | DIG4(pin7)→R20 2KΩ→按键→A/KI1(pin8) | 0x47 | 调速 |
| K3 | DIG3(pin6)→R15 2KΩ→按键→A/KI1(pin8) | 0x46 | 情景 |

新图的“速度调节按键/模式调节按键”文字与用户最终功能定义不同；按用户定义实施，不改电气键码。规格书第 4 页明确串接 2KΩ且“不支持组合键”。

### 通信协议与时序

- 第 3 页：START=SCL 高时 SDA 高→低；STOP=SCL 高时 SDA 低→高。
- 每字节 MSB 先、LSB 后。普通数据 SDA 在 SCL 高时保持稳定，只在 SCL 低时改变。
- 写入 8 bit 后释放 SDA，TM1650 在第 8 时钟下降沿后主动拉低 ACK，随后第 9 时钟；规格书描述遇到 SCL 上升沿释放为输入，应在第 9 上升沿附近可靠取 ACK。
- 模式命令字节 0x48，跟随一个显示控制字节；两字节都带第 9 ACK 时钟，然后 STOP。
- 显存写：START→0x68/0x6A/0x6C/0x6E→ACK→数据→ACK→STOP；地址和数据均原样发送 8 bit。这些是线上的字节，不可当 7 bit I2C 地址再次左移。
- 读键命令推荐 0x49；START→0x49→ACK→8 bit 按键数据→第 9 时钟→STOP。芯片在 SCL 下降沿输出读取数据，主机在高电平取样。
- 显示控制：bit0=1 开/0关；bit3=0 为8段、1为7段；bit6:4=001-111 对应1-7级亮度，000对应8级最亮。未用的位建议0。例如0x11=1级、8段、开显示；0x01=8级、8段、开显示。
- 第 7 页：上电复位时间最小10、典型30、最大60ms；显示扫描典型7ms；按键扫描/响应典型40ms。
- START/STOP建立保持、SCL高低宽度最小100ns；数据建立40ns、保持10ns；输出延迟2ns；最高平均4Mbps。板上4.7K上拉及布线电容使实际边沿变慢，软件使用微秒级低速时钟更有余量。
- 本规格书没有明确无键返回值、持续按住/松开是否额外使用bit7、读数据末尾第9时钟ACK来源或NACK终止要求。这些不得作为已证明事实，需在实际面板读取确认。列出的K2/K3查表码0x47/0x46确定。

## 电量 LED1-3 与照明 LED4

L1=PA14，经R14 1K；L2=PA10，经R21 1K；L3=PB8，经R22 1K。

| 物理元件 | 阳极 | 阴极 | 单灯点亮 IO 状态 |
|---|---|---|---|
| 电量 LED1 | L1/PA14 | L2/PA10 | PA14高、PA10低、PB8高阻 |
| 电量 LED2 | L1/PA14 | L3/PB8 | PA14高、PB8低、PA10高阻 |
| 电量 LED3 | L3/PB8 | L1/PA14 | PB8高、PA14低、PA10高阻 |

三只电量灯采用三 IO 分时驱动；每槽其余 IO 高阻，切换时先全高阻。其段极性与旧七灯矩阵不同。

新照明 LED4：VCC→R27 200Ω→CN1 pin7/LED4阳极，LED4阴极→CN1 pin8/GND。没有独立 MCU LED+ 控制。旧 PA12/LED+ 原本经R15 1K接CN1 pin7，新PA12已用于SCL。

## 方向、扳机、供电保持与接口

- 新方向：CN1 pin3=DIRCTION，经R54 1K至DR/PA13；输入需上拉以识别外部接地。旧R54=2K且和旧灯网络共享；新独立输入。方向开关“高/低→物理顺/逆”不能从图确认，需实机验证现有 `Tool_Dir_CW_Value=0`/CCW=1 是否一致。
- CN1 新针序：1=+5V，2=GND，3=DIRCTION，4=GND，5=KEY_EN，6=SPEED输入，7=照明阳极/VCC经200Ω，8=GND。旧pin4=VIN，是重大接线变化。
- 扳机模拟量：CN1 pin6→R26 1K→PA1/SPEED；R47 100K下拉、C17 100nF对地。旧串阻图号R64 1K，值及PA1用途未变。SPEED上升随扳机压下的具体范围须实测。
- KEY_EN 外部拉低，经R8 10K拉低PNP Q1(MMBT5401)基极，将VIN接到VCC；PB9/EN 经R10 10K驱动Q3(MMBT5551)基极，EN高使Q3导通拉低KEY_EN，保持VCC。R11 100K将EN基极下拉，EN低/高阻允许关机。Q1 R2 10K基极-发射极上拉。
- 新VCC经过R3 1K到U3 EN，U3提供+5V；VIN仍给驱动VM/VMR和功率桥供电。
- 独立K1：PA0/KEY经R69 1K到按钮，按钮另一侧接地；该键应上拉、低有效。
- JP1烧录口：1=+5V，2=GND，3=DSD/PA13，4=DSCK/PA14；与方向及电量L1共脚，应尊重现有SWD/GPIO接管时机。

## ADC、NTC、反电势

- 新VBus：VCC→R4 10K→VBus/PA2→R9 1K→GND，C9 100nF对地，VBus=VCC/11。现 `VBUS_SHUNT_RATIO=11`、`ADC_REFV=5` 正确匹配新电路，不需再次改比例。测到的是经Q1供电的VCC，不是Q1前VIN。
- 旧VBus接6D180 VOUT内部输出，图上不能独立推导其内部比例。
- TMOS仍PA4，R41=4.7K上拉+5V，C16=100nF对地；旧NTC1=100K/B3950，新NTC1=10K/B3435。
- 本次读取 `MOS_NTC_10K_3435K=0`、`MOS_NTC_100K_3950K=1`，应切换为1/0。现参数已提供10K/B3435表：95℃RT=1.079K、85℃RT=1.414K；继续保留原95℃保护、85℃恢复的物理温度和时间，新的ADC阈值自动由分压公式产生。
- TBAT仍PA3：+5V→R57 10K→DS→R60 1K→TBAT，C14新100nF；旧同电阻但C14=100pF/NC。新图外部DS器件型号未标，不可自行推导电池温度曲线。本次读取EN_BAT_TEMP_DETECT=0且注释用户确认不启用，不能自行启用。
- 反电势PA9/PA8/PB2/PB1未变。三相各10K上臂、2K下臂、100pF滤波，Bef_COM经三只20K接三相采样节点。新相位源是U_1/V_1/W_1（U3 VS节点，2Ω前），旧图标U/V/W（2Ω后）；电阻比例未变。

## 电流采样全部电阻、增益与现有参数

新图电流链路：低侧MOS源极汇成采样节点→R39 0.001Ω/1%/3W→GND。所有低侧回流通过该电阻。

| 电阻 | 新值 | 连线/作用 | 与旧图比较 |
|---|---|---|---|
| R39 | 1mΩ/1%/3W | 低侧MOS源极公共节点至GND | 值相同，旧还有R46并联 |
| R37 | 100K/1% | +5V至OP1_P/PA6 | 相同 |
| R38 | 1K/1% | R39高端至OP1_P/PA6 | 相同 |
| R40 | 1K/1% | R39低端GND至OP1_N/PA7 | 相同 |
| R59 | 10K/1% | OP1_O/PB0反馈至OP1_N/PA7 | 相同 |
| R58 | 10K/1% | OP1_O/PB0至I_AVG/PA5 | 相同 |

C23=1µF/10V，从I_AVG到GND，和R58组成平均电流滤波，名义RC=10ms。峰值电流取PB0运放输出，平均电流取PA5滤波节点，代码ADC_CHANNEL宏和Get_ADC_Result与此吻合。

反馈非反相增益 `1+R59/R40=11`。R37/R38在正输入对采样信号产生 `R37/(R37+R38)=100/101` 衰减，故电流净增益 `11*100/101=10.891089`。理论输出：

`Vout = 10.891089 * I * Rshunt + 11*5/101`

两图输入、反馈和滤波电阻完全一致，净增益一致。零电流偏置理论约0.544554V，5V/4095 ADC约446；实际应测量并校准，不硬编码该理论值。现 `CurrentOffsetCalibration()`从OP_O通道测偏置，后续电流减 `CurrentOffset_ad_data`，应保留。

旧R39 1mΩ与R46 2mΩ并联，等效0.666667mΩ；新图只有R39 1mΩ。同电流新信号幅度是旧图的1.5倍。但本次读取代码已经 `RSHUNT=1.0f`（mΩ）、`AMPLIFICATION_GAIN=10.89`、`ADC_REFV=5`、`ADC_REF_VALUE=4095`，已经匹配新图，不应把现系数再乘1.5。

现公式：`CURRENT_ADC_PER_A = RSHUNT*AMPLIFICATION_GAIN*4095/5/1000 = 8.91891 ADC/A`。名义净增益精确计算为8.919802 ADC/A，10.89的舍入误差约0.01%，不是本次应扩大修改的依据。

- `RSHUNT`无需改变，`AMPLIFICATION_GAIN`无需改变。
- MOS_SHORT_CURRENT、IBus_Avg_OCP1-4、IBus_Smart_Peak_OCP1-3、Block_Protect_Current1-4、IBus_Peak_IT_OCP、IBus_Peak_OCP_ONCE、IBus_IPD_OCP、LIMIT_*、CCW_AUTO_STOP_CURRENT* 全部通过CURRENT_ADC_PER_A派生ADC值，不需要手工乘系数或改物理A值。
- 示例现阈值：60A→535、65A→579、70A→624、80A→713、90A→802、100A→891、105A→936、150A→1337 ADC（不含已校准零点）。
- 使用1mΩ代码搭配旧0.666667mΩ图会导致名义保护在约1.5倍物理电流触发；新图回到名义值。这是电路与参数推导，不代表已确认旧板实际焊接R46，也不等于已完成新板安全验证。
- 保留偏置校准、欠压/过流/过温/堵转/看门狗保护。板上需验证实际R39焊接、运放输出与ADC的斜率、零点、峰均值和保护动作，不能仅靠编译证明电气保护。

## 未确定与实机验证项

1. 实际焊接BOM是否与两图一致，特别NTC、R39和旧R46；原理图本身不证明焊件。
2. 方向机械位置与软件CW/CCW高低映射；SPEED实际电压范围。
3. TM1650无键/长按/松开返回值及读数据末尾第9时钟，规格书未完整定义。
4. LED逻辑编号与物理LED5-8的顺序必须在功能要求中明确；视觉核对确定电气bit映射，不能自行推断用户的逻辑位置编号。
5. 新照明硬件依附VCC，软件不能提供独立LED+定时熄灭；保持上电就会照明，改变需要硬件能力。
6. 板测应记录硬件版本、供电、负载、参数及保护行为；当前资料核对没有硬件实测。
