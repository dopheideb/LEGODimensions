#define MAGIC 0x015ac37f
#define NT "\n\t"
#define NX "\n"

#include <stdint.h>

int main()
{
	asm volatile (
		NX "loop: cmp %0, %1"
		NT "beq loop"
	::
		"r" (MAGIC),
		"r" (MAGIC)
	);
	while (1) {}
}

struct vectors {
	uint32_t stack;
	void *entry;
};

const struct vectors vectors __attribute__((section (".isr_vector"))) = { 0x10000ffc, &main };
