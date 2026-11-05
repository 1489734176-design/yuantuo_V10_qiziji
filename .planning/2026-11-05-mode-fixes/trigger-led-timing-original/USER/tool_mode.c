#include "tool_mode.h"
#include "parameter.h"
#include "user_control.h"
#include "motor_control.h"
#include "flash_data_save.h"
#include "tm1650.h"

tool_mode_list_t tool_mode_list;
volatile uint8_t tool_mode_key_raw;
volatile uint8_t tool_mode_key_read_ok;
static uint8_t key_poll_count;
static uint8_t key_candidate;
static uint8_t key_stable_count;
static uint8_t key_press_latched;

void tool_mode_init(void)
{
	/* 中文：空Flash默认调速1档；上电默认不写Flash，避免无意义磨损。 */
	tool_mode_list.mode=TOOL_MODE_SPEED;
	tool_mode_list.speed_gear=1;
	tool_mode_list.scene_gear=1;
	tool_mode_list.save_pending=0;
	key_poll_count=0;
	key_candidate=0;
	key_stable_count=0;
	key_press_latched=0;
	tool_mode_key_raw=0;
	tool_mode_key_read_ok=0;
}

uint8_t tool_mode_gear(void)
{
	return (tool_mode_list.mode==TOOL_MODE_SCENE) ?
		tool_mode_list.scene_gear : tool_mode_list.speed_gear;
}

void tool_mode_key_event(uint8_t key)
{
	/* 中文：切到另一模式只切换；重复按当前模式键才循环升挡。 */
	if(key==TOOL_MODE_KEY_SPEED)
	{
		if(tool_mode_list.mode!=TOOL_MODE_SPEED)
		{
			tool_mode_list.mode=TOOL_MODE_SPEED;
		}
		else
		{
			if(++tool_mode_list.speed_gear>4) tool_mode_list.speed_gear=1;
		}
	}
	else if(key==TOOL_MODE_KEY_SCENE)
	{
		if(tool_mode_list.mode!=TOOL_MODE_SCENE)
		{
			tool_mode_list.mode=TOOL_MODE_SCENE;
		}
		else
		{
			if(++tool_mode_list.scene_gear>3) tool_mode_list.scene_gear=1;
		}
	}
	else
	{
		return;
	}
	/* 中文：旧用户结构保留0起始挡位索引，持久化使用新模块的1起始挡位。 */
	user_list.gears_level=tool_mode_gear()-1;
	tool_mode_list.save_pending=1;
	user_list.sleep_cnt=0;
}

uint8_t tool_mode_key_handle(void)
{
	uint8_t raw;
	uint8_t sample;
	if(++key_poll_count<10) return 0;
	key_poll_count=0;
	if(!TM1650_ReadKey(&raw))
	{
		tool_mode_key_read_ok=0;
		/* 中文：通信失败不当作释放；丢弃未完成去抖，保留已按住锁定。 */
		key_stable_count=0;
		return 1;
	}
	/* 中文：保留实板读数，便于区分K3接线/键码异常与模式处理问题。 */
	tool_mode_key_raw=raw;
	tool_mode_key_read_ok=1;
	if(!(raw & 0x40u)) sample=0;
	else if(raw==TOOL_MODE_KEY_SPEED || raw==TOOL_MODE_KEY_SCENE) sample=raw;
	else sample=0xffu;
	if(sample!=key_candidate)
	{
		key_candidate=sample;
		key_stable_count=1;
	}
	else if(key_stable_count<3)
	{
		key_stable_count++;
	}
	if(key_stable_count>=3)
	{
		if(sample==0)
		{
			key_press_latched=0;
		}
		else if(!key_press_latched)
		{
			/* 中文：未知键也锁住本次按压，确认释放前不接受组合/滑动换键。 */
			key_press_latched=1;
			tool_mode_key_event(sample);
		}
	}
	return 1;
}

uint16_t tool_mode_duty(uint16_t trigger_duty)
{
	static const uint8_t speed_percent[4]={38,58,78,100};
	uint16_t minimum=(uint16_t)MIN_TOOL_DUTY;
	uint16_t maximum;
	uint8_t gear;
	if(tool_mode_list.mode==TOOL_MODE_SCENE)
	{
		/* 中文：情景扳机仅参与原启动许可，目标与扣动深度无关；斜坡仍由pwm_set执行。 */
		return (tool_mode_list.scene_gear==1) ?
			(uint16_t)(((uint32_t)PWM_PRIOD_LOAD*20u)/100u) : (uint16_t)MAX_TOOL_DUTY;
	}
	gear=tool_mode_list.speed_gear;
	if(gear<1 || gear>4) gear=1;
	maximum=(uint16_t)(((uint32_t)PWM_PRIOD_LOAD*speed_percent[gear-1])/100u);
	if(trigger_duty<=minimum) return minimum;
	if(trigger_duty>=MAX_TOOL_DUTY) return maximum;
	/* 中文：原15..100%扳机映射重新线性分配到15..本挡上限，保留轻扣最低15%。 */
	return (uint16_t)(minimum+((uint32_t)(trigger_duty-minimum)*(maximum-minimum))/
		((uint16_t)MAX_TOOL_DUTY-minimum));
}

void tool_mode_save_handle(void)
{
	/* 中文：仅前台完全停机且松扳机时写Flash；制动/启动/运行期间只保留待保存标志。 */
	if(!tool_mode_list.save_pending || user_list.flag.bits.gToolTrigger ||
	   user_list.flag.bits.gToolEn || mc_core_list.motor_en ||
	   motor_control_list.bldc_state!=MOTOR_STOP)
	{
		return;
	}
	if(save_user_data_checked()) tool_mode_list.save_pending=0;
}
