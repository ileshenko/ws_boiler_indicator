/*
 * Boiler upper module;
 * LED = P2.7
 *
 * periodic send one byte data over UART:
 * period - once a second
 * DATA:
 *  0x0 <1 sec> [value] <1 sec>
 * main.c
 */
#include <msp430g2553.h>
#include "config_lib.h"
#include <common.h>
#include <msplib_common.h>
#include <thermistor.h>
#include <leds.h>
#include <x10.h>

unsigned long jiffies;

void timer_init(void)
{
	/* Timer A */
	/* Source SMCLK (1MHz) TASSEL_2
	 * divider 8 (125 kHz) ID_3
	 * Mode UP/Down MC_3
	 * Interrupt enable TAIE
	 */
    TA0CTL = TASSEL_2 | ID_3 | MC_3 | TAIE;
    TA0CCR0 = 0xF424; /* HZ = 1 125 kHz / 2 */

    jiffies = 0;
}

#if 0
void buttons_init(void)
{
	P1SEL &= ~BTN;									// switch to GPIO mode
	P1SEL2 &= ~BTN;									// switch to GPIO mode

	P1DIR &= ~BTN;									// Set as Input
	P1REN |= BTN;									// Poll Up/Down Resistor enable
	P1OUT |= BTN;									// Poll Up
	P1IE |= BTN;									// Interrupt Enabled
	P1IES |= BTN;									// Hi/Lo edge
	P1IFG &= ~BTN;									// IFG cleared
}
#endif

#define RCB BIT2
static unsigned short rc_init(void)
{
	char is_charged;

	P2SEL &= ~RCB;					// switch to GPIO mode
	P2SEL2 &= ~RCB;					// switch to GPIO mode

	P2DIR &= ~RCB;					// Set as Input
	P2REN |= RCB;					// Poll Up/Down Resistor enable
	P2OUT |= RCB;					// Poll Up
	P2IE &= ~RCB;					// Interrupt Disabled

	is_charged = P2IN & RCB;

	return is_charged ? 50*64 : 0; // 64 seconds in minute :)
}

#define TRIACB BIT5
static void triac_init(void)
{
	P2SEL &= ~TRIACB;							// switch to GPIO mode
	P2SEL2 &= ~TRIACB;							// switch to GPIO mode

	P2DIR |= TRIACB;							// Set as Output
	P2OUT |= TRIACB;							// Open Drain, "1" for switching off
}

static unsigned char data[LEDS_CNT + 3] = {MAGIC_BEGIN, 0, 0, 0, 0, 0, 0, 0xff};

void main(void)
{
	int i;
	unsigned char crc;
	unsigned short heat;

	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	default_state();
	heat = rc_init();

	clock_init(); /* DCO, MCLK and SMCLK - 1MHz */
	timer_init();
	x10tx_init();
	leds_init();
	thermistor_init();
	triac_init();

	leds_hello();
	for (;;)
	{
		if (heat)
		{
			if (--heat)
				P2OUT &= ~TRIACB;
			else
				P2OUT |= TRIACB; // Open Drain, "1" for switching off
		}

		led_toggle();
		themps_update();

		crc = MAGIC_BEGIN;

		for (i = 0; i<LEDS_CNT; i++)
		{
			data[i+1] = t[i];
			crc += data[i+1];
		}
		data[i+1] = heat >> 6; // 64 seconds in a minute
		crc += data[i+1];
		i++;

		data[i+1] = crc;
		x10tx_putn(data, sizeof(data));

		_BIS_SR(LPM0_bits + GIE);
	}
}

#pragma vector=TIMER0_A1_VECTOR
__interrupt void main_timer(void)
{
	switch(TA0IV)
	{
	case 10: /* TAIFG, timer overflow */
		break;
	case 2: /* TACCR1 CCIFG, compare 1 */
	case 4: /* TACCR2 CCIFG, compare 2 */
	default: /* impossible! */
		for (;;);
	}

	jiffies++;

	_BIC_SR_IRQ(LPM0_bits);
}
