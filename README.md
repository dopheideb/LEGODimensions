# Introduction

LEGO Dimensions is (in our opinion) a great toys-to-life game. The game 
can be expanded by buying extra LEGO characters+vehicles. That'll unlock 
more levels in the game.

# Security

The security of the tag part was broken in 2016. It is well known how 
the toypad writes and reads tags.

However, writing a tag was done by (Android) apps, and none of them seem 
to work anymore. One goal is to write a fresh tag with common tools, so 
we can write our own tags anytime anywhere.

The other goal is to obtain the actual firmware of the toypad. It is 
rumored that the security was broken because the details could be 
reverse engineered from the toypad firmware. However, we couldn't find 
sources *how* they actually obtained the firmware, nor the contents of 
said firmware.

# Obtaining the firmware

Just solder wires to the J2 header.

The pinout is:

| Pin number  | Connected to LPC11U35 pin | Meaning of LPC11U35 pin
| ----------- | ------------------------: | -----------------------
| 1           | 33                        | `VSS` (i.e. ground)
| 2           | 02                        | not(`RESET`)/`PIO0_0`
| 3           | 18                        | `PIO0_9`/**`MOSI0`**/`CT16B0_MAT1`
| 4           | 19                        | **`SWCLK`**/`PIO0_10`/`SCK0`/`CT16B0_MAT2`
| 5           | 25                        | **`SWDIO`**/`PIO0_15`/`AD4`/`CT32B1_MAT2`
| 6<br>&nbsp; | 06<br>29                  | `VDD`<br>&nbsp;

Closest to the corner is pin 1. Pin 6 is near the writing "J2".

Buy a Raspberry Pi Debug Probe 
(https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html) 
and connect it:
* Pin 1 to GND/black
* Pin 4 to SWCLK/orange/TX/SC
* Pin 5 to SWDIO/yellow/RX/SD

Use tools like openocd and gdb-multiarch to connect to the LPC11U35 and 
extract the firmware. The toypad does **not** use CRP (code read 
protection).

# Fysical aspects

## Bottom label

### Bottom label (Xbox 360)

Our label on the bottom reads:
```
©2015 The LEGO Group.    This device complies with Part 15 of
LEGO DIMENSIONS          the FCC Rules. Operation is subject to
Tracking #: 3000061480   the following two conditions: (1) this
IFETEL: RCPLE3015-0602   device may not cause harmful
                         interference, and (2) this device must
                         accept any interference received,
5V ⎓                     including interference that may cause
                         undesired operation.
  ____ _____    ___ _________  ___  
 / ___| ____|  / _ \___ / ___|/ _ \ 
| |   |  _|   | | | ||_ \___ \ (_) |
| |___| |___  | |_| |__) |__) \__, |
 \____|_____|  \___/____/____/  /_/ 
(CE 0359)
FCC ID: 2ADL5-3000061480
IC: 12514A-3000061480 Warner
Model/Modelo No.: 3000061480 for X360
WBIE, Burbank, CA 91522 USA                              China
```

### Bottom label (PS3/PS4/WiiU)

Our label on the bottom reads:
```
©2015 The LEGO Group.    This device complies with Part 15 of
LEGO DIMENSIONS          the FCC Rules. Operation is subject to
Tracking #: 3000061482   the following two conditions: (1) this
                         device may not cause harmful
                         interference, and (2) this device must
                         accept any interference received,
5V ⎓                     including interference that may cause
                         undesired operation.
  ____ _____    ___ _________  ___  
 / ___| ____|  / _ \___ / ___|/ _ \ 
| |   |  _|   | | | ||_ \___ \ (_) |
| |___| |___  | |_| |__) |__) \__, |
 \____|_____|  \___/____/____/  /_/ 
(CE 0359)
FCC ID: 2ADL5-3000061482
IC: 12514A-3000061482 Warner
Model No: 3000061482 for PS3/PS4/WiiU
WBIE, Burbank, CA 91522 USA                              China
```

## Main IC: LPC11Uxx

### Main IC: LPC11U35 (Xbox 360)

### Main IC: LPC11U24 (PS3/PS4/WiiU)

## Crystals

The crystals are the same for both Xbox 360 version and PS3/PS4/WiiU version.

The writings on the crystal for the LPC11Uxx chip:
```
CMT-XDAG32
```

