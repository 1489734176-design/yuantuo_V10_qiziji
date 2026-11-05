/**
 * @file     tm1650.c
 * @brief    TM1650 (HTM1650-TD) LED 驱动芯片底层驱动
 */

/** Define to prevent recursive inclusion */
#define _TM1650_C_

/** Files includes */
#include "tm1650.h"
#include "systick.h"

/*-------------------------------------------------------------------------------------
    电平操作
-------------------------------------------------------------------------------------*/
#define TM1650_SCL_HIGH()       (TM1650_SCL_PORT->BSRR = TM1650_SCL_PIN)
#define TM1650_SCL_LOW()        (TM1650_SCL_PORT->BRR  = TM1650_SCL_PIN)

// SDA 配置为开漏输出, 写 1 = 释放总线(靠外部 4.7K 上拉), 写 0 = 拉低
#define TM1650_SDA_HIGH()       (TM1650_SDA_PORT->BSRR = TM1650_SDA_PIN)
#define TM1650_SDA_LOW()        (TM1650_SDA_PORT->BRR  = TM1650_SDA_PIN)
#define TM1650_SDA_READ()       ((TM1650_SDA_PORT->IDR & TM1650_SDA_PIN) ? 1U : 0U)

// 时基
#define TM1650_DELAY()          systick_delay_us(TM1650_HALF_CYCLE_US)

/*-------------------------------------------------------------------------------------
    显存影子 + 脏标志
-------------------------------------------------------------------------------------*/
static uint8_t tm1650_shadow[TM1650_DIG_NUM];   // 当前要显示的内容
static uint8_t tm1650_dirty;                    // bit0~bit3 对应 DIG1~DIG4 待刷新
static uint8_t tm1650_step;                     // FlushStep 的轮询位置
static uint8_t tm1650_cfg_shadow;               // 最新系统配置，重试和强制刷新时保留
static uint8_t tm1650_cfg_dirty;                // 系统配置无 ACK 时保持待重试

/**=====================================================================================
 * @brief  毫秒延时, 仅上电复位等待使用
 * @param  n_ms 毫秒数
 * @return 无
 =======================================================================================*/
static void TM1650_Delay_ms(uint16_t n_ms)
{
    while (n_ms--)
    {
        systick_delay_us(1000);
    }
}

/**=====================================================================================
 * @brief  总线起始信号, CLK 为高时 DIO 由高变低
 * @param  无
 * @return 无
 =======================================================================================*/
static void TM1650_Start(void)
{
    TM1650_SDA_HIGH();
    TM1650_SCL_HIGH();
    TM1650_DELAY();

    TM1650_SDA_LOW();
    TM1650_DELAY();

    TM1650_SCL_LOW();
    TM1650_DELAY();
}

/**=====================================================================================
 * @brief  总线结束信号, CLK 为高时 DIO 由低变高
 * @param  无
 * @return 无
 =======================================================================================*/
static void TM1650_Stop(void)
{
    TM1650_SCL_LOW();
    TM1650_SDA_LOW();
    TM1650_DELAY();

    TM1650_SCL_HIGH();
    TM1650_DELAY();

    TM1650_SDA_HIGH();
    TM1650_DELAY();
}

/**=====================================================================================
 * @brief  发送一个字节, 高位先出, 并读回第 9 位 ACK
 * @param  dat 待发送的字节
 * @return 1 = 收到 ACK, 0 = 无应答
 =======================================================================================*/
