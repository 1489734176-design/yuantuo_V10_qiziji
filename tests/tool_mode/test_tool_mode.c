/* 运行真实控制/存储代码；仅TM1650与Flash物理操作由模拟硬件替代。 */
volatile uint32_t test_failure_line, test_case, test_checks;
#define CHECK(expr) do { test_checks++; if(!(expr)) { test_failure_line=__LINE__; return; } } while(0)
#define PAGE30 0x08007800u
#define PAGE31 0x08007c00u
static int32_t fail_after=-1;
static uint32_t flash_operations;
static uint8_t key_raw, key_ack=1;
static uint8_t erase_bad_readback, program_bad_readback;
static uint16_t last_pwm;
void PwmDutyUpdate(uint16_t duty) { last_pwm=duty; }

void FLASH_Unlock(void) {}
void FLASH_Lock(void) {}
void FLASH_ClearFlag(uint16_t flags) { (void)flags; }
FLASH_Status FLASH_ErasePage(uint32_t address)
{
	uint16_t i;
	volatile uint16_t *page=(volatile uint16_t *)address;
	flash_operations++;
	for(i=0;i<512;i++)
	{
		/* 用半页擦除失败验证另一页最新记录始终保留。 */
		if(fail_after==0) return FLASH_TIMEOUT;
		page[i]=0xffff;
		if(fail_after>0) fail_after--;
	}
	if(erase_bad_readback) page[0]=0xa65a;
	return FLASH_COMPLETE;
}
FLASH_Status FLASH_ProgramHalfWord(uint32_t address,uint16_t value)
{
	volatile uint16_t *cell=(volatile uint16_t *)address;
	flash_operations++;
	if(fail_after==0) return FLASH_TIMEOUT;
	if((*cell & value)!=value) return FLASH_ERROR_PG;
	*cell &= value;
	if(program_bad_readback) *cell^=1;
	if(fail_after>0) fail_after--;
	return FLASH_COMPLETE;
}
uint8_t TM1650_ReadKey(uint8_t *key) { *key=key_raw; return key_ack; }

static void erase_fixture(void)
{
	volatile uint16_t *page=(volatile uint16_t *)PAGE30;
	uint16_t i;
	for(i=0;i<1024;i++) page[i]=0xffff;
	fail_after=-1;
	flash_operations=0;
	erase_bad_readback=0;
	program_bad_readback=0;
}

#if !TEST_BASELINE
/* 库内部电机初始化由空硬件桩替代，应用mc_init和Flash恢复使用真实代码。 */
void mc_core_list_init(const McCore_Param_t *p) { (void)p; }
/** @brief 松扳机并停机后执行实际初始化及Flash恢复，建立各用例统一上电状态。 */
static void boot_fixture(void)
{
	user_list.flag.bits.gToolTrigger=0;
	user_list.flag.bits.gToolEn=0;
	mc_core_list.motor_en=0;
	motor_control_list.bldc_state=MOTOR_STOP;
	key_raw=0;
	key_ack=1;
	user_para_init();
	read_user_data();
}
static void key_ms(uint16_t milliseconds)
{
	while(milliseconds--) tool_mode_key_handle();
}
static void press(uint8_t raw)
{
	key_raw=raw;
	key_ms(30);
	key_raw=0;
	key_ms(30);
}
/** @brief 模拟已经通过既有方向安全门控的硬件方向，用真实API选择对应记忆。 */
static void direction(uint8_t reverse)
{
	user_list.flag.bits.gToolDir=reverse ? Tool_Dir_CCW_Value : Tool_Dir_CW_Value;
	tool_mode_set_direction(reverse);
}
/** @brief 验证反转K3仅切调速与情景4，K2恢复并循环反转调速记忆。 */
static void test_reverse_keys(void)
{
	erase_fixture(); boot_fixture();
	direction(TOOL_DIRECTION_REVERSE);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	CHECK(tool_mode_current()->scene_gear==4);
	tool_mode_key_event(0x47);
	CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x46);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==4);
	CHECK(tool_mode_duty(281)==1874);
	tool_mode_key_event(0x46);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==2);
	tool_mode_key_event(0x46);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==4);
	tool_mode_key_event(0x47);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==2);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==3);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==4);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==1);
}
/** @brief 验证正转既有K2四挡和K3三挡循环保持生效。 */
static void test_modes(void)
{
	erase_fixture(); boot_fixture();
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	CHECK(tool_mode_list.save_pending==0);
	tool_mode_key_event(0x46);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
	tool_mode_key_event(0x46);
	CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x47);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==3);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==4);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==1);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==3);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==1);
	tool_mode_key_event(0x45); CHECK(tool_mode_gear()==1 && tool_mode_current()->mode==TOOL_MODE_SCENE);
}
/** @brief 验证按下释放去抖、按住不重复及未知键锁定。 */
static void test_key_debounce(void)
{
	uint8_t tick;
	erase_fixture(); boot_fixture();
	for(tick=1;tick<=10;tick++) CHECK(tool_mode_key_handle()==(tick==10));
	key_raw=0x47; key_ms(20); key_raw=0; key_ms(30);
	CHECK(tool_mode_gear()==1);
	key_raw=0x47; key_ms(30);
	CHECK(tool_mode_gear()==2);
	key_ms(2000); CHECK(tool_mode_gear()==2);
	key_ack=0; key_ms(100); key_ack=1; key_ms(100);
	CHECK(tool_mode_gear()==2);
	key_raw=0; key_ms(20); key_raw=0x47; key_ms(100);
	CHECK(tool_mode_gear()==2);
	key_raw=0; key_ms(30); press(0x47); CHECK(tool_mode_gear()==3);
	/* 其他键及组合键不能冒充K2；换键前必须确认全部释放。 */
	key_raw=0x45; key_ms(30); key_raw=0x47; key_ms(30);
	CHECK(tool_mode_gear()==3);
	key_raw=0; key_ms(30); press(0x46);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
	key_raw=0xc7; key_ms(30); key_raw=0x47; key_ms(30);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
	/* bit6未置位即释放；随后三次有效K2才允许切回调速。 */
	key_raw=0x87; key_ms(30); press(0x47);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==3);
}
static void test_duty(void)
{
	/* 当前38/58/78/100%挡位表，周期1874；中点扳机输入1077。 */
	static const uint16_t maximum[]={712,1086,1461,1874};
	static const uint16_t middle[]={496,683,870,1077};
	uint8_t i;
	erase_fixture(); boot_fixture();
	for(i=0;i<4;i++)
	{
		CHECK(tool_mode_duty(0)==281);
		CHECK(tool_mode_duty(281)==281);
		CHECK(tool_mode_duty(1077)==middle[i]);
		CHECK(tool_mode_duty(1874)==maximum[i]);
		CHECK(tool_mode_duty(65535)==maximum[i]);
		mc_core_list.Board_ADC_DATA.u16VR=4000;
		pwm_duty_aim_deal(); user_gears_handle();
		CHECK(motor_control_list.pwm_duty_aim==maximum[i]);
		tool_mode_key_event(0x47);
	}
	tool_mode_key_event(0x46);
	CHECK(tool_mode_duty(281)==337 && tool_mode_duty(1874)==337);
	tool_mode_key_event(0x46); CHECK(tool_mode_duty(281)==1874);
	tool_mode_key_event(0x46); CHECK(tool_mode_duty(281)==1874);
}
/** @brief 验证扳机按住屏蔽按键，松开仍须有效释放后重新按下。 */
static void test_trigger_key_inhibit(void)
{
	erase_fixture(); boot_fixture();
	user_list.sleep_cnt=1234;
	user_list.flag.bits.gToolTrigger=1;
	key_raw=0x46; key_ms(100);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	CHECK(!tool_mode_list.save_pending && user_list.sleep_cnt==1234);
	tool_mode_key_event(0x47);
	CHECK(tool_mode_gear()==1 && !tool_mode_list.save_pending);
	/* 松扳机时K3仍按住，不得补执行被屏蔽的按压。 */
	user_list.flag.bits.gToolTrigger=0; key_ms(100);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED);
	key_ack=0; key_raw=0; key_ms(100); key_ack=1;
	key_raw=0x46; key_ms(100);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED);
	key_raw=0; key_ms(30); press(0x46);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
	user_list.flag.bits.gToolTrigger=1; key_raw=0x47; key_ms(100);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE);
	user_list.flag.bits.gToolTrigger=0; key_ms(100);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE);
	key_raw=0; key_ms(30); press(0x47);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	/* 未完成的20ms去抖被1ms扣动打断，松手后不得补执行。 */
	key_raw=0x47; key_ms(20);
	user_list.flag.bits.gToolTrigger=1; key_ms(1);
	user_list.flag.bits.gToolTrigger=0; key_ms(100);
	CHECK(tool_mode_gear()==1);
	key_raw=0; key_ms(30); press(0x47); CHECK(tool_mode_gear()==2);
	user_list.user_state=TOOL_POWER_DOWN;
	tool_mode_key_event(0x46);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==2);
}

