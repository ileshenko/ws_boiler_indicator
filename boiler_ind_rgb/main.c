/*
 * Boiler RGB module;
 *
 * main.c
 */
#include <msp430g2553.h>
#include "config.h"
#include <common.h>
#include <thermistor.h>
#include <leds.h>
#include <uart.h>
#include <string.h>

#define L(port, pin) (1<<((port-1)*8 + pin))
const int rgbs[LEDS_CNT][3] = {
		{L(2,5), L(2,4), L(2,3)},
		{L(2,2), L(1,7), L(1,6)},
		{L(1,5), L(2,0), L(2,1)},
		{L(1,2), L(1,3), L(1,4)},
		{L(2,7), L(2,6), L(1,0)},
};

typedef enum {
	CLR_R = 0,
	CLR_G = 1,
	CLR_B = 2,
	CLR_OFF = 3
} color_t;

enum {
	P1IDX = 0,
	P2IDX = 1
};

typedef union {
	int word;
	char ports[2];
} port_map_t;

static port_map_t rgb_ring[10];
static char rgb_program[LEDS_CNT][3]; /* color weights */
static char data[LEDS_CNT]; /* Packet received via UART */
static int rcv_pos;
static unsigned int timeout;

void timer_init(void)
{
	/* Timer A */
	/* Source SMCLK (1MHz) TASSEL_2
	 * divider 8 (125 kHz) ID_3
	 * Mode Up to CCR0 MC_1
	 * Up to CCR0
	 * Interrupt enable TAIE
	 */
    TA0CTL = TASSEL_2 | ID_3 | MC_1 | TAIE;
    TA0CCR0 = 125; /* result - 1 kHz*/

	/* Timer CLR_B */
	/* Source SMCLK (1MHz) TASSEL_2
	 * divider 8 (125 kHz) ID_3
	 * Mode Up to CCR0 MC_1
	 * Up to CCR0
	 * Interrupt enable TAIE
	 */
    TA1CTL = TASSEL_2 | ID_3 | MC_1 | TAIE;
    TA1CCR0 = 12500; /* result - 10 Hz*/
}

/* Transform rgb_program to rgb_ring */
static void leds_update(void)
{
	unsigned int j, ring_idx, led_idx;
	color_t clr;
	port_map_t accum[10];

	memset(accum, 0, sizeof(accum));

	for (led_idx = 0; led_idx < LEDS_CNT; led_idx++)
	{
		ring_idx = 0;
		for (clr = CLR_R; clr < CLR_OFF; clr ++)
		{
			j = rgb_program[led_idx][clr] & 0xf;
			while (j--)
				accum[ring_idx++].word |= rgbs[led_idx][clr];
		}
	}

	for (ring_idx = 0; ring_idx < 10; ring_idx++)
		rgb_ring[ring_idx].word = ~accum[ring_idx].word;
}

static void led_setup(int idx, unsigned char* weights)
{
	color_t color;

	for (color = CLR_R; color <= CLR_B; color++)
		rgb_program[idx][color] = weights[color];
}

static unsigned char t2b(char temp)
{
	if (temp <= 25)
		return 10;
	if (temp <= 35)
		return 35 - temp;
	return 0;
}

static unsigned char t2g(char temp)
{
	if (temp <= 25)
		return 0;
	if (temp <= 35)
		return temp - 25;
	if (temp <= 45)
		return 45 - temp;
	return 0;
}

static unsigned char t2r(char temp)
{
	if (temp <= 35)
		return 0;
	if (temp <= 45)
		return temp - 35;
	if (temp <= 55)
		return 10;
	if (temp <= 65)
		return (15 << 4) + 10;
	if (temp <= 75)
		return (7 << 4) + 10;
	if (temp <= 85)
		return (3 << 4) + 10;
	return (1 << 4) + 10;
}
static void led_temperature(int led_idx, char temp)
{
	unsigned char weights[3];

	weights[CLR_R] = t2r(temp);
	weights[CLR_G] = t2g(temp);
	weights[CLR_B] = t2b(temp);
	led_setup(led_idx, weights);
}

void rgb_init(void)
{
	P2DIR |= 0xff;			// OUT
	P2SEL &= ~0xff;			// GPIO mode
	P2SEL2 &= ~0xff;		// GPIO mode
	P2REN &= ~0xff;
	P2OUT &= ~0xff;			// Switch ON

	P1DIR |= 0xfd;			// OUT
	P1SEL &= ~0xfd;			// GPIO mode
	P1SEL2 &= ~0xfd;		// GPIO mode
	P1REN &= ~0xfd;
	P1OUT &= ~0xff;			// Switch ON
}

static unsigned char blinking[LEDS_CNT];

void main(void)
{
	int led;
	color_t clr;
	unsigned int sum, sum_old = 12345;
	unsigned char weights[3] = {0,0,0};

	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	default_state();
	clock_init(); /* DCO, MCLK and SMCLK - 1MHz */
	timer_init();
	uart_init();
	IE2 |= UCA0RXIE;

	rgb_init();

	rcv_pos = -1;
	clr = CLR_R;
	weights[clr] = 1;
	for (;;)
	{
		if(timeout < 2)
		{
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
		}
		if (timeout < 20)
		{
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
		}
		else
		{
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
		}

		_BIS_SR(LPM0_bits + GIE);
	}
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void rgb_timer(void)
{
	static int idx;

	switch(TA0IV)
	{
	case 10: /* TAIFG, timer overflow */
		break;
	case 2: /* TACCR1 CCIFG, compare 1 */
	case 4: /* TACCR2 CCIFG, compare 2 */
	default: /* impossible! */
		for (;;);
	}

	P2OUT = rgb_ring[idx].ports[1];
	P1OUT = rgb_ring[idx].ports[0];

	if (++idx >= 10)
		idx = 0;

//	_BIC_SR_IRQ(LPM0_bits);
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

	_BIC_SR_IRQ(LPM0_bits);
}


#pragma vector = USCIAB0RX_VECTOR
__interrupt void uscib0rx_isr (void)
{
	unsigned char byte;

	byte = UCA0RXBUF;
	if (rcv_pos == -1)
	{
		if (byte == MAGIC_BEGIN)
			rcv_pos++;
	}
	else if (rcv_pos == LEDS_CNT)
	{
		if (byte == MAGIC_END)
		{
			timeout = 0;
			rcv_pos = -1;
		}
	}
	else
		data[rcv_pos++] =  byte;
}
