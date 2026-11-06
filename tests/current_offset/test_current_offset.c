#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "main.h"
#include "board.h"
#include "parameter.h"
#include "user_control.h"
#include "motor_control.h"
#include "drv_adc.h"
#include "drv_pwm.h"
#include "systick.h"

User_list_Struct user_list;
Machine_ERROR_HOLD machine_error_hold;
Mc_core_list mc_core_list;
Motor_Control_list motor_control_list;
volatile uint32_t test_failure_line, test_case, test_checks;

#define CHECK(expr) do { test_checks++; if(!(expr)) { if(!test_failure_line) test_failure_line=__LINE__; return; } } while(0)

static uint16_t channel_zero[16];
static uint16_t channel_conversions[16];
static uint8_t channel_sample_time[16];
static uint8_t sample_time_written[16];
static uint8_t selected_channel, rank_max, any_enabled, adc_enabled;
static uint8_t external_trigger, tim_update_enabled, conversion_pending;
static uint8_t output_stopped, adc_eoc_interrupt;
static uint32_t elapsed_us, first_avg_conversion_us, restore_count;
static uint32_t stop_count, conversion_faults;
static uint8_t avg_fault_kind, avg_fault_index;
static uint16_t current_poll_count, longest_avg_poll_count;

#define AVG_FAULT_TIMEOUT (1u)
#define AVG_FAULT_ZERO (2u)
#define AVG_FAULT_FULL_SCALE (3u)

/** @brief 模拟关闭六路功率输出，只记录输出状态，避免替代被测零偏计算。 */
void Stop_Motor(void)
{
	output_stopped=1;
	stop_count++;
}

/** @brief 模拟微秒延时并累计停机后的实际等待时间，不在宿主机等待。 */
void systick_delay_us(u32 n_us)
{
	elapsed_us+=n_us;
}

/** @brief 模拟TIM更新中断开关，检查校准采样时中断已关闭。 */
void TIM_ITConfig(TIM_TypeDef *tim,uint32_t it,FunctionalState state)
{
	CHECK(tim==TIM1 && it==TIM_IT_Update);
	tim_update_enabled=(uint8_t)state;
}

/** @brief 模拟ADC开关，保留真实校准代码的通道切换顺序。 */
void ADC_Cmd(ADC_TypeDef *adc,FunctionalState state)
{
	CHECK(adc==ADC1);
	adc_enabled=(uint8_t)state;
	if(state==DISABLE) conversion_pending=0;
}

/** @brief 模拟外部触发开关，软件转换时必须保持关闭。 */
void ADC_ExternalTrigConvCmd(ADC_TypeDef *adc,FunctionalState state)
{
	CHECK(adc==ADC1);
	external_trigger=(uint8_t)state;
}

/** @brief 模拟ADC中断开关；生产校准沿用原EOC关闭逻辑。 */
void ADC_ITConfig(ADC_TypeDef *adc,uint32_t it,FunctionalState state)
{
	CHECK(adc==ADC1 && it==ADC_IT_EOC);
	adc_eoc_interrupt=(uint8_t)state;
}

/** @brief 模拟顺序采样通道数，单通道校准的最大rank必须为0。 */
void ADC_AnyChannelNumCfg(ADC_TypeDef *adc,uint8_t num)
{
	CHECK(adc==ADC1);
	rank_max=num;
}

/** @brief 模拟ADC通道选择，转换数据只写入实际选中的通道寄存器。 */
void ADC_AnyChannelSelect(ADC_TypeDef *adc,uint8_t rank,uint8_t channel)
{
	CHECK(adc==ADC1 && rank==0 && channel<16);
	selected_channel=channel;
}

/** @brief 模拟ANY通道开关，用于检查校准转换条件。 */
void ADC_AnyChannelCmd(ADC_TypeDef *adc,FunctionalState state)
{
	CHECK(adc==ADC1);
	any_enabled=(uint8_t)state;
}

/** @brief 记录真实ADC通道采样时间，避免将rank0误当作CH4。 */
void ADC_SampleTimeConfig(ADC_TypeDef *adc,uint8_t channel,uint8_t sample_time)
{
	CHECK(adc==ADC1 && channel<16);
	channel_sample_time[channel]=sample_time;
	sample_time_written[channel]=1;
}

