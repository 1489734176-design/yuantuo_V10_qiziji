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

/**
 * @brief 初始化双模式默认挡位、按键去抖及待保存标志，上电默认不写Flash。
 */
void tool_mode_init(void)
{
	/* 空Flash默认调速1档；上电默认不写Flash，避免无意义磨损。 */
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

/**
 * @brief 返回当前模式的挡位编号，调速为1～4，情景为1～3。
 */
uint8_t tool_mode_gear(void)
{
	return (tool_mode_list.mode==TOOL_MODE_SCENE) ?
		tool_mode_list.scene_gear : tool_mode_list.speed_gear;
}

/**
 * @brief 处理一次有效模式键按压；切换模式保留挡位，同模式按键循环升挡。
 * @param key K2调速键或K3情景键的TM1650键码。
 */
void tool_mode_key_event(uint8_t key)
{
	/* 扳机按住及休眠期间，模式/挡位按键均无效。 */
	if(user_list.flag.bits.gToolTrigger || user_list.user_state==TOOL_POWER_DOWN) return;
	/* 切到另一模式只切换；重复按当前模式键才循环升挡。 */
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
	/* 旧用户结构保留0起始挡位索引，持久化使用新模块的1起始挡位。 */
	user_list.gears_level=tool_mode_gear()-1;
	tool_mode_list.save_pending=1;
	user_list.sleep_cnt=0;
}

/**
 * @brief 每1ms调度按键，三次10ms稳定采样确认按压或释放；扣住扳机时屏蔽按键。
 * @return 1表示本次占用一次TM1650读事务，包括读取失败；0表示尚未到轮询时间。
 */
uint8_t tool_mode_key_handle(void)
{
	uint8_t raw;
	uint8_t sample;
	uint8_t inhibited=user_list.flag.bits.gToolTrigger || user_list.user_state==TOOL_POWER_DOWN;
	/* 每1ms阻断未完成按压；松扳机后还需按键稳定释放才能再按生效。 */
	if(inhibited)
	{
		key_press_latched=1;
		key_candidate=0;
		key_stable_count=0;
	}
	if(++key_poll_count<10) return 0;
	key_poll_count=0;
	if(!TM1650_ReadKey(&raw))
	{
		tool_mode_key_read_ok=0;
		/* 通信失败不当作释放；丢弃未完成去抖，保留已按住锁定。 */
		key_stable_count=0;
		return 1;
	}
	/* 保留实板读数，便于区分K3接线/键码异常与模式处理问题。 */
	tool_mode_key_raw=raw;
	tool_mode_key_read_ok=1;
	/* 仍保留键码观察及总线调度，按住扳机时不接受按压或释放事件。 */
	if(inhibited) return 1;
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
			/* 未知键也锁住本次按压，确认释放前不接受组合/滑动换键。 */
			key_press_latched=1;
			tool_mode_key_event(sample);
		}
	}
	return 1;
}

/**
 * @brief 按parameter.h中的当前挡百分比生成目标PWM计数。
 *        调速模式连续映射扳机输入；情景模式读取每挡独立的固定目标。
 * @param trigger_duty 原扳机映射的PWM计数。
 * @return 当前模式及挡位的目标PWM计数，实际输出继续经过软启动和保护。
 */
uint16_t tool_mode_duty(uint16_t trigger_duty)
{
	static const uint8_t speed_percent[4]={
		TOOL_SPEED_GEAR1_DUTY_PERCENT, TOOL_SPEED_GEAR2_DUTY_PERCENT,
		TOOL_SPEED_GEAR3_DUTY_PERCENT, TOOL_SPEED_GEAR4_DUTY_PERCENT
	};
	static const uint8_t scene_percent[3]={
		TOOL_SCENE_GEAR1_DUTY_PERCENT, TOOL_SCENE_GEAR2_DUTY_PERCENT,
		TOOL_SCENE_GEAR3_DUTY_PERCENT
	};
	uint16_t minimum=(uint16_t)MIN_TOOL_DUTY;
	uint16_t maximum;
	uint8_t gear;
	if(tool_mode_list.mode==TOOL_MODE_SCENE)
	{
		/* 情景三挡均读取独立宏定义；目标与扣动深度无关。 */
		gear=tool_mode_list.scene_gear;
		if(gear<1 || gear>3) gear=1;
		return (uint16_t)(((uint32_t)PWM_PRIOD_LOAD*scene_percent[gear-1])/100u);
	}
	gear=tool_mode_list.speed_gear;
	if(gear<1 || gear>4) gear=1;
	maximum=(uint16_t)(((uint32_t)PWM_PRIOD_LOAD*speed_percent[gear-1])/100u);
	if(maximum<minimum) maximum=minimum;
	if(trigger_duty<=minimum) return minimum;
	if(trigger_duty>=MAX_TOOL_DUTY) return maximum;
	/* 原扳机范围线性分配到最低占空比至本挡上限。 */
	return (uint16_t)(minimum+((uint32_t)(trigger_duty-minimum)*(maximum-minimum))/
		((uint16_t)MAX_TOOL_DUTY-minimum));
}

/**
 * @brief 前台确认扳机松开且电机完全停止后提交模式及挡位；失败保留待保存标志。
 */
void tool_mode_save_handle(void)
{
	/* 仅前台完全停机且松扳机时写Flash；制动/启动/运行期间只保留待保存标志。 */
	if(!tool_mode_list.save_pending || user_list.flag.bits.gToolTrigger ||
	   user_list.flag.bits.gToolEn || mc_core_list.motor_en ||
	   motor_control_list.bldc_state!=MOTOR_STOP)
	{
		return;
	}
	if(save_user_data_checked()) tool_mode_list.save_pending=0;
}