static uint8_t TM1650_WriteByte(uint8_t dat)
{
    uint8_t i;
    uint8_t ack;

    for (i = 0; i < 8; i++)
    {
        // CLK 低电平期间改变 DIO
        TM1650_SCL_LOW();
        TM1650_DELAY();

        if (dat & 0x80)
        {
            TM1650_SDA_HIGH();
        }
        else
        {
            TM1650_SDA_LOW();
        }
        dat <<= 1;
        TM1650_DELAY();

        // 上升沿锁存, 高电平期间 DIO 保持不变
        TM1650_SCL_HIGH();
        TM1650_DELAY();
        TM1650_DELAY();
    }

    // 第 9 个时钟: 释放总线, 由芯片拉低 DIO 表示应答
    TM1650_SCL_LOW();
    TM1650_DELAY();

    TM1650_SDA_HIGH();
    TM1650_DELAY();

    // 按规格书在第 9 个上升沿之前读 ACK，芯片在该上升沿后释放 SDA。
    ack = (TM1650_SDA_READ() == 0U) ? 1U : 0U;
    TM1650_SCL_HIGH();
    TM1650_DELAY();
    TM1650_DELAY();

    TM1650_SCL_LOW();
    TM1650_DELAY();

    return ack;
}

/**=====================================================================================
 * @brief  一次完整的 16 位传输: 起始 + 命令(或地址) + 数据 + 结束
 * @param  cmd  命令字或显存地址
 * @param  dat  数据字节
 * @return 1 = 两个字节都收到 ACK, 0 = 有一个无应答
 =======================================================================================*/
static uint8_t TM1650_Transfer(uint8_t cmd, uint8_t dat)
{
    uint8_t ack;

    TM1650_Start();
    ack = TM1650_WriteByte(cmd);
    // 命令未应答时立即 STOP，下一次前台服务再重试整个事务。
    if (ack)
    {
        ack = TM1650_WriteByte(dat);
    }
    TM1650_Stop();

    return ack;
}

/**=====================================================================================
 * @brief  SCL / SDA 引脚初始化, SCL 推挽输出, SDA 开漏输出
 * @param  无
 * @return 无
 =======================================================================================*/
static void TM1650_Gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);

    RCC_AHBPeriphClockCmd(TM1650_GPIO_CLK, ENABLE);

    GPIO_PinAFConfig(TM1650_SCL_PORT, TM1650_SCL_PIN_SRC, TM1650_PIN_AF_GPIO);
    GPIO_PinAFConfig(TM1650_SDA_PORT, TM1650_SDA_PIN_SRC, TM1650_PIN_AF_GPIO);

    // 先把总线置为空闲(双高), 再切成输出
    TM1650_SCL_HIGH();
    TM1650_SDA_HIGH();

    GPIO_InitStructure.GPIO_Pin   = TM1650_SCL_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(TM1650_SCL_PORT, &GPIO_InitStructure);

    // SDA 开漏, 读回第 9 位 ACK
    GPIO_InitStructure.GPIO_Pin   = TM1650_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_Init(TM1650_SDA_PORT, &GPIO_InitStructure);

    TM1650_SCL_HIGH();
    TM1650_SDA_HIGH();
}

/**=====================================================================================
 * @brief  初始化，上电后先写显存再开显示
 * @param  无
 * @return 无
 * @note   依赖 systick_delay_us, 调用前必须先完成 Systick_Init
 =======================================================================================*/
void TM1650_Init(void)
{
    uint8_t i;

    TM1650_Gpio_Init();

    // 等芯片内部上电复位结束
#if (TM1650_POR_WAIT_MS > 0)
    TM1650_Delay_ms(TM1650_POR_WAIT_MS);
#endif

    // 上电写零必须收到 ACK 才清脏；芯片尚未就绪时由前台单步重试。
    tm1650_dirty = (uint8_t)((1U << TM1650_DIG_NUM) - 1U);
    tm1650_step  = 0;
    for (i = 0; i < TM1650_DIG_NUM; i++)
    {
        tm1650_shadow[i] = 0x00;
        if (TM1650_Transfer((uint8_t)(TM1650_ADDR_BASE + (i << 1)), 0x00))
        {
            tm1650_dirty &= (uint8_t)(~(1U << i));
        }
    }

    // 再开显示
    TM1650_SetDisplay(TM1650_CFG_ON);
}

