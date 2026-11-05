from pathlib import Path

root = Path(__file__).resolve().parents[2]

def replace_bytes(path, before, after):
    p = root / path
    b = p.read_bytes()
    old = before.encode('utf-8')
    if old not in b:
        raise ValueError(f'{path}: replacement missing: {before[:70]}')
    p.write_bytes(b.replace(old, after.encode('utf-8')))

def replace_function(path, signature, body):
    p = root / path
    b = p.read_bytes()
    start = b.index(signature.encode())
    brace = b.index(b'{', start)
    end = brace + 1
    depth = 1
    while depth:
        depth += (b[end] == 123) - (b[end] == 125)
        end += 1
    newline = '\r\n' if b'\r\n' in b else '\n'
    p.write_bytes(b[:start] + body.replace('\n', newline).encode('utf-8') + b[end:])

p = root / 'USER/inc/board.h'
b = p.read_bytes()
start = b.index(b'#define EN_PIN_RCC_CLOCKGPIO')
end = b.index(b'#define T_BAT_PORT', start)
new = '''/* 飞机板：PB9电源保持；PA13独立方向；PA0独立K1。
 * PA11/12由TM1650的SDA/SCL独占，方向/按键不再复用挡位灯。 */
#define EN_PIN_RCC_CLOCKGPIO       (RCC_AHBENR_GPIOB_Msk)
#define EN_PORT                    GPIOB
#define EN_PIN                     GPIO_Pin_9
#define EN_PIN_SRC                 GPIO_PinSource9
#define EN_PIN_AF                  GPIO_AF_0
#define DIR_RCC_CLOCKGPIO          (RCC_AHBENR_GPIOA_Msk)
#define DIR_PORT                   GPIOA
#define DIR_PIN                    GPIO_Pin_13
#define DIR_PIN_SRC                GPIO_PinSource13
#define DIR_PIN_AF                 GPIO_AF_1
#define KEY_RCC_CLOCKGPIO          (RCC_AHBENR_GPIOA_Msk)
#define KEY_PORT                   GPIOA
#define KEY_PIN                    GPIO_Pin_0
#define KEY_PIN_SRC                GPIO_PinSource0
#define KEY_PIN_AF                 GPIO_AF_0

'''
p.write_bytes(b[:start] + new.replace('\n','\r\n').encode('utf-8') + b[end:])
replace_bytes('USER/board.c', '配置PA11为推挽输出（电源保持EN），并置高使能', '飞机板PB9为推挽输出（电源保持EN），并置高使能')
replace_bytes('USER/board.c', '/* PA11 用作 EN，保持普通 GPIO 功能。 */', '/* 飞机板PB9用作EN，PA11/12留给TM1650总线。 */')
replace_bytes('USER/board.c', 'PB8为方向/LED行1复用输入，PB9由LED COM4驱动', '飞机板PA13为独立方向输入；校准后才解除SWDIO复用')
replace_bytes('USER/board.c', '//\tGPIO_PinAFConfig(GPIOA, GPIO_PinSource13, GPIO_AF_1);', '/* 校准结束后由led_init调用，保留应用启动前的烧录窗口。 */\r\n\tGPIO_PinAFConfig(DIR_PORT, DIR_PIN_SRC, DIR_PIN_AF);')
replace_bytes('USER/board.c', '/* DIR/KEY共享灯脚由校准后的led_init接管，先保留PA14的SWD启动窗口。 */', '/* PA13方向和PA14电量灯由校准后的led_init接管，先保留SWD启动窗口。 */')
replace_bytes('USER/inc/parameter.h', '#define LED_SCAN_EN       (1)  // 新板7灯与方向/按键分时复用', '#define LED_SCAN_EN       (1)  // 飞机板三IO电量灯扫描；TM1650挡位灯在前台刷新')
replace_bytes('USER/inc/parameter.h', '#define MOS_NTC_10K_3435K 0\r\n#define MOS_NTC_100K_3950K 1', '/* 飞机板NTC1=10K/B3435、R41=4.7K；保持95℃保护、85℃恢复。 */\r\n#define MOS_NTC_10K_3435K 1\r\n#define MOS_NTC_100K_3950K 0')

