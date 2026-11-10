#include "main.h"
#include "board.h"
#include "user_control.h"
#include "motor_control.h"
#include "tool_mode.h"
#include "led.h"
#include "tm1650.h"

User_list_Struct user_list;
Machine_ERROR_HOLD machine_error_hold;
Mc_core_list mc_core_list;
Motor_Control_list motor_control_list;
volatile uint32_t test_failure_line, test_case, test_checks;
static uint8_t test_digits[4];

#define CHECK(expr) do { test_checks++; if(!(expr)) { test_failure_line=__LINE__; return test_failure_line; } } while(0)

/* 总线硬件由显存夹具替代，显示决策使用实际LED模块。 */
void TM1650_SetDigit(uint8_t index,uint8_t value) { if(index<4) test_digits[index]=value; }
uint8_t TM1650_GetDigit(uint8_t index) { return index<4 ? test_digits[index] : 0; }
void TM1650_Clear(void) { uint8_t i; for(i=0;i<4;i++) test_digits[i]=0; }
uint8_t TM1650_SetDisplay(uint8_t cfg) { (void)cfg; return 1; }
uint8_t TM1650_FlushStep(void) { return 1; }
uint8_t TM1650_ReadKey(uint8_t *key) { (void)key; return 0; }
uint8_t save_user_data_checked(void) { return 1; }
void Bsp_Key_Init(void) {}
void Bsp_dir_init(void) {}

/**
 * @brief 初始化正转情景1档运行夹具，直接提供校准后平均电流并从零开始模式时钟。
 */
static void prepare_forward_scene1(uint16_t current_adc)
{
	tool_mode_init();
	tool_mode_current()->mode=TOOL_MODE_SCENE;
	tool_mode_current()->scene_gear=1;
	user_list.user_state=TOOL_RUN;
	user_list.flag.bits.gToolTrigger=1;
	user_list.flag.bits.gToolEn=1;
	user_list.IBusAvgScene_Adc=current_adc;
	user_list.IBusAvgOffsetValid=1;
	mc_core_list.motor_en=1;
	motor_control_list.bldc_state=MOTOR_RUN;
}

/**
 * @brief 推进实际模式时钟一毫秒并取得实际模式模块计算的目标占空比。
 */
static uint16_t tick_mode_duty(void)
{
	tool_mode_tick_1ms();
	return tool_mode_duty(281);
}

/** @brief 用实际模式初始化建立3档运行夹具，提供校准后AVG电流而不修改目标算法。 */
static void prepare_forward_scene3(uint16_t current_adc)
{
	prepare_forward_scene1(current_adc);
	tool_mode_current()->scene_gear=3;
	user_list.flag.bits.gAutoStop=0;
	user_list.userErr.word=0;
	mc_core_list.MC_error.word=0;
	machine_error_hold.userErr_hold.word=0;
	machine_error_hold.MC_error_hold.word=0;
}

/**
 * @brief 验证默认和覆盖参数下，显示、反转、情景1档升速与3档电流降速配置均生效。
 *        期望计数按1874周期手算，避免根据被测函数重新生成期望。
 */