/**=====================================================================================
 * @brief  写系统参数, 设置亮度 / 段模式 / 工作模式 / 开关屏
 * @param  cfg 由 TM1650_BRIGHT_x | TM1650_SEG_MODE_x | TM1650_MODE_x | TM1650_DISP_x 组合
 * @return 1 = 收到 ACK, 0 = 无应答
 =======================================================================================*/
uint8_t TM1650_SetDisplay(uint8_t cfg)
{
    uint8_t ack;

    // 最新配置覆盖旧请求；只有完整事务 ACK 才取消待重试标志。
    tm1650_cfg_shadow = cfg;
    tm1650_cfg_dirty = 1U;
    ack = TM1650_Transfer(TM1650_CMD_SYS, cfg);
    if (ack)
    {
        tm1650_cfg_dirty = 0U;
    }
    return ack;
}

/**=====================================================================================
 * @brief  前台读取键盘原始扫描码，高位先读，末字节 NACK 后 STOP
 * @param  key 成功时写入原始键码；无 ACK 或空指针时保持原值
 * @return 1 = 命令收到 ACK 且已读取，0 = 失败
 * @note   只能在前台调用；不关闭电机中断，业务去抖由上层处理
 =======================================================================================*/
uint8_t TM1650_ReadKey(uint8_t *key)
{
    uint8_t i;
    uint8_t dat = 0U;

    if (key == 0)
    {
        return 0U;
    }

    TM1650_Start();
    if (!TM1650_WriteByte(TM1650_CMD_READKEY))
    {
        TM1650_Stop();
        return 0U;
    }

    // SDA 保持开漏释放，芯片在 SCL 下降沿输出下一位数据。
    TM1650_SDA_HIGH();
    for (i = 0; i < 8U; i++)
    {
        TM1650_SCL_LOW();
        TM1650_DELAY();
        TM1650_SCL_HIGH();
        TM1650_DELAY();
        dat = (uint8_t)((dat << 1) | TM1650_SDA_READ());
        TM1650_DELAY();
        TM1650_SCL_LOW();
        TM1650_DELAY();
    }

    // 只读取一个键码，末字节第 9 位保持 SDA 高，发送 NACK 后结束。
    TM1650_SDA_HIGH();
    TM1650_DELAY();
    TM1650_SCL_HIGH();
    TM1650_DELAY();
    TM1650_SCL_LOW();
    TM1650_DELAY();
    TM1650_Stop();

    *key = dat;
    return 1U;
}

/**=====================================================================================
 * @brief  改写显存影子里的一个位, 只置脏标志, 不立即上总线
 * @param  dig 位地址序号 0~3, 对应 DIG1~DIG4
 * @param  bit 段序号 0~7, 对应 A B C D E F G DP
 * @param  on  1 = 点亮, 0 = 熄灭
 * @return 无
 =======================================================================================*/
void TM1650_SetBit(uint8_t dig, uint8_t bit, uint8_t on)
{
    uint8_t mask;
    uint8_t dat;

    if ((dig >= TM1650_DIG_NUM) || (bit >= TM1650_SEG_NUM))
    {
        return;
    }

    mask = (uint8_t)(1U << bit);
    dat  = tm1650_shadow[dig];

    if (on)
    {
        dat |= mask;
    }
    else
    {
        dat &= (uint8_t)(~mask);
    }

    if (dat != tm1650_shadow[dig])
    {
        tm1650_shadow[dig] = dat;
        tm1650_dirty |= (uint8_t)(1U << dig);
    }
}

/**=====================================================================================
 * @brief  整字节改写显存影子, 只置脏标志, 不立即上总线
 * @param  dig 位地址序号 0~3
 * @param  dat 8 个段的通断, bit0=A ... bit7=DP
 * @return 无
 =======================================================================================*/
void TM1650_SetDigit(uint8_t dig, uint8_t dat)
{
    if (dig >= TM1650_DIG_NUM)
    {
        return;
    }

    if (dat != tm1650_shadow[dig])
    {
        tm1650_shadow[dig] = dat;
        tm1650_dirty |= (uint8_t)(1U << dig);
    }
}