/** @brief 验证Flash只在松扳机且完全停机时写入，重启恢复当前方向各挡。 */
static void test_save_and_gates(void)
{
	uint32_t before;
	erase_fixture(); boot_fixture(); tool_mode_save_handle(); CHECK(flash_operations==0);
	tool_mode_key_event(0x47);
	user_list.flag.bits.gToolTrigger=1; tool_mode_save_handle(); CHECK(flash_operations==0);
	user_list.flag.bits.gToolTrigger=0; motor_control_list.bldc_state=MOTOR_HBRAKE;
	tool_mode_save_handle(); CHECK(flash_operations==0);
	motor_control_list.bldc_state=MOTOR_STOP; mc_core_list.motor_en=1;
	tool_mode_save_handle(); CHECK(flash_operations==0);
	mc_core_list.motor_en=0; user_list.flag.bits.gToolEn=1;
	tool_mode_save_handle(); CHECK(flash_operations==0);
	user_list.flag.bits.gToolEn=0; tool_mode_save_handle();
	CHECK(tool_mode_list.save_pending==0 && flash_operations>0);
	before=flash_operations; tool_mode_save_handle(); CHECK(before==flash_operations);
	tool_mode_key_event(0x46); tool_mode_key_event(0x46); tool_mode_save_handle();
	boot_fixture();
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_current()->speed_gear==2 && tool_mode_current()->scene_gear==2);
}
static void test_partial_and_crc(void)
{
	uint8_t cut;
	for(cut=0;cut<8;cut++)
	{
		erase_fixture(); boot_fixture(); tool_mode_key_event(0x47); tool_mode_save_handle();
		tool_mode_key_event(0x47); fail_after=cut; tool_mode_save_handle();
		CHECK(tool_mode_list.save_pending==1);
		boot_fixture(); CHECK(tool_mode_gear()==2);
		fail_after=-1; tool_mode_key_event(0x47); tool_mode_save_handle();
		boot_fixture(); CHECK(tool_mode_gear()==3);
	}
	erase_fixture(); boot_fixture(); tool_mode_key_event(0x47); tool_mode_save_handle();
	tool_mode_key_event(0x47); tool_mode_save_handle();
	/* 更晚记录的数据损坏时回退前一条有效设置。 */
	*((volatile uint16_t *)(PAGE30+20+6))=0;
	boot_fixture(); CHECK(tool_mode_gear()==2);
}
static void test_rollover_power_failure(void)
{
	uint16_t i;
	erase_fixture(); boot_fixture();
	for(i=0;i<51;i++) { tool_mode_key_event(0x47); tool_mode_save_handle(); }
	CHECK(tool_mode_gear()==4);
	/* 下一页擦到一半失败，掉电重启仍从旧页恢复。 */
	*((volatile uint16_t *)PAGE31)=0;
	tool_mode_key_event(0x47); fail_after=256; tool_mode_save_handle();
	CHECK(tool_mode_list.save_pending==1);
	boot_fixture(); CHECK(tool_mode_gear()==4);
	fail_after=-1; tool_mode_key_event(0x47); tool_mode_save_handle();
	boot_fixture(); CHECK(tool_mode_gear()==1);
	for(i=0;i<51;i++) { tool_mode_key_event(0x47); tool_mode_save_handle(); }
	boot_fixture(); CHECK(tool_mode_gear()==4);
}
/** @brief 验证旧三半字记录恢复正转，迁移失败仍保留原记录。 */
static void test_legacy(void)
{
	volatile uint16_t *legacy=(volatile uint16_t *)(PAGE31+9*20);
	erase_fixture(); legacy[0]=0x55aa; legacy[1]=1; legacy[2]=1; boot_fixture();
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_current()->speed_gear==2 && tool_mode_current()->scene_gear==1);
	CHECK(tool_mode_list.setting[TOOL_DIRECTION_REVERSE].mode==TOOL_MODE_SPEED);
	CHECK(tool_mode_list.setting[TOOL_DIRECTION_REVERSE].speed_gear==1);
	CHECK(tool_mode_list.setting[TOOL_DIRECTION_REVERSE].scene_gear==4);
	CHECK(user_list.flag.bits.gAutoStopEn==1 && tool_mode_list.save_pending==0);
	tool_mode_key_event(0x46); fail_after=3; tool_mode_save_handle();
	boot_fixture(); CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==2);
	fail_after=-1; tool_mode_key_event(0x46); tool_mode_save_handle(); boot_fixture();
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
}
static void test_flash_readback(void)
{
	uint16_t i;
	erase_fixture(); boot_fixture(); tool_mode_key_event(0x47);
	program_bad_readback=1; tool_mode_save_handle();
	CHECK(tool_mode_list.save_pending==1);
	boot_fixture(); CHECK(tool_mode_gear()==1);
	erase_fixture(); boot_fixture();
	for(i=0;i<51;i++) { tool_mode_key_event(0x47); tool_mode_save_handle(); }
	CHECK(tool_mode_gear()==4);
	/* 擦除HAL报告成功但标志位未擦净，不能在旧magic下写入新数据。 */
	erase_bad_readback=1; tool_mode_key_event(0x47); tool_mode_save_handle();
	CHECK(tool_mode_list.save_pending==1);
	boot_fixture(); CHECK(tool_mode_gear()==4);
}
static void test_rollover_cutpoints(void)
{
	static uint16_t backup[1024];
	static const uint16_t cuts[]={0,1,255,511,512,513,514,515,516,517,518,519};
	volatile uint16_t *pages=(volatile uint16_t *)PAGE30;
	uint16_t i;
	uint8_t cut;
	erase_fixture(); boot_fixture();
	for(i=0;i<51;i++) { tool_mode_key_event(0x47); tool_mode_save_handle(); }
	for(i=0;i<1024;i++) backup[i]=pages[i];
	for(cut=0;cut<sizeof(cuts)/sizeof(cuts[0]);cut++)
	{
		for(i=0;i<1024;i++) pages[i]=backup[i];
		boot_fixture(); tool_mode_key_event(0x47); fail_after=cuts[cut]; tool_mode_save_handle();
		CHECK(tool_mode_list.save_pending==1);
		boot_fixture(); CHECK(tool_mode_gear()==4);
		fail_after=-1; tool_mode_key_event(0x47); tool_mode_save_handle();
		boot_fixture(); CHECK(tool_mode_gear()==1);
	}
}
/** @brief 验证v1兼容、无效版本及范围拒绝和32位序号自然回绕。 */
static void test_versions_ranges_and_wrap(void)
{
	/* 由独立Python binascii.crc_hqx计算的完整字面记录，覆盖版本/范围/回绕判定。 */
	static const uint16_t before_wrap[]={0xa65a,1,0,4,1,0xffff,0xffff,0x548e};
	static const uint16_t after_wrap[]={0xa65a,1,1,3,2,0,0,0xabfc};
	static const uint16_t bad_version[]={0xa65a,3,0,4,1,1,0,0xbd1f};
	static const uint16_t bad_range[]={0xa65a,1,0,5,1,1,0,0xfc26};
	volatile uint16_t *p0=(volatile uint16_t *)PAGE30;
	volatile uint16_t *p1=(volatile uint16_t *)PAGE31;
	uint8_t i;
	erase_fixture();
	for(i=0;i<8;i++) { p0[i]=before_wrap[i]; p1[i]=after_wrap[i]; }
	boot_fixture(); CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_current()->speed_gear==3 && tool_mode_gear()==2);
	CHECK(tool_mode_list.setting[TOOL_DIRECTION_REVERSE].mode==TOOL_MODE_SPEED);
	CHECK(tool_mode_list.setting[TOOL_DIRECTION_REVERSE].speed_gear==1 && tool_mode_list.setting[TOOL_DIRECTION_REVERSE].scene_gear==4);
	for(i=0;i<8;i++) p1[10+i]=bad_version[i];
	boot_fixture(); CHECK(tool_mode_gear()==2);
	for(i=0;i<8;i++) p1[20+i]=bad_range[i];
	boot_fixture(); CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x46); tool_mode_save_handle(); boot_fixture(); CHECK(tool_mode_gear()==3);
	erase_fixture(); p1[0]=0x55aa; p1[1]=0; p1[2]=0; p1[90]=0x55aa;
	boot_fixture(); CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1 && flash_operations==0);
	tool_mode_current()->speed_gear=5; tool_mode_list.save_pending=1; tool_mode_save_handle();
	CHECK(tool_mode_list.save_pending==1 && flash_operations==0);
}
static void test_real_pwm_ramp(void)
{
	uint16_t i,previous;
	erase_fixture(); boot_fixture(); tool_mode_key_event(0x46);
	motor_control_list.pwm_duty_virtual=0;
	mc_core_list.pwm_actual_runing_data=0;
	motor_control_list.pwm_duty_aim=tool_mode_duty(281);
	pwm_set(); CHECK(last_pwm==281);
	previous=last_pwm;
	for(i=0;i<1200;i++)
	{
		pwm_set(); CHECK(last_pwm>=previous && last_pwm<=337); previous=last_pwm;
	}
	CHECK(last_pwm==337);
	tool_mode_key_event(0x46); motor_control_list.pwm_duty_aim=tool_mode_duty(281);
	for(i=0;i<12000;i++)
	{
		pwm_set(); CHECK(last_pwm>=previous && last_pwm<=1874); previous=last_pwm;
	}
	CHECK(last_pwm==1874);
}

