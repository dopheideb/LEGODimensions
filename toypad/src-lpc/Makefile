test-lpc.elf:

CFLAGS = -mthumb -specs=nosys.specs -ffunction-sections -Wl,--gc-sections
PREFIX = arm-none-eabi-

%.elf: %.c Makefile memory.ld
	$(PREFIX)gcc $(CFLAGS) -T memory.ld $< -o $@

disasm: test-lpc.elf
	$(PREFIX)objdump -d $<
