test-lpc.elf:

CFLAGS = -mthumb -mcpu=cortex-m0 -O1 -specs=nosys.specs -fdata-sections -ffunction-sections
#CFLAGS += -Wl,--gc-sections
PREFIX = arm-none-eabi-

%.elf: %.c Makefile memory.ld
	$(PREFIX)gcc $(CFLAGS) -T memory.ld $< -o $@

%.bin: %.elf Makefile
	$(PREFIX)objcopy -O binary $< $@

disasm: test-lpc.elf
	$(PREFIX)objdump --disassemble $<

upload: test-lpc.elf
	## Switch LPCXpresso from DFU to CMSIS.
	#-dfu-util --device=-,1fc9:000c --cfg=1 --intf=0 --transfer-size=2048 --download=./LPC432x_CMSIS_DAP_V5_460.bin.hdr; sleep 3
	openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000" -c "program $< verify reset exit"

dfu: test-lpc.bin
	dfu-util --alt 0 --download $<

ocd:
	openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000"

debug: test-lpc.elf
	gdb-multiarch -x ../src-pc/commands.gdb

clean:
	-rm test-lpc.elf test-lpc.bin
