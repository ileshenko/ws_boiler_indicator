/*
 * Boiler RGB module;
 *
 * main.c
 */
#include <msp430g2553.h>
#include "config_lib.h"
//#include "config.h"
#include <common.h>
#include <msplib_common.h>
#include <x10.h>
#include "rgb.h"

static unsigned char packet[LEDS_CNT+2]; /* Packet received via UART */

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
	unsigned char *iter = packet;
	int i = LEDS_CNT+1;
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

	if (idx >= LEDS_CNT+2) /* packet includes CRC byte */
	{
		is_ready = 1;
		idx = -1;
	}
}

void main(void)
{
	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	default_state();
	clock_init(); /* DCO, MCLK and SMCLK - 1MHz */
	timer_init();
	x10rx_init(x10rx_cb);
	rgb_init();

	is_ready = 0;
	for (;;)
	{
		if (is_ready && is_packet_correct())
		{
			rgb_temp_update(packet);
			rgb_heat_update((packet[LEDS_CNT]+9)/10);
			timeout = 0;
		}

		/* Sync is OK, if delay is less than 10 sec. */
		rgb_sync_update(timeout < 100);

		rgb_do_10hz();

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

