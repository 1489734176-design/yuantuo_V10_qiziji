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
static volatile uint32_t mode_clock_ms;
static volatile uint32_t reverse_scene_full_start;
static volatile uint8_t reverse_scene_stage;
static uint8_t scene_load_running;
static uint8_t scene_load_boost;
static uint8_t scene_load_pending;
static uint32_t scene_load_run_start;
static uint32_t scene_load_confirm_start;

#define REVERSE_SCENE_RAMP (0u)
#define REVERSE_SCENE_HOLD (1u)
#define REVERSE_SCENE_LOW  (2u)

/**
 * @brief 清除正转情景1挡本次运行的负载升速和计时，不改变模式挡位记忆。
 */
static void tool_mode_scene_load_reset(void)
{
	scene_load_running=0;
	scene_load_boost=0;
	scene_load_pending=0;
	scene_load_run_start=0;
	scene_load_confirm_start=0;
}

/**
 * @brief 初始化正反转独立模式、挡位、情景曲线及负载状态，上电默认不写Flash。
 */
void tool_mode_init(void)
{
	/* 空Flash默认调速1档；上电默认不写Flash，避免无意义磨损。 */
	tool_mode_list.setting[TOOL_DIRECTION_FORWARD].mode=TOOL_MODE_SPEED;
	tool_mode_list.setting[TOOL_DIRECTION_FORWARD].speed_gear=1;
	tool_mode_list.setting[TOOL_DIRECTION_FORWARD].scene_gear=1;
	tool_mode_list.setting[TOOL_DIRECTION_REVERSE].mode=TOOL_MODE_SPEED;
	tool_mode_list.setting[TOOL_DIRECTION_REVERSE].speed_gear=1;
	tool_mode_list.setting[TOOL_DIRECTION_REVERSE].scene_gear=4;
	tool_mode_list.active_direction=TOOL_DIRECTION_FORWARD;
	tool_mode_list.save_pending=0;
	mode_clock_ms=0;
	reverse_scene_full_start=0;
	reverse_scene_stage=REVERSE_SCENE_RAMP;
	tool_mode_scene_load_reset();
	key_poll_count=0;
	key_candidate=0;
	key_stable_count=0;
	key_press_latched=0;
	tool_mode_key_raw=0;
	tool_mode_key_read_ok=0;
}

/**
 * @brief 返回当前已确认方向的独立模式和挡位设置。
 */
tool_mode_setting_t *tool_mode_current(void)
{
	return &tool_mode_list.setting[tool_mode_list.active_direction];
}

/**
 * @brief 恢复开关所在方向的记忆，同步档位并清除反转曲线及正转负载升速进度。
 */
void tool_mode_restore_direction(uint8_t reverse)
{
	tool_mode_list.active_direction=reverse ? TOOL_DIRECTION_REVERSE : TOOL_DIRECTION_FORWARD;
	reverse_scene_stage=REVERSE_SCENE_RAMP;
	tool_mode_scene_load_reset();
	user_list.gears_level=tool_mode_gear()-1;
}

/**
 * @brief 停稳后实际换向，双调速沿用当前档位，涉及情景则恢复目标方向记忆。
 */
void tool_mode_set_direction(uint8_t reverse)
{
	uint8_t direction=reverse ? TOOL_DIRECTION_REVERSE : TOOL_DIRECTION_FORWARD;
	tool_mode_setting_t *source=tool_mode_current();
	tool_mode_setting_t *target=&tool_mode_list.setting[direction];
	if(tool_mode_list.active_direction==direction) return;
	if(source->mode==TOOL_MODE_SPEED && target->mode==TOOL_MODE_SPEED &&
	   target->speed_gear!=source->speed_gear)
	{
		target->speed_gear=source->speed_gear;
		tool_mode_list.save_pending=1;
	}
	tool_mode_restore_direction(reverse);
}

/**
 * @brief 在1ms中断内递增情景曲线及负载检测时钟，前台按时间差计时。
 */
void tool_mode_tick_1ms(void)
{
	mode_clock_ms++;
}

/**
 * @brief 返回当前挡位，调速1～4，正转情景1～3，反转情景固定4档。
 */
