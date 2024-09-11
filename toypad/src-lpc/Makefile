test-lpc.elf:

%.elf: %.c Makefile memory.ld
	arm-none-eabi-gcc -mthumb -specs=nosys.specs -T memory.ld $< -o $@
