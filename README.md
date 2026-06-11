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
* Pin 1 to GND/blaack
* Pin 4 to SWCLK/orange/TX/SC
* Pin 5 to SWDIO/yellow/RX/SD

Use tools like openocd and gdb-multiarch to connect to the LPC11U35 and 
extract the firmware. The toypad does **not** use CRP (code read 
protection).
