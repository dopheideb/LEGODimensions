#define MAGIC 0x015ac37f
#define NT "\n\t"
#define NX "\n"

#include <stdint.h>

int main()
{
	asm volatile (
		NX "1: b 1b"
		NX "1: cmp %0, %1"
		NT "bne 1f"
		NT "cmp %0, %2"
		NT "bne 1f"
		NT "cmp %0, %3"
		NT "bne 1f"
		NT "cmp %0, %4"
		NT "bne 1f"
		NT "cmp %0, %5"
		NT "bne 1f"
		NT "cmp %0, %6"
		NT "beq 1b"
		NX "1: b 1b"
	::
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC),
		"r" (MAGIC)
	);
}

struct vectors {
	uint32_t stack;
	void *entry;
};

const struct vectors vectors __attribute__((section (".isr_vector"))) = { 0x10000ffc, &main };