/** @brief 建立反转情景4运行许可，实际PWM初值保持零以模拟既有软启动。 */
static void reverse_run_fixture(void)
{
	erase_fixture(); boot_fixture(); direction(TOOL_DIRECTION_REVERSE);
	tool_mode_key_event(0x46);
	user_list.flag.bits.gToolTrigger=1;
	user_list.flag.bits.gToolEn=1;
	mc_core_list.motor_en=1;
	motor_control_list.bldc_state=MOTOR_RUN;
	mc_core_list.pwm_actual_runing_data=0;
}
/** @brief 只驱动真实1ms接口，模拟中断持续计时而前台可暂时丢帧。 */
static void tick_ms(uint16_t milliseconds)
{
	while(milliseconds--) tool_mode_tick_1ms();
}
/** @brief 验证实际满值前不计时，满值后849/850ms边界及低目标持续保持。 */
static void test_reverse_hold_boundary(void)
{
	uint16_t i;
	reverse_run_fixture();
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(2000);
	CHECK(tool_mode_duty(1874)==1874);
	mc_core_list.pwm_actual_runing_data=1873;
	tick_ms(2000); CHECK(tool_mode_duty(281)==1874);
	mc_core_list.pwm_actual_runing_data=1874;
	tool_mode_pwm_update(1874);
	CHECK(tool_mode_duty(281)==1874);
	/* 前台重复计算目标不能把850ms计时提前耗尽。 */
	for(i=0;i<2000;i++) CHECK(tool_mode_duty(281)==1874);
	tick_ms(849); CHECK(tool_mode_duty(281)==1874);
	/* PWM持续发布满值不能改写首次满值时间戳。 */
	tool_mode_pwm_update(1874);
	tick_ms(1); CHECK(tool_mode_duty(281)==281);
	mc_core_list.pwm_actual_runing_data=281;
	tick_ms(5000); CHECK(tool_mode_duty(1874)==281);
}
/** @brief 验证保护降低实际PWM不延长850ms，前台丢帧后按真实时间切换。 */
static void test_reverse_hold_frontend_gap(void)
{
	reverse_run_fixture();
	mc_core_list.pwm_actual_runing_data=1874;
	tool_mode_pwm_update(1874);
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(400);
	mc_core_list.pwm_actual_runing_data=1873;
	tool_mode_pwm_update(1873);
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(450); CHECK(tool_mode_duty(281)==281);
	reverse_run_fixture();
	mc_core_list.pwm_actual_runing_data=1874;
	tool_mode_pwm_update(1874);
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(850); CHECK(tool_mode_duty(281)==281);
}
/** @brief 验证扳机、工具、电机和状态机任一失去许可后，新运行必须重新计满850ms。 */
static void test_reverse_hold_stop_and_fault_reset(void)
{
	uint8_t gate;
	for(gate=0;gate<6;gate++)
	{
		reverse_run_fixture();
		mc_core_list.pwm_actual_runing_data=1874;
		tool_mode_pwm_update(1874);
		CHECK(tool_mode_duty(281)==1874);
		tick_ms(800); CHECK(tool_mode_duty(281)==1874);
		if(gate==0) user_list.flag.bits.gToolTrigger=0;
		if(gate==1) user_list.flag.bits.gToolEn=0;
		if(gate==2) mc_core_list.motor_en=0;
		if(gate==3) motor_control_list.bldc_state=MOTOR_STOP;
		if(gate==4) motor_control_list.bldc_state=MOTOR_HBRAKE;
		if(gate==5) motor_control_list.bldc_state=MOTOR_POSITION;
		CHECK(tool_mode_duty(281)==1874);
		tick_ms(1000); CHECK(tool_mode_duty(281)==1874);
		user_list.flag.bits.gToolTrigger=1;
		user_list.flag.bits.gToolEn=1;
		mc_core_list.motor_en=1;
		motor_control_list.bldc_state=MOTOR_RUN;
		tool_mode_pwm_update(1874);
		CHECK(tool_mode_duty(281)==1874);
		tick_ms(849); CHECK(tool_mode_duty(281)==1874);
		tick_ms(1); CHECK(tool_mode_duty(281)==281);
	}
}
/** @brief 验证切出反转及重新选择情景均不会复用上一轮已经耗尽的满值阶段。 */
static void test_reverse_hold_direction_reset(void)
{
	reverse_run_fixture();
	mc_core_list.pwm_actual_runing_data=1874;
	tool_mode_pwm_update(1874);
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(850); CHECK(tool_mode_duty(281)==281);
	direction(TOOL_DIRECTION_FORWARD);
	CHECK(tool_mode_duty(1874)==712);
	direction(TOOL_DIRECTION_REVERSE);
	tool_mode_pwm_update(1874);
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(849); CHECK(tool_mode_duty(281)==1874);
	tick_ms(1); CHECK(tool_mode_duty(281)==281);
	user_list.flag.bits.gToolTrigger=0;
	tool_mode_key_event(0x46);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED);
	tool_mode_key_event(0x46);
	user_list.flag.bits.gToolTrigger=1;
	tool_mode_pwm_update(1874);
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(849); CHECK(tool_mode_duty(281)==1874);
	tick_ms(1); CHECK(tool_mode_duty(281)==281);
}
/** @brief 验证反转满目标和降至15%目标均继续通过实际pwm_set软斜坡。 */
static void test_reverse_real_pwm_ramps(void)
{
	uint16_t i,previous;
	reverse_run_fixture();
	motor_control_list.pwm_duty_virtual=0;
	motor_control_list.pwm_duty_aim=tool_mode_duty(281);
	pwm_set(); CHECK(last_pwm==281);
	previous=last_pwm;
	for(i=0;i<12000;i++)
	{
		pwm_set(); CHECK(last_pwm>=previous && last_pwm<=1874); previous=last_pwm;
	}
	CHECK(last_pwm==1874);
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(850);
	motor_control_list.pwm_duty_aim=tool_mode_duty(281);
	CHECK(motor_control_list.pwm_duty_aim==281);
	pwm_set(); CHECK(last_pwm>281 && last_pwm<=previous); previous=last_pwm;
	for(i=0;i<12000;i++)
	{
		pwm_set(); CHECK(last_pwm<=previous && last_pwm>=281); previous=last_pwm;
	}
	CHECK(last_pwm==281);
	tick_ms(3000); CHECK(tool_mode_duty(1874)==281);
}
/** @brief 实际PWM中断升到首次满值，期间按16次PWM调用驱动1ms时间，不让前台观察满值。 */
static void reverse_first_full_without_frontend(void)
{
	uint16_t interrupts;
	reverse_run_fixture();
	motor_control_list.pwm_duty_virtual=0;
	motor_control_list.pwm_duty_aim=tool_mode_duty(281);
	for(interrupts=0;interrupts<12000;interrupts++)
	{
		pwm_set();
		if(last_pwm==1874) return;
		if((interrupts%16u)==15u) tool_mode_tick_1ms();
	}
	test_failure_line=__LINE__;
}
/** @brief 验证首次实际满值在PWM中断捕获，前台延后850ms才计算目标仍立即降至15%。 */
static void test_reverse_pwm_first_full_frontend_gap(void)
{
	reverse_first_full_without_frontend();
	if(test_failure_line) return;
	CHECK(last_pwm==1874);
	tick_ms(850);
	CHECK(tool_mode_duty(281)==281);
}
/** @brief 验证实际PWM短暂满值后回落，前台未看到满值也不能丢掉850ms起点。 */
static void test_reverse_pwm_brief_full_frontend_gap(void)
{
	reverse_first_full_without_frontend();
	if(test_failure_line) return;
	CHECK(last_pwm==1874);
	tick_ms(400);
	/* 当前参数未启用pwm_set限流，夹具直接模拟保护降低实际占空比。 */
	mc_core_list.pwm_actual_runing_data=1873;
	tool_mode_pwm_update(1873);
	tick_ms(450);
	CHECK(tool_mode_duty(281)==281);
}
/** @brief 构造正转情景2/调速3、反转情景4/调速4，确保三个设置均独立记忆。 */
static void dual_setting_fixture(void)
{
	tool_mode_key_event(0x47); tool_mode_key_event(0x47);
	tool_mode_key_event(0x46); tool_mode_key_event(0x46);
	direction(TOOL_DIRECTION_REVERSE);
	tool_mode_key_event(0x47); tool_mode_key_event(0x47); tool_mode_key_event(0x47);
	tool_mode_key_event(0x46);
}
/** @brief 验证两个方向分别保存mode、speed_gear、scene_gear，切方向本身不产生Flash写请求。 */
static void test_dual_direction_memory(void)
{
	volatile uint16_t *record=(volatile uint16_t *)PAGE30;
	erase_fixture(); boot_fixture();
	direction(TOOL_DIRECTION_REVERSE); CHECK(!tool_mode_list.save_pending);
	direction(TOOL_DIRECTION_FORWARD); CHECK(!tool_mode_list.save_pending);
	dual_setting_fixture();
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==4);
	CHECK(tool_mode_current()->speed_gear==4);
	direction(TOOL_DIRECTION_FORWARD);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==2);
	CHECK(tool_mode_current()->speed_gear==3);
	direction(TOOL_DIRECTION_REVERSE);
	tool_mode_save_handle();
	CHECK(!tool_mode_list.save_pending);
	/* 记录8半字仍使用原20字节槽；方向模式和调速档分别压入低/高字节。 */
	CHECK(record[0]==0xa65a && record[1]==2 && record[2]==0x0101 && record[3]==0x0403);
	CHECK(record[4]==2 && record[5]==1 && record[6]==0 && record[8]==0xffff && record[9]==0xffff);
	boot_fixture();
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==2 && tool_mode_current()->speed_gear==3);
	direction(TOOL_DIRECTION_REVERSE);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==4 && tool_mode_current()->speed_gear==4);
	CHECK(!tool_mode_list.save_pending);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==4 && tool_mode_current()->mode==TOOL_MODE_SPEED);
	direction(TOOL_DIRECTION_FORWARD); CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==2);
}
/** @brief 验证双向记录任意半字中断写入后恢复同一完整旧记录，CRC损坏也同时回退两方向。 */
static void test_dual_direction_partial_and_crc(void)
{
	uint8_t cut;
	for(cut=0;cut<8;cut++)
	{
		erase_fixture(); boot_fixture(); dual_setting_fixture(); tool_mode_save_handle();
		tool_mode_key_event(0x46); tool_mode_key_event(0x47);
		direction(TOOL_DIRECTION_FORWARD); tool_mode_key_event(0x46);
		fail_after=cut; tool_mode_save_handle(); CHECK(tool_mode_list.save_pending);
		boot_fixture(); CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==2);
		direction(TOOL_DIRECTION_REVERSE);
		CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_current()->speed_gear==4 && tool_mode_gear()==4);
		fail_after=-1; tool_mode_key_event(0x46); tool_mode_save_handle();
		boot_fixture(); direction(TOOL_DIRECTION_REVERSE);
		CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==4);
	}
	erase_fixture(); boot_fixture(); dual_setting_fixture(); tool_mode_save_handle();
	tool_mode_key_event(0x46); tool_mode_save_handle();
	*((volatile uint16_t *)(PAGE30+20+4))^=0x0100;
	boot_fixture(); CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==2);
	direction(TOOL_DIRECTION_REVERSE); CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_current()->speed_gear==4);
}
/** @brief 验证独立计算CRC的v1记录兼容、v2字段范围拒绝及迁移中断不损坏v1。 */
static void test_v1_v2_migration_and_ranges(void)
{
	static const uint16_t v1[]={0xa65a,1,1,3,2,7,0,0xfad1};
	static const uint16_t v2[]={0xa65a,2,0x0101,0x0403,2,8,0,0x0145};
	static const uint16_t invalid[][8]={
		{0xa65a,2,0x0201,0x0403,2,9,0,0x5ab5},
		{0xa65a,2,0x0101,0x0003,2,9,0,0xb637},
		{0xa65a,2,0x0101,0x0503,2,9,0,0xcf90},
		{0xa65a,2,0x0101,0x0403,4,9,0,0xfa10}
	};
	volatile uint16_t *page=(volatile uint16_t *)PAGE30;
	uint8_t i,j,cut;
	for(cut=0;cut<8;cut++)
	{
		erase_fixture(); for(i=0;i<8;i++) page[i]=v1[i]; boot_fixture();
		CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==2 && tool_mode_current()->speed_gear==3);
		direction(TOOL_DIRECTION_REVERSE);
		CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1 && tool_mode_current()->scene_gear==4);
		tool_mode_key_event(0x46); fail_after=cut; tool_mode_save_handle(); CHECK(tool_mode_list.save_pending);
		boot_fixture(); direction(TOOL_DIRECTION_REVERSE); CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
		fail_after=-1; tool_mode_key_event(0x46); tool_mode_save_handle();
		boot_fixture(); direction(TOOL_DIRECTION_REVERSE); CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==4);
	}
	erase_fixture(); for(i=0;i<8;i++) { page[i]=v1[i]; page[10+i]=v2[i]; }
	for(j=0;j<sizeof(invalid)/sizeof(invalid[0]);j++)
	{
		for(i=0;i<8;i++) page[20+i]=invalid[j][i];
		boot_fixture(); direction(TOOL_DIRECTION_REVERSE);
		CHECK(tool_mode_current()->mode==TOOL_MODE_SCENE && tool_mode_gear()==4 && tool_mode_current()->speed_gear==4);
	}
	page[13]^=0x0100; boot_fixture(); direction(TOOL_DIRECTION_REVERSE);
	CHECK(tool_mode_current()->mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	/* 非当前方向的无效RAM设置也禁止保存，避免损坏另一方向记忆。 */
	tool_mode_current()->speed_gear=5; direction(TOOL_DIRECTION_FORWARD);
	tool_mode_list.save_pending=1; tool_mode_save_handle();
	CHECK(tool_mode_list.save_pending && flash_operations==0);
}
/** @brief 验证两个换向方向及四种模式组合，只有双调速沿用当前档位并标记变化待保存。 */
static void test_speed_direction_transfer(void)
{
	uint8_t source_dir,source_mode,target_mode,source_gear,target_gear;
	uint8_t target_dir,expected_gear,changed;
	tool_mode_setting_t *source;
	tool_mode_setting_t *target;
	for(source_dir=0;source_dir<2;source_dir++)
	for(source_mode=0;source_mode<2;source_mode++)
	for(target_mode=0;target_mode<2;target_mode++)
	for(source_gear=1;source_gear<=4;source_gear++)
	for(target_gear=1;target_gear<=4;target_gear++)
	{
		erase_fixture(); boot_fixture();
		target_dir=1u-source_dir;
		tool_mode_list.active_direction=source_dir;
		source=&tool_mode_list.setting[source_dir];
		target=&tool_mode_list.setting[target_dir];
		source->mode=source_mode;
		target->mode=target_mode;
		source->speed_gear=source_gear;
		target->speed_gear=target_gear;
		expected_gear=(source_mode==TOOL_MODE_SPEED && target_mode==TOOL_MODE_SPEED) ? source_gear : target_gear;
		changed=(expected_gear!=target_gear);
		direction(target_dir);
		CHECK(target->speed_gear==expected_gear && source->speed_gear==source_gear);
		CHECK(target->mode==target_mode && source->mode==source_mode);
		CHECK(tool_mode_list.active_direction==target_dir);
		CHECK(tool_mode_list.save_pending==changed);
		CHECK(tool_mode_gear()==(target_mode==TOOL_MODE_SPEED ? expected_gear : (target_dir==TOOL_DIRECTION_REVERSE ? 4 : 1)));
		CHECK(user_list.gears_level==tool_mode_gear()-1);
		/* 同方向重复确认不改变档位，也不丢弃已有的待保存标志。 */
		direction(target_dir);
		CHECK(target->speed_gear==expected_gear && tool_mode_list.save_pending==changed);
	}
}

