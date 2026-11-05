from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def edit(relative, transform):
    path = ROOT / relative
    data = path.read_bytes()
    text = data.decode('utf-8').replace('\r\n', '\n')
    result = transform(text)
    newline = '\r\n' if b'\r\n' in data else '\n'
    path.write_bytes(result.replace('\n', newline).encode('utf-8'))


light_tests = '''static void test_release_sleep_and_repress(void)
{
	unsigned i;
	reset_light_fixture();
	run_light_tick();
	CHECK(TM1650_GetDigit(0)==0x40 && (ledData.Word & 0x0e));
	user_list.flag.bits.gToolTrigger=0;
	run_light_tick();
	for(i=0;i<1000;i++) run_light_tick();
	CHECK(user_list.user_state==TOOL_STOP && user_list.sleep_cnt==1000);
	user_list.flag.bits.gToolTrigger=1;
	run_light_tick();
	CHECK(user_list.user_state==TOOL_RUN && user_list.sleep_cnt==0);
	CHECK(ledData.Word & 0x0e);
	user_list.flag.bits.gToolTrigger=0;
	run_light_tick();
	for(i=1;i<=14999;i++)
	{
		run_light_tick();
		CHECK(user_list.user_state==TOOL_STOP);
		CHECK(TM1650_GetDigit(0)==0x40 && (EN_PORT->ODR & EN_PIN));
		if(i<4999) CHECK(ledData.Word & 0x0e);
		else CHECK((ledData.Word & 0x0e)==0);
	}
	CHECK(user_list.sleep_cnt==14999);
	run_light_tick();
	CHECK(user_list.user_state==TOOL_POWER_DOWN); /* 松手计满15s休眠。 */
	CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==0 && ledData.Word==0);
	CHECK(!user_list.flag.bits.gToolEn);
	run_light_tick();
	CHECK(!(EN_PORT->ODR & EN_PIN));
	for(i=0;i<9;i++) LED_Scan();
	CHECK(!(GPIOA->CRH & GPIO_CRH_MODE14) && !(GPIOB->CRH & GPIO_CRH_MODE8));
	user_list.flag.bits.gToolTrigger=1;
	run_light_tick();
	CHECK(user_list.user_state==TOOL_STOP && (EN_PORT->ODR & EN_PIN));
	CHECK(TM1650_GetDigit(0)==0x40 && (ledData.Word & 0x0e));
}

static void test_battery_window_hold_and_repress(void)
{
	unsigned i;
	reset_light_fixture();
	run_light_tick();
	CHECK(ledData.Word & 0x0e);
	/* 持续扣住超过uint16_t范围，5s超时后不能溢出重新点亮。 */
	for(i=1;i<=66000;i++)
	{
		run_light_tick();
		if(i<5000) CHECK(ledData.Word & 0x0e);
		else CHECK((ledData.Word & 0x0e)==0);
		CHECK(user_list.user_state==TOOL_RUN && user_list.sleep_cnt==0);
		CHECK(TM1650_GetDigit(0)==0x40 && (EN_PORT->ODR & EN_PIN));
	}
	user_list.flag.bits.gToolTrigger=0;
	run_light_tick();
	for(i=0;i<1000;i++) run_light_tick();
	CHECK((ledData.Word & 0x0e)==0 && user_list.sleep_cnt==1000);
	/* 模式键只能延长待机，不能重启电量5s窗口。 */
	tool_mode_key_event(TOOL_MODE_KEY_SCENE);
	run_light_tick();
	CHECK((ledData.Word & 0x0e)==0 && TM1650_GetDigit(1)==0x20);
	user_list.flag.bits.gToolTrigger=1;
	run_light_tick();
	CHECK((ledData.Word & 0x0e)!=0 && user_list.sleep_cnt==0);
	for(i=1;i<5000;i++)
	{
		run_light_tick();
		CHECK(ledData.Word & 0x0e);
	}
	run_light_tick();
	CHECK((ledData.Word & 0x0e)==0 && TM1650_GetDigit(1)==0x20);
}

static void test_fault_hold_and_release_sleep(void)
{
	unsigned i;
	reset_light_fixture();
	user_list.user_state=TOOL_ERROR_STOP;
	user_list.userErr.bits.gNtcMos=1;
	machine_error_hold.userErr_hold.bits.gNtcMos=1;
	for(i=0;i<30050;i++)
	{
		run_light_tick();
		CHECK(user_list.user_state==TOOL_ERROR_STOP && user_list.sleep_cnt==0);
		CHECK(TM1650_GetDigit(0)==0x40 && (EN_PORT->ODR & EN_PIN));
		if(i>=5000) CHECK((ledData.Word & 0x0e)==0);
		CHECK(!user_list.flag.bits.gToolEn);
	}
	user_list.flag.bits.gToolTrigger=0;
	for(i=0;i<14999;i++)
	{
		run_light_tick();
		CHECK(user_list.user_state==TOOL_ERROR_STOP);
		CHECK(TM1650_GetDigit(0)==0x40 && (ledData.Word & 0x0e)==0);
	}
	CHECK(user_list.sleep_cnt==14999);
	run_light_tick();
	CHECK(user_list.user_state==TOOL_POWER_DOWN);
	CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==0 && ledData.Word==0);
	CHECK(machine_error_hold.userErr_hold.bits.gNtcMos); /* 关灯不得清保护。 */
	run_light_tick();
	CHECK(!(EN_PORT->ODR & EN_PIN));
}

'''


