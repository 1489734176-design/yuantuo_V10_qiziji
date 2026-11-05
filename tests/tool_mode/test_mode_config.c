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
 * @brief 验证默认参数和独立覆盖参数下，七挡灯号及目标占空比均实际生效。
 *        期望计数按1874周期手算，避免根据被测函数重新生成期望。
 */
uint32_t run_tests(void)
{
#ifdef TEST_MODE_CONFIG_OVERRIDE
	const uint8_t speed_masks[]={0x08,0x40,0x10,0x20};
	const uint8_t scene_masks[]={0x20,0x40,0x10};
	const uint16_t speed_max[]={468,843,1218,1592};
	const uint16_t speed_middle[]={374,561,749,936};
	const uint16_t scene_duty[]={655,1030,1405};
#else
	const uint8_t speed_masks[]={0x40,0x20,0x10,0x08};
	const uint8_t scene_masks[]={0x40,0x10,0x08};
	const uint16_t speed_max[]={712,1086,1461,1874};
	const uint16_t speed_middle[]={496,683,870,1077};
	const uint16_t scene_duty[]={337,1874,1874};
#endif
	uint8_t i;
	user_list.user_state=TOOL_STOP;
	user_list.VBus_Vx100=2100;
	tool_mode_init();
	for(i=0;i<4;i++)
	{
		test_case=i+1;
		tool_mode_list.speed_gear=i+1;
		UO_Led_Handler();
		CHECK(TM1650_GetDigit(0)==speed_masks[i] && TM1650_GetDigit(1)==0);
		CHECK(tool_mode_duty(0)==281 && tool_mode_duty(281)==281);
		CHECK(tool_mode_duty(1077)==speed_middle[i]);
		CHECK(tool_mode_duty(1874)==speed_max[i] && tool_mode_duty(65535)==speed_max[i]);
	}
	tool_mode_list.mode=TOOL_MODE_SCENE;
	for(i=0;i<3;i++)
	{
		test_case=i+5;
		tool_mode_list.scene_gear=i+1;
		UO_Led_Handler();
		CHECK(TM1650_GetDigit(0)==0 && TM1650_GetDigit(1)==scene_masks[i]);
		CHECK(tool_mode_duty(0)==scene_duty[i]);
		CHECK(tool_mode_duty(1077)==scene_duty[i] && tool_mode_duty(1874)==scene_duty[i]);
	}
	return 0;
}