/** @brief 验证正转1档覆盖反转旧3档，反向换回沿用当前4档，且新档位能掉电保存。 */
static void test_speed_transfer_save(void)
{
	erase_fixture(); boot_fixture();
	tool_mode_list.setting[TOOL_DIRECTION_REVERSE].speed_gear=3;
	direction(TOOL_DIRECTION_REVERSE);
	CHECK(tool_mode_current()->speed_gear==1 && tool_mode_duty(1874)==712);
	CHECK(tool_mode_list.save_pending);
	tool_mode_save_handle(); CHECK(!tool_mode_list.save_pending && flash_operations>0);
	boot_fixture();
	CHECK(tool_mode_list.setting[TOOL_DIRECTION_REVERSE].speed_gear==1);
	direction(TOOL_DIRECTION_REVERSE);
	tool_mode_key_event(0x47); tool_mode_key_event(0x47); tool_mode_key_event(0x47);
	direction(TOOL_DIRECTION_FORWARD);
	CHECK(tool_mode_current()->speed_gear==4 && tool_mode_duty(1874)==1874);
	tool_mode_save_handle(); boot_fixture();
	CHECK(tool_mode_current()->speed_gear==4);
	CHECK(tool_mode_list.setting[TOOL_DIRECTION_REVERSE].speed_gear==4);
}

