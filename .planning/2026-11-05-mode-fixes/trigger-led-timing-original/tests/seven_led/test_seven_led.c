#include <string.h>

volatile uint32_t test_failure_line, test_case, test_checks;
volatile uint32_t test_boot_scenario;
#define CHECK(x) do { test_checks++; if(!(x)) { test_failure_line = __LINE__; return; } } while(0)

static void test_alarm_mapping(void)
{
	/* Literal bit/flash pairs from the reference, plus the new fourth OCP stage. */
	const uint32_t faults[] = {1u, 1u<<12, 1u<<11, 1u<<19, 2u, 4u, 8u, 16u,
		1u<<6, 1u<<7, 1u<<8, 1u<<18, 1u<<9, 1u<<14, 1u<<15, 1u<<20, 32u, 1u<<13};
	const uint8_t flashes[] = {1, 2, 3, 3, 5, 5, 5, 5, 6, 6, 6, 7, 8, 9, 9, 12, 13, 14};
	unsigned i;
	for(i=0; i<sizeof(flashes); i++)
	{
		memset(&machine_error_hold, 0, sizeof(machine_error_hold));
		memset(&user_led_control, 0, sizeof(user_led_control));
		machine_error_hold.userErr_hold.word=faults[i];
		CHECK(LedError_Control()==1);
		CHECK(user_led_control.LedRunCnt_Once==flashes[i]);
	}
	machine_error_hold.userErr_hold.word=0;
	memset(&user_led_control, 0, sizeof(user_led_control));
	machine_error_hold.MC_error_hold.bits.gBlock=1;
	CHECK(LedError_Control() && user_led_control.LedRunCnt_Once==4);
	machine_error_hold.MC_error_hold.word=0;
	memset(&user_led_control, 0, sizeof(user_led_control));
	machine_error_hold.MC_error_hold.bits.gLackPhase=1;
	CHECK(LedError_Control() && user_led_control.LedRunCnt_Once==10);
	machine_error_hold.MC_error_hold.word=0;
	memset(&user_led_control, 0, sizeof(user_led_control));
	machine_error_hold.MC_error_hold.bits.gLosStep=1;
	CHECK(LedError_Control() && user_led_control.LedRunCnt_Once==11);
	/* Simultaneous errors retain the reference's hardware-OCP priority. */
	machine_error_hold.userErr_hold.word=1;
	memset(&user_led_control, 0, sizeof(user_led_control));
	CHECK(LedError_Control() && user_led_control.LedRunCnt_Once==1);
}

static void test_matrix_and_sampling(void)
{
	unsigned led, slot;
	/* 新图实际二极管方向：LED1 L1->L2，LED2 L1->L3，LED3 L3->L1。 */
	for(led=1; led<=3; led++)
	{
		GPIOA->CRL=0x77777770;
		GPIOA->CRH=0x44444444;
		GPIOB->CRH=0x44444444;
		GPIOA->ODR=0x1800; /* SDA/SCL电平由驱动独占，扫描不得改写。 */
		GPIOB->ODR=0x200;  /* EN=PB9必须持续保持。 */
		led_init();
		CHECK((GPIOA->CRL & 0xfffffff0)==0x77777770);
		CHECK((GPIOA->ODR & 0x1800)==0x1800);
		CHECK(GPIOB->ODR & 0x200);
		user_list.user_state=TOOL_STOP;
		user_list.led_scan_start_flag=1;
		ledData.Word=1u<<led;
		for(slot=1; slot<=3; slot++)
		{
			GPIOA->IDR=(slot==2 ? 0 : 0x2001);
			LED_Scan();
			CHECK((GPIOA->ODR & 0x1800)==0x1800 && (GPIOB->ODR & 0x200));
			CHECK(user_list.led_input_valid);
			CHECK(user_list.Dir_Data==(slot==2 ? 0 : 1));
			CHECK(user_list.Key_Data==(slot==2 ? 0 : 1));
			CHECK(((GPIOA->CRH>>20)&0xf)==8); /* DIR为独立上拉输入。 */
			CHECK((GPIOA->CRL&0xf)==8); /* K1为独立上拉输入。 */
			if(slot==led)
			{
				CHECK(((GPIOA->CRH>>24)&0xf)==1); /* L1=PA14 */
				CHECK(((GPIOA->CRH>>8)&0xf)==(led==1 ? 1 : 0)); /* L2=PA10 */
				CHECK((GPIOB->CRH&0xf)==(led==1 ? 0 : 1)); /* L3=PB8 */
				CHECK(!!(GPIOA->ODR&0x4000)==(led==3 ? 0 : 1));
				if(led==1) CHECK(!(GPIOA->ODR&0x400));
				else CHECK(!!(GPIOB->ODR&0x100)==(led==3 ? 1 : 0));
			}
			else
			{
				CHECK(((GPIOA->CRH>>24)&0xf)==0);
				CHECK(((GPIOA->CRH>>8)&0xf)==0 && (GPIOB->CRH&0xf)==0);
			}
		}
	}
	user_list.user_state=TOOL_POWER_DOWN;
	ledData.Word=0xfe;
	LED_Scan();
	CHECK(((GPIOA->CRH>>24)&0xf)==0 && ((GPIOA->CRH>>8)&0xf)==0 && (GPIOB->CRH&0xf)==0);
	CHECK((GPIOA->ODR&0x1800)==0x1800 && (GPIOB->ODR&0x200));
	user_list.led_scan_start_flag=0;
	LED_Scan();
	CHECK(!user_list.led_input_valid);
}

