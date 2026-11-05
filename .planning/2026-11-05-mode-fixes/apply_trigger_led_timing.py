from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def patch(relative, replacements):
    path = ROOT / relative
    data = path.read_bytes()
    newline = b'\r\n' if b'\r\n' in data else b'\n'
    for old, new, expected in replacements:
        old_bytes = old.encode('utf-8').replace(b'\n', newline)
        new_bytes = new.encode('utf-8').replace(b'\n', newline)
        count = data.count(old_bytes)
        if count != expected:
            raise ValueError(f'{relative}: expected {expected} occurrences, found {count}: {old!r}')
        data = data.replace(old_bytes, new_bytes)
    path.write_bytes(data)


patch('USER/inc/parameter.h', [
    ('5000ul//10000', '15000ul // 松开扳机后15s休眠，重新扣动清零\n#define BATTERY_LED_TIME (5000u) // 每次重新扣动扳机，电量灯显示5s', 1),
])

patch('USER/led.c', [
    ('static uint8_t tm_display_state=0xff;', '''static uint8_t tm_display_state=0xff;
static uint8_t battery_trigger_previous;
static uint16_t battery_display_ms=BATTERY_LED_TIME;''', 1),
    ('\ttm_display_state=0xff;', '''	tm_display_state=0xff;
	battery_trigger_previous=0;
	battery_display_ms=BATTERY_LED_TIME;''', 1),
    ('\tuint8_t mask;\n\tif(user_list.user_state==TOOL_POWER_DOWN)', '''	uint8_t mask;
	uint8_t trigger=user_list.flag.bits.gToolTrigger;
	uint8_t battery_display_en;
	/* 中文：只有松开后重新扣动才重启5s；长按及松手不清零电量计时。 */
	if(trigger && !battery_trigger_previous) battery_display_ms=0;
	battery_trigger_previous=trigger;
	if(user_list.user_state==TOOL_POWER_DOWN)''', 1),
    ('\t\tledData.Word=0;\n\t\tuser_led_control.LedRunTimes=0;', '''		ledData.Word=0;
		battery_display_ms=BATTERY_LED_TIME;
		user_led_control.LedRunTimes=0;''', 1),
    ('\tif(LedError_Control() && user_list.flag.bits.gToolTrigger)', '''	battery_display_en=(battery_display_ms<BATTERY_LED_TIME);
	if(battery_display_en && LedError_Control() && trigger)''', 1),
    ('\t\tLedSoc_Run(user_list.VBus_Vx100, 1, 0);', '\t\tLedSoc_Run(user_list.VBus_Vx100, battery_display_en, 0);', 1),
    ('\t/* 实板DIG1绿色、DIG2红色；调速1..4档按LED8/G到LED5/D单灯显示。\n\t * 情景1..3档仍按LED5/D、LED6/E、LED7/F显示，只修正颜色通道。 */', '''	/* 中文：计数到5s饱和，避免长时间扣住后溢出重新点亮；模式灯保持到休眠。 */
	if(battery_display_ms<BATTERY_LED_TIME) battery_display_ms++;
	/* 实板DIG1绿色、DIG2红色；调速1..4档LED8/G到LED5/D。
	 * 中文：情景1..3档改为LED7/F、LED6/E、LED5/D，均单灯显示。 */''', 1),
    ('(7u-tool_mode_gear()) : (tool_mode_gear()+2u)', '(7u-tool_mode_gear()) : (6u-tool_mode_gear())', 1),
])

patch('USER/tool_mode.c', [
    ('void tool_mode_key_event(uint8_t key)\n{', '''void tool_mode_key_event(uint8_t key)
{
	/* 中文：扳机按住及休眠期间，模式/挡位按键均无效。 */
	if(user_list.flag.bits.gToolTrigger || user_list.user_state==TOOL_POWER_DOWN) return;''', 1),
    ('\tuint8_t sample;\n\tif(++key_poll_count<10)', '''	uint8_t sample;
	uint8_t inhibited=user_list.flag.bits.gToolTrigger || user_list.user_state==TOOL_POWER_DOWN;
	/* 中文：每1ms阻断未完成按压；松扳机后还需按键稳定释放才能再按生效。 */
	if(inhibited)
	{
		key_press_latched=1;
		key_candidate=0;
		key_stable_count=0;
	}
	if(++key_poll_count<10)''', 1),
    ('\ttool_mode_key_read_ok=1;\n\tif(!(raw & 0x40u))', '''	tool_mode_key_read_ok=1;
	/* 中文：仍保留键码观察及总线调度，按住扳机时不接受按压或释放事件。 */
	if(inhibited) return 1;
	if(!(raw & 0x40u))''', 1),
])

patch('USER/user_control.c', [
    ('void user_state_control(void)\n{', '''void user_state_control(void)
{
	/* 中文：扣动扳机清零15s待机计时；正常/故障/方向未就绪时长按均保持唤醒。 */
	if(user_list.flag.bits.gToolTrigger) user_list.sleep_cnt=0;''', 1),
    ('if(++user_list.sleep_cnt>SLEEP_TIME)', 'if(!user_list.flag.bits.gToolTrigger && ++user_list.sleep_cnt>=SLEEP_TIME)', 2),
])
print('Applied scene order, trigger key inhibit, 5s battery and 15s sleep changes.')