p = root / 'USER/inc/led.h'
b = p.read_bytes()
start = b.index('/* 新板'.encode('utf-8'))
end = b.index(b'typedef union', start)
new = '''/* 飞机板电量三IO：L1=PA14，L2=PA10，L3=PB8。
 * LED1:L1->L2，LED2:L1->L3，LED3:L3->L1；未使用的一端为模拟高阻。
 * LED4照明直接接VCC，由PB9整板断电关闭；PA12只用于TM1650 SCL。 */
#define LED_L1_PORT GPIOA
#define LED_L1_PIN  GPIO_Pin_14
#define LED_L2_PORT GPIOA
#define LED_L2_PIN  GPIO_Pin_10
#define LED_L3_PORT GPIOB
#define LED_L3_PIN  GPIO_Pin_8

'''
p.write_bytes(b[:start] + new.encode('utf-8') + b[end:])
replace_bytes('USER/inc/led.h', 'void UO_Led_Handler(void);', 'void UO_Led_Handler(void);\n/* 前台每次最多一项TM1650事务，避免占用电机中断。 */\nvoid led_tm1650_handle(void);')

p = root / 'USER/led.c'
b = p.read_bytes()
end = b.index(b'uint8_t LedError_Control(void)')
new = '''#include "board.h"
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

'''
p.write_bytes(new.replace('\n','\r\n').encode('utf-8') + b[end:])

replace_bytes('USER/main.c', '#include "led.h"', '#include "led.h"\r\n#include "tool_mode.h"\r\n#include "tm1650.h"')
replace_bytes('USER/main.c', '\tu8 i;', '\tu8 i;\r\n\tuint8_t tm_bus_used; // 前台每1ms最多一次TM1650读写事务')
replace_bytes('USER/main.c', '/* 电源保持端口初始化（EN = PA11） */', '/* 飞机板电源保持PB9，PA11/12专用于TM1650。 */')
replace_bytes('USER/main.c', '\tled_init();', '\tled_init();\r\n\t/* 总线初始化在电机中断开放之前，上电等待不影响运行保护。 */\r\n\tTM1650_Init();')
replace_bytes('USER/main.c', '/* 每次方向去抖之前等待新的复用输入采样。 */', '/* 每次方向去抖之前等待独立PA13方向输入的新采样。 */')
replace_bytes('USER/main.c', '\t\t\t/* 目标PWM占空比计算（扳机电压映射） */', '\t\t\t/* 先处理模式/挡位，读键与显示刷新分帧，长按不会连加。 */\r\n\t\t\ttm_bus_used=tool_mode_key_handle();\r\n\t\t\t/* 目标PWM占空比计算（扳机电压映射） */')
replace_bytes('USER/main.c', '\t\t\t/* 根据状态机设置电机使能标志 */', '\t\t\t/* 空闲帧刷新TM1650；每次仅一个配置或显存事务。 */\r\n\t\t\tif(!tm_bus_used) led_tm1650_handle();\r\n\t\t\t/* 根据状态机设置电机使能标志 */')
replace_bytes('USER/main.c', '\t\t\t/* 初始位置检测（IPD）触发', '\t\t\t/* 发布使能状态后，只有松扳机且电机完全停止才提交掉电设置。 */\r\n\t\t\ttool_mode_save_handle();\r\n\t\t\t/* 初始位置检测（IPD）触发')

p = root / 'KEIL_PRJ/MM32SPIN0230_Demo.uvprojx'
b = p.read_bytes()
b = b.replace(b'IROM(0x08000000,0x8000)', b'IROM(0x08000000,0x7800)')
b = b.replace(b'<Size>0x8000</Size>', b'<Size>0x7800</Size>')
b = b.replace(b'..\\MOTOR_CONTROL;..\\MC_CORE</IncludePath>', b'..\\MOTOR_CONTROL;..\\MC_CORE;..\\TM1650</IncludePath>')
pos = b.index(b'            <File>\r\n              <FileName>Bat_com.c')
new = '''            <!-- 飞机板：新增TM1650及双模式控制，末尾两页Flash专用掉电存储。 -->
            <File>
              <FileName>tool_mode.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\\USER\\tool_mode.c</FilePath>
            </File>
            <File>
              <FileName>tm1650.c</FileName>
              <FileType>1</FileType>
              <FilePath>..\\TM1650\\tm1650.c</FilePath>
            </File>
'''
b = b[:pos] + new.replace('\n','\r\n').encode('utf-8') + b[pos:]
p.write_bytes(b)
print('Applied new-board IO, LED/NTC, main-loop and Keil integration.')
