test-lpc.elf:

CFLAGS = -mthumb -specs=nosys.specs
#CFLAGS += -fdata-sections -ffunction-sections -Wl,--gc-sections
PREFIX = arm-none-eabi-

%.elf: %.c Makefile memory.ld
	$(PREFIX)gcc $(CFLAGS) -T memory.ld $< -o $@

disasm: test-lpc.elf
	$(PREFIX)objdump -d $<

upload: test-lpc.elf
	openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000" -c "program $< verify reset exit"

ocd:
	openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000"

debug: test-lpc.elf
	gdb-multiarch -x ../src-pc/commands.gdb