static void test_swd_startup_not_remapped_early(void)
{
	uint32_t mode_a, mode_b;
	memset(&user_list, 0, sizeof(user_list));
	GPIOA->AFRH=0xf00fffff;
	GPIOA->CRH=0x44444444;
	GPIOB->CRH=0x44444444;
	Bsp_Gpio_Init(); /* 校准前不接管PA13/14，保留原有烧录连接窗口。 */
	CHECK((GPIOA->AFRH & 0x0ff00000)==0);
	CHECK(((GPIOA->CRH>>20)&0xff)==0x44);
	mode_a=GPIOA->CRH;
	mode_b=GPIOB->CRH;
	LED_Scan();
	CHECK(GPIOA->CRH==mode_a && GPIOB->CRH==mode_b);
	led_init();
	CHECK((GPIOA->AFRH & 0x0ff00000)==0x01100000);
	CHECK(((GPIOA->CRH>>20)&0xff)==0x08); /* PA13上拉、PA14模拟高阻。 */
	CHECK((GPIOB->CRH&0xf)==0);
}

static void test_direction_and_key(void)
{
	unsigned i;
	memset(&user_list, 0, sizeof(user_list));
	memset(&mc_core_list, 0, sizeof(mc_core_list));
	memset(&motor_control_list, 0, sizeof(motor_control_list));
	motor_control_list.bldc_state=MOTOR_STOP;
	user_list.flag.bits.gToolTrigger=1;
	user_direction_handle();
	CHECK(!user_list.direction_ready && user_list.dir_retrig_need);
	user_list.led_input_valid=1;
	user_list.Dir_Data=0;
	GPIOB->IDR=0xffff; /* 点灯阶段IDR与缓存相反，逻辑仍应读取缓存。 */
	for(i=0;i<12;i++) user_direction_handle();
	/* 正常按扳机上电：首次方向稳定后不应要求松手重扣。 */
	CHECK(user_list.direction_ready && user_list.dir_valid && !user_list.dir_retrig_need);
	user_list.flag.bits.gToolTrigger=0;
	user_direction_handle();
	CHECK(user_list.direction_ready && user_list.dir_valid && mc_core_list.mc_dir==0);
	user_list.flag.bits.gToolTrigger=1;
	motor_control_list.bldc_state=MOTOR_RUN;
	mc_core_list.motor_en=1;
	user_list.Dir_Data=1;
	for(i=0;i<12;i++) user_direction_handle();
	CHECK(!user_list.dir_valid && user_list.dir_retrig_need && mc_core_list.mc_dir==0);
	motor_control_list.bldc_state=MOTOR_HBRAKE;
	user_list.flag.bits.gToolTrigger=0;
	user_direction_handle();
	CHECK(mc_core_list.mc_dir==0 && !user_list.direction_ready);
	motor_control_list.bldc_state=MOTOR_STOP;
	mc_core_list.motor_en=0;
	user_list.flag.bits.gToolTrigger=1;
	user_direction_handle();
	CHECK(mc_core_list.mc_dir==0 && !user_list.direction_ready);
	user_list.flag.bits.gToolTrigger=0;
	user_direction_handle();
	CHECK(mc_core_list.mc_dir==1 && user_list.direction_ready);
	user_list.Key_Data=0;
	for(i=0;i<60;i++) CHECK(key_scan()==0);
	user_list.Key_Data=1;
	CHECK(key_scan()==1);
	CHECK(key_scan()==0);
	user_list.led_input_valid=0;
	user_list.Key_Data=0;
	for(i=0;i<60;i++) CHECK(key_scan()==0);
	user_list.led_input_valid=1;
	user_list.Key_Data=1;
	CHECK(key_scan()==0);
}

