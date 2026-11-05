from pathlib import Path

root = Path(__file__).resolve().parents[2]
p = root / 'tests/seven_led/test_seven_led.c'
s = p.read_text(encoding='utf-8')

def replace_function(signature, body):
    global s
    start = s.index(signature)
    brace = s.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    s = s[:start] + body + s[end:]

replace_function('static void test_matrix_and_sampling(void)', r'''static void test_matrix_and_sampling(void)
{
	unsigned led, slot;
	/* 新图实际二极管方向：LED1 L1->L2，LED2 L1->L3，LED3 L3->L1。 */
	for(led=1; led<=3; led++)
	{
		GPIOA->CRL=0x77777770;
		GPIOA->CRH=0x44444444;
		GPIOB->CRH=0x44444444;
		GPIOA->ODR=0x1800; /* SDA/SCL电平由驱动独占，扫描不得改写。 */
		GPIOB->ODR=0x200;  /* EN=PB9必须持续保持。 */
		led_init();
		CHECK((GPIOA->CRL & 0xfffffff0)==0x77777770);
		CHECK((GPIOA->ODR & 0x1800)==0x1800);
		CHECK(GPIOB->ODR & 0x200);
		user_list.user_state=TOOL_STOP;
		user_list.led_scan_start_flag=1;
		ledData.Word=1u<<led;
		for(slot=1; slot<=3; slot++)
		{
			GPIOA->IDR=(slot==2 ? 0 : 0x2001);
			LED_Scan();
			CHECK((GPIOA->ODR & 0x1800)==0x1800 && (GPIOB->ODR & 0x200));
			CHECK(user_list.led_input_valid);
			CHECK(user_list.Dir_Data==(slot==2 ? 0 : 1));
			CHECK(user_list.Key_Data==(slot==2 ? 0 : 1));
			CHECK(((GPIOA->CRH>>20)&0xf)==8); /* DIR为独立上拉输入。 */
			CHECK((GPIOA->CRL&0xf)==8); /* K1为独立上拉输入。 */
			if(slot==led)
			{
				CHECK(((GPIOA->CRH>>24)&0xf)==1); /* L1=PA14 */
				CHECK(((GPIOA->CRH>>8)&0xf)==(led==1 ? 1 : 0)); /* L2=PA10 */
				CHECK((GPIOB->CRH&0xf)==(led==1 ? 0 : 1)); /* L3=PB8 */
				CHECK(!!(GPIOA->ODR&0x4000)==(led==3 ? 0 : 1));
				if(led==1) CHECK(!(GPIOA->ODR&0x400));
				else CHECK(!!(GPIOB->ODR&0x100)==(led==3 ? 1 : 0));
			}
			else
			{
				CHECK(((GPIOA->CRH>>24)&0xf)==0);
				CHECK(((GPIOA->CRH>>8)&0xf)==0 && (GPIOB->CRH&0xf)==0);
			}
		}
	}
	user_list.user_state=TOOL_POWER_DOWN;
	ledData.Word=0xfe;
	LED_Scan();
	CHECK(((GPIOA->CRH>>24)&0xf)==0 && ((GPIOA->CRH>>8)&0xf)==0 && (GPIOB->CRH&0xf)==0);
	CHECK((GPIOA->ODR&0x1800)==0x1800 && (GPIOB->ODR&0x200));
	user_list.led_scan_start_flag=0;
	LED_Scan();
	CHECK(!user_list.led_input_valid);
}''')
replace_function('static void test_swd_startup_not_remapped_early(void)', r'''static void test_swd_startup_not_remapped_early(void)
{
	uint32_t mode_a, mode_b;
	memset(&user_list, 0, sizeof(user_list));
	GPIOA->AFRH=0xf00fffff;
	GPIOA->CRH=0x44444444;
	GPIOB->CRH=0x44444444;
	Bsp_Gpio_Init(); /* 校准前不接管PA13/14，保留原有烧录连接窗口。 */
	CHECK((GPIOA->AFRH & 0x0ff00000)==0);
	CHECK(((GPIOA->CRH>>20)&0xff)==0x44);
	mode_a=GPIOA->CRH;
	mode_b=GPIOB->CRH;
	LED_Scan();
	CHECK(GPIOA->CRH==mode_a && GPIOB->CRH==mode_b);
	led_init();
	CHECK((GPIOA->AFRH & 0x0ff00000)==0x01100000);
	CHECK(((GPIOA->CRH>>20)&0xff)==0x08); /* PA13上拉、PA14模拟高阻。 */
	CHECK((GPIOB->CRH&0xf)==0);
}''')
p.write_text(s, encoding='utf-8')
