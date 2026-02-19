/* vim: set tabstop=8 tw=76 ai: */

// https://github.com/Wiznet/HTTPServer_LPC11E36_LPCXpresso/blob/master/lpc_chip_11exx/src/wwdt_11xx.c
// https://github.com/Wiznet/HTTPServer_LPC11E36_LPCXpresso/blob/master/lpc_chip_11exx/src/uart_11xx.c

#include <stdint.h>
#include "watchdog.h"
#define _BV(bit) (1ULL << (bit))

#define LPC_SYSCON_BASE       0x40048000
#define LPC_SYSCON_SYSPLLCLKSEL (*((volatile unsigned int *)(LPC_SYSCON_BASE + 0x40)))
#define LPC_SYSCON_SYSPLLCTRL   (*((volatile unsigned int *)(LPC_SYSCON_BASE + 0x08)))
#define LPC_SYSCON_PDRUNCFG     (*((volatile unsigned int *)(LPC_SYSCON_BASE + 0x238)))
#define LPC_SYSCON_SYSPLLSTAT   (*((volatile unsigned int *)(LPC_SYSCON_BASE + 0x0C)))
#define LPC_SYSCON_MAINCLKSEL   (*((volatile unsigned int *)(LPC_SYSCON_BASE + 0x70)))
#define LPC_SYSCON_MAINCLKUEN   (*((volatile unsigned int *)(LPC_SYSCON_BASE + 0x74)))

// User manual 9.5.3.3 GPIO port direction registers
#define GPIO_PORT_DIR0 (*((volatile unsigned int *) 0x50002000))
#define GPIO_PORT_DIR1 (*((volatile unsigned int *) 0x50002004))
#define GPIO_PORT_SET0 (*((volatile unsigned int *) 0x50002200))
#define GPIO_PORT_SET1 (*((volatile unsigned int *) 0x50002204))
#define GPIO_PORT_CLR0 (*((volatile unsigned int *) 0x50002280))
#define GPIO_PORT_CLR1 (*((volatile unsigned int *) 0x50002284))
#define GPIO_PORT_NOT0 (*((volatile unsigned int *) 0x50002300))
#define GPIO_PORT_NOT1 (*((volatile unsigned int *) 0x50002304))


// User manual Chapter 12: LPC11Uxx USART
#define LPC_USART_BASE	0x40008000
#define LPC_USART_DLL	(*((volatile unsigned int *)(LPC_USART_BASE + 0x00)))
#define LPC_USART_DLM	(*((volatile unsigned int *)(LPC_USART_BASE + 0x04)))
#define UARTFCR		(*((volatile unsigned int *) 0x40008008))
#define LPC_USART_LCR	(*((volatile unsigned int *)(LPC_USART_BASE + 0x0C)))
#define LPC_USART_FDR	(*((volatile unsigned int *)(LPC_USART_BASE + 0x28)))
#define UARTCLKDIV	(*((volatile unsigned int *) 0x40048098))

// THR: Transmit Holding Register
#define LPC_USART_THR (*((volatile unsigned int *)(LPC_USART_BASE + 0x00)))

// LSR: Line Status Register
#define LPC_USART_LSR (*((volatile unsigned int *)(LPC_USART_BASE + 0x14)))

// THR: THR is empty
#define LPC_USART_LSR_THRE (1 << 5)

#define LPC_IOCON_BASE 0x40044000
#define LPC_IOCON_PIO0_18 (*((volatile unsigned int *)(LPC_IOCON_BASE + 18 * 0x04)))
#define LPC_IOCON_PIO0_19 (*((volatile unsigned int *)(LPC_IOCON_BASE + 19 * 0x04)))


void initUSART()
{
	// Set pin function to "RXD".
	LPC_IOCON_PIO0_18 &= (0xF8 | 0x1);
	
	// Set pint function to "TXD".
	LPC_IOCON_PIO0_18 &= (0xF8 | 0x1);
	
	// 
	UARTCLKDIV = 1;
	
	// 
	UARTFCR = _BV(0)	// FIFOEN
		| _BV(1)	// RXFIFORES
		| _BV(2)	// TXFIFORES
		;
}


