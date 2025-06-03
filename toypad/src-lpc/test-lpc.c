#include <stdint.h>

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

int main()
{
	GPIO_NOT = GPIO_MASK_A | GPIO_MASK_B;	// Set A and B high.
	GPIO_DIR = GPIO_DIR_MASK;	// Set A and B to output.
	GPIO_LED_DIR |= LEDS_MASK;	// Enable leds output.
	clear_red();
	clear_green();
	clear_blue();

	set_red();
	while (GPIO_PBYTE_IN) {}	// Wait for avr to be ready.
	GPIO_NOT = GPIO_MASK_A;
	clear_red();

	set_green();
	while (!GPIO_PBYTE_IN) {}	// Wait for avr pulse.
	clear_green();

	set_blue();
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
