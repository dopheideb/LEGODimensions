#!/bin/bash

arm-none-eabi-gcc\
	-Wall\
	-mthumb\
	-mcpu=cortex-m0\
	-O1\
	-specs=nosys.specs\
	-fdata-sections\
	-ffunction-sections\
	-T memory.ld\
	usart-copilot.c\
	-o usart-copilot.elf\
&& dfu-util\
	--device=-,1fc9:000c\
	--cfg=1\
	--intf=0\
	--transfer-size=2048\
	--download=./LPC432x_CMSIS_DAP_V5_460.bin.hdr
## We expect "dfu-util: unable to read DFU status after completion (LIBUSB_ERROR_IO)".
if [ $? -ne 74 ]
then
	echo 'FAIL.'
	exit 1
fi

sleep 3\
&& openocd\
	-f interface/cmsis-dap.cfg\
	-f target/lpc11xx.cfg\
	-c 'adapter speed 5000'\
	-c 'program usart-copilot.elf verify reset exit'\
&& echo 'Done.'