void configureBaudRate(unsigned int baudRate)
{
	LPC_USART_LCR |= 0x3;	// 8-bit characters,
	
	// Enable access to Divisor Latches by setting DLAB bit in LCR
	LPC_USART_LCR |= (1 << 7);
	
	// See User manual 12.5.14.1.2 Example 2: UART_PCLK = 12.0 MHz, BR = 115200
	if (baudRate == 115200)
	{
		LPC_USART_DLM = 0;
		LPC_USART_DLL = 4;
		
		uint8_t divaddval = 5;
		uint8_t mulval = 8;
		LPC_USART_FDR = (mulval << 4) | divaddval;
		
		return;
	}
    unsigned int divisor = 12000000 / (16 * baudRate);
    unsigned int dlm = divisor / 256;
    unsigned int dll = divisor % 256;
    unsigned int mulVal = 1; // Multiplier value for fractional divider
    unsigned int divAddVal = 0; // Divider add value for fractional divider


    // Set DLM and DLL
    LPC_USART_DLM = dlm;
    LPC_USART_DLL = dll;

    // Set FDR
    LPC_USART_FDR = (mulVal << 4) | divAddVal;

    // Disable access to Divisor Latches by clearing DLAB bit in LCR
    LPC_USART_LCR &= ~(1 << 7);
}

void sendChar(char c)
{
	// Wait until the THR is empty.
	while (!(LPC_USART_LSR & LPC_USART_LSR_THRE))
		;

	// Send the character.
	LPC_USART_THR = c;
}

void sendString(const char *str)
{
	while(*str)
	{
		sendChar(*str++);
	}
}

void wdt_feed(void)
{
	//__asm volatile ("cpsid i");
	WDFEED = 0xAA;
	WDFEED = 0x55;
	//__asm volatile ("cpsie i");
}

int main()
{
	initUSART();
	configureBaudRate(115200);
	while(1)
	{
		sendString("hoi\r\n");
	}
	
	// From the user manual:
	// 
	// Remark: The frequency of the watchdog oscillator is undefined
	// after reset. The watchdog oscillator frequency must be programmed
	// by writing to the WDTOSCCTRL register before using the watchdog
	// oscillator.
	
	//uint8_t divsel = 0x00;	// Divider: 2
	uint8_t divsel = 0x1F;		// Divider: 64
	uint8_t freqsel = 0x1;		// Frequency: 0.6 MHz
	//uint8_t freqsel = 0xF;	// Frequency: 4.6 MHz
	WDTOSCCTRL =
		(divsel << 0)
		|
		(freqsel << 5)
		;
	
	// Set the watchdog timer (24 bit register).
	//WDTC = 0x0000FF;
	WDTC = 0xFFFFFF;
	
	WDCLKSEL =
		// Use watchdog oscillator, not the IRC.
		_BV(WDCLKSEL_CLKSEL)
		|
		// Lock our CLKSEL choice.
		_BV(WDCLKSEL_LOCK)
		;

	// Power the watchdog oscillator.
	PDRUNCFG &= ~_BV(WDTOSC_PD);
	
	// Enable the watchdog. (This does NOT start it.)
	WDMOD =   _BV(WDEN)
		| _BV(WDRESET)
		//| _BV(WDPROTECT)
		//| _BV(LOCK)
		;
	
	// Start the watchdog.
	//wdt_feed();
	
	// Set LEDs as output
	GPIO_PORT_DIR1 |=
		_BV(0)		// D7, bottom red led
		|
		_BV(1)		// D6
		|
		_BV(2)		// D5
		|
		_BV(3)		// D4, top red led
		|
		_BV(24)		// RGB: red
		|
		_BV(25)		// RGB: green
		|
		_BV(26)		// RGB: blue
		;
	GPIO_PORT_SET1 = _BV(24);	// Disable red.
	//GPIO_PORT_SET1 = _BV(25);	// Green.
	GPIO_PORT_SET1 = _BV(26);	// Disable blue.

	uint32_t x = 0xffffff;
	while(1)
	{
		wdt_feed();
		//uint32_t timer_value = WDTV;
		uint32_t timer_value = x--;
		uint32_t set_leds = 0;
		uint32_t clr_leds = 0;
		if (timer_value & 0xFF0000)
			set_leds |= _BV(3);
		else
			clr_leds |= _BV(3);
		if (timer_value & 0x00FF00)
			set_leds |= _BV(2);
		else
			clr_leds |= _BV(2);
		if (timer_value & 0x0000FF)
			set_leds |= _BV(1);
		else
			clr_leds |= _BV(1);
		
		GPIO_PORT_SET1 = clr_leds;
		GPIO_PORT_CLR1 = set_leds;
		GPIO_PORT_NOT1 = _BV(0);
		//for(uint16_t u = 0; u < 0x4000; ++u) {}
	}
}

struct vectors {
	uint32_t stack;
	void *entry;
};

const struct vectors vectors __attribute__((section (".isr_vector"))) =
{
	0x10000ffc,
	&main
};
