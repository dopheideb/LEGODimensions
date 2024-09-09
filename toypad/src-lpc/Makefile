test-lpc.elf:

%.elf: %.c Makefile
	arm-none-eabi-gcc -mthumb -specs=nano.specs -specs=nosys.specs $< -o $@
