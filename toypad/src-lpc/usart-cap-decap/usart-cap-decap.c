#include <stdint.h>

#define SYSPLLCLKSEL	(*(volatile uint32_t*)0x40048040)
#define SYSPLLCLKUEN	(*(volatile uint32_t*)0x40048044)
#define MAINCLKSEL	(*(volatile uint32_t*)0x40048070)
#define MAINCLKUEN	(*(volatile uint32_t*)0x40048074)
#define SYSAHBCLKCTRL	(*(volatile uint32_t*)0x40048080)
#define UARTCLKDIV	(*(volatile uint32_t*)0x40048098)

#define IOCON_BASE	0x40044000
#define IOCON_PIO0_18	(*(volatile uint32_t*)(IOCON_BASE + 4 * 18))	// UART0: RXD
#define IOCON_PIO0_19	(*(volatile uint32_t*)(IOCON_BASE + 4 * 19))	// UART0: TXD

#define UART0_BASE	0x40008000
#define UART0_RBR	(*(volatile uint32_t*)(UART0_BASE + 0x00))
#define UART0_THR	(*(volatile uint32_t*)(UART0_BASE + 0x00))
#define UART0_LSR	(*(volatile uint32_t*)(UART0_BASE + 0x14))
#define UART0_LCR	(*(volatile uint32_t*)(UART0_BASE + 0x0C))
#define UART0_DLL	(*(volatile uint32_t*)(UART0_BASE + 0x00))
#define UART0_DLM	(*(volatile uint32_t*)(UART0_BASE + 0x04))
#define UART0_FDR	(*(volatile uint32_t*)(UART0_BASE + 0x28))

#define GPIO_BASE	0x50000000
#define GPIO_PORT_DIR0	(*(volatile uint32_t *)(GPIO_BASE | 0x2000))
#define GPIO_PORT_DIR1	(*(volatile uint32_t *)(GPIO_BASE | 0x2004))
#define GPIO_PORT_SET0	(*(volatile uint32_t *)(GPIO_BASE | 0x2200))
#define GPIO_PORT_SET1	(*(volatile uint32_t *)(GPIO_BASE | 0x2204))
#define GPIO_PORT_CLR0	(*(volatile uint32_t *)(GPIO_BASE | 0x2280))
#define GPIO_PORT_CLR1	(*(volatile uint32_t *)(GPIO_BASE | 0x2284))
#define GPIO_PORT_NOT0	(*(volatile uint32_t *)(GPIO_BASE | 0x2300))
#define GPIO_PORT_NOT1	(*(volatile uint32_t *)(GPIO_BASE | 0x2304))

#define LED_RED 24
#define LED_GREEN 25
#define LED_BLUE 26
#define LEDS_MASK ((1 << LED_RED) | (1 << LED_GREEN) | (1 << LED_BLUE))

static inline void set_red()	{ GPIO_PORT_CLR1 = 1 << LED_RED; }
static inline void set_green()	{ GPIO_PORT_CLR1 = 1 << LED_GREEN; }
static inline void set_blue()	{ GPIO_PORT_CLR1 = 1 << LED_BLUE; }
static inline void clear_red()	{ GPIO_PORT_SET1 = 1 << LED_RED; }
static inline void clear_green(){ GPIO_PORT_SET1 = 1 << LED_GREEN; }
static inline void clear_blue()	{ GPIO_PORT_SET1 = 1 << LED_BLUE; }
static inline void flip_red()	{ GPIO_PORT_NOT1 = 1 << LED_RED; }
static inline void flip_green()	{ GPIO_PORT_NOT1 = 1 << LED_GREEN; }
static inline void flip_blue()	{ GPIO_PORT_NOT1 = 1 << LED_BLUE; }

// Delay approximately 33 ms @ 12MHz
void delay()
{
	for (volatile int i = 0; i < 100000; i++)
	{}
}

void delay_ms(uint32_t ms)
{
	// 12 MHz = 12,000,000 cycles/sec
	// 1 ms = 12,000 cycles
	// Assume each loop iteration takes ~4 cycles
	// So we need 3000 iterations per ms
	for (uint32_t i = 0; i < ms; i++)
	{
		for (volatile uint32_t j = 0; j < 1000; j++)
		{}
	}
}


void uart_init()
{
	// Enable clocks to UART and IOCON.
	SYSAHBCLKCTRL |= (1 << 12) | (1 << 16);

	// Set UART clock divider to 1 (system clock).
	UARTCLKDIV = 1;

	// Configure RXD (PIO0_18) and TXD (PIO0_19).
	IOCON_PIO0_18 = 0x01;	// RXD
	IOCON_PIO0_19 = 0x01;	// TXD

	// Set baud rate to 115200.
	//
	// Check chapter 12 of the user manual for details. Specifically 
	// the formula in section 12.5.14 ("USART Fractional Divider 
	// Register"):
	//
	//   Baudrate = PCLK / (16 * (256 * DLM + DLL) * (1 + DivAddVal / MulVal))
	//
	// Section 12.5.14.1.2 ("Example 2: UART_PCLK = 12.0 MHz, BR = 
	// 115200") is exactly what we need:
	//   DLM = 0
	//   DLL = 4
	//   DIVADDVAL = 5
	//   MULVAL = 8
	UART0_LCR = 0x80;			// Enable access to Divisor Latches.
	UART0_DLM = 0;
	UART0_DLL = 4;
	UART0_FDR = (5 << 0) | (8 << 4);	// DivAddVal = 5, MulVal = 8
	UART0_LCR = 0x03;			// 8 bits, no parity, 1 stop bit

	// UART is ready.
}

void uart_send_byte(uint8_t b)
{
	// Wait for THR empty.
	while (!(UART0_LSR & (1 << 5)))
	{}

	// Send the byte.
	UART0_THR = b;
}

void uart_send_string(const char * str)
{
	while (*str)
	{
		uart_send_byte(*str++);
	}
}

uint8_t uart_read_byte__blocking()
{
	// Wait until RDR (Receiver Data Read).
	while (!(UART0_LSR & (1 << 0)))
	{}
	
	// Read and return.
	return UART0_RBR & 0xFF;
}

int main()
{
	// Switch to green LED first, so we know our program at least 
	// somewhat works.
	GPIO_PORT_DIR1 |= LEDS_MASK;	// Enable leds output.
	clear_red();
	clear_green();
	clear_blue();
	set_green();

	uart_init();
	uart_send_string("hello world\r\nThy bidding?\r\n");

	while (1)
	{
		uint8_t byte = uart_read_byte__blocking();
		flip_green();
		flip_blue();

		uint8_t return_byte = '?';
		if (byte >= 32 && byte < 127)
		{
			if (byte >= 'A' && byte <= 'Z')
			{
				return_byte = 'a' + (byte - 'A');
			}
			else if (byte >= 'a' && byte <= 'z')
			{
				return_byte = 'A' + (byte - 'a');
			}
			else
			{
				return_byte = byte;
			}
		}
		uart_send_byte(return_byte);
		uart_send_string("\r\n");
	}
	uart_send_string("hello world\r\n");
}

struct vectors {
	uint32_t stack;
	void * entry;
};

const struct vectors vectors __attribute__((section (".isr_vector"))) =
{
	0x10000ffc,
	&main
};
