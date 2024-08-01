SHELL		= /bin/sh
BUILD_DATE	= $(shell date +'\"%y%m%d\"')

MCU		= atmega32u4
F_CPU		= 16000000
F_USB		= ${F_CPU}
TARGET		= glitcher
#OPTIMIZATION	= 1
OPTIMIZATION	= g

CPP_SOURCES	= ${TARGET}.cpp
C_SOURCES	=

OBJS = $(filter %.o,$(CPP_SOURCES:.cpp=.o) $(C_SOURCES:.c=.o))

CC  = avr-gcc
CXX = avr-g++

CXXFLAGS =
CXXFLAGS += -std=c++17
CXXFLAGS += -g
CXXFLAGS += -Wformat -Werror=format-security -Wall -Wextra -Werror -O${OPTIMIZATION}
CXXFLAGS += -mmcu=${MCU}
CXXFLAGS += -DF_CPU=${F_CPU}
CXXFLAGS += -DF_USB=${F_USB}
CXXFLAGS += -I/usr/include/simavr



## The first target is also the target for a "make" without arguments.
${TARGET}.hex:



upload: ${TARGET}.hex Makefile
	avrdude -p ${MCU} -c avr109 -P /dev/ttyACM0 -D -U flash:w:${TARGET}.hex:i

%.hex: %.elf Makefile
	avr-objcopy -O ihex -R .eeprom $< $@

${TARGET}.elf: ${OBJS} Makefile
	avr-g++ -mmcu=${MCU} ${OBJS} -o $@
	avr-size --format=avr --mcu=${MCU} $@

%.o: %.cpp Makefile
	${CXX} ${CXXFLAGS} $< -o $@ -c

%.s: %.cpp Makefile
	${CXX} ${CXXFLAGS} -S -fverbose-asm $< -o $@ -c

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
		${TARGET}.sym
