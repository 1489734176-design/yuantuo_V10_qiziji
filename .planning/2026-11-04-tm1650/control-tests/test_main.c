#include "main.h"
#include "board.h"
#include "user_control.h"
#include "parameter.h"
#include "motor_control.h"
#include "motor_config.h"
#include "flash_data_save.h"
#include "drv_pwm.h"
User_list_Struct user_list;
Machine_ERROR_HOLD machine_error_hold;
Mc_core_list mc_core_list;
Motor_Control_list motor_control_list;
#include "tool_mode.h"
void user_para_init(void)
{
	user_list.userErr.word=0;
	user_list.user_state=TOOL_STOP;
	/* 中文：双模式默认值先初始化，随后上电read_user_data恢复已提交设置。 */
	tool_mode_init();
		user_list.flag.bits.gToolDir= Tool_Dir_Init;
	/* 扳机为上电来源：首次稳定方向确认后允许本次扣动启动。 */
	user_list.direction_initialized=0;
	user_list.dir_valid=0;
	user_list.dir_retrig_need=1;

}
void pwm_duty_aim_deal(void)
{
	
	#if(TRG_DIGITAL_EN)
		motor_control_list.pwm_duty_aim_per = MAX_TOOL_DUTY;
	#else
		/* 电位器连续调速：扳机电压线性映射为目标占空比 */
		if(mc_core_list.Board_ADC_DATA.u16VR < MIN_TRG_ADC)
		{
			motor_control_list.pwm_duty_aim_per = MIN_TOOL_DUTY;
		}
		else if(mc_core_list.Board_ADC_DATA.u16VR >= MAX_TRG_ADC)
		{
			motor_control_list.pwm_duty_aim_per = MAX_TOOL_DUTY;
		}
		else
		{
			motor_control_list.pwm_duty_aim_per = (uint16_t)((float)(mc_core_list.Board_ADC_DATA.u16VR - MIN_TRG_ADC)*K_ADC_DUTY + MIN_TOOL_DUTY);
		}
	#endif
	#if(0)
	
		
		#if 0
	if(motor_run_count<50)
	
	{
		pwm_duty_aim_PI_OUT=0;
	
	}
	else 
	{
		if(mc_core_list.time_count_60_degree>((u32)(1500*MOTOR_TIMR_FRQ_MHZ)))
		{
			PI_p=mc_core_list.time_count_60_degree-((u32)(1500*MOTOR_TIMR_FRQ_MHZ));
			PI_p=(PI_p>>2);
			if(PI_sum_i<PWM_PRIOD_LOAD)
			{
				PI_sum_i++;
				
			}
			pwm_duty_aim_PI_OUT=PI_sum_i+PI_p;
		}
		
		else 
		{
			PI_p=((u32)(1500*MOTOR_TIMR_FRQ_MHZ))-mc_core_list.time_count_60_degree;
			PI_p=(PI_p>>2);
			if(PI_sum_i>LIMIT_MIN_PWN_DUTY)
			{
				PI_sum_i--;
			}
			if(PI_sum_i>PI_p)
			{
				pwm_duty_aim_PI_OUT=PI_sum_i-PI_p;
			}
			else 
			{
				pwm_duty_aim_PI_OUT=LIMIT_MIN_PWN_DUTY;
			
			}
		}
		

	}
	
		if(pwm_duty_aim_PI_OUT>LIMIT_MAX_PWN_DUTY)
		{
			pwm_duty_aim_PI_OUT=LIMIT_MAX_PWN_DUTY;
		
		}
			if(pwm_duty_aim_PI_OUT>motor_control_list.pwm_duty_aim_per)
		{
			motor_control_list.pwm_duty_aim=pwm_duty_aim_PI_OUT;
		}
		else 
		{
		
			motor_control_list.pwm_duty_aim=motor_control_list.pwm_duty_aim_per;
		}
		#else
//		motor_control_list.pwm_duty_aim_per=(u16)((PWM_PRIOD_LOAD)*1.0f);
		motor_control_list.pwm_duty_aim=motor_control_list.pwm_duty_aim_per;
		#endif
	#endif

}
void user_gears_handle(void)
{
	
	#if 0
		u8 res=0;
		static uint8_t pulse_dir=0;
		static uint16_t pulse_cout=0;
		pulse_th_handle();
		res=key_scan();
		if(user_list.user_state==TOOL_STOP)
		{
				if(res==1)
				{
					if(user_list.gears_level<1)
					{
						user_list.gears_level++;
						
					}
					else
					{
						user_list.gears_level=0;
					}
					save_user_data();	
			//		user_list.gears_level_changed=1;
				}
		}

	if(user_list.gears_level==0)
	{
		if(pulse_mode)
		{
			motor_control_list.pwm_duty_aim_per=(motor_control_list.pwm_duty_aim_per*PWM_DUTY_GEARS1)>>15;
		
			if(pulse_dir==0)
			{
				
				if(motor_control_list.pwm_duty_aim>LIMIT_MIN_PWN_DUTY)
				{
					motor_control_list.pwm_duty_aim-=4;
				}
				pulse_cout++;
				if(pulse_cout>=250)
				{
					pulse_dir=1;
				}
			}
			else 
			{
				if(motor_control_list.pwm_duty_aim<pulse_max_duty||motor_control_list.pwm_duty_aim<((u32)(PWM_PRIOD_LOAD*0.20)))
				{
					motor_control_list.pwm_duty_aim+=4;

					
				}
				
				if(pulse_cout) 
				{
					pulse_cout--;
					
				}
				else 
				{
					pulse_max_duty+=((u32)(PWM_PRIOD_LOAD*0.02f));
					if(pulse_max_duty>((u32)(PWM_PRIOD_LOAD*0.45f)))
					{
						
						pulse_max_duty=((u32)(PWM_PRIOD_LOAD*0.45f));
					}
					pulse_dir=0;
				}
			
			}
		}
		else 
		{
			motor_control_list.pwm_duty_aim=(motor_control_list.pwm_duty_aim_per*PWM_DUTY_GEARS1)>>15;
		
		}
		
	}
	else 
	{
	
		motor_control_list.pwm_duty_aim=(motor_control_list.pwm_duty_aim_per*PWM_DUTY_GEARS3)>>15;
	}
	#else 
	/* 中文：调速重映射到本挡上限；情景采用固定目标，保留底层软启动及保护。 */
	motor_control_list.pwm_duty_aim=tool_mode_duty(motor_control_list.pwm_duty_aim_per);
	#endif
}
void pwm_set(void)
{
	static u8 pwm_add_count=0;
	static u8 pwm_sub_count=0;
	static u8 pwm_add_fast_count=0;
	static u8 pwm_sub_fast_count=0;
	u8 limit_current_on=0;

	#if(LIMIT_PEAK_CURRENT_EN)
	uint16_t IBusPeak_Adc_temp=0;
	#endif
	#if(LIMIT_AVG_CURRENT_EN)
	u32 limit_current_temp=0;
	uint16_t IBusAvg_Adc_temp=0;
	#endif

	/* 计算峰值电流ADC值（减去偏置，并校准VCC增益） */
	#if(LIMIT_PEAK_CURRENT_EN)
		if(mc_core_list.Board_ADC_DATA.u16I_Peak>mc_core_list.CurrentOffset_ad_data)
	{
		IBusPeak_Adc_temp=mc_core_list.Board_ADC_DATA.u16I_Peak-mc_core_list.CurrentOffset_ad_data;
		IBusPeak_Adc_temp=((((u32)IBusPeak_Adc_temp)*user_list.Vcc_cal_gain)>>15);
	}
	else
	{
		IBusPeak_Adc_temp=0;
	}
	#endif

	/* 计算平均电流ADC值（减去偏置，并校准VCC增益） */
	#if(LIMIT_AVG_CURRENT_EN)
	if( mc_core_list.Board_ADC_DATA.u16I_SUM_Avg> mc_core_list.CurrentOffset_ad_data)
	{
		IBusAvg_Adc_temp=mc_core_list.Board_ADC_DATA.u16I_SUM_Avg-mc_core_list.CurrentOffset_ad_data;
		IBusAvg_Adc_temp=((((u32)IBusAvg_Adc_temp)*user_list.Vcc_cal_gain)>>15);
	}
	else
	{
		IBusAvg_Adc_temp=0;
	}
	#endif

	/* 峰值电流限流判断 */
	#if(LIMIT_PEAK_CURRENT_EN)
	if(IBusPeak_Adc_temp>LIMIT_PEAK_CURRENT_AdcValue)
	{
		limit_current_on=1;
	}
	#endif

	/* 平均电流限流判断：根据转向和档位选择不同的限流阈值 */
	#if(LIMIT_AVG_CURRENT_EN)
	if(user_list.flag.bits.gToolDir==Tool_Dir_CCW_Value)
	{
		/* 反转：使用反转限流值 */
		limit_current_temp=LIMIT_AVG_CURRENT_AdcValue_CCW;
	}
	else
	{
		/* 正转：根据档位选择限流值，档位越高限流越大 */
		if(user_list.gears_level==0)
		{
			limit_current_temp=LIMIT_AVG_CURRENT1_AdcValue;
		}
		else if(user_list.gears_level==1)
		{
			limit_current_temp=LIMIT_AVG_CURRENT2_AdcValue;
		}
		else
		{
			limit_current_temp=LIMIT_AVG_CURRENT3_AdcValue;
		}
	}
	if(IBusAvg_Adc_temp>limit_current_temp)
	{
		limit_current_on=1;
	}
	#endif

	/* 虚拟占空比斜坡上升：每PWM_ADD_GAP次中断增加PWM_ADD_VALUE */
	if((motor_control_list.pwm_duty_virtual<motor_control_list.pwm_duty_aim))
	{
		pwm_add_count++;
		if(pwm_add_count>=PWM_ADD_GAP)
		{
			pwm_add_count=0;
			motor_control_list.pwm_duty_virtual+=PWM_ADD_VALUE;
			/* 不超过目标值 */
			if(motor_control_list.pwm_duty_virtual>motor_control_list.pwm_duty_aim)
			{
				motor_control_list.pwm_duty_virtual=motor_control_list.pwm_duty_aim;
			}
		}
	}
	/* 虚拟占空比斜坡下降：每PWM_SUB_GAP次中断减少PWM_SUB_VALUE */
	else if(motor_control_list.pwm_duty_virtual>motor_control_list.pwm_duty_aim)
	{
		pwm_sub_count++;
		if(pwm_sub_count>=PWM_SUB_GAP)
		{
			pwm_sub_count=0;
			if(motor_control_list.pwm_duty_virtual>PWM_SUB_VALUE)
			{
				motor_control_list.pwm_duty_virtual-=PWM_SUB_VALUE;
			}
			/* 不低于目标值 */
			if(motor_control_list.pwm_duty_virtual<motor_control_list.pwm_duty_aim)
			{
				motor_control_list.pwm_duty_virtual=motor_control_list.pwm_duty_aim;
			}
		}
	}

	/* 虚拟占空比限幅 */
	if(motor_control_list.pwm_duty_virtual<LIMIT_MIN_PWN_DUTY)
	{
		motor_control_list.pwm_duty_virtual=LIMIT_MIN_PWN_DUTY;
	}
	if(motor_control_list.pwm_duty_virtual>LIMIT_MAX_PWN_DUTY)
	{
		motor_control_list.pwm_duty_virtual=LIMIT_MAX_PWN_DUTY;
	}

	#if LIMIT_SPEED_EN
	/* 速度闭环模式：实际占空比跟踪pwm_duty_aim2（PI输出） */
		if((mc_core_list.pwm_actual_runing_data<motor_control_list.pwm_duty_aim2)&&(limit_current_on==0))
	{
		pwm_add_fast_count++;
		if(pwm_add_fast_count>=PWM_ADD_FAST_GAP)
		{
			pwm_add_fast_count=0;
			mc_core_list.pwm_actual_runing_data+=PWM_ADD_FAST_VALUE;
			if(mc_core_list.pwm_actual_runing_data>motor_control_list.pwm_duty_aim2)
			{
				mc_core_list.pwm_actual_runing_data=motor_control_list.pwm_duty_aim2;
			}
		}
	}
	if(mc_core_list.pwm_actual_runing_data>motor_control_list.pwm_duty_aim2||limit_current_on)
	{
		pwm_sub_fast_count++;
		if(pwm_sub_fast_count>=PWM_SUB_FAST_GAP)
		{
			pwm_sub_fast_count=0;
			if(mc_core_list.pwm_actual_runing_data>PWM_SUB_FAST_VALUE)
			{
				mc_core_list.pwm_actual_runing_data-=PWM_SUB_FAST_VALUE;
			}
			if((mc_core_list.pwm_actual_runing_data<motor_control_list.pwm_duty_aim2)&&(limit_current_on==0))
			{
				mc_core_list.pwm_actual_runing_data=motor_control_list.pwm_duty_aim2;
			}
		}
	}
	#else
	/* 非速度闭环模式：实际占空比快速跟踪虚拟占空比 */
	if((mc_core_list.pwm_actual_runing_data<motor_control_list.pwm_duty_virtual)&&(limit_current_on==0))
	{
		pwm_add_fast_count++;
		if(pwm_add_fast_count>=PWM_ADD_FAST_GAP)
		{
			pwm_add_fast_count=0;
			mc_core_list.pwm_actual_runing_data+=PWM_ADD_FAST_VALUE;
			if(mc_core_list.pwm_actual_runing_data>motor_control_list.pwm_duty_virtual)
			{
				mc_core_list.pwm_actual_runing_data=motor_control_list.pwm_duty_virtual;
			}
		}
	}
	if(mc_core_list.pwm_actual_runing_data>motor_control_list.pwm_duty_virtual||limit_current_on)
	{
		pwm_sub_fast_count++;
		if(pwm_sub_fast_count>=PWM_SUB_FAST_GAP)
		{
			pwm_sub_fast_count=0;
			if(mc_core_list.pwm_actual_runing_data>PWM_SUB_FAST_VALUE)
			{
				mc_core_list.pwm_actual_runing_data-=PWM_SUB_FAST_VALUE;
			}
			if((mc_core_list.pwm_actual_runing_data<motor_control_list.pwm_duty_virtual)&&(limit_current_on==0))
			{
				mc_core_list.pwm_actual_runing_data=motor_control_list.pwm_duty_virtual;
			}
		}
	}
	#endif

	/* 重启功能：有负载且方向改变时限制最小占空比，防止重启冲击 */
	#if(RESTAR_EN&&Wrench_EN)
	if(user_list.Load_exist||(user_list.flag.bits.gToolDir==Tool_Dir_CCW_Value&&user_list.flag.bits.gAutoStopEn))
	{
		if(mc_core_list.pwm_actual_runing_data<RESTAR_MIN_PWM_LIMIT_DUTY)
		{
			mc_core_list.pwm_actual_runing_data=RESTAR_MIN_PWM_LIMIT_DUTY;
		}
	}
	#endif

	/* 实际输出占空比限幅 */
	if(mc_core_list.pwm_actual_runing_data<LIMIT_MIN_PWN_DUTY)
	{
		mc_core_list.pwm_actual_runing_data=LIMIT_MIN_PWN_DUTY;
	}
	if(mc_core_list.pwm_actual_runing_data>LIMIT_MAX_PWN_DUTY)
	{
		mc_core_list.pwm_actual_runing_data=LIMIT_MAX_PWN_DUTY;
	}

	/* 更新PWM硬件寄存器 */
	PwmDutyUpdate(mc_core_list.pwm_actual_runing_data);
}
#define TEST_BASELINE 0
#include "D:/mym/NXN/3.yuantuo/V10_qizhiji/core/NX32F002-V1_01-lib-release/tests/tool_mode/test_tool_mode.c"