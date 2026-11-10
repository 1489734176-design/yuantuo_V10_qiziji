# 核对结果

- USER/tool_mode.c情景3档当前固定100%；tool_mode_duty每1ms在user_oc_handle之后调用。
- USER/user_control.c现有IBusAvgScene_Adc由AVG独立零偏扣除并乘Vcc_cal_gain得到，避免把Peak零偏套用AVG。
- parameter.h现有CURRENT_ADC_PER_A约8.91991；18A按原(u16)写法换算为160码，比较使用>=。
- 情景2/3档另有到位停机判据（当前20A/50ms、25A/100ms峰值）；原故障保护不得绕过。
- 当前有Git元数据、main分支；用户要求当前原文件直接改，保持其他工作不动。
- tests/tool_mode包含真实ARM C/Unicorn回归；仓库指导所述无测试框架已过时。
- Keil UV4路径D:/Users/mym02/AppData/Local/Keil_v5/UV4/UV4.exe；临时复制项目仅重定向输出目录即可隔离Objects/Listings。
- 现有PWM下降每6个16kHz中断减1码；100%到30%约492ms，触发后立即改变目标并沿原软坡下降。
- 实际峰值积分保护为60A/800ms、70A/100ms、80A/10ms；既有注释的30A/50ms不对应当前参数。
- 原控制回归基线失败于case36；测试代理正在核实用户当前到位计数与旧测试差异，仅适配测试。
