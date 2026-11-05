#include "tm1650.h"
#include "systick.h"

/* 仅替代物理时间；驱动、GPIO/RCC HAL 都使用真实 ARM 代码。 */
volatile uint32_t test_delay_us;
const uint32_t test_sda_pin = TM1650_SDA_PIN;
const uint32_t test_scl_pin = TM1650_SCL_PIN;

void systick_delay_us(uint32_t us)
{
	test_delay_us += us;
}