/** @brief 验证首次反转上电恢复反转记忆，不将默认正转选择误判为实际换向。 */
static void test_boot_speed_direction_restore(void)
{
	uint8_t i;
	erase_fixture(); boot_fixture();
	tool_mode_list.setting[TOOL_DIRECTION_REVERSE].speed_gear=3;
	user_list.led_input_valid=1;
	user_list.Dir_Data=1;
	user_list.flag.bits.gToolTrigger=1;
	for(i=0;i<12;i++) user_direction_handle();
	CHECK(user_list.direction_initialized && user_list.direction_ready);
	CHECK(tool_mode_list.active_direction==TOOL_DIRECTION_REVERSE && tool_mode_gear()==3);
	CHECK(user_list.gears_level==2 && !tool_mode_list.save_pending);
	/* 启动方向确认后再真正切回正转，两边调速应沿用反转3档。 */
	user_list.flag.bits.gToolTrigger=0;
	user_list.Dir_Data=0;
	for(i=0;i<12;i++) user_direction_handle();
	CHECK(tool_mode_list.active_direction==TOOL_DIRECTION_FORWARD && tool_mode_gear()==3);
	CHECK(tool_mode_list.save_pending);
}
/** @brief 建立正转情景1档运行许可，并确认初始正常目标为18%。 */
static void scene_load_fixture(void)
{
	erase_fixture(); boot_fixture();
	machine_error_hold.userErr_hold.word=0;
	machine_error_hold.MC_error_hold.word=0;
	mc_core_list.MC_error.word=0;
	user_list.IBusAvg_Adc=0;
	user_list.IBusAvgScene_Adc=0;
	user_list.IBusAvgOffset_Adc=100;
	user_list.IBusAvgOffsetValid=1;
	tool_mode_key_event(0x46);
	user_list.flag.bits.gToolTrigger=1;
	user_list.flag.bits.gToolEn=1;
	mc_core_list.motor_en=1;
	motor_control_list.bldc_state=MOTOR_RUN;
	CHECK(tool_mode_duty(281)==337);
}

