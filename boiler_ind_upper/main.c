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
#include "config.h"
#include "../common/common.h"
#include <thermistor.h>
#include <leds.h>
#include <uart.h>

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
    TA0CCR0 = 0xF424; /* 125 kHz / 2 */

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

static char data[LEDS_CNT + 3] = {MAGIC_BEGIN, 0, 0, 0, 0, 0, 0xff, MAGIC_END};

void main(void)
{
	int i;
	unsigned char crc;

	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	default_state();
	clock_init(); /* DCO, MCLK and SMCLK - 1MHz */
	timer_init();
	uart_init();
	leds_init();
	thermistor_init();

	leds_hello();
	for (;;)
	{
		led_toggle();
		themps_update();

		crc = MAGIC_BEGIN;

		for (i = 0; i<LEDS_CNT; i++)
		{
			data[i+1] = t[i];
			crc += t[i];
		}
		data[i+1] = crc;
		uart_data(data, LEDS_CNT + 3);

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

#pragma vector=TIMER1_A1_VECTOR
__interrupt void timer1_Stub(void)
{
	for (;;);
}

#pragma vector = USCIAB0RX_VECTOR
__interrupt void uscib0rx_Stub (void)
{
	for (;;);
}
