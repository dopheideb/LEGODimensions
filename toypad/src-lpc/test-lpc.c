#define MAGIC 0x015ac37f
#define NT "\n\t"
#define NX "\n"

// Use GPIO port 0, because those pins are available on our chip.

#if 0
#define PIN_A 2
#define PIN_B 7
#define PIN_IN 5

#define GPIO_BASE 0x50000000

#define GPIO_DIR (*(volatile uint32_t *)(GPIO_BASE | 0x2000))
#define GPIO_DIR_MASK ((1 << PIN_A) | (1 << PIN_B))

#define GPIO_PBYTE_IN (*(volatile uint32_t *)(GPIO_BASE | 0x0000 | PIN_IN))

#define GPIO_NOT (*(volatile uint32_t *)(GPIO_BASE | 0x2300))
#define GPIO_MASK_A (1 << PIN_A)
#define GPIO_MASK_B (1 << PIN_B)
#else
// Proto board; use LED for A/B.
#define PIN_A 24
#define PIN_B 25
#define PIN_IN 5

#define GPIO_BASE 0x50000000

#define GPIO_DIR (*(volatile uint32_t *)(GPIO_BASE | 0x2004))
#define GPIO_DIR_MASK ((1 << PIN_A) | (1 << PIN_B))

#define GPIO_PBYTE_IN (*(volatile uint32_t *)(GPIO_BASE | 0x0004 | PIN_IN))

#define GPIO_NOT (*(volatile uint32_t *)(GPIO_BASE | 0x2304))
#define GPIO_MASK_A (1 << PIN_A)
#define GPIO_MASK_B (1 << PIN_B)
#endif

#include <stdint.h>

int main()
{
	GPIO_NOT = GPIO_MASK_A | GPIO_MASK_B;	// Set A and B high.
	GPIO_DIR = GPIO_DIR_MASK;	// Set A and B to output.
	while (GPIO_PBYTE_IN) {}	// Wait for avr to be ready.
	GPIO_NOT = GPIO_MASK_A;
	while (!GPIO_PBYTE_IN) {}	// Wait for avr pulse.
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
