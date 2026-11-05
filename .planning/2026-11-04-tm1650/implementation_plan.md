# 新板 TM1650 模式控制实施计划

**Goal:** 按用户新图和明确行为更新当前工作目录固件。
**Architecture:** TM1650保留用户驱动的显存/dirty框架，增加带ACK的键盘读和失败重试。tool_mode模块统一按键去抖、双模式挡位和目标占空比，Flash模块保存模式及两种模式各自挡位，现有电机状态机与加速斜坡负责执行。
**Spec:** task_plan.md 中用户规格；硬件事实见 findings.md 和 pdf-audit。

## 全局约束
- 所有产品改动加中文注释；不编辑厂商HAL/CMSIS、预编译MC_CORE库和已有生成输出。
- K2=0x47为调速，K3=0x46为情景，用户要求覆盖图中功能文字。
- 调速1..4档上限40/60/80/100%，保留最低15%和连续扳机映射；将原15..100%线性映射到15..该档上限。
- 情景1..3档目标20/100/100%，通过原pwm_set斜坡从原最低启动占空比上升。
- 首次空Flash为调速1档；切换模式只切换，再按同模式键才升档，两种模式分别记忆挡位。
- 前台保存仅在扳机松开、MOTOR_STOP、motor_en=0、gToolEn=0；EN拉低前执行同一保存服务。运行按键只改RAM，停车后持久化。
- 保留所有电流/温度/电压/看门狗/堵转保护和首次上电启动及方向联锁。

## 任务1：模式/占空比/Flash（代理负责）
Files: USER/tool_mode.c, USER/inc/tool_mode.h, USER/user_control.c, USER/flash_data_save.c, USER/inc/flash_data_save.h, tests/tool_mode/*。
Interfaces: tool_mode_list_t tool_mode_list含mode,speed_gear,scene_gear,save_pending；宏TOOL_MODE_SPEED=0,TOOL_MODE_SCENE=1；tool_mode_init(), tool_mode_key_handle()每1ms，tool_mode_key_event(uint8_t key)处理一次已去抖按键，tool_mode_duty(uint16_t trigger_duty)，tool_mode_gear()当前1起始档，tool_mode_save_handle()前台安全保存。消费TM1650_ReadKey(uint8_t *key)，返回1=ACK成功且已读，0=失败。
- [ ] 写行为测试并先确认旧代码缺少接口/行为。
- [ ] 新模块和最小user_control集成，已有user_gears_handle使用tool_mode_duty，user_para_init调用tool_mode_init。
- [ ] Flash保留旧3半字记录可读，扩展模式/双挡位、版本/完整性校验；最后提交标志，损坏或半写记录回退前一有效记录，避免每次启动重写默认；安全门控/保存失败保持pending。
- [ ] 测试档位循环、长按/去抖/读失败、切模式不升档、逐档扳机端点/中间值、情景与扳机深度无关、空flash/已存/损坏/半写/槽满和保存门控。

## 任务2：TM1650和新IO/显示（主代理负责）
Files: TM1650/tm1650.c,h, USER/led.c,inc/led.h, USER/board.c,inc/board.h, USER/main.c, KEIL_PRJ/MM32SPIN0230_Demo.uvprojx, tests/tm1650/*, tests/seven_led/*。
- [ ] 驱动总线模拟测试读取0x49、MSB数据、末字节NACK、ACK失败dirty重试。
- [ ] 按新图更新SDA/SCL、DIR/EN/照明和LED电量映射，停止旧矩阵输出和SWD复用，保留兼容输入缓存。
- [ ] TM1650显示前台每次最多一个显存写或键盘读，电机中断只采样独立DIR，无阻塞通信。
- [ ] 电量/故障保持既有逻辑，挡位严格单个红或绿，5s休眠全部熄灭。
- [ ] main在duty计算前处理模式键，motor_en发布后执行安全保存，工程加入TM1650及tool_mode源文件和include路径。
- [ ] IROM排除末尾Flash持久化区域，克隆构建项目输出到任务目录，进行独立审查和全量重编译。

## 任务3：最终验证和交付
- [ ] 新旧行为回归；独立审查并修复实质问题。
- [ ] Keil -r clean rebuild，检查警告和ROM/RAM大小，HEX/BIN仅放新output目录。
- [ ] 中文说明列出IO、模式表、保存边界、构建结果和未执行的实板验证。
