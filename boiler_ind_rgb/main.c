/*
 * Boiler RGB module;
 *
 * main.c
 */
#include <msp430g2553.h>
#include "config_lib.h"
//#include "config.h"
//#include <common.h>
#include <msplib_common.h>
#include <x10.h>

#include "rgb.h"
//#include <thermistor.h>
//#include <leds.h>
//#include <uart.h>
//#include <string.h>


static unsigned char packet[LEDS_CNT+1]; /* Packet received via UART */

//static signed char rcv_pos;
static char is_ready;

static unsigned int timeout;
static unsigned int jiffies;
unsigned int ms_cnt;

void timer_init(void)
{
	/* Timer A (RGB)*/
	/* Source SMCLK (1MHz) TASSEL_2
	 * divider 8 (125 kHz) ID_3
	 * Mode Up to CCR0 MC_1
	 * Up to CCR0
	 * Interrupt enable TAIE
	 */
    TA0CTL = TASSEL_2 | ID_3 | MC_1 | TAIE;
    TA0CCR0 = 125; /* result - 1 kHz*/
    ms_cnt = 0;

	/* Timer B (main clock)*/
	/* Source SMCLK (1MHz) TASSEL_2
	 * divider 8 (125 kHz) ID_3
	 * Mode Up to CCR0 MC_1
	 * Up to CCR0
	 * Interrupt enable TAIE
	 */
    TA1CTL = TASSEL_2 | ID_3 | MC_1 | TAIE;
    TA1CCR0 = 12500; /* result - 10 Hz */

    jiffies = 0;
}

static int is_packet_correct(void)
{
	unsigned char *iter= packet;
	int i = LEDS_CNT;
	char crc = MAGIC_BEGIN;

	is_ready = 0; /* reset flag */

	while (i--)
		crc += *iter++;

	return crc == *iter;
}

void x10rx_cb(unsigned char ch)
{
	static signed char idx;
	static unsigned int old_jiffies;

	/* delay more than 200 ms means, we begin to receive new packet */
	if (jiffies - old_jiffies  >= 2)
		idx = -1;

	old_jiffies = jiffies;

	if (idx == -1)
	{
		if (ch == MAGIC_BEGIN)
			idx++;
		return;
	}
	packet[idx++] = ch;

	if (idx >= LEDS_CNT+1) /* packet includes CRC byte */
	{
		is_ready = 1;
		idx = -1;
	}
}

void main(void)
{
//	int led;
//	color_t clr;
//	unsigned int sum, sum_old = 12345;
//	unsigned char weights[3] = {0,0,0};

	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	default_state();
	clock_init(); /* DCO, MCLK and SMCLK - 1MHz */
	timer_init();
	x10rx_init(x10rx_cb);

	rgb_init();

//	rcv_pos = -1;
	is_ready = 0;
//	clr = CLR_R;
//	weights[clr] = 1;
	for (;;)
	{
		if (is_ready && is_packet_correct())
		{
			rgb_update(packet);
			timeout = 0;
		}

		if (timeout < 100)
			rgb_blinking();
		else
			rgb_nosync();

#if 0
		if(timeout < 2)
		{
			rgb_update(temps);
#if 0
			sum = 0;
			for (led = 0; led < LEDS_CNT; led++)
				sum += data[led];

			if (sum != sum_old)
			{
				sum_old = sum;
				for (led = 0; led < LEDS_CNT; led++)
				{
//					led_temperature(led, 74 + led * 3);
					led_temperature(led, data[led]);
					blinking[led] = 1;
				}
				leds_update();
			}
#endif
		}
		if (timeout < 20)
		{
			rgb_blinking();
#if 0
			int need_update = 0;

			for (led = 0; led < LEDS_CNT; led++)
			{
				unsigned char delta = rgb_program[led][CLR_R] & 0xf0;
				if (delta && !--blinking[led])
				{
					need_update = 1;
					delta >>= 4;
					rgb_program[led][CLR_R] ^= 0x8;
					blinking[led] = rgb_program[led][CLR_R] & 0x8 ? delta : 1;
				}
			}
			if (need_update)
				leds_update();
#endif
		}
		else
		{
			rgb_nosync();
#if 0
			if (led >= LEDS_CNT)
				led = 0;

			weights[clr] <<= 1;
			if (weights[clr] > 10)
			{
				if (++led >= LEDS_CNT)
				{
					led = 0;
					weights[clr] = 0;
					if (++clr > CLR_B)
						clr = CLR_R;
				}
				weights[clr] = 1;
			}

			led_setup(led, weights);
			leds_update();
#endif
		}
#endif
		_BIS_SR(LPM0_bits + GIE);
	}
}

#pragma vector=TIMER1_A1_VECTOR
__interrupt void main_timer(void)
{
	switch(TA1IV)
	{
	case 10: /* TAIFG, timer overflow */
		break;
	case 2: /* TACCR1 CCIFG, compare 1 */
	case 4: /* TACCR2 CCIFG, compare 2 */
	default: /* impossible! */
		for (;;);
	}

	timeout++;
	jiffies++;

	_BIC_SR_IRQ(LPM0_bits);
}

