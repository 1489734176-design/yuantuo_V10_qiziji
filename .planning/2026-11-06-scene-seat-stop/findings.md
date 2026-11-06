# 情景到位停机核对记录

- tool_mode_current()->mode和scene_gear是当前模式与情景挡位来源；反转情景固定4挡。
- main.c的1ms路径先user_oc_handle更新IBusPeak_Adc，后处理按键和目标，随后user_state_control消费gAutoStop。
- user_state_control在TOOL_RUN收到gAutoStop清gToolEn并进入TOOL_STOP；停止态等扳机松开清gAutoStop。
- 旧user_WrenchKey_Handler不参与当前Wrench_EN=0主循环，仅参考其停机标志机制。
- Multistage_Integral_Peak_OCP_EN=1，现有25A/120ms、30A/50ms、40A/20ms积分保护全部启用。
- 因Q15整数步长取整，持续超过30A时第二级约51次调用触发，早于新增三挡100ms条件。
- 用户明确指定峰值IBusPeak_Adc，不能换为AVG或情景一挡AVG自身零偏通路。
- 核实并保留当前工作区用户提交的新保护参数，旧测试硬编码28A阈值可能需要参数化维护。

## 验收边界
- 二挡：IBusPeak_Adc > TOOL_SCENE_GEAR2_STOP_CURRENT_AdcValue，连续100次；三挡对应30A。
- 等于或低于阈值立即清零。不同挡位不能共享已累计时间。
- 不写pwm_duty_aim/pwm_duty_aim_per，不用Load_exist，不清已有故障或gAutoStop锁存。
- 模式2/3的使能是运行时派生状态，不写入Flash。
- 无在线调试/板测环境；主机模拟不能当作实际到位力矩或安全行为验证。
