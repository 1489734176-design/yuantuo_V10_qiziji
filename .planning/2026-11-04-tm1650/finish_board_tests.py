from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=root/'tests/seven_led/test_seven_led.c'
s=p.read_text(encoding='utf-8')
s=s.replace('GPIOA->ODR & LED_LIGHT_PIN','TM1650_GetDigit(1)==0x08')
s=s.replace('!(TM1650_GetDigit(1)==0x08)', 'TM1650_GetDigit(1)==0')
s=s.replace('GPIOA->ODR & EN_PIN', 'EN_PORT->ODR & EN_PIN')
s=s.replace('\tled_init();\n\tGPIO_SetBits(EN_PORT, EN_PIN);', '\tled_init();\n\ttool_mode_init();\n\tGPIO_SetBits(EN_PORT, EN_PIN);')
s=s.replace('uint32_t run_tests(void)\n{','uint32_t run_tests(void)\n{\n\ttool_mode_init();')
pos=s.index('uint32_t run_tests(void)')
new=r'''static void test_mode_led_and_ntc(void)
{
	unsigned i;
	const uint8_t masks[]={0x08,0x10,0x20,0x40};
	memset(&user_list,0,sizeof(user_list));
	memset(&machine_error_hold,0,sizeof(machine_error_hold));
	user_list.user_state=TOOL_STOP;
	user_list.VBus_Vx100=2100;
	tool_mode_init();
	for(i=0;i<4;i++)
	{
		tool_mode_list.speed_gear=i+1;
		UO_Led_Handler();
		CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==masks[i]);
	}
	tool_mode_list.mode=TOOL_MODE_SCENE;
	for(i=0;i<3;i++)
	{
		tool_mode_list.scene_gear=i+1;
		UO_Led_Handler();
		CHECK(TM1650_GetDigit(0)==masks[i] && TM1650_GetDigit(1)==0);
		CHECK(TM1650_GetDigit(2)==0 && TM1650_GetDigit(3)==0);
	}
	user_list.user_state=TOOL_POWER_DOWN;
	UO_Led_Handler();
	CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==0 && ledData.Word==0);
	/* 新10K/B3435+4.7K分压：700 ADC超过95℃，1100 ADC低于85℃。
	 * 实际保护函数必须先置故障，再经过恢复去抖解除。 */
	user_list.Vcc_cal_gain=32768;
	mc_core_list.Board_ADC_DATA.u16NTC_MOS=700;
	for(i=0;i<51;i++) User_Temperature_Handler();
	CHECK(user_list.userErr.bits.gNtcMos);
	mc_core_list.Board_ADC_DATA.u16NTC_MOS=1100;
	for(i=0;i<501;i++) User_Temperature_Handler();
	CHECK(!user_list.userErr.bits.gNtcMos);
}

'''
s=s[:pos]+new+s[pos:]
s=s.replace('\ttest_matrix_and_sampling();','\ttest_mode_led_and_ntc();\n\tif(test_failure_line) return test_failure_line;\n\ttest_matrix_and_sampling();')
p.write_text(s,encoding='utf-8')
print('Updated existing LED/sleep fixtures and added mode-color/NTC behavior checks.')
