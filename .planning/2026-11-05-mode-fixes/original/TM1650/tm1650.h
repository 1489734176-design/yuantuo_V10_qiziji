/** Define to prevent recursive inclusion */
#ifndef __TM1650_H
#define __TM1650_H

/** Files includes */
#include <stdint.h>
#include "mm32_device.h"
#include "hal_conf.h"

/*-------------------------------------------------------------------------------------
    引脚定义
-------------------------------------------------------------------------------------*/
#define TM1650_GPIO_CLK             (RCC_AHBENR_GPIOA_Msk)

#define TM1650_SCL_PORT             GPIOA
// 新板原理图：SCL=PA12，SDA=PA11，SDA 使用外部 4.7K 上拉。
#define TM1650_SCL_PIN              GPIO_Pin_12
#define TM1650_SCL_PIN_SRC          GPIO_PinSource12

#define TM1650_SDA_PORT             GPIOA
#define TM1650_SDA_PIN              GPIO_Pin_11
#define TM1650_SDA_PIN_SRC          GPIO_PinSource11

#define TM1650_PIN_AF_GPIO          GPIO_AF_0

/*-------------------------------------------------------------------------------------
    时序参数
-------------------------------------------------------------------------------------*/
#define TM1650_HALF_CYCLE_US        (3)     // 时基
#define TM1650_POR_WAIT_MS          (60)    // 复位时间

/*-------------------------------------------------------------------------------------
    指令
-------------------------------------------------------------------------------------*/
#define TM1650_CMD_SYS              (0x48)     
#define TM1650_CMD_READKEY          (0x49)     

// 显存地址
#define TM1650_ADDR_DIG1            (0x68)
#define TM1650_ADDR_DIG2            (0x6A)
#define TM1650_ADDR_DIG3            (0x6C)
#define TM1650_ADDR_DIG4            (0x6E)
#define TM1650_ADDR_BASE            TM1650_ADDR_DIG1

#define TM1650_DIG_NUM              (4)
#define TM1650_SEG_NUM              (8)

/*-------------------------------------------------------------------------------------
    0x48 指令的数据字

        B7      B6 B5 B4      B3        B2        B1     B0
        0       亮度          段模式     工作模式   0      开关
-------------------------------------------------------------------------------------*/
// B6~B4 亮度
#define TM1650_BRIGHT_8             (0x00)      
#define TM1650_BRIGHT_1             (0x10)
#define TM1650_BRIGHT_2             (0x20)
#define TM1650_BRIGHT_3             (0x30)
#define TM1650_BRIGHT_4             (0x40)
#define TM1650_BRIGHT_5             (0x50)
#define TM1650_BRIGHT_6             (0x60)
#define TM1650_BRIGHT_7             (0x70)

// B3 段模式
#define TM1650_SEG_MODE_8           (0x00)     
#define TM1650_SEG_MODE_7           (0x08)     

// B2 工作模式
#define TM1650_MODE_NORMAL          (0x00)
#define TM1650_MODE_STANDBY         (0x04)

// B0 开关
#define TM1650_DISP_OFF             (0x00)
#define TM1650_DISP_ON              (0x01)

// 常用组合
#define TM1650_CFG_ON               (TM1650_BRIGHT_8 | TM1650_SEG_MODE_8 | TM1650_MODE_NORMAL | TM1650_DISP_ON)
#define TM1650_CFG_OFF              (TM1650_BRIGHT_8 | TM1650_SEG_MODE_8 | TM1650_MODE_NORMAL | TM1650_DISP_OFF)

/*-------------------------------------------------------------------------------------
    接口
-------------------------------------------------------------------------------------*/
void    TM1650_Init(void);
uint8_t TM1650_SetDisplay(uint8_t cfg);
/* 前台读取原始键码：成功返回1，命令无ACK返回0且不改写输出。 */
uint8_t TM1650_ReadKey(uint8_t *key);

void    TM1650_SetBit(uint8_t dig, uint8_t bit, uint8_t on);
void    TM1650_SetDigit(uint8_t dig, uint8_t dat);
uint8_t TM1650_GetDigit(uint8_t dig);
void    TM1650_Clear(void);

void    TM1650_Flush(void);
/* 每次最多一个事务；返回1=ACK成功，0=失败或无待刷新内容。 */
uint8_t TM1650_FlushStep(void);
void    TM1650_Refresh(void);

#endif
