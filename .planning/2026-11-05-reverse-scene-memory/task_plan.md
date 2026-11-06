# 反转情景4档与双向记忆

## Goal
反转情景固定4档LED5；K3只切调速/情景，正转按键不变。正常斜坡到100%，实际达到后保持850ms，再同速降到15%持续运行。调速灯按8、87、876、8765累加；正反转分别记住模式和档位，但双方都为调速时真实换向沿用当前档位，兼容原掉电记录。

## Phases
1. complete — 查明按键、方向、PWM、LED与Flash逻辑；用户确认仅反转K3切换。
2. complete — 已直接修改8个原始C/H，23组行为回归及相关显示/按键测试通过。
3. complete — 独立审查发现的首满值捕获缺口已修复并复审；最终Keil AC6全量编译0错误0警告。
4. complete — 验证结果、固件及待实板检查记录已整理，尚未上板。
5. complete — 调速灯逐档累加；两方向均为调速模式时，真正换向沿用当前档位，涉及情景继续恢复记忆；首次上电独立恢复。
6. complete — 26组主模式回归、默认/覆盖灯号、seven_led及5组TM1650总线测试通过，独立审查无阻塞，Keil全量0错误0警告。

## Design / implementation
- USER/inc/tool_mode.h：每方向独立setting(mode/speed_gear/scene_gear)，active_direction选择当前setting；反转scene_gear固定4。
- USER/tool_mode.c：tool_mode_current()提供当前setting，tool_mode_set_direction()在真正换向后恢复档位；反转K3 toggle。增加ISR毫秒时钟与三阶段曲线（缓升/满占空比保持/低占空比持续），松扳机、停止、失去许可和换向复位。
- USER/user_control.c：仅在两处实际提交方向后切换setting，保持原停止与松扳机门控。
- USER/mm32_it.c：1ms分支递增曲线时钟，避免前台丢帧缩短/延长软件计数。
- USER/led.c：反转情景显示独立配置的LED5。
- USER/inc/parameter.h：新增反转情景LED、100%、15%、850ms配置及范围校验。
- USER/flash_data_save.c：保持20字节槽和8半字记录，版本2的mode/speed分别以低字节正转、高字节反转编码；正转scene仍1~3，反转scene隐含4。读取兼容版本1与旧日志，保留CRC、提交标志、双页轮换及停机保存门控。

## Verification
对真实tool_mode/Flash源码执行行为测试：反转固定档和K3切模式、正转不回归、方向恢复、达到满占空比才计时、849/850ms边界、停止/故障重置、双向掉电保存与v1迁移。Keil AC6 -r重编译，审阅全部警告并确认新AXF/HEX/BIN。
硬件未连接，无法代替板测；交付记录板测所需参数与观察项。

## Decisions
用户2026-11-05明确：仅反转修改情景键，正转保留现有逻辑。
原PWM上下坡及实际跟踪均6次PWM中断变化1计数，直接复用，无需改MC_CORE或保护参数。
直接修改用户指定工作目录，隔离构建输出，保留现有无关.claude文件。

## Errors
工具输出过长发生截断，后续限制按文件/区段读取。
git外部ignore不可读告警，不影响仓库内源码diff。

## Next Step
本轮软件修改和验证完成；交付最新HEX/BIN，实板验证待执行。
