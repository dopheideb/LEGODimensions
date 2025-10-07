usart-cap-decap.elf:

CFLAGS = -mthumb -mcpu=cortex-m0 -O1 -specs=nosys.specs -fdata-sections -ffunction-sections
#CFLAGS += -Wl,--gc-sections
PREFIX = arm-none-eabi-

%.elf: %.c Makefile memory.ld
	$(PREFIX)gcc $(CFLAGS) -T memory.ld $< -o $@

%.bin: %.elf Makefile
	$(PREFIX)objcopy -O binary $< $@

disasm: usart-cap-decap.elf
	$(PREFIX)objdump --disassemble $<

upload: usart-cap-decap.elf
	## Switch LPCXpresso from DFU to CMSIS.
	-dfu-util --device=-,1fc9:000c --cfg=1 --intf=0 --transfer-size=2048 --download=./LPC432x_CMSIS_DAP_V5_460.bin.hdr; sleep 3
	openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000" -c "program $< verify reset exit"

dfu: usart-cap-decap.bin
	dfu-util --alt 0 --download $<

ocd:
	openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000"

clean:
	-rm -f -- usart-cap-decap.elf usart-cap-decap.bin
