#include "board.h"
#include "led.h"
#include "user_control.h"
#include "parameter.h"
#include "tool_mode.h"
#include "tm1650.h"

User_LED_Control_t user_led_control;
volatile BITS_UNION ledData;
static uint8_t led_scan_index;
static uint8_t tm_display_state=0xff;

/* 切灯先让三端模拟高阻，再设置电平并使能选中的阳/阴极，防止串亮。
 * 只修改L1/L2/L3，不能触碰PB9电源保持、PA11/12总线和PA13方向。 */
static void led_matrix_release(void)
{
	MODIFY_REG(GPIOA->CRH, GPIO_CRH_MODE14 | GPIO_CRH_CNF14 | GPIO_CRH_MODE10 | GPIO_CRH_CNF10, 0);
	MODIFY_REG(GPIOB->CRH, GPIO_CRH_MODE8 | GPIO_CRH_CNF8, 0);
}

void led_init(void)
{
	RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOA_Msk | RCC_AHBENR_GPIOB_Msk, ENABLE);
	/* 校准后接管PA13/14；方向独立上拉输入，PA14灯端先保持高阻。 */
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource14, GPIO_AF_1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_0);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_0);
	led_matrix_release();
	Bsp_dir_init();
	Bsp_Key_Init();
	ledData.Word=0;
	led_scan_index=0;
	tm_display_state=0xff;
	user_list.led_input_valid=0;
	user_list.Key_Data=1;
	user_list.Dir_Data=1;
}

/* 16kHz中断仅扫描电量灯并缓存独立DIR/K1，TM1650通信全部留在前台。
 * 三灯各一槽，整轮187.5us；采样与点灯不再互相复用。 */
void LED_Scan(void)
{
	if(!user_list.led_scan_start_flag)
	{
		led_scan_index=0;
		user_list.led_input_valid=0;
		return;
	}
	user_list.Dir_Data=GPIO_ReadInputDataBit(DIR_PORT, DIR_PIN);
	user_list.Key_Data=GPIO_ReadInputDataBit(KEY_PORT, KEY_PIN);
	user_list.led_input_valid=1;
	if(++led_scan_index>3) led_scan_index=1;
	led_matrix_release();
	if(user_list.user_state==TOOL_POWER_DOWN || !(ledData.Word & (1u<<led_scan_index))) return;
	if(led_scan_index==1)
	{
		GPIO_SetBits(GPIOA, LED_L1_PIN);
		GPIO_ResetBits(GPIOA, LED_L2_PIN);
		MODIFY_REG(GPIOA->CRH, GPIO_CRH_MODE14 | GPIO_CRH_MODE10,
			(1u<<GPIO_CRH_MODE14_Pos) | (1u<<GPIO_CRH_MODE10_Pos));
	}
	else
	{
		if(led_scan_index==2)
		{
			GPIO_SetBits(GPIOA, LED_L1_PIN);
			GPIO_ResetBits(GPIOB, LED_L3_PIN);
		}
		else
		{
			GPIO_ResetBits(GPIOA, LED_L1_PIN);
			GPIO_SetBits(GPIOB, LED_L3_PIN);
		}
		MODIFY_REG(GPIOA->CRH, GPIO_CRH_MODE14, 1u<<GPIO_CRH_MODE14_Pos);
		MODIFY_REG(GPIOB->CRH, GPIO_CRH_MODE8, 1u<<GPIO_CRH_MODE8_Pos);
	}
}

void UO_Led_Handler(void)
{
	uint8_t mask;
	if(user_list.user_state==TOOL_POWER_DOWN)
	{
		/* LED4照明随整板EN断电，挡位显存和三IO电量灯同时清零。 */
		ledData.Word=0;
		user_led_control.LedRunTimes=0;
		user_led_control.LedDeal=0;
		TM1650_Clear();
		return;
	}
	if(LedError_Control() && user_list.flag.bits.gToolTrigger)
	{
		LedError_Run();
	}
	else
	{
		user_led_control.LedDeal=0;
		user_led_control.LedRunTimes=0;
		LedSoc_Run(user_list.VBus_Vx100, 1, 0);
	}
	/* LED5/6/7/8分别是D/E/F/G，DIG1红色、DIG2绿色，严格单灯显示。 */
	mask=(uint8_t)(1u<<(tool_mode_gear()+2u));
	TM1650_SetDigit(0, tool_mode_list.mode==TOOL_MODE_SCENE ? mask : 0);
	TM1650_SetDigit(1, tool_mode_list.mode==TOOL_MODE_SPEED ? mask : 0);
	TM1650_SetDigit(2, 0);
	TM1650_SetDigit(3, 0);
}