uint32_t run_tests(void)
{
#ifdef TEST_MODE_CONFIG_OVERRIDE
	const uint8_t speed_masks[]={0x08,0x48,0x58,0x78};
	const uint8_t scene_masks[]={0x20,0x40,0x10};
	const uint16_t speed_max[]={468,843,1218,1592};
	const uint16_t speed_middle[]={374,561,749,936};
	const uint16_t scene_duty[]={655,1030,1405};
	const uint8_t reverse_mask=0x20;
	const uint16_t reverse_full=1686,reverse_low=374,reverse_time=321;
	/* 1874*82%=1536；8.91891码/A：4.7A向上取42码，2.6A向下取23码。 */
	const uint16_t scene1_boost=1536,load_on_adc=42,load_off_adc=23;
	const uint16_t startup_ms=37,load_on_ms=11,load_off_ms=43;
	/* 27A向下取240码；1874*45%=843，3档正常75%=1405。 */
	const uint16_t scene3_limit_adc=240,scene3_limit_duty=843,scene3_confirm_ms=73;
#else
	const uint8_t speed_masks[]={0x40,0x60,0x70,0x78};
	const uint8_t scene_masks[]={0x40,0x20,0x10};
	const uint16_t speed_max[]={712,1086,1461,1874};
	const uint16_t speed_middle[]={496,683,870,1077};
	const uint16_t scene_duty[]={337,1874,1874};
	const uint8_t reverse_mask=0x08;
	const uint16_t reverse_full=1874,reverse_low=281,reverse_time=850;
	/* 1874*100%=1874；8.91891码/A：3.5A向上取32码，2.0A向下取17码。 */
	const uint16_t scene1_boost=1874,load_on_adc=32,load_off_adc=17;
	const uint16_t startup_ms=100,load_on_ms=20,load_off_ms=100;
	/* 18A向下取160码；1874*30%=562，3档正常100%=1874。 */
	const uint16_t scene3_limit_adc=160,scene3_limit_duty=562,scene3_confirm_ms=200;
#endif
	uint8_t i;
	uint16_t elapsed;
	user_list.user_state=TOOL_STOP;
	user_list.VBus_Vx100=2100;
	tool_mode_init();
	for(i=0;i<4;i++)
	{
		test_case=i+1;
		tool_mode_current()->speed_gear=i+1;
		UO_Led_Handler();
		CHECK(TM1650_GetDigit(0)==speed_masks[i] && TM1650_GetDigit(1)==0);
		CHECK(tool_mode_duty(0)==281 && tool_mode_duty(281)==281);
		CHECK(tool_mode_duty(1077)==speed_middle[i]);
		CHECK(tool_mode_duty(1874)==speed_max[i] && tool_mode_duty(65535)==speed_max[i]);
	}
	tool_mode_current()->mode=TOOL_MODE_SCENE;
	for(i=0;i<3;i++)
	{
		test_case=i+5;
		tool_mode_current()->scene_gear=i+1;
		UO_Led_Handler();
		CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==scene_masks[i]);
		CHECK(tool_mode_duty(0)==scene_duty[i]);
		CHECK(tool_mode_duty(1077)==scene_duty[i] && tool_mode_duty(1874)==scene_duty[i]);
	}
	test_case=8;
	tool_mode_set_direction(TOOL_DIRECTION_REVERSE);
	tool_mode_key_event(TOOL_MODE_KEY_SCENE);
	UO_Led_Handler();
	CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==reverse_mask);
	CHECK(tool_mode_gear()==4 && tool_mode_duty(281)==reverse_full);
	user_list.flag.bits.gToolTrigger=1;
	user_list.flag.bits.gToolEn=1;
	mc_core_list.motor_en=1;
	motor_control_list.bldc_state=MOTOR_RUN;
	mc_core_list.pwm_actual_runing_data=reverse_full;
	tool_mode_pwm_update(reverse_full);
	CHECK(tool_mode_duty(281)==reverse_full);
	{
		uint16_t milliseconds;
		for(milliseconds=0;milliseconds<reverse_time-1;milliseconds++) tool_mode_tick_1ms();
	}
	CHECK(tool_mode_duty(281)==reverse_full);
	tool_mode_tick_1ms(); CHECK(tool_mode_duty(281)==reverse_low);

	/* 启动时即有负载：屏蔽结束前不得提前确认，随后须完整满足配置的进入时间。 */
	test_case=9;
	prepare_forward_scene1(load_on_adc);
	CHECK(tool_mode_duty(281)==scene_duty[0]);
	for(elapsed=1;elapsed<startup_ms;elapsed++) CHECK(tick_mode_duty()==scene_duty[0]);
	CHECK(tick_mode_duty()==scene_duty[0]);
	for(elapsed=1;elapsed<load_on_ms;elapsed++) CHECK(tick_mode_duty()==scene_duty[0]);
	CHECK(tick_mode_duty()==scene1_boost);

	/* 稳定运行后用边界电流加载：少一码不得升速，等于向上取整门限才开始确认。 */
	test_case=10;
	prepare_forward_scene1(load_on_adc-1);
	CHECK(tool_mode_duty(281)==scene_duty[0]);
	for(elapsed=0;elapsed<startup_ms+load_on_ms+3;elapsed++) CHECK(tick_mode_duty()==scene_duty[0]);
	user_list.IBusAvgScene_Adc=load_on_adc;
	CHECK(tool_mode_duty(281)==scene_duty[0]);
	for(elapsed=1;elapsed<load_on_ms;elapsed++) CHECK(tick_mode_duty()==scene_duty[0]);
	CHECK(tick_mode_duty()==scene1_boost);

	/* 卸载边界独立确认：退出门限多一码仍保持升速，等于向下取整门限才计时恢复。 */
	test_case=11;
	user_list.IBusAvgScene_Adc=load_off_adc+1;
	CHECK(tool_mode_duty(281)==scene1_boost);
	for(elapsed=0;elapsed<load_off_ms+3;elapsed++) CHECK(tick_mode_duty()==scene1_boost);
	user_list.IBusAvgScene_Adc=load_off_adc;
	CHECK(tool_mode_duty(281)==scene1_boost);
	for(elapsed=1;elapsed<load_off_ms;elapsed++) CHECK(tick_mode_duty()==scene1_boost);
	CHECK(tick_mode_duty()==scene_duty[0]);

	/* 扳机仍扣住时再次加载，只重新执行负载确认，不能重新套用启动屏蔽。 */
	test_case=12;
	user_list.IBusAvgScene_Adc=load_on_adc;
	CHECK(tool_mode_duty(281)==scene_duty[0]);
	for(elapsed=1;elapsed<load_on_ms;elapsed++) CHECK(tick_mode_duty()==scene_duty[0]);
	CHECK(tick_mode_duty()==scene1_boost);

	/* 三档严格边界读取配置：等于门限不累计，多一码必须连续满配置的真实毫秒时间。 */
	test_case=13;
	prepare_forward_scene3(scene3_limit_adc-1);
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	user_list.IBusAvgScene_Adc=scene3_limit_adc;
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	for(elapsed=0;elapsed<scene3_confirm_ms+3;elapsed++) CHECK(tick_mode_duty()==scene_duty[2]);
	user_list.IBusAvgScene_Adc=scene3_limit_adc+1;
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	for(elapsed=1;elapsed<scene3_confirm_ms;elapsed++) CHECK(tick_mode_duty()==scene_duty[2]);
	CHECK(tick_mode_duty()==scene3_limit_duty);
	CHECK(tool_mode_duty(281)==scene3_limit_duty && tool_mode_duty(1874)==scene3_limit_duty);

	/* 按住电流回落或撤销许可不清锁存，松开后正常目标也读取三档正常配置。 */
	test_case=14;
	user_list.IBusAvgScene_Adc=0;
	user_list.flag.bits.gToolEn=0;
	CHECK(tool_mode_duty(281)==scene3_limit_duty);
	user_list.flag.bits.gToolEn=1;
	CHECK(tool_mode_duty(281)==scene3_limit_duty);
	user_list.flag.bits.gToolTrigger=0;
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	user_list.flag.bits.gToolTrigger=1;
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	user_list.IBusAvgScene_Adc=scene3_limit_adc+1;
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	for(elapsed=1;elapsed<scene3_confirm_ms;elapsed++) CHECK(tick_mode_duty()==scene_duty[2]);
	CHECK(tick_mode_duty()==scene3_limit_duty);

	/* 等于门限打断确认：旧时间不能累计到下一段超限，需重新满足配置的72/73或199/200ms。 */
	test_case=15;
	prepare_forward_scene3(scene3_limit_adc+1);
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	for(elapsed=1;elapsed<scene3_confirm_ms;elapsed++) CHECK(tick_mode_duty()==scene_duty[2]);
	user_list.IBusAvgScene_Adc=scene3_limit_adc;
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	user_list.IBusAvgScene_Adc=scene3_limit_adc+1;
	CHECK(tool_mode_duty(281)==scene_duty[2]);
	for(elapsed=1;elapsed<scene3_confirm_ms;elapsed++) CHECK(tick_mode_duty()==scene_duty[2]);
	CHECK(tick_mode_duty()==scene3_limit_duty);
	return 0;
}