def update_light_tests(text):
    start = text.index('static void test_release_sleep_and_repress(void)')
    end = text.index('static void test_mode_led_and_ntc(void)', start)
    text = text[:start] + light_tests + text[end:]
    text = text.replace('const uint8_t scene_masks[]={0x08,0x10,0x20};',
                        'const uint8_t scene_masks[]={0x20,0x10,0x08};')
    text = text.replace('情景保持LED5/D到LED7/F。', '情景LED7/F到LED5/D。')
    text = text.replace('\tif(test_boot_scenario)\n', '''	if(test_boot_scenario>=6)
	{
		test_case=test_boot_scenario;
		if(test_boot_scenario==6) test_battery_window_hold_and_repress();
		if(test_boot_scenario==7) test_release_sleep_and_repress();
		if(test_boot_scenario==8) test_fault_hold_and_release_sleep();
		return test_failure_line;
	}
	if(test_boot_scenario)
''', 1)
    text = text.replace('''	test_case=6;
	test_release_sleep_and_repress();
	if(test_failure_line) return test_failure_line;
	test_case=7;
	test_fault_hold_and_release_sleep();
	if(test_failure_line) return test_failure_line;
''', '')
    return text


key_tests = '''static void test_trigger_key_inhibit(void)
{
	erase_fixture(); boot_fixture();
	user_list.sleep_cnt=1234;
	user_list.flag.bits.gToolTrigger=1;
	key_raw=0x46; key_ms(100);
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	CHECK(!tool_mode_list.save_pending && user_list.sleep_cnt==1234);
	tool_mode_key_event(0x47);
	CHECK(tool_mode_gear()==1 && !tool_mode_list.save_pending);
	/* 松扳机时K3仍按住，不得补执行被屏蔽的按压。 */
	user_list.flag.bits.gToolTrigger=0; key_ms(100);
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED);
	key_ack=0; key_raw=0; key_ms(100); key_ack=1;
	key_raw=0x46; key_ms(100);
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED);
	key_raw=0; key_ms(30); press(0x46);
	CHECK(tool_mode_list.mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
	user_list.flag.bits.gToolTrigger=1; key_raw=0x47; key_ms(100);
	CHECK(tool_mode_list.mode==TOOL_MODE_SCENE);
	user_list.flag.bits.gToolTrigger=0; key_ms(100);
	CHECK(tool_mode_list.mode==TOOL_MODE_SCENE);
	key_raw=0; key_ms(30); press(0x47);
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	/* 未完成的20ms去抖被1ms扣动打断，松手后不得补执行。 */
	key_raw=0x47; key_ms(20);
	user_list.flag.bits.gToolTrigger=1; key_ms(1);
	user_list.flag.bits.gToolTrigger=0; key_ms(100);
	CHECK(tool_mode_gear()==1);
	key_raw=0; key_ms(30); press(0x47); CHECK(tool_mode_gear()==2);
	user_list.user_state=TOOL_POWER_DOWN;
	tool_mode_key_event(0x46);
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_gear()==2);
}
'''


def update_key_tests(text):
    text = text.replace('当前39/58/78/98%挡位表', '当前38/58/78/100%挡位表')
    text = text.replace('maximum[]={730,1086,1461,1836}', 'maximum[]={712,1086,1461,1874}')
    text = text.replace('middle[]={505,683,870,1058}', 'middle[]={496,683,870,1077}')
    text = text.replace('static void test_save_and_gates(void)', key_tests + '\nstatic void test_save_and_gates(void)', 1)
    return text.replace('RUN(11,test_real_pwm_ramp);', 'RUN(11,test_real_pwm_ramp);\n\tRUN(12,test_trigger_key_inhibit);', 1)


edit('tests/seven_led/test_seven_led.c', update_light_tests)
edit('tests/tool_mode/test_tool_mode.c', update_key_tests)
edit('tests/seven_led/run_tests.py', lambda text: text.replace('range(6)', 'range(9)')
     .replace('count=40_000_000', 'count=80_000_000')
     .replace("    print(f'PASS: {label}", "    label = {6: 'battery 5s hold/repress', 7: 'release 15s sleep/wake', 8: 'fault 5s display/15s sleep'}.get(boot_scenario, label)\n    print(f'PASS: {label}"))
print('Updated behavior tests; production source unchanged.')