void led_tm1650_handle(void)
{
	uint8_t cfg=user_list.user_state==TOOL_POWER_DOWN ? TM1650_CFG_OFF : TM1650_CFG_ON;
	/* 开关设置和显存分帧；失败保持请求，下一可用前台帧重试。 */
	if(tm_display_state!=cfg)
	{
		if(TM1650_SetDisplay(cfg)) tm_display_state=cfg;
	}
	else
	{
		TM1650_FlushStep();
	}
}

uint8_t LedError_Control(void)
{
#if RESTAR_EN
	if(machine_error_hold.userErr_hold.word==0 &&
	   (machine_error_hold.MC_error_hold.word & 0xfffb)==0 &&
	   (!machine_error_hold.MC_error_hold.bits.gBlock || user_list.Load_exist<RESTAR_COUNT))
#else
	if(machine_error_hold.userErr_hold.word==0 && machine_error_hold.MC_error_hold.word==0)
#endif
	{
		user_led_control.LedRunCnt_Once=0;
		user_led_control.LedDeal=0;
		user_led_control.LedRunTimes=0;
		return 0;
	}
	if(!user_led_control.LedDeal && !user_led_control.LedRunTimes)
	{
		user_led_control.LedDeal=1;
		user_led_control.LedRunTimes=10;
		user_led_control.LedRunMode=1;
		if(machine_error_hold.userErr_hold.bits.gIBusAcmpOCP)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_HW_OCP;
		}
		else if(machine_error_hold.userErr_hold.bits.gNtcBat)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_BAT_TEMP;
		}
		else if(machine_error_hold.userErr_hold.bits.gNtcMos || machine_error_hold.userErr_hold.bits.gMos_overload)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_MOS_TEMP;
		}
		else if(machine_error_hold.MC_error_hold.bits.gBlock)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_BLOCK;
		}
		else if(machine_error_hold.userErr_hold.bits.gIBusPeakOCP1 || machine_error_hold.userErr_hold.bits.gIBusAvgOCP1 ||
		        machine_error_hold.userErr_hold.bits.gIBusAvgOCP2 || machine_error_hold.userErr_hold.bits.gIBusAvgOCP3)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_SW_OCP;
		}
		else if(machine_error_hold.userErr_hold.bits.gVBusStbUVP || machine_error_hold.userErr_hold.bits.gVBusRunUVP1 ||
		        machine_error_hold.userErr_hold.bits.gVBusRunUVP2)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_UVP;
		}
		else if(machine_error_hold.userErr_hold.bits.fly_machine)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_FLY_MACHINE;
		}
		else if(machine_error_hold.userErr_hold.bits.gVBusOVP)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_OVP;
		}
		else if(machine_error_hold.userErr_hold.bits.gMosBridgeH || machine_error_hold.userErr_hold.bits.gMosBridgeL)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_MOS_BRIDGE;
		}
		else if(machine_error_hold.MC_error_hold.bits.gLackPhase)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_LACK_PHASE;
		}
		else if(machine_error_hold.MC_error_hold.bits.gLosStep)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_LOSE_STEP;
		}
		else if(machine_error_hold.userErr_hold.bits.gBatComFault)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_BAT_COM;
		}
		else if(machine_error_hold.userErr_hold.bits.gIBusAvgOCP4)
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_IBUS_OCP4;
		}
		else
		{
			user_led_control.LedRunCnt_Once=LED_FAULT_OTHER;
		}
	}
	return (user_led_control.LedRunCnt_Once!=0);
}

void LedError_Run(void)
{
    static uint16_t bzCnt  = 0;     /* 当前剩余闪烁段数 */
    static uint16_t bzTime = 0;     /* 段内计时 */

    if(user_led_control.LedDeal)
    {
        LED_ERROR_OFF();

        bzCnt  = user_led_control.LedRunCnt_Once;
        bzTime = 0;

        user_led_control.LedDeal = 0;
    }

    if(!user_led_control.LedRunTimes)
    {
        LED_ERROR_OFF();

        bzCnt  = 0;
        bzTime = 0;
        return;
    }

    if(bzCnt)
    {
        bzTime++;

        if((bzTime >= 1) && (bzTime <= 500))        /* 灭 500ms */
        {
            LED_ERROR_OFF();
        }
        else if(bzTime < 700)                       /* 亮 200ms */
        {
            LED_ERROR_ON();
        }

        if(user_led_control.LedRunMode == 0)
        {
            if((bzTime >= 700) && (bzTime <= 900))
            {
                LED_ERROR_OFF();
            }
            else if((bzTime >= 900) && (bzTime <= 1100))
            {
                LED_ERROR_ON();
            }

            if(bzTime > 1100)
            {
                bzTime = 0;
                bzCnt--;
            }
        }
        else
        {
            if(bzTime > 700)
            {
                bzTime = 0;
                bzCnt--;
            }
        }
    }
    else
    {
        LED_ERROR_OFF();

        if(bzTime++ >= 1500)                        /* 轮间隔 1500ms */
        {
            bzTime = 0;
            bzCnt  = user_led_control.LedRunCnt_Once;
            user_led_control.LedRunTimes--;
        }
    }
}

