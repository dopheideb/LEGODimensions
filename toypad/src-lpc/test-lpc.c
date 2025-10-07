#include <stdint.h>

#define _BV(bit) (1ULL << (bit))

#define MAGIC 0x015ac37f
#define NT "\n\t"
#define NX "\n"

// Use GPIO port 0, because those pins are available on our chip.

#define GPIO_BASE 0x50000000

#define GPIO_LED_DIR (*(volatile uint32_t *)(GPIO_BASE | 0x2004))
#define GPIO_LED_SET (*(volatile uint32_t *)(GPIO_BASE | 0x2204))
#define GPIO_LED_CLR (*(volatile uint32_t *)(GPIO_BASE | 0x2284))
#define GPIO_LED_NOT (*(volatile uint32_t *)(GPIO_BASE | 0x2304))

#define LED_RED 24
#define LED_GREEN 25
#define LED_BLUE 26

#define LEDS_MASK ((1 << LED_RED) | (1 << LED_GREEN) | (1 << LED_BLUE))

static inline void set_red() { GPIO_LED_CLR = 1 << LED_RED; }
static inline void set_green() { GPIO_LED_CLR = 1 << LED_GREEN; }
static inline void set_blue() { GPIO_LED_CLR = 1 << LED_BLUE; }
static inline void clear_red() { GPIO_LED_SET = 1 << LED_RED; }
static inline void clear_green() { GPIO_LED_SET = 1 << LED_GREEN; }
static inline void clear_blue() { GPIO_LED_SET = 1 << LED_BLUE; }
static inline void flip_red() { GPIO_LED_NOT = 1 << LED_RED; }
static inline void flip_green() { GPIO_LED_NOT = 1 << LED_GREEN; }
static inline void flip_blue() { GPIO_LED_NOT = 1 << LED_BLUE; }

// Note: internal pull-up is enabled on these pins after reset.
#define PIN_A 2
#define PIN_B 7
#define PIN_IN 13

#define GPIO_DIR (*(volatile uint32_t *)(GPIO_BASE | 0x2000))
#define GPIO_DIR_MASK ((1 << PIN_A) | (1 << PIN_B))

#define GPIO_PBYTE_IN (*(volatile uint8_t *)(GPIO_BASE | 0x0000 | PIN_IN))

#define GPIO_NOT (*(volatile uint32_t *)(GPIO_BASE | 0x2300))
#define GPIO_MASK_A (1 << PIN_A)
#define GPIO_MASK_B (1 << PIN_B)

#define SYSAHBCLKCTRL (*((volatile unsigned int *) 0x40048080))
#define SYSAHBCLKCTRL_WWDT (_BV(15))

#define WDMOD (*((volatile unsigned int *) 0x40004000))
#define WDMOD_WDEN	(_BV(0))
#define WDMOD_WDRESET	(_BV(1))
#define WDMOD_WDTOF	(_BV(2))
#define WDMOD_WDINT	(_BV(3))
#define WDMOD_WDPROTECT	(_BV(4))
#define WDMOD_LOCK	(_BV(5))

#define WDTC (*((volatile unsigned int *) 0x40004004))
#define WDFEED (*((volatile unsigned int *) 0x40004008))
#define WDCLKSEL   (*((volatile unsigned int *) 0x40004010))
#define WDCLKSEL_CLKSEL (_BV( 0))
#define WDCLKSEL_LOCK   (_BV(31))

#define WDTOSCCTRL (*((volatile unsigned int *) 0x40048024))
#define WDTOSCCTRL_DIVSEL_2  (0x00)
#define WDTOSCCTRL_DIVSEL_64 (0x1F)
#define WDTOSCCTRL_FREQSEL_0_6_MHZ (0x1)
#define WDTOSCCTRL_FREQSEL_4_6_MHZ (0xF)
/* 0.6 MHz / 64 = 9375 Hz */
#define WDTOSCCTRL_FREQ_9375_HZ ((WDTOSCCTRL_DIVSEL_64 << 0) | (WDTOSCCTRL_FREQSEL_0_6_MHZ << 5))
#define PDRUNCFG (*((volatile unsigned int *) 0x40048238))
#define PDRUNCFG_WDTOSC_PD (_BV(6))

static inline void watchdog_feed()
{
	__asm volatile (NX "cpsid i" ::: "memory");
	WDFEED = 0xAA;
	WDFEED = 0x55;
	__asm volatile (NX "cpsie i" ::: "memory");
}
static inline void watchdog_start()
{
	watchdog_feed();
	// We have to wait 3 WDLCK cycles before setting WDPROTECT.
	//WDMOD |= WDMOD_WDPROTECT;
}
static inline void watchdog_init()
{
	// Power up the watchdog oscillator, since it is not powered by 
	// default. Note: active-low logic.
	PDRUNCFG &= ~PDRUNCFG_WDTOSC_PD;

	// Enable clock for the (windowed) watchdog.
	SYSAHBCLKCTRL |= SYSAHBCLKCTRL_WWDT;

	// Use watchdog oscillator, not the IRC. And lock our choice.
	WDCLKSEL = WDCLKSEL_CLKSEL
		| WDCLKSEL_LOCK
		;

	// UM10462 states: "Remark: The frequency of the watchdog 
	// oscillator is undefined after reset."
	WDTOSCCTRL = WDTOSCCTRL_FREQ_9375_HZ;

	// Set timer to 1 second.
	WDTC = 1 * 9375 / 4;	// 4 is the pre scaler (fixed divider).

	// Enable the watchdog. This does NOT start it (feeding once 
	// does).
	WDMOD =   WDMOD_WDEN
		| WDMOD_WDRESET
		| WDMOD_LOCK
		;
}

int main()
{
	GPIO_NOT = GPIO_MASK_A | GPIO_MASK_B;	// Set A and B high.
	GPIO_DIR = GPIO_DIR_MASK;	// Set A and B to output.
	GPIO_LED_DIR |= LEDS_MASK;	// Enable leds output.
	clear_red();
	clear_green();
	clear_blue();
	watchdog_init();

	set_red();
	while (GPIO_PBYTE_IN) {}	// Wait for avr to be ready.
	GPIO_NOT = GPIO_MASK_A;
	clear_red();

	set_green();
	while (!GPIO_PBYTE_IN) {}	// Wait for avr pulse.
	clear_green();

	set_blue();
	watchdog_start();

	GPIO_NOT = GPIO_MASK_B;
	asm volatile (
		NX "loop: cmp %0, %1"
		NT "bne glitch"
		NT "cmp %0, %2"
		NT "bne glitch"
		NT "cmp %0, %3"
		NT "bne glitch"
		NT "cmp %0, %4"
		NT "bne glitch"
		NT "cmp %0, %5"
		NT "bne glitch"
		NT "cmp %0, %6"
		NT "beq loop"
		NX "glitch:"
	::
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC)
	);
	GPIO_NOT = GPIO_MASK_A;
	// Glitched! White smoke!
	set_red();
	set_green();
	set_blue();
	while (1) {}
}

struct vectors {
	uint32_t stack;
	void *entry;
};

const struct vectors vectors __attribute__((section (".isr_vector"))) = { 0x10000ffc, &main };

#define WEAK __attribute__((weak))
WEAK void _close_r() {}
WEAK void _lseek_r() {}
WEAK void _read_r() {}
WEAK void _write_r() {}
WEAK void _sbrk() {}