/** @brief 模拟一次硬件转换；每路前两次故意给出大瞬态，其后16次围绕独立零点变化。 */
void ADC_SoftwareStartConvCmd(ADC_TypeDef *adc,FunctionalState state)
{
	uint16_t conversion;
	uint16_t value;
	CHECK(adc==ADC1 && state==ENABLE);
	if(!output_stopped || tim_update_enabled || external_trigger ||
	   !adc_enabled || !any_enabled || rank_max!=0 || conversion_pending ||
	   (selected_channel!=OP_O_CHANNEL && selected_channel!=ISUMAVG_CHANNEL))
	{
		conversion_faults++;
	}
	conversion=channel_conversions[selected_channel]++;
	if(selected_channel==ISUMAVG_CHANNEL && conversion==0)
	{
		first_avg_conversion_us=elapsed_us;
		/* CH5原保护零偏必须先完成，再切换CH4独立测量。 */
		if(channel_conversions[OP_O_CHANNEL]!=18) conversion_faults++;
	}
	if(conversion==0) value=2000;
	else if(conversion==1) value=1000;
	else value=(uint16_t)(channel_zero[selected_channel]+((conversion&1u) ? 1 : -1));
	if(selected_channel==ISUMAVG_CHANNEL && conversion==avg_fault_index)
	{
		if(avg_fault_kind==AVG_FAULT_ZERO) value=0;
		if(avg_fault_kind==AVG_FAULT_FULL_SCALE) value=4095;
	}
	*(&(ADC1->ADDR0)+selected_channel)=value;
	conversion_pending=1;
	current_poll_count=0;
}

/** @brief 模拟转换结束状态；故意不提供独立电流返回值，生产代码须读正确ADDR寄存器。 */
FlagStatus ADC_GetFlagStatus(ADC_TypeDef *adc,uint32_t flag)
{
	(void)adc;
	(void)flag;
	current_poll_count++;
	if(selected_channel==ISUMAVG_CHANNEL && current_poll_count>longest_avg_poll_count)
		longest_avg_poll_count=current_poll_count;
	if(selected_channel==ISUMAVG_CHANNEL && avg_fault_kind==AVG_FAULT_TIMEOUT &&
	   channel_conversions[ISUMAVG_CHANNEL]==avg_fault_index+1u)
		return RESET;
	return conversion_pending ? SET : RESET;
}

/** @brief 清除模拟EOC标志，同时保留通道数据以符合硬件读取方式。 */
void ADC_ClearFlag(ADC_TypeDef *adc,uint32_t flag)
{
	CHECK(adc==ADC1 && flag==ADC_FLAG_EOC);
	conversion_pending=0;
}

/** @brief 模拟恢复系统9通道扫描和PWM外部触发，记录真实校准函数确实执行恢复。 */
void Board_ADC_Init(void)
{
	restore_count++;
	rank_max=8;
	any_enabled=1;
	adc_enabled=1;
	external_trigger=1;
}

/** @brief 建立上电静止夹具，给CH5和CH4不同零点，并预置过期结果以识别未更新。 */
static void calibration_fixture(uint16_t peak_zero,uint16_t average_zero,uint16_t ccr4)
{
	uint8_t channel;
	memset(&user_list,0,sizeof(user_list));
	memset(&machine_error_hold,0,sizeof(machine_error_hold));
	memset(&mc_core_list,0,sizeof(mc_core_list));
	memset(&motor_control_list,0,sizeof(motor_control_list));
	memset(channel_conversions,0,sizeof(channel_conversions));
	memset(sample_time_written,0,sizeof(sample_time_written));
	memset(channel_sample_time,0xff,sizeof(channel_sample_time));
	for(channel=0;channel<16;channel++)
	{
		channel_zero[channel]=777;
		*(&(ADC1->ADDR0)+channel)=3333;
	}
	channel_zero[OP_O_CHANNEL]=peak_zero;
	channel_zero[ISUMAVG_CHANNEL]=average_zero;
	user_list.IBusAvgOffset_Adc=1234;
	user_list.IBusAvgOffsetValid=0;
	user_list.IBusAvg_Adc=246;
	user_list.IBusPeak_Adc=357;
	user_list.Vcc_cal_gain=49152;
	mc_core_list.CurrentOffset_ad_data=2345;
	mc_core_list.Board_ADC_DATA.u16I_SUM_Avg=1357;
	mc_core_list.Board_ADC_DATA.u16I_Peak=2468;
	mc_core_list.MC_error.word=0x10;
	mc_core_list.block_count=0x12345678;
	motor_control_list.bldc_state=MOTOR_STOP;
	ADC1->ADSTA=0;
	TIM1->CCR4=ccr4;
	selected_channel=15;
	rank_max=8;
	any_enabled=1;
	adc_enabled=1;
	external_trigger=1;
	tim_update_enabled=1;
	conversion_pending=0;
	output_stopped=0;
	adc_eoc_interrupt=1;
	elapsed_us=0;
	first_avg_conversion_us=0;
	restore_count=0;
	stop_count=0;
	conversion_faults=0;
	avg_fault_kind=0;
	avg_fault_index=0;
	current_poll_count=0;
	longest_avg_poll_count=0;
}