uint8_t tool_mode_gear(void)
{
	tool_mode_setting_t *setting=tool_mode_current();
	if(setting->mode==TOOL_MODE_SCENE)
	{
		return tool_mode_list.active_direction==TOOL_DIRECTION_REVERSE ? 4 : setting->scene_gear;
	}
	return setting->speed_gear;
}

/**
 * @brief 处理一次有效按压，反转情景键只切模式，正转按键与调速升挡保持原逻辑。
 *        有效切换后清除本次运行的情景曲线和负载升速进度。
 * @param key K2调速键或K3情景键的TM1650键码。
 */
void tool_mode_key_event(uint8_t key)
{
	tool_mode_setting_t *setting=tool_mode_current();
	/* 扳机按住及休眠期间，模式/挡位按键均无效。 */
	if(user_list.flag.bits.gToolTrigger || user_list.user_state==TOOL_POWER_DOWN) return;
	/* 切到另一模式只切换；重复按当前模式键才循环升挡。 */
	if(key==TOOL_MODE_KEY_SPEED)
	{
		if(setting->mode!=TOOL_MODE_SPEED)
		{
			setting->mode=TOOL_MODE_SPEED;
		}
		else
		{
			if(++setting->speed_gear>4) setting->speed_gear=1;
		}
	}
	else if(key==TOOL_MODE_KEY_SCENE)
	{
		if(tool_mode_list.active_direction==TOOL_DIRECTION_REVERSE)
		{
			setting->mode=(setting->mode==TOOL_MODE_SCENE) ? TOOL_MODE_SPEED : TOOL_MODE_SCENE;
			setting->scene_gear=4;
		}
		else if(setting->mode!=TOOL_MODE_SCENE)
		{
			setting->mode=TOOL_MODE_SCENE;
		}
		else
		{
			if(++setting->scene_gear>3) setting->scene_gear=1;
		}
	}
	else
	{
		return;
	}
	/* 旧用户结构保留0起始挡位索引，持久化使用新模块的1起始挡位。 */
	reverse_scene_stage=REVERSE_SCENE_RAMP;
	tool_mode_scene_load_reset();
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
 * @brief 在实际PWM更新处捕获反转情景首满值，避免前台延迟或限流回落漏掉起点。
 */
void tool_mode_pwm_update(uint16_t duty)
{
	uint16_t full=(uint16_t)(((uint32_t)PWM_PRIOD_LOAD*TOOL_REVERSE_SCENE_FULL_DUTY_PERCENT)/100u);
	if(reverse_scene_stage!=REVERSE_SCENE_RAMP ||
	   tool_mode_list.active_direction!=TOOL_DIRECTION_REVERSE ||
	   tool_mode_current()->mode!=TOOL_MODE_SCENE ||
	   !user_list.flag.bits.gToolTrigger || !user_list.flag.bits.gToolEn ||
	   !mc_core_list.motor_en || motor_control_list.bldc_state!=MOTOR_RUN || duty<full)
	{
		return;
	}
	/* 先发布起点再发布阶段，前台只在看到保持阶段后读取时间戳。 */
	reverse_scene_full_start=mode_clock_ms;
	reverse_scene_stage=REVERSE_SCENE_HOLD;
}

/**
 * @brief 反转情景软升至满值，按PWM更新捕获的起点保持指定时间，再软降至低速。
 */
static uint16_t tool_mode_reverse_scene_duty(void)
{
	uint16_t full=(uint16_t)(((uint32_t)PWM_PRIOD_LOAD*TOOL_REVERSE_SCENE_FULL_DUTY_PERCENT)/100u);
	uint16_t low=(uint16_t)(((uint32_t)PWM_PRIOD_LOAD*TOOL_REVERSE_SCENE_LOW_DUTY_PERCENT)/100u);
	uint32_t start;
	uint32_t now;
	/* 停机、松扳机或撤销许可均从升速阶段重新开始，底层保护继续控制实际输出。 */
	if(!user_list.flag.bits.gToolTrigger || !user_list.flag.bits.gToolEn ||
	   !mc_core_list.motor_en || motor_control_list.bldc_state!=MOTOR_RUN)
	{
		reverse_scene_stage=REVERSE_SCENE_RAMP;
		return full;
	}
	if(reverse_scene_stage==REVERSE_SCENE_HOLD)
	{
		/* 按阶段、起点、当前时钟顺序取快照，防止中断新起点与旧时钟相减下溢。 */
		start=reverse_scene_full_start;
		now=mode_clock_ms;
		if((uint32_t)(now-start)>=TOOL_REVERSE_SCENE_FULL_TIME_MS)
		{
			reverse_scene_stage=REVERSE_SCENE_LOW;
		}
	}
	return reverse_scene_stage==REVERSE_SCENE_LOW ? low : full;
}

/**
 * @brief 正转情景1挡按平均电流确认负载升速，卸载后恢复正常目标，保持原PWM软坡。
 */
static uint16_t tool_mode_scene_load_duty(void)
{
	uint16_t normal=(uint16_t)(((uint32_t)PWM_PRIOD_LOAD*TOOL_SCENE_GEAR1_DUTY_PERCENT)/100u);
	uint16_t boost=(uint16_t)(((uint32_t)PWM_PRIOD_LOAD*TOOL_SCENE_GEAR1_BOOST_DUTY_PERCENT)/100u);
	uint32_t now;
	uint32_t confirm_ms;
	uint8_t threshold_met;
	/* 任一许可或保护条件失效都清除升速状态；实际停机仍由原保护及电机状态机处理。 */
	if(!user_list.flag.bits.gToolTrigger || !user_list.flag.bits.gToolEn ||
	   !mc_core_list.motor_en || motor_control_list.bldc_state!=MOTOR_RUN ||
	   user_list.userErr.word || mc_core_list.MC_error.word ||
	   machine_error_hold.userErr_hold.word || machine_error_hold.MC_error_hold.word)
	{
		tool_mode_scene_load_reset();
		return normal;
	}
	now=mode_clock_ms;
	if(!scene_load_running)
	{
		scene_load_running=1;
		scene_load_run_start=now;
	}
	/* 启动屏蔽只作用于本次运行开始，避免启动尖峰直接触发满占空比。 */
	if((uint32_t)(now-scene_load_run_start)<TOOL_SCENE_GEAR1_STARTUP_MASK_MS)
	{
		scene_load_pending=0;
		return normal;
	}
	/* 使用扣零偏及VCC校准后的平均电流；独立升降阈值形成迟滞。 */
	if(scene_load_boost)
	{
		threshold_met=user_list.IBusAvg_Adc<=TOOL_SCENE_GEAR1_LOAD_OFF_ADC;
		confirm_ms=TOOL_SCENE_GEAR1_LOAD_OFF_MS;
	}
	else
	{
		threshold_met=user_list.IBusAvg_Adc>=TOOL_SCENE_GEAR1_LOAD_ON_ADC;
		confirm_ms=TOOL_SCENE_GEAR1_LOAD_ON_MS;
	}
	if(!threshold_met)
	{
		scene_load_pending=0;
	}
	else if(!scene_load_pending)
	{
		scene_load_pending=1;
		scene_load_confirm_start=now;
	}
	else if((uint32_t)(now-scene_load_confirm_start)>=confirm_ms)
	{
		scene_load_boost=!scene_load_boost;
		scene_load_pending=0;
	}
	return scene_load_boost ? boost : normal;
}

/**
 * @brief 按parameter.h中的当前挡百分比生成目标PWM计数。
 *        调速映射扳机，正转情景1挡按负载升降速，反转情景按定时曲线降速。
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
	tool_mode_setting_t *setting=tool_mode_current();
	if(setting->mode==TOOL_MODE_SCENE)
	{
		if(tool_mode_list.active_direction==TOOL_DIRECTION_REVERSE)
		{
			tool_mode_scene_load_reset();
			return tool_mode_reverse_scene_duty();
		}
		reverse_scene_stage=REVERSE_SCENE_RAMP;
		/* 正转1挡按负载切换目标，2/3挡仍读取固定目标；均与扣动深度无关。 */
		gear=setting->scene_gear;
		if(gear<1 || gear>3) gear=1;
		if(gear==1) return tool_mode_scene_load_duty();
		tool_mode_scene_load_reset();
		return (uint16_t)(((uint32_t)PWM_PRIOD_LOAD*scene_percent[gear-1])/100u);
	}
	reverse_scene_stage=REVERSE_SCENE_RAMP;
	tool_mode_scene_load_reset();
	gear=setting->speed_gear;
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