/** @brief 验证3.5A升速、2.0A卸载迟滞及20/100ms确认边界。 */
static void test_scene_load_thresholds(void)
{
	scene_load_fixture();
	tick_ms(100); CHECK(tool_mode_duty(281)==337);
	/* 8.91891码/A，31码低于3.5A，32码达到；峰值电流不参与本档负载判断。 */
	user_list.IBusAvgScene_Adc=31;
	user_list.IBusPeak_Adc=2000;
	tick_ms(1000); CHECK(tool_mode_duty(1874)==337);
	user_list.IBusAvgScene_Adc=32;
	CHECK(tool_mode_duty(281)==337);
	tick_ms(19); CHECK(tool_mode_duty(281)==337);
	tick_ms(1); CHECK(tool_mode_duty(281)==1874);
	user_list.IBusAvgScene_Adc=18;
	tick_ms(1000); CHECK(tool_mode_duty(281)==1874);
	/* 17码低于2.0A；卸载持续确认后恢复原档目标，扳机仍然扣住。 */
	user_list.IBusAvgScene_Adc=17;
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(99); CHECK(tool_mode_duty(281)==1874);
	tick_ms(1); CHECK(tool_mode_duty(1874)==337);
	user_list.IBusAvgScene_Adc=12;
	tick_ms(5000); CHECK(tool_mode_duty(281)==337);
}