/** @brief 执行真实双通道校准，检查零点归属、瞬态丢弃、静止等待及外设恢复。 */
static void test_dual_channel_calibration(uint16_t peak_zero,uint16_t average_zero,uint16_t ccr4)
{
	Mc_core_list original_core;
	calibration_fixture(peak_zero,average_zero,ccr4);
	original_core=mc_core_list;
	CurrentOffsetCalibration();
	if(test_failure_line) return;
	CHECK(mc_core_list.CurrentOffset_ad_data==peak_zero);
	CHECK(user_list.IBusAvgOffset_Adc==average_zero && user_list.IBusAvgOffsetValid==1);
	CHECK(channel_conversions[OP_O_CHANNEL]==18 && channel_conversions[ISUMAVG_CHANNEL]==18);
	CHECK(sample_time_written[ISUMAVG_CHANNEL] && channel_sample_time[ISUMAVG_CHANNEL]==ADC_SampleTime_3_5);
	CHECK(first_avg_conversion_us>=50000 && stop_count==1);
	CHECK(conversion_faults==0 && !conversion_pending);
	CHECK(restore_count==1 && external_trigger && adc_enabled && any_enabled && rank_max==8);
	CHECK(!tim_update_enabled && !adc_eoc_interrupt && TIM1->CCR4==ccr4);
	CHECK(motor_control_list.u8Calibration_OK_flag==1);
	/* 新校准不可篡改原保护值、VCC校准增益或MC库运行数据。 */
	CHECK(user_list.IBusAvg_Adc==246 && user_list.IBusPeak_Adc==357 && user_list.Vcc_cal_gain==49152);
	original_core.CurrentOffset_ad_data=peak_zero;
	CHECK(memcmp(&mc_core_list,&original_core,sizeof(mc_core_list))==0);
}

/** @brief 模拟CH4转换超时或有效样本顶格/归零，失败后仍恢复扫描且保留CH5保护零点。 */
static void test_average_calibration_failure(uint8_t fault,uint8_t sample)
{
	calibration_fixture(480,432,1111);
	avg_fault_kind=fault;
	avg_fault_index=sample;
	/* 过期有效标志也必须撤销，避免本次失败继续沿用不确定的零点。 */
	user_list.IBusAvgOffsetValid=1;
	CurrentOffsetCalibration();
	if(test_failure_line) return;
	CHECK(mc_core_list.CurrentOffset_ad_data==480 && channel_conversions[OP_O_CHANNEL]==18);
	CHECK(user_list.IBusAvgOffsetValid==0);
	CHECK(restore_count==1 && external_trigger && adc_enabled && any_enabled && rank_max==8);
	CHECK(TIM1->CCR4==1111 && !tim_update_enabled && !adc_eoc_interrupt);
	CHECK(motor_control_list.u8Calibration_OK_flag==1);
	CHECK(conversion_faults==0 && !conversion_pending && first_avg_conversion_us>=50000);
	CHECK(user_list.IBusAvg_Adc==246 && user_list.IBusPeak_Adc==357 && user_list.Vcc_cal_gain==49152);
	if(fault==AVG_FAULT_TIMEOUT)
	{
		/* 不允许卡死在转换等待；至多100次轮询，且失败后不继续采后续样本。 */
		CHECK(longest_avg_poll_count<=100 && longest_avg_poll_count>1);
		CHECK(channel_conversions[ISUMAVG_CHANNEL]==sample+1u && elapsed_us<60000);
	}
}

/** @brief 丢弃阶段的异常值不能污染后续16次正常样本的独立零点。 */
static void test_discarded_rail(uint8_t fault,uint8_t sample)
{
	calibration_fixture(480,432,217);
	avg_fault_kind=fault;
	avg_fault_index=sample;
	CurrentOffsetCalibration();
	if(test_failure_line) return;
	CHECK(user_list.IBusAvgOffsetValid==1 && user_list.IBusAvgOffset_Adc==432);
	CHECK(channel_conversions[ISUMAVG_CHANNEL]==18 && restore_count==1);
	CHECK(conversion_faults==0 && TIM1->CCR4==217);
}

/** @brief 运行真实CurrentOffsetCalibration回归；运行/Flash恢复与PWM曲线由主模式回归覆盖。 */
void run_tests(void)
{
	test_case=1;
	test_dual_channel_calibration(480,432,217);
	if(test_failure_line) return;
	test_case=2;
	test_dual_channel_calibration(420,650,1590);
	if(test_failure_line) return;
	test_case=3;
	/* MC_CORE二进制接口的既有布局，来自当前库头文件和链接映射。 */
	CHECK(sizeof(ADC_Value_TypeDef)==26 && sizeof(Mc_core_list)==116);
	CHECK(offsetof(Mc_core_list,CurrentOffset_ad_data)==96);
	CHECK(offsetof(Mc_core_list,pwm_actual_runing_data)==52 && offsetof(Mc_core_list,motor_en)==65);
	test_case=4; test_average_calibration_failure(AVG_FAULT_TIMEOUT,0);
	if(test_failure_line) return;
	test_case=5; test_average_calibration_failure(AVG_FAULT_TIMEOUT,9);
	if(test_failure_line) return;
	test_case=6; test_average_calibration_failure(AVG_FAULT_ZERO,7);
	if(test_failure_line) return;
	test_case=7; test_average_calibration_failure(AVG_FAULT_FULL_SCALE,12);
	if(test_failure_line) return;
	test_case=8; test_discarded_rail(AVG_FAULT_ZERO,0);
	if(test_failure_line) return;
	test_case=9; test_discarded_rail(AVG_FAULT_FULL_SCALE,1);
}
