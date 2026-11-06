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
	/* 双模式默认值先初始化，随后上电read_user_data恢复已提交设置。 */
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
	/* 调速重映射到本挡上限；情景采用固定目标，保留底层软启动及保护。 */
	motor_control_list.pwm_duty_aim=tool_mode_duty(motor_control_list.pwm_duty_aim_per);
	#endif
}
void user_direction_handle(void)
{
	static uint8_t candidate_dir=0;
	static uint8_t stable_count=0;
	uint8_t requested_dir;

	user_list.direction_ready=0;
	if(!user_list.led_input_valid)
	{
		stable_count=0;
		user_list.dir_valid=0;
		user_list.dir_retrig_need=1;
		return;
	}
	/* 参考板：低电平正转，高电平反转；禁止直接读取正在点灯的PB8。 */
	requested_dir=user_list.Dir_Data ? Tool_Dir_CCW_Value : Tool_Dir_CW_Value;
	if(requested_dir != candidate_dir)
	{
		candidate_dir=requested_dir;
		stable_count=0;
	}
	if(stable_count < 10)
	{
		stable_count++;
	}
	if(stable_count < 10)
	{
		return;
	}
	/* 首次上电只是同步开关方向，不是运行换向；无需松开供电扳机。
	   仍须采样有效、完成去抖且电机停止，所有故障许可由原状态机检查。 */
	if(!user_list.direction_initialized)
	{
		if(motor_control_list.bldc_state==MOTOR_STOP &&
		   !mc_core_list.motor_en && !user_list.flag.bits.gToolEn)
		{
			user_list.flag.bits.gToolDir=requested_dir;
			mc_core_list.mc_dir=requested_dir;
			tool_mode_restore_direction(requested_dir==Tool_Dir_CCW_Value);
			user_list.direction_initialized=1;
			user_list.dir_retrig_need=0;
			user_list.dir_valid=1;
			user_list.direction_ready=1;
		}
		return;
	}
	/* 运行中检测到换向先停机，保持原换相方向直到停稳并松开扳机。 */
	if(requested_dir != mc_core_list.mc_dir)
	{
		user_list.dir_valid=0;
		user_list.dir_retrig_need=1;
	}
	if(motor_control_list.bldc_state != MOTOR_STOP ||
	   mc_core_list.motor_en || user_list.flag.bits.gToolEn)
	{
		return;
	}
	if(user_list.flag.bits.gToolTrigger==0)
	{
		user_list.dir_retrig_need=0;
	}
	if(user_list.dir_retrig_need)
	{
		return;
	}
	user_list.flag.bits.gToolDir=requested_dir;
	mc_core_list.mc_dir=requested_dir;
	tool_mode_set_direction(requested_dir==Tool_Dir_CCW_Value);
	user_list.dir_valid=1;
	user_list.direction_ready=1;
}
void user_oc_handle(void)
{
		static u16 IBusAvgOCP1_count=0;
	static u16 IBusAvgOCP2_count=0;
	static u16 IBusAvgOCP3_count=0;
		static u16 IBusAvgOCP4_count=0;
	static u16 IBusPeakOCP1_count=0;
		static u16 IBusAvg_rec_count=0;
		#if (!Multistage_Integral_Peak_OCP_EN)
	static u16 IBusPeakOCP2_count=0;
	static u16 IBusPeakOCP3_count=0;
	static u16 IBusPeakREC_count=0;
	#endif
	u16  ibus_temp=0;
	u16  ipeak_temp=0;
	   
		
		if(mc_core_list.Board_ADC_DATA.u16I_SUM_Avg>mc_core_list.CurrentOffset_ad_data)
		{
			ibus_temp=mc_core_list.Board_ADC_DATA.u16I_SUM_Avg-mc_core_list.CurrentOffset_ad_data;
			ibus_temp=((((u32)ibus_temp)*user_list.Vcc_cal_gain)>>15);
		}
		else 
		{
			ibus_temp=0;
		
		}
			if(mc_core_list.Board_ADC_DATA.u16I_Peak>mc_core_list.CurrentOffset_ad_data)
		{
			ipeak_temp=mc_core_list.Board_ADC_DATA.u16I_Peak-mc_core_list.CurrentOffset_ad_data;
			user_list.IBusPeak_Adc=((((u32)ipeak_temp)*user_list.Vcc_cal_gain)>>15);
		}
		else 
		{
			user_list.IBusPeak_Adc=0;
		
		}	
		
		
//		user_list.IBusAvg1_Adc=(u16)((float)mc_core_list.pwm_actual_runing_data *ibus_temp/PWM_PRIOD_LOAD);	
//		user_list.IBusAvg_Adc=(user_list.IBusAvg1_Adc>>3)+user_list.IBusAvg_Adc-(user_list.IBusAvg_Adc>>3);
		user_list.IBusAvg_Adc=ibus_temp;
		//------------------ocp1
		if(user_list.IBusAvg_Adc>IBus_Avg_OCP1_AdcValue)
		{
				
			IBusAvgOCP1_count++;
			if(IBusAvgOCP1_count>IBus_Avg_OCP1_MS_CONUT)
			{
				IBusAvgOCP1_count=IBus_Avg_OCP1_MS_CONUT;
				user_list.userErr.bits.gIBusAvgOCP1=1;
				IBusAvg_rec_count=0;
			}
		}
		else 
		{
			if(IBusAvgOCP1_count>0)
			{
				IBusAvgOCP1_count--;
			}
			else
			{
				user_list.userErr.bits.gIBusAvgOCP1=0;
			
			}
		}
			////------------------ocp2
			if(user_list.IBusAvg_Adc>IBus_Avg_OCP2_AdcValue)
		{
				
			IBusAvgOCP2_count++;
			if(IBusAvgOCP2_count>IBus_Avg_OCP2_MS_CONUT)
			{
				IBusAvgOCP2_count=IBus_Avg_OCP2_MS_CONUT;
				user_list.userErr.bits.gIBusAvgOCP2=1;
				IBusAvg_rec_count=0;
			}
		}
		else 
		{
			if(IBusAvgOCP2_count>0)
			{
				IBusAvgOCP2_count--;
			}
			else
			{
				user_list.userErr.bits.gIBusAvgOCP2=0;
			
			}
			
		}
		
			//------------------ocp3
			if(user_list.IBusAvg_Adc>IBus_Avg_OCP3_AdcValue)
		{
				
			IBusAvgOCP3_count++;
			if(IBusAvgOCP3_count>IBus_Avg_OCP3_MS_CONUT)
			{
				IBusAvgOCP3_count=IBus_Avg_OCP3_MS_CONUT;
				user_list.userErr.bits.gIBusAvgOCP3=1;
				IBusAvg_rec_count=0;
			}
		}
		else 
		{
			if(IBusAvgOCP3_count>0)
			{
				IBusAvgOCP3_count--;
			}
			else
			{
				user_list.userErr.bits.gIBusAvgOCP3=0;
			
			}
			
		}	
				//------------------ocp4
		if(user_list.IBusAvg_Adc>IBus_Avg_OCP4_AdcValue)
		{
				
			IBusAvgOCP4_count++;
			if(IBusAvgOCP4_count>IBus_Avg_OCP4_MS_CONUT)
			{
				IBusAvgOCP4_count=IBus_Avg_OCP4_MS_CONUT;
				user_list.userErr.bits.gIBusAvgOCP4=1;
				IBusAvg_rec_count=0;
			}
		}
		else 
		{
			if(IBusAvgOCP4_count>0)
			{
				IBusAvgOCP4_count--;
			}
			else
			{
				user_list.userErr.bits.gIBusAvgOCP4=0;
			
			}
			
		}
		
		
		if(user_list.IBusAvg_Adc<IBus_Avg_OCP1_AdcValue&&motor_control_list.bldc_state==MOTOR_STOP)
		{
			IBusAvg_rec_count++;
			if(IBusAvg_rec_count>IBus_Avg_REC_MS_CONUT)
			{
				IBusAvg_rec_count=0;
				user_list.userErr.bits.gIBusAvgOCP4=0;
				user_list.userErr.bits.gIBusAvgOCP3=0;
				user_list.userErr.bits.gIBusAvgOCP2=0;
				user_list.userErr.bits.gIBusAvgOCP1=0;
				IBusAvgOCP4_count=0;
				IBusAvgOCP3_count=0;
				IBusAvgOCP2_count=0;
				IBusAvgOCP1_count=0;
			}
			
		}


#if Multistage_Integral_Peak_OCP_EN
		
			  if(user_list.IBusPeak_Adc > IBus_Smart_Peak_OCP3_AdcValue)
        {

						IBusPeakOCP1_count+=IBus_Smart_OCP3_MS_Operation_Value;
            if(IBusPeakOCP1_count >= 32768 )
            {
               IBusPeakOCP1_count= 32768;
							//mos���ش���
							if(user_list.userErr.bits.gIBusPeakOCP1==0)
							{
//								user_list.Mos_overload_count++;
							
							}
							user_list.userErr.bits.gIBusPeakOCP1 = 1; 
            }
        }
		    else  if(user_list.IBusPeak_Adc > IBus_Smart_Peak_OCP2_AdcValue)
        {

						IBusPeakOCP1_count+=IBus_Smart_OCP2_MS_Operation_Value;
            if(IBusPeakOCP1_count >= 32768 )
            {
               IBusPeakOCP1_count= 32768;
							//mos���ش���
							if(user_list.userErr.bits.gIBusPeakOCP1==0)
							{
//								user_list.Mos_overload_count++;
							
							}
							user_list.userErr.bits.gIBusPeakOCP1 = 1; 
            }
        }
				else if(user_list.IBusPeak_Adc > IBus_Smart_Peak_OCP1_AdcValue)
				{
				
						IBusPeakOCP1_count+=IBus_Smart_OCP1_MS_Operation_Value;
            if(IBusPeakOCP1_count >= 32768 )
            {
               IBusPeakOCP1_count= 32768;
							//mos���ش���
							if(user_list.userErr.bits.gIBusPeakOCP1==0)
							{
//								user_list.Mos_overload_count++;
							
							}
							user_list.userErr.bits.gIBusPeakOCP1 = 1; 
            }
				
				
				}
        else  
					
        {
					
						if(motor_control_list.bldc_state==MOTOR_STOP)
						{
								if(IBusPeakOCP1_count>IBus_Smart_REC_MS_Operation_Value)
							{
									IBusPeakOCP1_count-=IBus_Smart_REC_MS_Operation_Value;
							}
							else
							{
								IBusPeakOCP1_count=0;
									user_list.userErr.bits.gIBusPeakOCP1 = 0;
							}
						}
						else
						{
					
							if(IBusPeakOCP1_count>IBus_Smart_Peak_OCP1_AdcValue)
							{
									IBusPeakOCP1_count-=IBus_Smart_Peak_OCP1_AdcValue;
							}
							else
							{
								IBusPeakOCP1_count=0;
									user_list.userErr.bits.gIBusPeakOCP1 = 0;
							}
						}
        }
		
#else
        if(user_list.IBusPeak_Adc > IBus_Peak_OCP1_AdcValue)
        {
            if(IBusPeakOCP1_count++ >= IBus_Peak_OCP1_MS_CONUT )
            {
               IBusPeakOCP1_count= IBus_Peak_OCP1_MS_CONUT;

							user_list.userErr.bits.gIBusPeakOCP1 = 1; 
            }
        }
        else
        {
            if(IBusPeakOCP1_count)
            {
                IBusPeakOCP1_count--;
            }
            else
            {
//								user_list.userErr.bits.gIBusPeakOCP1 = 0;
            }
        }
				
				
				
				  if(user_list.IBusPeak_Adc > IBus_Peak_OCP2_AdcValue)
        {
            if(IBusPeakOCP2_count++ >= IBus_Peak_OCP2_MS_CONUT )
            {
               IBusPeakOCP2_count= IBus_Peak_OCP2_MS_CONUT;

							user_list.userErr.bits.gIBusPeakOCP1 = 1; 
            }
        }
        else
        {
            if(IBusPeakOCP2_count)
            {
                IBusPeakOCP2_count--;
            }
            else
            {
//								user_list.userErr.bits.gIBusPeakOCP1 = 0;
            }
        }
				
				
					if(user_list.IBusPeak_Adc > IBus_Peak_OCP3_AdcValue)
        {
            if(IBusPeakOCP3_count++ >= IBus_Peak_OCP3_MS_CONUT )
            {
               IBusPeakOCP3_count= IBus_Peak_OCP3_MS_CONUT;

							user_list.userErr.bits.gIBusPeakOCP1 = 1; 
            }
        }
        else
        {
            if(IBusPeakOCP3_count)
            {
                IBusPeakOCP3_count--;
            }
            else
            {
//								user_list.userErr.bits.gIBusPeakOCP1 = 0;
            }
        }
				
				if(user_list.userErr.bits.gIBusPeakOCP1)
				{
					if(user_list.IBusPeak_Adc < IBus_Peak_OCP1_AdcValue)
					{
						IBusPeakREC_count++;
						if(IBusPeakREC_count>IBus_Peak_REC_MS_CONUT)
						{
							user_list.userErr.bits.gIBusPeakOCP1=0;
						}
										
					}
				}
				else 
				{
					IBusPeakREC_count=0;
				
				}
				
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
	tool_mode_pwm_update((uint16_t)mc_core_list.pwm_actual_runing_data);
}
#define TEST_BASELINE 0
#define TEST_ONLY_CASE 0
#include "D:/mym/NXN/3.yuantuo/V10_qizhiji/core/NX32F002-V1_01-lib-release/tests/tool_mode/test_tool_mode.c"