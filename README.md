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
