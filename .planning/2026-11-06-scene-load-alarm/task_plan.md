# 情景1档负载升速与报警显示独立于电量窗口

## Goal
情景1档检测3.5A负载后升到最大占空比，卸载后恢复该档正常占空比；报警按原节奏闪烁，不受电量5s窗口限制。直接修改原始.c/.h并添加中文注释。

## Phases
1. complete — 核对实际平均电流校准链路、卸载判断及原报警节奏。
2. complete — 行为红测后实现情景1档负载状态机和报警窗口修复。
3. complete — 相关回归、独立审查、Keil警告审阅完整重编译。
4. complete — 记录参数、固件与实板验证限制，实板测试待有硬件后执行。

## Implementation choices
- 正转情景1档采用现有校准后的user_list.IBusAvg_Adc与CURRENT_ADC_PER_A，原过流、温度、欠压、堵转及PWM斜坡保留。
- 用户实测空载1.3A；进入阈值3.5A，退出阈值设为2.0A。启动屏蔽100ms、负载连续确认20ms、卸载连续确认100ms，参数集中parameter.h。
- 运行许可丢失、停机、松扳机、模式/档位/方向切换应复位负载升速状态。
- 用户进一步明确持续循环：报警去掉电量窗口门控，以活动标志替代10轮倒计数；每轮间隔后继续，松手停止，原扳机/休眠、故障优先级和报码节奏保留。
- root负责tool_mode/parameter及主模式测试；alarm_window_fix负责led.c及seven_led回归；scene_current_audit只读核对电流链路。

## Baseline
2026-11-06 HEAD=20b0946d594849989f44374d314304de8b92a549；工作区仅无关.claude未跟踪。前轮功能已提交。

## Next Step
软件实现与验证完成，最终交付continuous-alarm-build内HEX/BIN及continuous-alarm.md；需实板核对报警循环、电流校准、负载/卸载效果和保护。
