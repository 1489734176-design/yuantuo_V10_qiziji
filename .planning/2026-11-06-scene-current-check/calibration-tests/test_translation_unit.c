#include "D:/mym/NXN/3.yuantuo/V10_qizhiji/core/NX32F002-V1_01-lib-release/tests/current_offset/test_current_offset.c"
void CurrentOffsetCalibration(void)
{
	u16 t_ccr4_temp;
	u8 i=0;
	u32 ibus_adc_sum=0;
	u16 avg_sample;
	u8 avg_valid=1;
	u8 avg_wait_count;

	/* 停止电机输出 */
	Stop_Motor();
	user_list.IBusAvgOffsetValid=0;
	user_list.IBusAvgOffset_Adc=0;
	user_list.IBusAvgScene_Adc=0;
	/* 关闭TIM1更新中断 */
	TIM_ITConfig(TIM1, TIM_IT_Update, DISABLE);
	systick_delay_us(500);

	/* 保存并修改TIM1 CCR4为最大值，确保ADC触发关闭 */
	t_ccr4_temp=TIM1->CCR4;
	TIM1->CCR4 = 65535;
	systick_delay_us(2000);

	/* 等待ADC转换完成 */
	while(ADC1->ADSTA&0x00000004)
	{;}

	/* 关闭ADC外部触发转换 */
	ADC_ExternalTrigConvCmd(ADC1, DISABLE);
	systick_delay_us(500);

	/* 禁用ADC和中断，切换通道进行校准采样 */
	ADC_Cmd(ADC1, DISABLE);
	ADC_ITConfig(ADC1, ADC_IT_EOC, DISABLE);
	TIM_ITConfig(TIM1, TIM_IT_Update, DISABLE);

	/* 配置ADC单通道模式，选择运放输出通道 */
	ADC_AnyChannelNumCfg(ADC1, 0);
	ADC_AnyChannelSelect(ADC1, 0, OP_O_CHANNEL);
	ADC_AnyChannelCmd(ADC1, ENABLE);

	/* 确认外部触发关闭 */
	ADC_ExternalTrigConvCmd(ADC1, DISABLE);
	ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
	ADC_SampleTimeConfig(ADC1,0, ADC_SampleTime_3_5);
	ADC_Cmd(ADC1, ENABLE);
	systick_delay_us(500);

	/* 丢弃前两次采样，稳定ADC */
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == 0);
	ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == 0);
	ADC_ClearFlag(ADC1, ADC_FLAG_EOC);

	/* 采集16次取平均作为零偏值 */
	ibus_adc_sum=0;
	for(i=0;i<16;i++)
	{
		ADC_SoftwareStartConvCmd(ADC1, ENABLE);
		while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == 0);
		ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
		ibus_adc_sum += (uint16_t)GET_ADC_VALUE(OP_O_CHANNEL);
	}
	/* 右移4位等价于除以16，得到16次采样的平均值 */
	mc_core_list.CurrentOffset_ad_data=ibus_adc_sum>>4;

	/* AVG经10k/1uF低通；停机再等待50ms稳定，零偏与运行都使用CH4原始ADC码。 */
	ADC_Cmd(ADC1, DISABLE);
	ADC_AnyChannelSelect(ADC1, 0, ISUMAVG_CHANNEL);
	ADC_SampleTimeConfig(ADC1, ISUMAVG_CHANNEL, ADC_SampleTime_3_5);
	ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
	ADC_Cmd(ADC1, ENABLE);
	systick_delay_us(50000);
	ibus_adc_sum=0;
	for(i=0;i<18;i++)
	{
		ADC_SoftwareStartConvCmd(ADC1, ENABLE);
		avg_wait_count=0;
		/* 新增校准等待有界；失败只禁用负载升速，随后仍恢复原采样和保护。 */
		while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC)==0)
		{
			if(++avg_wait_count>=100)
			{
				avg_valid=0;
				break;
			}
			systick_delay_us(1);
		}
		ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
		if(!avg_valid) break;
		avg_sample=(u16)GET_ADC_VALUE(ISUMAVG_CHANNEL);
		/* 切通道后的前两次丢弃；有效样本贴电源轨时不采用这个零偏。 */
		if(i>=2)
		{
			if(avg_sample==0 || avg_sample>=4095)
			{
				avg_valid=0;
				break;
			}
			ibus_adc_sum+=avg_sample;
		}
	}
	if(avg_valid)
	{
		user_list.IBusAvgOffset_Adc=(u16)(ibus_adc_sum>>4);
		user_list.IBusAvgOffsetValid=1;
	}

	/* 禁用ADC，恢复系统ADC配置 */
	ADC_Cmd(ADC1, DISABLE);
	systick_delay_us(100);
	Board_ADC_Init();

	/* 恢复TIM1 CCR4值 */
	TIM1->CCR4=t_ccr4_temp;

	/* 标记校准完成 */
	motor_control_list.u8Calibration_OK_flag=1;
}