/**=====================================================================================
 * @brief  读回显存影子, 用于读改写
 * @param  dig 位地址序号 0~3
 * @return 该位当前的 8 个段状态, 越界返回 0
 =======================================================================================*/
uint8_t TM1650_GetDigit(uint8_t dig)
{
    if (dig >= TM1650_DIG_NUM)
    {
        return 0x00;
    }

    return tm1650_shadow[dig];
}

/**=====================================================================================
 * @brief  全部熄灭, 只改影子
 * @param  无
 * @return 无
 =======================================================================================*/
void TM1650_Clear(void)
{
    uint8_t i;

    for (i = 0; i < TM1650_DIG_NUM; i++)
    {
        TM1650_SetDigit(i, 0x00);
    }
}

/**=====================================================================================
 * @brief  把所有变化过的位一次性写进芯片
 * @param  无
 * @return 无
 * @note   最坏情况 4 个位全脏, 耗时约 1.5ms, 对时间敏感的场合用 TM1650_FlushStep
 =======================================================================================*/
void TM1650_Flush(void)
{
    uint8_t i;

    for (i = 0; i < TM1650_DIG_NUM; i++)
    {
        if (tm1650_dirty & (1U << i))
        {
            // 无 ACK 时保留该位，下一次 Flush/FlushStep 重新发送。
            if (TM1650_Transfer((uint8_t)(TM1650_ADDR_BASE + (i << 1)), tm1650_shadow[i]))
            {
                tm1650_dirty &= (uint8_t)(~(1U << i));
            }
        }
    }
    // 批量接口也处理此前失败的系统配置；运行中应使用单步接口。
    if (tm1650_cfg_dirty)
    {
        TM1650_SetDisplay(tm1650_cfg_shadow);
    }
}

/**=====================================================================================
 * @brief  每次最多只写一个位, 把刷新耗时摊开, 建议放在 1ms 任务里周期调用
 * @param  无
 * @return 1 = 本次事务 ACK 成功, 0 = 事务失败或无待刷新内容
 * @note   单次耗时约 380us, 芯片内置锁存器, 没有变化时总线上不会有任何波形
 =======================================================================================*/
uint8_t TM1650_FlushStep(void)
{
    uint8_t i;

    // 配置重试也是一个完整事务，本次不再发送显存以限制前台阻塞。
    if (tm1650_cfg_dirty)
    {
        return TM1650_SetDisplay(tm1650_cfg_shadow);
    }

    if (tm1650_dirty == 0x00)
    {
        return 0;
    }

    // 从上次的位置往下找, 保证 4 个位轮流得到刷新机会
    for (i = 0; i < TM1650_DIG_NUM; i++)
    {
        if (++tm1650_step >= TM1650_DIG_NUM)
        {
            tm1650_step = 0;
        }

        if (tm1650_dirty & (1U << tm1650_step))
        {
            // 失败仍然占用本次服务，保留脏标志，不能继续尝试第二个事务。
            if (TM1650_Transfer((uint8_t)(TM1650_ADDR_BASE + (tm1650_step << 1)), tm1650_shadow[tm1650_step]))
            {
                tm1650_dirty &= (uint8_t)(~(1U << tm1650_step));
                return 1U;
            }
            return 0U;
        }
    }

    return 0;
}

/**=====================================================================================
 * @brief  强制重写全部显存和系统参数, 用于定期纠正受干扰后的错误状态
 * @param  无
 * @return 无
 * @note   仅用于非运行时前台；运行中通过单步接口刷新，禁止在中断调用
 =======================================================================================*/
void TM1650_Refresh(void)
{
    // 重写最新影子及系统配置，避免刷新把休眠状态或亮度恢复成默认值。
    tm1650_dirty = (uint8_t)((1U << TM1650_DIG_NUM) - 1U);
    tm1650_cfg_dirty = 1U;
    TM1650_Flush();
}
