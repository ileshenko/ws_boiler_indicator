/*
 * rgb.c
 *
 *  Created on: Apr 27, 2013
 *      Author: Igor
 */
#include <msp430g2553.h>
#include "rgb.h"
#include <common.h>
#include <string.h>

extern unsigned int ms_cnt;

#define L(port, pin) (1<<((port-1)*8 + pin))
const int rgbs[LEDS_CNT][3] = {
		{L(2,7), L(2,6), L(1,0)},
		{L(1,2), L(1,3), L(1,4)},
		{L(1,5), L(2,0), L(2,1)},
		{L(2,2), L(1,7), L(1,6)},
		{L(2,5), L(2,4), L(2,3)},
};

typedef enum {
	CLR_R = 0,
	CLR_G = 1,
	CLR_B = 2,
	CLR_OFF = 3
} color_t;

typedef union {
	int word;
	char ports[2];
} port_map_t;

static port_map_t rgb_ring[10];
static char rgb_program[LEDS_CNT][3]; /* color weights */
static unsigned char prev_temp[LEDS_CNT] = {99, 10, 99, 30, 40};
static char is_synced;
static unsigned char heat_lvl;
static unsigned char blinking[LEDS_CNT];

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
	P1OUT &= ~0xfd;			// Switch ON
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
			/* All weights together should never exceed 10 */
			while (j--)
				accum[ring_idx++].word |= rgbs[led_idx][clr];
		}
	}

	for (ring_idx = 0; ring_idx < 10; ring_idx++)
		rgb_ring[ring_idx].word = ~accum[ring_idx].word;
}

static int is_different(unsigned char temp1, unsigned char temp2)
{
	unsigned char threshhold = 55;

	if (temp1 < 25 && temp2 < 25)
		return 0;
	if (temp1 < 45 || temp2 < 45)
		return temp1 != temp2;
	while (threshhold < 95)
	{
		if (temp1 <= threshhold || temp2 <= threshhold)
			return temp1 > threshhold || temp2 > threshhold;
		threshhold += 10;
	}
	return 0;
}

static is_same_temp(unsigned char *temps)
{
	unsigned int i;

	for (i = 0; i < LEDS_CNT; i++)
	{
		if (is_different(temps[i], prev_temp[i]))
			return 0;
	}
	return 1;
}

static void save_temps(const unsigned char *temps)
{
	unsigned int i;

	for (i = 0; i < LEDS_CNT; i++)
		prev_temp[i] = temps[i];
}

static show_saved(void)
{
	unsigned int led;

	for (led = 0; led < LEDS_CNT; led++)
	{
		led_temperature(led, prev_temp[led]);
		blinking[led] = 1;
	}
	leds_update();

}
void rgb_temp_update(unsigned char *temps)
{
	if (is_same_temp(temps))
		return;

	save_temps(temps);
	show_saved();
}

static void rgb_blinking(void)
{
	int need_update = 0;
	int led;
	unsigned char delta;

	for (led = 0; led < LEDS_CNT; led++)
	{
		delta = rgb_program[led][CLR_R] & 0xf0;
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

static void rgb_nosync(void)
{
	static int led;
	static color_t clr = CLR_R;
	static unsigned char weights[3] = {1,0,0};
	static const unsigned char zero_temps[LEDS_CNT] = {0};

	/* Reset saved temperatures. Otherwise it can be problem after sync
	 * restoring with same values  */
	save_temps(zero_temps);

	weights[clr] <<= 1;
	if (weights[clr] > 10)
	{
		if (++led >= LEDS_CNT)
		{
			led = 0;
			if (++clr > CLR_B)
				clr = CLR_R;
		}
		weights[clr] = 1;
	}

	led_setup(led, weights);
	leds_update();
}

static void rgb_heating(unsigned char stage)
{
	if (heat_lvl < stage)
		return;

	if (stage == 0)
		memset(rgb_program, 0, sizeof(rgb_program));
	else
	{
		/* It may be updated by new packet */
		rgb_program[5-stage][CLR_G] = 0;
		rgb_program[5-stage][CLR_B] = 0;

		rgb_program[5-stage][CLR_R] = 10;
	}
	leds_update();
}

void rgb_heat_update(unsigned char val)
{
	if (val > 5)
		val = 5;

	if (heat_lvl && !val)
		show_saved();

	heat_lvl = val;
}

void rgb_sync_update(char is_sync_ok)
{
	is_synced = is_sync_ok;

}

void rgb_do_10hz(void)
{
	static unsigned char heat_show_stage;
	unsigned char m;

	if (!is_synced)
		return rgb_nosync();

	if (!heat_lvl)
		return rgb_blinking();

	heat_show_stage++;
	m = heat_show_stage & 0x7;
	/* 8 ticks for heating, 8 ticks for temperature displaying */
	if (heat_show_stage & 0x8)
		return rgb_heating(m);

	/* If we here - this mean we want to show during 8 ticks saved temperature */
	if (!m)
		show_saved();

	rgb_blinking();
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

	ms_cnt++;
//	_BIC_SR_IRQ(LPM0_bits);
}
