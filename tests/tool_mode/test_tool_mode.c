/* 中文：运行真实控制/存储代码；仅TM1650与Flash物理操作由模拟硬件替代。 */
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
		/* 中文：用半页擦除失败验证另一页最新记录始终保留。 */
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
static void test_modes(void)
{
	erase_fixture(); boot_fixture();
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	CHECK(tool_mode_list.save_pending==0);
	tool_mode_key_event(0x46);
	CHECK(tool_mode_list.mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
	tool_mode_key_event(0x46);
	CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x47);
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_gear()==1);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==3);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==4);
	tool_mode_key_event(0x47); CHECK(tool_mode_gear()==1);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==3);
	tool_mode_key_event(0x46); CHECK(tool_mode_gear()==1);
	tool_mode_key_event(0x45); CHECK(tool_mode_gear()==1 && tool_mode_list.mode==TOOL_MODE_SCENE);
}
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
	/* 中文：其他键及组合键不能冒充K2；换键前必须确认全部释放。 */
	key_raw=0x45; key_ms(30); key_raw=0x47; key_ms(30);
	CHECK(tool_mode_gear()==3);
	key_raw=0; key_ms(30); press(0x46);
	CHECK(tool_mode_list.mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
	key_raw=0xc7; key_ms(30); key_raw=0x47; key_ms(30);
	CHECK(tool_mode_list.mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
	/* 中文：bit6未置位即释放；随后三次有效K2才允许切回调速。 */
	key_raw=0x87; key_ms(30); press(0x47);
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_gear()==3);
}
static void test_duty(void)
{
	/* 当前39/58/78/98%挡位表，周期1874；中点扳机输入1077。 */
	static const uint16_t maximum[]={730,1086,1461,1836};
	static const uint16_t middle[]={505,683,870,1058};
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
	CHECK(tool_mode_duty(281)==374 && tool_mode_duty(1874)==374);
	tool_mode_key_event(0x46); CHECK(tool_mode_duty(281)==1874);
	tool_mode_key_event(0x46); CHECK(tool_mode_duty(281)==1874);
}
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
	CHECK(tool_mode_list.mode==TOOL_MODE_SCENE && tool_mode_list.speed_gear==2 && tool_mode_list.scene_gear==2);
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
	/* 中文：更晚记录的数据损坏时回退前一条有效设置。 */
	*((volatile uint16_t *)(PAGE30+20+6))=0;
	boot_fixture(); CHECK(tool_mode_gear()==2);
}
static void test_rollover_power_failure(void)
{
	uint16_t i;
	erase_fixture(); boot_fixture();
	for(i=0;i<51;i++) { tool_mode_key_event(0x47); tool_mode_save_handle(); }
	CHECK(tool_mode_gear()==4);
	/* 中文：下一页擦到一半失败，掉电重启仍从旧页恢复。 */
	*((volatile uint16_t *)PAGE31)=0;
	tool_mode_key_event(0x47); fail_after=256; tool_mode_save_handle();
	CHECK(tool_mode_list.save_pending==1);
	boot_fixture(); CHECK(tool_mode_gear()==4);
	fail_after=-1; tool_mode_key_event(0x47); tool_mode_save_handle();
	boot_fixture(); CHECK(tool_mode_gear()==1);
	for(i=0;i<51;i++) { tool_mode_key_event(0x47); tool_mode_save_handle(); }
	boot_fixture(); CHECK(tool_mode_gear()==4);
}
static void test_legacy(void)
{
	volatile uint16_t *legacy=(volatile uint16_t *)(PAGE31+9*20);
	erase_fixture(); legacy[0]=0x55aa; legacy[1]=1; legacy[2]=1; boot_fixture();
	CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_list.speed_gear==2 && tool_mode_list.scene_gear==1);
	CHECK(user_list.flag.bits.gAutoStopEn==1 && tool_mode_list.save_pending==0);
	tool_mode_key_event(0x46); fail_after=3; tool_mode_save_handle();
	boot_fixture(); CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_gear()==2);
	fail_after=-1; tool_mode_key_event(0x46); tool_mode_save_handle(); boot_fixture();
	CHECK(tool_mode_list.mode==TOOL_MODE_SCENE && tool_mode_gear()==1);
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
	/* 中文：擦除HAL报告成功但标志位未擦净，不能在旧magic下写入新数据。 */
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
static void test_versions_ranges_and_wrap(void)
{
	/* 中文：由独立Python binascii.crc_hqx计算的完整字面记录，覆盖版本/范围/回绕判定。 */
	static const uint16_t before_wrap[]={0xa65a,1,0,4,1,0xffff,0xffff,0x548e};
	static const uint16_t after_wrap[]={0xa65a,1,1,3,2,0,0,0xabfc};
	static const uint16_t bad_version[]={0xa65a,2,0,4,1,1,0,0xbe6a};
	static const uint16_t bad_range[]={0xa65a,1,0,5,1,1,0,0xfc26};
	volatile uint16_t *p0=(volatile uint16_t *)PAGE30;
	volatile uint16_t *p1=(volatile uint16_t *)PAGE31;
	uint8_t i;
	erase_fixture();
	for(i=0;i<8;i++) { p0[i]=before_wrap[i]; p1[i]=after_wrap[i]; }
	boot_fixture(); CHECK(tool_mode_list.mode==TOOL_MODE_SCENE && tool_mode_list.speed_gear==3 && tool_mode_gear()==2);
	for(i=0;i<8;i++) p1[10+i]=bad_version[i];
	boot_fixture(); CHECK(tool_mode_gear()==2);
	for(i=0;i<8;i++) p1[20+i]=bad_range[i];
	boot_fixture(); CHECK(tool_mode_gear()==2);
	tool_mode_key_event(0x46); tool_mode_save_handle(); boot_fixture(); CHECK(tool_mode_gear()==3);
	erase_fixture(); p1[0]=0x55aa; p1[1]=0; p1[2]=0; p1[90]=0x55aa;
	boot_fixture(); CHECK(tool_mode_list.mode==TOOL_MODE_SPEED && tool_mode_gear()==1 && flash_operations==0);
	tool_mode_list.speed_gear=5; tool_mode_list.save_pending=1; tool_mode_save_handle();
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
	for(i=0;i<200;i++)
	{
		pwm_set(); CHECK(last_pwm>=previous && last_pwm<=374); previous=last_pwm;
	}
	CHECK(last_pwm==374);
	tool_mode_key_event(0x46); motor_control_list.pwm_duty_aim=tool_mode_duty(281);
	for(i=0;i<1600;i++)
	{
		pwm_set(); CHECK(last_pwm>=previous && last_pwm<=1874); previous=last_pwm;
	}
	CHECK(last_pwm==1874);
}
#endif

void run_tests(void)
{
#if TEST_BASELINE
	erase_fixture(); user_para_init(); read_user_data();
	mc_core_list.Board_ADC_DATA.u16VR=4000;
	pwm_duty_aim_deal(); user_gears_handle();
	test_case=1;
	/* 中文：空Flash调速1档满扳机应为40%；旧固件实际输出100%，这是预期红测。 */
	CHECK(motor_control_list.pwm_duty_aim==749);
#else
#define RUN(n,fn) do { test_case=n; fn(); if(test_failure_line) return; } while(0)
	RUN(1,test_modes); RUN(2,test_key_debounce); RUN(3,test_duty);
	RUN(4,test_save_and_gates); RUN(5,test_partial_and_crc);
	RUN(6,test_rollover_power_failure); RUN(7,test_legacy);
	RUN(8,test_flash_readback); RUN(9,test_rollover_cutpoints);
	RUN(10,test_versions_ranges_and_wrap); RUN(11,test_real_pwm_ramp);
#endif
}
