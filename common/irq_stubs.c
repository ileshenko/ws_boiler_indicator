/*
 * irq_stubs.c
 *
 *  Created on: Mar 4, 2013
 *      Author: Igor
 */
#include <msp430g2553.h>

#pragma vector=TIMER0_A0_VECTOR
__interrupt void t0_a0_Stub (void)
{
	for (;;);
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void t1_a0_Stub (void)
{
	for (;;);
}

#if 0
#pragma vector=TIMER1_A1_VECTOR
__interrupt void t1_a1_Stub (void)
{
	for (;;);
}
#endif

#pragma vector=WDT_VECTOR
__interrupt void wdt_Stub (void)
{
	for (;;);
}

#pragma vector = PORT1_VECTOR
__interrupt void port1_Stub (void)
{
		for (;;);
}

//#pragma vector=USI_VECTOR
#pragma vector=ADC10_VECTOR
__interrupt void adc10_Stub (void)
{
	for (;;);
}

#pragma vector=PORT2_VECTOR
__interrupt void port2_Stub (void)
{
	for (;;);
}

#pragma vector=NMI_VECTOR
__interrupt void nmi_Stub (void)
{
	for (;;);
}

#pragma vector=COMPARATORA_VECTOR
__interrupt void comparator_Stub (void)
{
	for (;;);
}

