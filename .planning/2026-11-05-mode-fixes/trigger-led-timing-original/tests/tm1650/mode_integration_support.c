#include "main.h"
#include "board.h"
#include "user_control.h"
#include "motor_control.h"
#include "tool_mode.h"
#include "tm1650.h"
#include "systick.h"

User_list_Struct user_list;
Machine_ERROR_HOLD machine_error_hold;
Mc_core_list mc_core_list;
Motor_Control_list motor_control_list;

const uint32_t test_sda_pin = TM1650_SDA_PIN;
const uint32_t test_scl_pin = TM1650_SCL_PIN;
volatile uint32_t test_delay_us;

/* 中文：只替代物理等待和本回归未调用的板级/Flash操作；键盘和显示均用真实代码。 */
void systick_delay_us(uint32_t us) { test_delay_us += us; }
void Bsp_Key_Init(void) {}
void Bsp_dir_init(void) {}
uint8_t save_user_data_checked(void) { return 1; }

void test_mode_init(void)
{
	user_list.user_state=TOOL_STOP;
	user_list.VBus_Vx100=2100;
	tool_mode_init();
}

uint32_t test_mode_state(void)
{
	return (uint32_t)tool_mode_list.mode |
		((uint32_t)tool_mode_list.speed_gear << 8) |
		((uint32_t)tool_mode_list.scene_gear << 16);
}

void test_power_down(void)
{
	user_list.user_state=TOOL_POWER_DOWN;
}
