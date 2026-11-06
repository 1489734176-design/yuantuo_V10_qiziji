# 调查证据

- 基线HEAD已由用户提交到aa96a37；开始时parameter.h仅多两行空行，原Keil生成物有用户改动，保留。
- main每1ms先实际user_oc_handle，再user_gears_handle→tool_mode_duty。MOTOR_RUN内pwm_set输出目标，最小15%最大100%，现有可选限速/限流均关闭，无20%软件钳位证据。
- 28A门限是多级峰值保护IBus_Smart_Peak_OCP1，来自CH5/PB0，不是情景负载使用的CH4/PA5。
- CH4经R58=10k/C23=1uF约10ms硬件低通，增益10.891、1mΩ，8.91891 ADC码/A；静态预计约0.545V，5.5A约0.604V。
- PA5配置模拟输入，CH4 rank4、CH5 rank3，12位右对齐。TIM1_CC4双边沿触发，CCR4=duty-120，9通道扫描。尚未发现确定的通道读取/覆盖错误。
- 电流软件平均/IIR被注释，现用最新CH4读数。当前连续20ms超阈判定遇任一低样本清零，比原OCP加减积分更易被波动打断；这只是待验证嫌疑，不能无视CH4的硬件低通。
- 原阈值ceil(3.5*8.91891)=32；用户指定的(u16)表达式得到31，用与OCP一致的>31仍有效触发于32。宏写法变化不能解释或单独修复持续5.5A不升速。
- 用户实测AVG停机527mV、1.3A空转540mV，5.5A负载不方便测量。13mV增量按图纸折算1.19A，与1.3A输入基本相符；不证明CH5零点与CH4相同，也不足以证实本板根因。
- AXF反汇编排除mc_init→mc_core_list_init以及mc_core_adc_isr_handle及BEMF处理链覆写平均电流字段或校准零偏：mc_core_list基址0x20000054，平均字段+0x0E、零偏+0x60；相关库写入避开这些位置。
- 宏形式调整后BIN与上轮持续报警BIN的SHA256完全相同：64E4436763B47720E198D16D2720CD16A9684A629B295E9D26C6943B1FAF911C。因此不能把宏形式调整宣称为实板未升速的修复。
- 后续已实现独立CH4上电零偏：只供情景负载检测，旧CH5零偏与全部保护保持。527mV仅为实测参考，未硬编码到固件。
- 初始化顺序陷阱已覆盖：校准后read_user_data会调用tool_mode_init；新校准放在user_list尾部，只由user_para_init清除，不被模式/Flash恢复和mc_init清掉，不改变MC_CORE库结构。
