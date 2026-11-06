# 情景二三挡螺丝到位停机 Implementation Plan

**Goal:** 情景二挡峰值电流大于25A、三挡大于30A，分别持续100ms后置gAutoStop停机。
**Architecture:** 在USER/user_control.c新增独立user_screw_seat_stop_handle()，每1ms在模式更新之后、工具状态机之前调用。使用现有峰值ADC和停机锁存，不计算PWM目标，不处理堵转重启最低目标。
**Tech Stack:** Cortex-M0 C / Keil AC6 / 现有Unicorn ARM C验证。
**Spec:** 用户本轮请求；具体边界及现有过流冲突见findings.md。

## Global Constraints
- 直接用apply_patch修改原.c/.h，添加中文函数头及关键逻辑注释。
- 参数放parameter.h，保持CURRENT_ADC_PER_A、_AdcValue、_MS、_MS_CONUT写法。
- 保留全部现有电流、温度、电压、堵转和看门狗保护。
- 保留一挡负载升速、反转四挡、方向记忆、LED及报警逻辑。
- 用户工作区已有提交及后续修改不覆盖；不提交、不改MC_CORE ABI。

## Review Focus
- 25A/30A严格大于门限；99次不触发，第100次触发。
- 任一电流低样本立即清计数，禁止跨松扳机、模式、挡位和方向累计。
- 自动停机锁存由原状态机等松扳机后解除，保护函数不能主动清除。
- 平均电流和Load_exist不得影响该判据，函数不得写PWM目标。
- 现有30A约51ms过流会优先于三挡100ms判据，必须保留并披露。

## Next Step
代码及软件验证完成；交付新函数和固件，并说明现有30A过流保护优先及未板测限制。

### Phase 1: 核对并明确实现边界
**Status:** complete
- [x] 核对模式API、峰值ADC更新、主循环顺序和gAutoStop消费者。
- [x] 记录原多级峰值过流与新判据的先后关系。

### Phase 2: 实现及回归
**Status:** complete
- [x] tests/tool_mode先跑原基线，再用缺失功能空桩验证新增测试失败。
- [x] 新增user_screw_seat_stop_handle声明/实现及25A、30A、100ms配置。
- [x] main.c在user_state_control之前每1ms调用新函数。
- [x] 新增边界、断续电流、模式方向切换及锁存回归通过。
- [x] 复核旧用例因用户已修改参数失配的部分，参数化保持原断言意义。

### Phase 3: 验证交付
**Status:** complete
- [x] 独立源码审阅，修复旧编译分支使能覆盖及测试主循环顺序问题。
- [x] 全量控制46例66724项、LED九组918908项、默认/覆盖配置通过；Keil干净重建0错0警告。
- [x] 记录测试结果和无法现场板测的限制，提供原文件和固件路径。

## Decisions
- 按现有1ms慢循环累计100个连续有效高电流样本，不额外加入启动屏蔽。
- gAutoStopEn在正转情景2/3挡置1，其余置0，运行许可失败只清计数。
- 新判据仅在扳机、工具及电机使能、有效正转、TOOL_RUN/MOTOR_RUN且无故障时计数。
- 不调整原过流参数；功能代码与现有保护同时生效，先达到的条件先停机。
