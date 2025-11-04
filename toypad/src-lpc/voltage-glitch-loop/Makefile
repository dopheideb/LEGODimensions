voltage-glitch-loop.elf:

CFLAGS = -mthumb -mcpu=cortex-m0 -O1 -specs=nosys.specs -fdata-sections -ffunction-sections
#CFLAGS += -Wl,--gc-sections
PREFIX = arm-none-eabi-

%.elf: %.c Makefile memory.ld
	$(PREFIX)gcc $(CFLAGS) -T memory.ld $< -o $@

%.asm: %.c Makefile memory.ld
	$(PREFIX)gcc -S $(CFLAGS) -T memory.ld $< -o $@

%.bin: %.elf Makefile
	$(PREFIX)objcopy -O binary $< $@

disasm: voltage-glitch-loop.elf
	$(PREFIX)objdump --disassemble $<

upload: voltage-glitch-loop.bin
	## Compute Cortex checksum.
	./cm3_checksum.py $<
	## Switch LPCXpresso from DFU to CMSIS.
	-dfu-util --device=-,1fc9:000c --cfg=1 --intf=0 --transfer-size=2048 --download=./LPC432x_CMSIS_DAP_V5_460.bin.hdr; sleep 3
	openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000" -c "program $< verify reset exit"

#dfu: voltage-glitch-loop.bin
#	dfu-util --alt 0 --download $<

ocd:
	openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000"

clean:
	-rm -f -- voltage-glitch-loop.elf voltage-glitch-loop.bin
