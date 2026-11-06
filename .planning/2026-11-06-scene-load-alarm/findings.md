# Findings
- 正转情景1档当前固定18%，由tool_mode_duty返回；目标经原pwm_set软坡输出。
- main 1ms内先user_oc_handle，再tool_mode_duty。实际启用user_control.c的#else分支user_oc_handle，u16I_SUM_Avg减CurrentOffset_ad_data、Vcc_cal_gain Q15校准后写IBusAvg_Adc。
- RSHUNT=1mΩ，放大10.89倍，12-bit ADC，5V参考；CURRENT_ADC_PER_A约8.919，1ADC约0.112A。3.5A为31.216 ADC，阈值需考虑量化并上板校准。
- UO_Led_Handler用battery_display_en同时门控报警与电量，5s后清零报警进度，是报警被截断的根因。
- 原LedError_Run为500ms灭、约200ms亮、轮间1500ms；每次装载10轮；持续故障时LedError_Control会按原逻辑重新装载10轮。
- 用户反馈样机卸载空转电流为1.3A，采用2.0A卸载阈值并留波动余量；默认负载向上取32码，卸载向下取17码，空载1.3A约12码。
- 正转情景1挡采用启动屏蔽100ms、连续负载确认20ms、连续卸载确认100ms；使用中断毫秒时钟差值而非前台调用次数。