static void test_blink_cycle(void)
{
	unsigned ticks=0, rises=0;
	uint8_t was_on=0;
	memset(&machine_error_hold, 0, sizeof(machine_error_hold));
	memset(&user_led_control, 0, sizeof(user_led_control));
	machine_error_hold.userErr_hold.bits.gIBusAvgOCP4=1;
	LedError_Control();
	while(user_led_control.LedRunTimes==10 && ticks++<12000)
	{
		LedError_Run();
		CHECK(LED1==LED2 && LED2==LED3);
		if(LED1 && !was_on) rises++;
		was_on=LED1;
	}
	CHECK(rises==13 && user_led_control.LedRunTimes==9);
}

static void test_soc_immediate_run(void)
{
	unsigned i;
	memset(&user_list, 0, sizeof(user_list));
	memset(&machine_error_hold, 0, sizeof(machine_error_hold));
	memset(&user_led_control, 0, sizeof(user_led_control));
	led_init();
	user_list.led_scan_start_flag=1;
	user_list.user_state=TOOL_RUN;
	user_list.flag.bits.gToolTrigger=1;
	user_list.VBus_Vx100=2100;
	UO_Led_Handler();
	CHECK((ledData.Word & 0x0e)==0x0e); /* 未经过停机等待，首次运行也应亮三灯 */
	LED_Scan();
	CHECK(((GPIOA->CRH>>24) & 0xf)==1 && !(GPIOA->ODR & GPIO_Pin_10));
	CHECK(GPIOA->ODR & GPIO_Pin_13); /* 独立方向上拉不被电量灯扫描拉低。 */
	/* 保留负载压降期间电量灯稳定，避免简单删除运行状态门控。 */
	user_list.VBus_Vx100=1500;
	for(i=0;i<1000;i++)
	{
		UO_Led_Handler();
		CHECK((ledData.Word & 0x0e)==0x0e);
	}
	user_list.user_state=TOOL_STOP;
	user_list.flag.bits.gToolTrigger=0;
	user_list.VBus_Vx100=1780;
	for(i=0;i<100;i++) UO_Led_Handler();
	CHECK((ledData.Word & 0x0e)==0x06); /* 松手后仍能更新为两灯 */
	user_list.user_state=TOOL_RUN;
	user_list.flag.bits.gToolTrigger=1;
	machine_error_hold.MC_error_hold.bits.gBlock=1;
	UO_Led_Handler();
	CHECK((ledData.Word & 0x0e)==0 && user_led_control.LedRunCnt_Once==4);
	for(i=0;i<500;i++) UO_Led_Handler();
	CHECK((ledData.Word & 0x0e)==0x0e); /* 故障显示仍优先于电量 */
}

static void test_soc_boot_levels(void)
{
	/* 由运行器为每组新建CPU，确保都是上电时尚未初始化的SOC。 */
	const uint16_t voltage[]={2100,1780,1680,1500};
	const uint8_t lamps[]={0x0e,0x06,0x02,0};
	unsigned i, index=test_boot_scenario-1;
	led_init();
	user_list.user_state=TOOL_RUN;
	user_list.flag.bits.gToolTrigger=1;
	user_list.VBus_Vx100=voltage[index];
	UO_Led_Handler();
	CHECK((ledData.Word & 0x0e)==lamps[index]);
	if(index==3)
	{
		for(i=1;i<250;i++) UO_Led_Handler();
		CHECK((ledData.Word & 0x0e)==0x02); /* 最低电量仍按参考程序闪第1灯 */
	}
}

static void reset_light_fixture(void)
{
	memset(&user_list, 0, sizeof(user_list));
	memset(&machine_error_hold, 0, sizeof(machine_error_hold));
	memset(&mc_core_list, 0, sizeof(mc_core_list));
	memset(&motor_control_list, 0, sizeof(motor_control_list));
	memset(&user_led_control, 0, sizeof(user_led_control));
	led_init();
	tool_mode_init();
	GPIO_SetBits(EN_PORT, EN_PIN);
	user_list.led_scan_start_flag=1;
	user_list.dir_valid=1;
	user_list.direction_ready=1;
	user_list.user_state=TOOL_RUN;
	user_list.flag.bits.gToolTrigger=1;
	user_list.VBus_Vx100=2100;
}

static void run_light_tick(void)
{
	/* 与main的1ms任务顺序相同：先状态机，再灯显示。 */
	user_state_control();
	UO_Led_Handler();
}

