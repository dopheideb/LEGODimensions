#define MAGIC 0x015ac37f
#define NT "\n\t"
#define NX "\n"

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