/** @brief 验证启动尖峰屏蔽、重复调用不耗时、负载与卸载抖动均需重新连续确认。 */
static void test_scene_load_debounce(void)
{
	uint16_t i;
	scene_load_fixture(); user_list.IBusAvgScene_Adc=100;
	tick_ms(99); CHECK(tool_mode_duty(281)==337);
	tick_ms(1); CHECK(tool_mode_duty(281)==337);
	for(i=0;i<1000;i++) CHECK(tool_mode_duty(281)==337);
	tick_ms(19); CHECK(tool_mode_duty(281)==337);
	user_list.IBusAvgScene_Adc=31; CHECK(tool_mode_duty(281)==337);
	user_list.IBusAvgScene_Adc=32; CHECK(tool_mode_duty(281)==337);
	tick_ms(19); CHECK(tool_mode_duty(281)==337);
	tick_ms(1); CHECK(tool_mode_duty(281)==1874);
	user_list.IBusAvgScene_Adc=12; CHECK(tool_mode_duty(281)==1874);
	tick_ms(99); CHECK(tool_mode_duty(281)==1874);
	user_list.IBusAvgScene_Adc=18; CHECK(tool_mode_duty(281)==1874);
	user_list.IBusAvgScene_Adc=12; CHECK(tool_mode_duty(281)==1874);
	tick_ms(99); CHECK(tool_mode_duty(281)==1874);
	tick_ms(1); CHECK(tool_mode_duty(281)==337);
}

/** @brief 验证松扳机、撤销使能、停机及实时/锁存故障清除升速进度，不绕过保护。 */
static void test_scene_load_reset(void)
{
	uint8_t gate;
	for(gate=0;gate<8;gate++)
	{
		scene_load_fixture(); user_list.IBusAvgScene_Adc=32;
		tick_ms(100); (void)tool_mode_duty(281);
		tick_ms(20); CHECK(tool_mode_duty(281)==1874);
		if(gate==0) user_list.flag.bits.gToolTrigger=0;
		if(gate==1) user_list.flag.bits.gToolEn=0;
		if(gate==2) mc_core_list.motor_en=0;
		if(gate==3) motor_control_list.bldc_state=MOTOR_HBRAKE;
		if(gate==4) user_list.userErr.bits.gNtcMos=1;
		if(gate==5) mc_core_list.MC_error.bits.gBlock=1;
		if(gate==6) machine_error_hold.userErr_hold.bits.gVBusStbUVP=1;
		if(gate==7) machine_error_hold.MC_error_hold.bits.gBlock=1;
		CHECK(tool_mode_duty(281)==337);
		user_list.flag.bits.gToolTrigger=1; user_list.flag.bits.gToolEn=1;
		mc_core_list.motor_en=1; motor_control_list.bldc_state=MOTOR_RUN;
		user_list.userErr.word=0; mc_core_list.MC_error.word=0;
		machine_error_hold.userErr_hold.word=0; machine_error_hold.MC_error_hold.word=0;
		CHECK(tool_mode_duty(281)==337);
		tick_ms(99); CHECK(tool_mode_duty(281)==337);
		tick_ms(1); CHECK(tool_mode_duty(281)==337);
		tick_ms(20); CHECK(tool_mode_duty(281)==1874);
	}
}

/** @brief 验证升速仅用于正转情景1档，换模式/方向或换挡后清除旧状态。 */
static void test_scene_load_mode_scope(void)
{
	scene_load_fixture(); user_list.IBusAvgScene_Adc=32;
	tick_ms(100); (void)tool_mode_duty(281);
	tick_ms(20); CHECK(tool_mode_duty(281)==1874);
	user_list.flag.bits.gToolTrigger=0;
	tool_mode_key_event(0x47);
	CHECK(tool_mode_duty(1874)==712);
	tool_mode_key_event(0x46);
	user_list.flag.bits.gToolTrigger=1;
	CHECK(tool_mode_duty(281)==337);
	tick_ms(100); (void)tool_mode_duty(281);
	tick_ms(20); CHECK(tool_mode_duty(281)==1874);
	direction(TOOL_DIRECTION_REVERSE);
	CHECK(tool_mode_duty(1874)==712);
	direction(TOOL_DIRECTION_FORWARD);
	CHECK(tool_mode_duty(281)==337);
	user_list.flag.bits.gToolTrigger=0;
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==2 && tool_mode_duty(281)==1874);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==3 && tool_mode_duty(281)==1874);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==1 && tool_mode_duty(281)==337);
}

/** @brief 验证实际过流采样链先扣零偏及校准增益，再供情景负载判断使用。 */
static void test_scene_load_calibrated_current(void)
{
	scene_load_fixture();
	mc_core_list.CurrentOffset_ad_data=100;
	user_list.Vcc_cal_gain=49152;
	mc_core_list.Board_ADC_DATA.u16I_Peak=100;
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=121;
	user_oc_handle(); CHECK(user_list.IBusAvg_Adc==31);
	tick_ms(100); CHECK(tool_mode_duty(281)==337);
	tick_ms(20); CHECK(tool_mode_duty(281)==337);
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=122;
	user_oc_handle(); CHECK(user_list.IBusAvg_Adc==33);
	CHECK(tool_mode_duty(281)==337);
	tick_ms(20); CHECK(tool_mode_duty(281)==1874);
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=108;
	user_oc_handle(); CHECK(user_list.IBusAvg_Adc==12);
	CHECK(tool_mode_duty(281)==1874);
	tick_ms(100); CHECK(tool_mode_duty(281)==337);
	/* 原始值低于零偏必须钳为零，不能无符号下溢误判重载。 */
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=99;
	user_oc_handle(); CHECK(user_list.IBusAvg_Adc==0);
}
/** @brief 模拟CH4/CH5不同零点，验证5.5A负载使用AVG自身零偏并经过实际PWM软坡。 */
static void test_scene_independent_avg_offset(void)
{
	uint16_t i,previous;
	scene_load_fixture();
	user_list.IBusAvgOffset_Adc=432;
	user_list.IBusAvgOffsetValid=1;
	user_list.Vcc_cal_gain=32768;
	/* 峰值零偏451码为测试构造值，不代表本次实板CH5测量结果。 */
	mc_core_list.CurrentOffset_ad_data=451;
	mc_core_list.Board_ADC_DATA.u16I_Peak=463;
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=443;
	user_oc_handle();
	CHECK(user_list.IBusAvgScene_Adc==11);
	CHECK(user_list.IBusAvg_Adc==0 && user_list.IBusPeak_Adc==12);
	tick_ms(100); user_gears_handle();
	CHECK(motor_control_list.pwm_duty_aim==337);
	/* 49码约5.5A，错扣峰值零点仅30码；情景应升速，原保护读数保持原算法。 */
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=481;
	user_oc_handle();
	CHECK(user_list.IBusAvgScene_Adc==49 && user_list.IBusAvg_Adc==30);
	user_gears_handle(); tick_ms(20); user_gears_handle();
	CHECK(motor_control_list.pwm_duty_aim==1874);
	motor_control_list.pwm_duty_virtual=337;
	mc_core_list.pwm_actual_runing_data=337;
	previous=337;
	for(i=0;i<12000;i++)
	{
		pwm_set(); CHECK(last_pwm>=previous && last_pwm<=1874); previous=last_pwm;
	}
	CHECK(last_pwm==1874);
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=443;
	user_oc_handle(); user_gears_handle(); tick_ms(100); user_gears_handle();
	CHECK(motor_control_list.pwm_duty_aim==337);
	for(i=0;i<12000;i++)
	{
		pwm_set(); CHECK(last_pwm<=previous && last_pwm>=337); previous=last_pwm;
	}
	CHECK(last_pwm==337 && user_list.IBusAvgOffset_Adc==432);
}