static void test_release_sleep_and_repress(void)
{
	unsigned i;
	reset_light_fixture();
	run_light_tick();
	CHECK(TM1650_GetDigit(0)==0x40);
	CHECK((ledData.Word & 0x0e)!=0);
	user_list.flag.bits.gToolTrigger=0;
	run_light_tick(); /* RUN -> STOP，下一次1ms任务开始休眠计数。 */
	for(i=0;i<1000;i++) run_light_tick();
	CHECK(user_list.user_state==TOOL_STOP && (TM1650_GetDigit(0)==0x40));
	user_list.flag.bits.gToolTrigger=1;
	run_light_tick();
	CHECK(user_list.user_state==TOOL_RUN && user_list.sleep_cnt==0);
	user_list.flag.bits.gToolTrigger=0;
	run_light_tick();
	for(i=0;i<5000;i++)
	{
		run_light_tick();
		CHECK(user_list.user_state==TOOL_STOP);
		CHECK((TM1650_GetDigit(0)==0x40) && (ledData.Word & 0x0e));
	}
	CHECK(user_list.sleep_cnt==5000);
	run_light_tick();
	CHECK(user_list.user_state==TOOL_POWER_DOWN); /* 与参考一致：计数>5000 */
	CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==0 && ledData.Word==0);
	CHECK(!user_list.flag.bits.gToolEn);
	run_light_tick();
	CHECK(!(EN_PORT->ODR & EN_PIN));
	for(i=0;i<9;i++) LED_Scan();
	CHECK(!(GPIOA->CRH & GPIO_CRH_MODE14) && !(GPIOB->CRH & GPIO_CRH_MODE8));
	user_list.flag.bits.gToolTrigger=1;
	run_light_tick();
	CHECK(user_list.user_state==TOOL_STOP && (EN_PORT->ODR & EN_PIN));
	CHECK((TM1650_GetDigit(0)==0x40) && (ledData.Word & 0x0e));
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
		CHECK(TM1650_GetDigit(0)==0x40); /* 故障长按超过30s也不再关闭照明。 */
		CHECK(!user_list.flag.bits.gToolEn);
	}
	CHECK(user_led_control.LedRunCnt_Once==3);
	user_list.flag.bits.gToolTrigger=0;
	for(i=0;i<5000;i++)
	{
		run_light_tick();
		CHECK(user_list.user_state==TOOL_ERROR_STOP);
		CHECK((TM1650_GetDigit(0)==0x40) && (ledData.Word & 0x0e));
	}
	CHECK(user_list.sleep_cnt==5000);
	run_light_tick();
	CHECK(user_list.user_state==TOOL_POWER_DOWN);
	CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==0 && ledData.Word==0);
	CHECK(machine_error_hold.userErr_hold.bits.gNtcMos); /* 关灯不得清保护。 */
	run_light_tick();
	CHECK(!(EN_PORT->ODR & EN_PIN));
}

static void test_mode_led_and_ntc(void)
{
	unsigned i;
	/* 实板调速：DIG1绿色，LED8/G到LED5/D；情景保持LED5/D到LED7/F。 */
	const uint8_t speed_masks[]={0x40,0x20,0x10,0x08};
	const uint8_t scene_masks[]={0x08,0x10,0x20};
	memset(&user_list,0,sizeof(user_list));
	memset(&machine_error_hold,0,sizeof(machine_error_hold));
	user_list.user_state=TOOL_STOP;
	user_list.VBus_Vx100=2100;
	tool_mode_init();
	for(i=0;i<4;i++)
	{
		tool_mode_list.speed_gear=i+1;
		UO_Led_Handler();
		CHECK(TM1650_GetDigit(0)==speed_masks[i] && TM1650_GetDigit(1)==0);
		CHECK(TM1650_GetDigit(2)==0 && TM1650_GetDigit(3)==0);
	}
	tool_mode_list.mode=TOOL_MODE_SCENE;
	for(i=0;i<3;i++)
	{
		tool_mode_list.scene_gear=i+1;
		UO_Led_Handler();
		CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==scene_masks[i]);
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

uint32_t run_tests(void)
{
	tool_mode_init();
	if(test_boot_scenario==5)
	{
		test_case=8;
		test_swd_startup_not_remapped_early();
		return test_failure_line;
	}
	if(test_boot_scenario)
	{
		test_case=1;
		test_soc_boot_levels();
		return test_failure_line;
	}
	test_case=1;
	test_alarm_mapping();
	if(test_failure_line) return test_failure_line;
	test_case=2;
	test_mode_led_and_ntc();
	if(test_failure_line) return test_failure_line;
	test_matrix_and_sampling();
	if(test_failure_line) return test_failure_line;
	test_case=3;
	test_direction_and_key();
	if(test_failure_line) return test_failure_line;
	test_case=4;
	test_blink_cycle();
	if(test_failure_line) return test_failure_line;
	test_case=5;
	test_soc_immediate_run();
	if(test_failure_line) return test_failure_line;
	test_case=6;
	test_release_sleep_and_repress();
	if(test_failure_line) return test_failure_line;
	test_case=7;
	test_fault_hold_and_release_sleep();
	if(test_failure_line) return test_failure_line;
	test_case=8;
	test_swd_startup_not_remapped_early();
	return test_failure_line;
}
