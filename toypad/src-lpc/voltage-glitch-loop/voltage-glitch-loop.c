#include <stdint.h>

#define NULL ( (void *) 0)
#define _BV(bit) (1ULL << (bit))

#define MAGIC 0x015ac37f
#define NT "\n\t"
#define NX "\n"

#define NVIC_ISER	(*(volatile uint32_t*)0xE000E100)
#define NVIC_ISER_USART	(21)

#define SCB_AIRCR	(*(volatile uint32_t*)0xE000ED0C)
#define AIRCR_VECTKEY	(0x05FA << 16)
#define SYSRESETREQ	(1 << 2)

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
#define UART0_DLL	(*(volatile uint32_t*)(UART0_BASE + 0x00))
#define UART0_DLM	(*(volatile uint32_t*)(UART0_BASE + 0x04))
#define UART0_IER	(*(volatile uint32_t*)(UART0_BASE + 0x04))
#define IER_RBRINTEN	(1)
#define UART0_IIR	(*(volatile uint32_t*)(UART0_BASE + 0x08))
#define UART0_LCR	(*(volatile uint32_t*)(UART0_BASE + 0x0C))
#define UART0_LSR	(*(volatile uint32_t*)(UART0_BASE + 0x14))	// Line Status Register
#define LSR_RDR		(0)						// Receiver Data Ready
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

#define LED_RED		24
#define LED_GREEN	25
#define LED_BLUE	26
#define LEDS_MASK	((1 << LED_RED) | (1 << LED_GREEN) | (1 << LED_BLUE))

static inline void set_red()	{ GPIO_PORT_CLR1 = 1 << LED_RED; }
static inline void set_green()	{ GPIO_PORT_CLR1 = 1 << LED_GREEN; }
static inline void set_blue()	{ GPIO_PORT_CLR1 = 1 << LED_BLUE; }
static inline void clear_red()	{ GPIO_PORT_SET1 = 1 << LED_RED; }
static inline void clear_green(){ GPIO_PORT_SET1 = 1 << LED_GREEN; }
static inline void clear_blue()	{ GPIO_PORT_SET1 = 1 << LED_BLUE; }
static inline void flip_red()	{ GPIO_PORT_NOT1 = 1 << LED_RED; }
static inline void flip_green()	{ GPIO_PORT_NOT1 = 1 << LED_GREEN; }
static inline void flip_blue()	{ GPIO_PORT_NOT1 = 1 << LED_BLUE; }



void reset_chip()
{

}



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
	// Assume each loop iteration takes ~12 cycles
	// So we need 1000 iterations per ms.
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

	// Enable receive interrrupt.
	UART0_IER = _BV(IER_RBRINTEN);

	// Enable USART0 IRQ in NVIC's Interrupt Set-enable Register.
	NVIC_ISER = _BV(NVIC_ISER_USART);
}

void uart_send_byte(uint8_t b)
{
	// Wait for THR empty.
	while (!(UART0_LSR & (1 << 5)))
	{}

	// Send the byte (put in the the transmit holding register).
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

void uart_irq_handler()
{
	if (!(UART0_LSR & _BV(LSR_RDR)))
	{
		// No pending data.
		return;
	}

	uint8_t byte = uart_read_byte__blocking();
	flip_green();
	flip_blue();

	switch (byte)
	{
		// R: We must reset.
		case 'R':
			SCB_AIRCR = AIRCR_VECTKEY | SYSRESETREQ;
			while (1);	// Wait for reset.
			break;

		// 'E': echo (to check if we are still alive).
		case 'E':
			uart_send_byte('E');
			break;

		default:
			uart_send_byte('?');
	}
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
	// Send "B", meaning we are done "B"ooting. The voltage 
	// glitching may commence.
	uart_send_string("B");

	while (1)
	{
		volatile uint32_t magic0 = MAGIC;
		volatile uint32_t magic1 = MAGIC;
		volatile uint32_t magic2 = MAGIC;
		volatile uint32_t magic3 = MAGIC;
		volatile uint32_t magic4 = MAGIC;
		volatile uint32_t magic5 = MAGIC;
		volatile uint32_t magic6 = MAGIC;
		asm volatile (
			NX ".loop:"
			NT "cmp %0, %1"
			NT "bne .glitch"
			NT "cmp %0, %2"
			NT "bne .glitch"
			NT "cmp %0, %3"
			NT "bne .glitch"
			NT "cmp %0, %4"
			NT "bne .glitch"
			NT "cmp %0, %5"
			NT "bne .glitch"
			NT "cmp %0, %6"
			NT "beq .loop"
			NX ".glitch:"
		: // Output operands.
			"=r" (magic0),
			"=r" (magic1),
			"=r" (magic2),
			"=r" (magic3),
			"=r" (magic4),
			"=r" (magic5),
			"=r" (magic6)
		: // Input operands.
			"r" (magic0),
			"r" (magic1),
			"r" (magic2),
			"r" (magic3),
			"r" (magic4),
			"r" (magic5),
			"r" (magic6)
		: "memory"
		);

		// If we broke free of the asm loop, a glitch happened. 
		// Report.
		uart_send_string("!");
	}
}

void hang(){while(1);}

struct vectors {
	uint32_t stack;
	void * core_interrupts[1];
};

#define initial_stack 0x10000ffc
const struct vectors vectors __attribute__((section (".fault_vector"))) =
{
	initial_stack,		// 0 Initial stack pointer
	&main,			// 1 Reset handler
};
const void * irqs[32] __attribute__((section (".irq_vector"))) =
{
	NULL,			// IRQ 0: PIN_INT0
	NULL,			// IRQ 1: PIN_INT1
	NULL,			// IRQ 2: PIN_INT2
	NULL,			// IRQ 3: PIN_INT3
	NULL,			// IRQ 4: PIN_INT4
	NULL,			// IRQ 5: PIN_INT5
	NULL,			// IRQ 6: PIN_INT6
	NULL,			// IRQ 7: PIN_INT7
	NULL,			// IRQ 8: GINT0
	NULL,			// IRQ 9: GINT1
	NULL,			// IRQ 10: Reserved
	NULL,			// IRQ 11: Reserved
	NULL,			// IRQ 12: Reserved
	NULL,			// IRQ 13: Reserved
	NULL,			// IRQ 14: SSP1
	NULL,			// IRQ 15: I2C
	NULL,			// IRQ 16: CT16B0
	NULL,			// IRQ 17: CT16B1
	NULL,			// IRQ 18: CT32B0
	NULL,			// IRQ 19: CT32B1
	NULL,			// IRQ 20: SSP0
	&uart_irq_handler,	// IRQ 21: USART
};

void _close_r() {}
void _lseek_r() {}
void _read_r() {}
void _write_r() {}
void _sbrk() {}
