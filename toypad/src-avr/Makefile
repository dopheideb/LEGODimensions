SHELL		= /bin/sh
BUILD_DATE	= $(shell date +'\"%y%m%d\"')

MCU		= atmega32u4
MCU_TYPE	= avr5
F_CPU		= 16000000
TARGET		= glitcher
#OPTIMIZATION	= 1
OPTIMIZATION	= g

CPP_SOURCES	= ${TARGET}.cpp usart.cpp
C_SOURCES	=
ASM_SOURCES	=

OBJS = $(filter %.o,$(CPP_SOURCES:.cpp=.o) $(C_SOURCES:.c=.o) $(ASM_SOURCES:.S=.o))

CC  = avr-gcc
CXX = avr-g++

CFLAGS =
CFLAGS += -mmcu=${MCU}
CFLAGS += -DF_CPU=${F_CPU}
CFLAGS += -I.

CXXFLAGS =
CXXFLAGS += -std=c++17
CXXFLAGS += -g
CXXFLAGS += -Werror
CXXFLAGS += -Wformat -Werror=format-security -Wall -Wextra -O${OPTIMIZATION}
CXXFLAGS += -mmcu=${MCU}
CXXFLAGS += -DF_CPU=${F_CPU}
CXXFLAGS += -DUSART_ECHO
#CXXFLAGS += -ffixed-r16
#CXXFLAGS += -I/usr/include/simavr

LDFLAGS =
#LDFLAGS += --verbose
LDFLAGS += -nostartfiles
#LDFLAGS += -Wl,-Map=$(@:.elf=.map),--cref

PORT ?= /dev/ttyACM1

#.INTERMEDIATE: asm-defines.h dummy.c



## The first target is also the target for a "make" without arguments.
${TARGET}.hex:



upload: ${TARGET}.hex Makefile
	timeout 5 bash -c 'while [ ! -e ${PORT} ]; do sleep 0.1; done' &&\
	sleep 0.5 &&\
	avrdude\
		-p ${MCU}\
		-c avr109\
		-P ${PORT}\
		-D\
		-U flash:w:${TARGET}.hex:i

%.hex: %.elf Makefile
	avr-objcopy -O ihex -R .eeprom $< $@

${TARGET}.elf: ${OBJS} Makefile
	avr-g++ ${LDFLAGS} -mmcu=${MCU} ${OBJS} -o $@
	avr-size --format=avr --mcu=${MCU} $@

${TARGET}.objdump: ${TARGET}.hex Makefile
	avr-objdump\
		--architecture=${MCU_TYPE}\
		--disassemble-all\
		${TARGET}.hex\
		>\
		${TARGET}.objdump

%.o: %.cpp ${TARGET}.hpp Makefile
	${CXX} ${CXXFLAGS} $< -o $@ -c

#%.o: %.S ${TARGET}.hpp Makefile asm-defines.h
#	@echo Make $@ from $<
#	${CC} ${CFLAGS} -c -o $@   $<

%.s: %.cpp ${TARGET}.hpp Makefile
	${CXX} ${CXXFLAGS} -S -fverbose-asm $< -o $@ -c

#dummy.c: Makefile
#	touch -- $@

#asm-defines.h: dummy.c Makefile
#	${CC} ${CFLAGS} -S -o asm-defines.h dummy.c

.PHONY: clean
clean:
	rm\
		--force\
		--\
		${OBJS}\
		${TARGET}.bin\
		${TARGET}.eep\
		${TARGET}.elf\
		${TARGET}.hex\
		${TARGET}.lss\
		${TARGET}.s\
		${TARGET}.sym