static const uint16_t deltaVolt=20; /* 0.2V滞回，沿用参考程序 */
void LedSoc_Run(uint32_t volt_vx100,uint8_t display_en,uint8_t fast_mode)
{
	static uint16_t ms500cnt = 0;    // 500ms时间基准计数器
	static uint16_t msUpdateCnt = 0; // SOC更新定时器
	static uint8_t PackSocStatus = 0;// 当前SOC等级（0~4）

	//10次滤波增加稳定性
	ms500cnt++;

	/* 扳机上电后可能直接进入运行，首次电量不能等待停机刷新。
	   用当前电压直接初始化等级；后续仍按参考的停机更新/滞回规则处理。 */
	if(PackSocStatus == 0)
	{
		if(volt_vx100 > SOC3_MV)
		{
			PackSocStatus = 4;
		}
		else if(volt_vx100 > SOC2_MV)
		{
			PackSocStatus = 3;
		}
		else if(volt_vx100 > SOC1_MV)
		{
			PackSocStatus = 2;
		}
		else
		{
			PackSocStatus = 1;
		}
	}

	// 需要更新SOC或快速刷新模式
	if(user_led_control.gSocUpdate||fast_mode)
	{
		// 滞回比较状态机，根据电压判断SOC等级
		switch(PackSocStatus)
		{
			case 4:

				if(volt_vx100< (SOC3_MV -deltaVolt ))  // 滞回比较（防止边界抖动）
				{
					PackSocStatus = 3;
				}

				break;

			case 3:
		        if(volt_vx100> (SOC3_MV  ))
		        {
		            PackSocStatus = 4;
		        }
		        if(volt_vx100< (SOC2_MV -deltaVolt ))
		        {
		            PackSocStatus = 2;
		        }
		        break;

			case 2:
		        if(volt_vx100> (SOC2_MV  ))
		        {
		            PackSocStatus = 3;
		        }
		        if(volt_vx100< (SOC1_MV -deltaVolt ))
		        {
		            PackSocStatus = 1;
		        }
		        break;

			case 1:
		        if(volt_vx100> (SOC1_MV  ))
		        {
		            PackSocStatus = 2;
		        }

		        break;
		}

		user_led_control.gSocUpdate = 0; // 清除更新标志

	}
	else
	{
		// 非运行状态下累计计时
		if(user_list.user_state!=TOOL_RUN)
		{
			msUpdateCnt++;
		}
		else
		{
			msUpdateCnt=0;
		}
		// 每20个周期触发一次SOC更新
		if(msUpdateCnt> 20)
		{
				user_led_control.gSocUpdate = 1;
		    msUpdateCnt =0;
		}
	}

	// 500ms基准计时归零
	if(ms500cnt >= 500)  //500ms
	{
		ms500cnt  = 0;
	}


	/*soc display*/
	// 显示未使能，关闭所有LED并返回
	if(display_en == 0)
	{
		LED3_OFF;
		LED2_OFF;
		LED1_OFF;

		return;
	}

	// 根据SOC等级控制LED显示
	switch(PackSocStatus)
	{
		case 4 :
		    LED1_ON;
		    LED2_ON;
			LED3_ON;
		break;

		case 3 :
		    LED1_ON;
		    LED2_ON;
			LED3_OFF;
		break;

		case 2 :
		    LED1_ON;
		    LED2_OFF;
			LED3_OFF;
		break;

		case 1 :
			if(ms500cnt == 250)
			{
				LED1_FLASH;
			}
			LED2_OFF;
			LED3_OFF;
		break;

		case 0 :
		        LED1_OFF;
		        LED2_OFF;
		        LED3_OFF;
		break;

		default :
		break;

	}
}