/** @brief 验证应用初始化清有效标志，模拟校准后Flash/电机初始化及换模式均不擦掉零偏。 */
static void test_scene_avg_offset_lifetime(void)
{
	erase_fixture();
	user_list.IBusAvgOffset_Adc=432;
	user_list.IBusAvgScene_Adc=99;
	user_list.IBusAvgOffsetValid=1;
	user_para_init();
	CHECK(!user_list.IBusAvgOffsetValid && user_list.IBusAvgOffset_Adc==0 && user_list.IBusAvgScene_Adc==0);
	/* 模拟真实上电顺序：应用初始化→独立校准→读Flash→电机初始化。 */
	user_list.IBusAvgOffset_Adc=432;
	user_list.IBusAvgOffsetValid=1;
	read_user_data(); mc_init();
	CHECK(user_list.IBusAvgOffset_Adc==432 && user_list.IBusAvgOffsetValid);
	tool_mode_set_direction(TOOL_DIRECTION_REVERSE);
	tool_mode_set_direction(TOOL_DIRECTION_FORWARD);
	CHECK(user_list.IBusAvgOffset_Adc==432 && user_list.IBusAvgOffsetValid);
	scene_load_fixture();
	user_list.IBusAvgOffsetValid=0;
	user_list.IBusAvg_Adc=100;
	user_list.IBusAvgScene_Adc=100;
	tick_ms(100); CHECK(tool_mode_duty(281)==337);
	tick_ms(20); CHECK(tool_mode_duty(281)==337);
}
/** @brief AVG独立校准无效时仍保留原28A峰值保护，不能被情景升速条件屏蔽。 */
static void test_scene_avg_failure_preserves_peak_protection(void)
{
	uint16_t i;
	scene_load_fixture();
	user_list.IBusAvgOffsetValid=0;
	user_list.IBusAvgOffset_Adc=432;
	user_list.Vcc_cal_gain=32768;
	mc_core_list.CurrentOffset_ad_data=451;
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=432;
	/* 校准后250码约28.03A；原28A积分每次加327，第101次达到32768。 */
	mc_core_list.Board_ADC_DATA.u16I_Peak=701;
	for(i=0;i<100;i++)
	{
		user_oc_handle();
		CHECK(!user_list.userErr.bits.gIBusPeakOCP1);
		CHECK(tool_mode_duty(281)==337 && user_list.IBusAvgScene_Adc==0);
	}
	user_oc_handle();
	CHECK(user_list.IBusPeak_Adc==250 && user_list.userErr.bits.gIBusPeakOCP1);
	CHECK(tool_mode_duty(281)==337);
}
#endif

/** @brief 运行模式、反转时序、Flash兼容、换向记忆及情景1档独立电流校准回归。 */
void run_tests(void)
{
#if TEST_BASELINE
	erase_fixture(); user_para_init(); read_user_data();
	mc_core_list.Board_ADC_DATA.u16VR=4000;
	pwm_duty_aim_deal(); user_gears_handle();
	test_case=1;
	/* 空Flash调速1档满扳机应为40%；旧固件实际输出100%，这是预期红测。 */
	CHECK(motor_control_list.pwm_duty_aim==749);
#else
#define RUN(n,fn) do { if(TEST_ONLY_CASE && TEST_ONLY_CASE!=n) break; test_case=n; fn(); if(test_failure_line) return; } while(0)
	RUN(13,test_reverse_keys);
	RUN(1,test_modes); RUN(2,test_key_debounce); RUN(3,test_duty);
	RUN(4,test_save_and_gates); RUN(5,test_partial_and_crc);
	RUN(6,test_rollover_power_failure); RUN(7,test_legacy);
	RUN(8,test_flash_readback); RUN(9,test_rollover_cutpoints);
	RUN(10,test_versions_ranges_and_wrap); RUN(11,test_real_pwm_ramp);
	RUN(12,test_trigger_key_inhibit);
	RUN(14,test_reverse_hold_boundary); RUN(15,test_reverse_hold_frontend_gap);
	RUN(16,test_reverse_hold_stop_and_fault_reset); RUN(17,test_reverse_hold_direction_reset);
	RUN(18,test_reverse_real_pwm_ramps); RUN(19,test_dual_direction_memory);
	RUN(20,test_dual_direction_partial_and_crc); RUN(21,test_v1_v2_migration_and_ranges);
	RUN(22,test_reverse_pwm_first_full_frontend_gap); RUN(23,test_reverse_pwm_brief_full_frontend_gap);
	RUN(24,test_speed_direction_transfer); RUN(25,test_speed_transfer_save); RUN(26,test_boot_speed_direction_restore);
	RUN(27,test_scene_load_thresholds); RUN(28,test_scene_load_debounce); RUN(29,test_scene_load_reset);
	RUN(30,test_scene_load_mode_scope); RUN(31,test_scene_load_calibrated_current);
	RUN(32,test_scene_independent_avg_offset); RUN(33,test_scene_avg_offset_lifetime);
	RUN(34,test_scene_avg_failure_preserves_peak_protection);
#endif
}