The writings on the crystal for the MF63001 chip:
```
CMT-XDAJ32
```

Both are assumed to be 32 MHz quartz crystal oscillators.

## NFC IC: MFRC63001

The writings on the chip:
```
NXP        NXP
63001      63001
46 11      67 08
ZSD518     ZSD446
```

These are MFRC630 chips.

## Hex inverting buffer: HEF4049BT

The writings on the chip:
```
NXP           NXP
HEF4049BT     HEF4049BT
CEX54004      CHE99112
TnD15054      TnD15184
```

The LEDs on the left and right pad probably draw too much current for 
the LPC11Uxx to feed the LEDs directly.

## Xbox security chip

### Xbox security chip (Xbox version)

Writings on the chip:
```
XBOX 2
859815
H1533
```

### Xbox security chip (PS3/PS4/WiiU)

The Xbox security chip is not present on the PS3/PS4/WiiU version. 
However, some space is reserved for a chip.

Since the PCB also states "XBONE VERSION", it is reckoned that 
PS3/PS4/WiiU is a stripped down Xbox One version.

# Pinout LPC11U35

The pinout is the following:

| LPC11U35 pin | Symbol if GPIO | Function/symbol | Connected to               | Remarks
| -----------: | -------------- | --------------- | -------------------------- | ----------
|            1 | PIO1_19        | SSEL1           |                            | SSP: Slave select
|            2 | PIO0_0         | not RESET       | J2 header, pin 2           |
|            3 | PIO0_1         | PIO0_1          | ?Xbox security chip?       |
|            4 |                |                 |                            |
|            5 |                |                 |                            |
|            6 |                |                 |                            |
|            7 |                |                 |                            |
|            8 | PIO0_2         | PIO0_2          | Right pad, blue            | Via hex inverter.
|            9 |                |                 |                            |
|           10 | PIO0_4         | PIO0_4          | Left pad, green            | Via hex inverter.
|           11 | PIO0_5         | PIO0_5          | Left pad, blue             | Via hex inverter.
|           12 | PIO0_21        | MOSI1           |                            | SSP1
|           13 |                |                 |                            |
|           14 |                |                 |                            |
|           15 | PIO0_6         | PIO0_6          | Right pad, green           | Via hex inverter.
|           16 | PIO0_7         | PIO0_7          | Right pad, red             | Via hex inverter.
|           17 |                |                 |                            |
|           18 | PIO0_9         | MOSI0           | J2 header, pin 3           |
|           19 | PIO0_10        | SWCLK           | J2 header, pin 4           | Serial Wire Debug: clock<br>Raspberry Pi Debug Probe: SWCLK/orange/TX/SC
|           20 | PIO0_22        | MISO1           |                            | SSP1
|           21 | PIO0_11        | PIO0_11         | Center pad, red            |
|           22 | PIO0_12        | PIO0_12         | Center pad, green          |
|           23 | PIO0_13        | PIO0_13         | Center pad, blue           |
|           24 |                |                 |                            |
|           25 | PIO0_15        | SWDIO           | J2 header, pin 5           | Serial Wire Debug: data<br>Raspberry Pi Debug Probe: SWDIO/yellow/RX/SD
|           26 |                |                 |                            |
|           27 |                |                 |                            |
|           28 | PIO1_15        | SCK1            |                            | SSP1
|           29 |                |                 |                            |
|           30 | PIO0_17        | PIO0_17         | Xbox security chip         | Output, clock-ish, see 0x309c.
|           31 | PIO0_18        | PIO0_18         | Left pad, red              | Via hex inverter.
|           32 | PIO0_19        | PIO0_19         | Xbox security chip         | Input, data from Xbox security chip.
|           33 |                | Vss             | Ground<br>J2 header, pin 1 | Raspberry Pi Debug Probe: GND/black

# Xbox security chip

Writings on the chip:
```
XBOX 2
859815
H1533
```

Commands:

| Command        | Meaning                                     |
| -------------- | ------------------------------------------- |
| 09 05 00 00 00 | Initialize                                  |
| 09 0f 03 00 00 | ??                                          |
| 09 18 01 00 01 | UsbdSecXSM3GetResponseVerifyProtocolData1?? |
| 09 5b 00 00 17 | GetIdentificationProtocolData               |
| 09 5c 00 00 ?? | ??                                          |
