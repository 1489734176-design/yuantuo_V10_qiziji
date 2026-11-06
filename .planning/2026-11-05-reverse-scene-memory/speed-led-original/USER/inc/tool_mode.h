#ifndef __TOOL_MODE_H
#define __TOOL_MODE_H

#include <stdint.h>

/* 正反转分别保存模式及挡位，挡位从1开始，反转情景固定4档。 */
#define TOOL_MODE_SPEED  (0u)
#define TOOL_MODE_SCENE  (1u)
#define TOOL_DIRECTION_FORWARD (0u)
#define TOOL_DIRECTION_REVERSE (1u)
#define TOOL_MODE_KEY_SPEED (0x47u)
#define TOOL_MODE_KEY_SCENE (0x46u)

typedef struct
{
	uint8_t mode;
	uint8_t speed_gear;
	uint8_t scene_gear;
} tool_mode_setting_t;

typedef struct
{
	tool_mode_setting_t setting[2];
	uint8_t active_direction;
	uint8_t save_pending;
} tool_mode_list_t;

extern tool_mode_list_t tool_mode_list;
/* 调试器查看最近成功读到的原始键码，以及最近一次读事务是否成功。
 * K2应为0x47、K3应为0x46；失败时保留上次键码，由read_ok=0标明无新数据。 */
extern volatile uint8_t tool_mode_key_raw;
extern volatile uint8_t tool_mode_key_read_ok;
void tool_mode_init(void);
tool_mode_setting_t *tool_mode_current(void);
void tool_mode_set_direction(uint8_t reverse);
void tool_mode_tick_1ms(void);
/* PWM实际输出更新后调用，捕获反转情景首次达到升速目标的时刻。 */
void tool_mode_pwm_update(uint16_t duty);
/* 每1ms调用；返回1表示本次已占用一次TM1650读事务（包括失败）。 */
uint8_t tool_mode_key_handle(void);
void tool_mode_key_event(uint8_t key);
uint16_t tool_mode_duty(uint16_t trigger_duty);
uint8_t tool_mode_gear(void);
void tool_mode_save_handle(void);

#endif
