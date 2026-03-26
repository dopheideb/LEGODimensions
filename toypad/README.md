# Introduction

We think that the LEGO Dimensions Toy Pad contains the software 
(firmware) to generate the correct password to read the actual LEGO 
Dimension NFC toy tags. This is based upon hints in [Proxmark topic 
12016](http://www.proxmark.org/forum/viewtopic.php?id=12016) and 
[Proxmark topic 
2657](http://www.proxmark.org/forum/viewtopic.php?id=2657).

We were unable to find the firmware online. However, we would love to 
have access to the firmware to confirm the above assumption.

# Opening the toy pad

Beware: It is quite easy to break a plastic clamps while trying to open 
the toy pad. [iFixit guide 
100155](https://documents.cdn.ifixit.com/pdf/ifixit/guide_100155_en.pdf) 
has some pointers though.

# Microcontrollers inside the toy pad

There are only a few microcontrollers inside the LEGO Dimensions toy 
pad. The USB cable is attached to an NXP LPC11U35. Since that 
microcontroller is a general purpose ARM processor, it is a very good 
candidate to have the firmware to generate the correct key for an NFC 
tag.

# NXP LPC11U35

## Datasheet

NXP's Datasheet about LPC11U3X: 
<https://www.nxp.com/docs/en/data-sheet/LPC11U3X.pdf>

Keil's User Manual:
<https://www.keil.com/dd/docs/datashts/nxp/lpc11uxx/um10462.pdf>

## QFP

The datasheet tells us that the LPC11U35 can be subdivided into:
* LPC11U35FHN33/401 - HVQFN33 - 33 terminals - body 7 x 7 x 0.85 mm
* LPC11U35FBD48/401 - LQFP48 - 48 leads - body 7 x 7 x 1.4 mm
* LPC11U35FBD64/401 - LQFP64 - 64 leads - body 10 x 10 x 1.4 mm
* LPC11U35FHI33/501 - HVQFN33 - 33 terminals - body 5 x 5 x 0.85 mm
* LPC11U35FET48/501 - TFBGA48 - 48 balls; body 4.5 x 4.5 x 0.7 mm

The LPC11U35 on the toypad, has 32 visible pins, so it must an HVQFN: 
"plastic thermal enhanced very thin quad flat package".

The LPC11U35 on the toypad measures 5 x 5 mm, so it must be the 
"LPC11U35FHI33/501" version.

Wikipedia on Quad flat package: 
<https://en.wikipedia.org/wiki/Quad_flat_package>

## Serial wire debug (SWD)

We had no luck in accessing the LPC11U35 via Serial wire debug. We 
reckon LEGO has disabled SWD via "Code Read Protection" (CRP).

## CRP levels

There are three levels of Code Read Protection

## Power supply: 3V3

The datasheet states in chapter 2 ("Features and benefits"):
> `Single 3.3 V power supply (1.8 V to 3.6 V)`

So the LPC11U35 should still operate without problems at 1.8 Volts. In 
order to force a glitch we must supply a voltage **below** 1.8V.

## Brown out detection

The datasheet states several threshold voltages for brown out detection. 
See chapter 9.1 ("BOD static characteristics"). The lowest threshold for 
receiving an interrupt, is 2.22V. The lowest threshold for a reset, is 
1.46V.

This might very well indicate that the chip still functions correctly at 
even 1.46V. We reckon we must supply a voltage near 1.46V in order to 
see glitches.

# Firmware retrieval (1)

## Code Read Protection options

The UM10462 LPC11Uxx User manual gives in chapter 20 in table 346 all 
the Code Read Protection (CRP) options.

> | Name     | Pattern programmed in 0x000002FC
> | :------- | :-------------------------------
> | `NO_ISP` | `0x4E697370`
> | `CRP1`   | `0x12345678`
> | `CRP2`   | `0x87654321`
> | `CRP3`   | `0x43218765`

Note that the CRP value is a 32-bit value. A 32-bit integer can have 
4294967296 different values. But only four have a special meaning. This 
means that any of the other 4294967292 values mean **NO CODE 
PROTECTION** at all. A single bit flip is all it takes!

## Changing the firmware

Changing the firmware in location 0x2FC is beyond our capabilities; we 
will not travel that road.

## Boot process flowchart

The UM10462 LPC11Uxx User manual gives in chapter 20.10 the "Boot 
process flowchart". The chart in (pseudo) C:
```
int main()
{
  reset();
  initialize();
  if (crp_option != CRP_OPTION_1 && crp_option != CRP_OPTION_2 && crp_option != CRP_OPTION_3)
    enable_swd();
  
  if(WATCHDOG_FLAG_SET || crp_option == CRP_OPTION_3 || crp_option == CRP_OPTION_NO_ISP)
  {
    if(user_code_is_valid())
      execute_internal_user_code()
    else {
      /* Not of interest; there is valid user code in our device. */
    }
  }
  else
  {
    /* Enter ISP MODE if the correct GPIO pins have the correct logical 
    value. PIO_1 == LOW, PIO_3 == HIGH for USB ISP, LOW for UART ISP. */
  }
}
```

enable_debugging() is the part where SWD is allowed. ISP is a more 
"manual" protocol for accessing the flash (but not debugging).

We could get access to the flash contents, either through SWD or through 
ISP.

SWD can be enabled by manipulating swd_option. ISP needs that as well, 
plus some extra things. So using SWD is easier. It is also more easily 
accessible on the board.

## Manipulate the boot process

The value of the Code Read Protection is stored in the firmware, in 
location 0x2FC. We don't have the powers of changing the firmware. 
However, we have some knowledge about (micro)controllers, since we have 
dealt with AVRs and Z80s in the past. So, we know a thing or two about 
how a microcontroller functions.

One interesting property to note about microcontrollers, is that they 
typically do **not** have instructions to jump based on the firmware, 
only jump instructions based on memory, or even only based on registers. 
This implies that the boot process must execute at least 3 instructions:
1. LOAD firmware value 0x2FC into some register
2. COMPARE that register with the magic value for CRP option 3
3. JUMP based on flag register

What if we could **change** the content of the register after step 1? 
That would change the flag register and processor would take another 
branch in the boot process. Nice!

But how do we change the value of a register?!

## Change the value of a register

A register is made up of flip-flops (latches). See 
<https://en.wikipedia.org/wiki/Flip-flop_(electronics)> for more 
information. Of interest is the following paragraph about SR NOR latch:

> The R = S = 1 combination is called a restricted combination or a 
> forbidden state because, as both NOR gates then output zeros, it 
> breaks the logical equation Q = not Q. The combination is also 
> inappropriate in circuits where both inputs may go low simultaneously 
> (i.e. a transition from restricted to hold). The output could remain 
> in a metastable state and may eventually lock at either 1 or 0 
> depending on the propagation time relations between the gates (a race 
> condition).

<https://techiescience.com/how-can-one-minimize-flip-flop-glitches-effective-strategies-and-solutions/> 
tells us the following about sequential glitches:

> Sequential Glitches: These occur in sequential logic circuits, such as 
> flip-flops, when the input signal changes during the setup or hold 
> time of the flip-flop. This can cause the flip-flop to enter a 
> metastable state, leading to unpredictable behavior.

We would very much appreciate such a sequential glitch, as a single 
glitch means a bit flip and any single bit flip in the register holding 
the CRP options, means that there is **no code protection** at all.

A (sequential) glitch is a way of altering the register from "outside"

# Glitching

## Definition

From <https://foldoc.org/glitch>
```
/glich/ [German "glitschen" to slip, via Yiddish "glitshen", to slide or 
skid] 1. (Electronics) When the inputs of a circuit change, and the 
outputs change to some random value for some very brief time before they 
settle down to the correct value. If another circuit inspects the output 
at just the wrong time, reading the random value, the results can be 
very wrong and very hard to debug (a glitch is one of many causes of 
electronic heisenbugs).
```

This is why we need a glitch:
> `and the outputs change to some random value for some very brief time`

## Prior work by Chris Gerlinsky

Chris Gerlinsky presented his work on Recon Brussels 2017. The 
presentation was named "Breaking Code Read Protection on the NXP LPC 
family Microcontroller". The video is online at 
<https://www.youtube.com/watch?v=98eqp4WmHoQ>

His sources are available at 
<https://github.com/akacastor/xplain-glitcher>

<https://www.recon.cx/2017/brussels/talks/breaking_crp_on_nxp.html> 
states:
> As glitch attacks go, this is a simple and ‘beginner-level’ attack 
> which should be easily reproducible. The talk will include hardware 
> and software design, including schematics and source code, for a 
> glitcher able to bypass CRP.

## The power of lack of power

Chris Gerlinksy deliberately used a drop in voltage to force a bitflip. 
This is drop in voltage is known as a brownout. See 
<https://en.wikipedia.org/wiki/Brownout_(electricity)>.

Delivering decreased power to the LEGO Dimensions toypad seems like a 
doable task. The LPC11U35 used on the toypad is only 5mm by 5mm, too 
small to solder wires to (at least too small for *us*). However, the J2 
header looks more promising. (Note from Bas: Speak for yourself, I could 
solder wires to it. :-P But J2 is much more convenient, so we use that. 
:-D)

## The J2 header on the toypad 

The pinout is:

| Pin number  | Connected to LPC11U35 pin | Meaning of LPC11U35 pin
| ----------- | ------------------------: | -----------------------
| 1           | 33                        | `VSS` (i.e. ground)
| 2           | 02                        | not(`RESET`)/`PIO0_0`
| 3           | 18                        | `PIO0_9`/**`MOSI0`**/`CT16B0_MAT1` (?but why?)
| 4           | 19                        | **`SWCLK`**/`PIO0_10`/`SCK0`/`CT16B0_MAT2`
| 5           | 25                        | **`SWDIO`**/`PIO0_15`/`AD4`/`CT32B1_MAT2`
| 6<br>&nbsp; | 06<br>29                  | `VDD`<br>&nbsp;

Closest to the corner is pin 1. Pin 6 is near the writing "J2".

The pins on the header have **no resistance** to the counterpart on the 
LPC11U35. It seems it is mere wire between header and microcontroller. 
And that is a good thing; we have direct control, not regulated or 
impaired.

We can supply power to the microcontroller via the J2 header, because of 
the Vdd pin. This is most welcome to be able to force a bitflip via a 
brownout!

Having access to the Serial Wire Debug pins is of course also very nice. 
**If** we can glitch the boot process and force it to enter programming 
mode, then we can use SWD to read the whole firmware.

## Making our own glitcher

We are opportunistic hackers: we will try to build a glitcher with 
equipment and parts we have laying around:
- Lab power supplies
- STM32F070F6P6
- Texas Instruments CD74HC4053
- Raspberry Pi Debug Probe
- Rigol DS1102E digital oscilloscope
- Breadboard
- Jumper wires
- Transistors
- Resistors

Lab power supplies:
- One power supply to deliver "a just enough" but very stable voltage.
- One power supply to deliver "just too low" yet very stable voltage.

Rigol DS1102E:
This is a 2 channel 100 MHz, 1Gsa/s digital oscilloscope. Such a scope 
is might handy for debugging, checking actual voltages and actual 
voltage responses, checking the actual width of a pulse.

Raspberry Pi Debug Probe:
This debug probe can talk Serial Wire Debug (SWD) to an ARM processor. 
More specific, it can talk SWD to the LPC11U35, either the one on the 
toypad or the ones we bought for development/testing.

This "talking" also means: be able to read memory, registers, set the 
program counter. The probe is very powerful when combined with gdb (GNU 
debugger). We can then step/halt/examine the firmware like it is a 
regular program.

CD74HC4053:

The CD74HC4053 will act as a switch. A switch we control externally 
(from the STM32). The CD74HC4053 will switch between the "good" and 
"bad" voltage (both are input), to the output voltage connected to the 
Vdd pins (plural) of the LPC11U35. The CD74HC4053 is designed for this 
job, and switches fast enough for us.

STM32F070F6P6:
- This is a 3V3 chip. Since the LPC11U35 is also 3V3, we don't have to 
convert voltages. (We tried using an Atmel AVR (5V chip) at first, but 
the converting of voltage made was annoying and added unnecessary 
complexity.)
- Switching. The STM32 controls the CD74HC4053, i.e. when to apply "good" 
voltage to the LPC11U35, and when and how long to apply "bad" voltage to 
the toypad.
- Enabling. The STM32 controls the reset pin of the LPC11U35. The STM32 
determines when the toypad boots/resets.
- Communication. The STM32 has a serial port. We connect that port to 
our server/laptop, so we can interact with the STM32. For instance: 
control how long to apply the "bad" voltage.
- Timing. The STM32 can run a timer on 48 MHz. We use that timer to 
control how long to apply the "bad" voltage.

## STM32 firmware

We think we should build the following state machine for the AVR:
1. Start.
2. Blocking: Read serial command "\<delay_between_end_of_reset_and_begin_of_brownout\> \<brownout_length\>".
3. Configure timers:
   - Configure timer 1 such that it will give a reset pulse to the LPC11U35 of 1ms.
   - Configure timer 3 such that it will start when timer 1 stops the reset pulse. (STM32 can chain timers in hardware!)
   - Configure timer 3 such that it will give a pulse of "brownout length", i.e. that 
4. Back to state 2.

Visualization:
```
            Beginning of reset pulse (timer 1).
            v
------------.           ,----
             \         /
Reset pulse   \       /
               `-----'
                     ^-- End of reset pulse (timer 1 overflows), start of delay timer (timer 3).
                     |
                     |
                     v
LPC timeline         __________________________________
                     ^               ^             ^
                     |               |             End of LPC glitch window
                     |               |
                     |               Start of LPC glitch window
                     LPC starts to boot

                                    v-- End of delay timer (timer 3 overflows), start of voltage glitch.
good voltage -----------------------.           ,-------------------
                                     \         /
Voltage to LPC11U15                   \       /
                                       \     /
bad voltage                             `---'
                                            ^-- End of voltage glitch.
```

## Glitch Controller Software

Glitch attempts usually fail. For this reason, it is important to 
automate the process, so a glitch can be attempted many times in a short 
time. For this, some software on the main computer is required. This 
software needs to have the following connections:

1. Serial port to the STM32, to start a new glitch attempt.
2. USB port to the STM32, to program new firmware.
3. USB port to the Raspberry Pi Debug Probe for the Serial Wire Debug 
(SWD) connection.

First, the firmware needs to be programmed into the STM32. After that, 
the following procure should be used to glitch:

1. Send glitch command to STM32.
2. Test if the glitch attempt was successful, using the Raspberry Pi 
Debug Probe.
3. If unsuccesful: restart from 1.
4. If successful: dump the firmware of the LPC11U35/toypad.

## Determine timing and duration

### Determine duration of the reset pulse

The duration of the reset pulse is easy: we use a static 1 millisecond. 
Only 50 ns is required according to the datasheet, so 1 ms is more than 
enough. But 1 ms is handy for visualization on the oscilloscope.

### Determine duration of the glitch pulse

The duration of the glitch pulse can be determined by "sweeping" while 
using a non CRP enabled LPC11U35. Try 100 times a duration of 1 CPU 
cycle, try 100 times a duration of 2 CPU cycles, etc. With custom 
firmware, it will be easy to know when a glitch was successful. The 
number of cycles having the most successful glitches, is the best glitch 
pulse width.

Our test environment has no capacitors, the toypad does. Capacitor 
influence how a voltage drops and rises. So the observed optimum glitch 
pulse width in the test environment is only a starting point/minimum for 
the pulse width needed for the toypad.

### Determine the glitch window length

After examining the boot ROM, it was clear that there wasn't a single 
instruction that had to be targeted, but the glitch could happen during 
several (consecutive) instructions. To be precise: from 0x1fff00a8 upto 
and including 0x1fff00c2. Converted from instructions to CPU cycles and 
summing leads to a glitch window of 24 CPU cycles.

Each CPU cyle of the LPC11U35 is 1/12 of a microseconds (since the 
LPC11U35 runs at 12 MHz during the boot ROM). So 24 CPU cycles represent 
a window of 2.0 microseconds.

### Determine the start of the glitch window with respect to end of reset

Our glitch firmware starts the glitch pulse after a predetermined number 
of microseconds. But what should that number be? How long does the 
LPC11U35 take to boot, and arrive at the start of the glitch window? We 
couldn't find a direct answer.

However, we were able to measure it *indirectly*. Visualisation:
```
LPC timeline:


   LPC starts to boot
   |
   |              Start of LPC glitch window       Start of custom pulse
   |              |                                |
   |              |     End of LPC glitch window   |  End of custom pulse, jump to start of LPC glitch window.
   |              |     |                          |  |
   v              v     v                          v  v
   ----------------------------------------------------------------------
   |              |                                   |
   |              |                                   |
   |<------------>|<--------------------------------->|
   |    WANTED               MEASUREMENT#2            |
   |                                                  |
   |<------------------------------------------------>|
                       MEASUREMENT#1
```

Create a custom firmware:
1. Let the firmware give a pulse on a certain pin.
2. Hook up the scope:
   - Channel A to that pin.
   - Channel B to the reset pulse pin.
3. Measure (with said scope) the time between the end of the reset pulse 
(i.e. the start of the LPC11U35) and the end of the custom pulse.
4. Write down this measurement. This is "MEASUREMENT#1" of the 
visualisation.

Rewrite the custom firmware. After the custom pulse, have it jump to the 
boot ROM, to the start of the glitch window. Measure the time from the 
end of the first pulse to the end of the consecutive pulse. Write down 
this measurement. This is "MEASUREMENT#2" of the visualisation.

We can now easily calculate the delay between the end of the reset pulse 
and the start of the glitch window:
```
    delay = MEASUREMENT#1 - MEASUREMENT#2
```

This is the delay that our glitcher firmware must introduce after ending 
the reset pulse. A small sweep range may be needed, since a voltage drop 
is not immediate, but may take a some time/some CPU cycles. (Or quite 
some time if we somehow cannot eliminate large capacitors on the 
toypad.)

Actual measurements:
- MEASUREMENT#1: 75.9 microseconds
- MEASUREMENT#2: 50.4 microseconds

So the delay must be 75.9 - 50.4 = 25.5 microseconds

## Capacitors

The capacitors on the toypad are marked
1. "74B32 106A" (2 pieces) - Tantalum capacitor, 10 micro Farad, 10V max
2. "74B32 475C" (1 piece)  - Tantalum capacitor, 4.7 micro Farad, 16V max

Fall time: It takes around 1 millisecond to drop from 2V8 to 1V1. (Roughly 1 RC time.)
Rise time: It takes around 1 millisecond to rise from 1V1 to 2V8.

# Unsorted

## Unsorted links

- <https://github.com/vekexasia/lpc_voltage_glitch_test>
- <https://arxiv.org/pdf/2108.06131.pdf>
- <https://www.design-reuse.com/articles/48553/how-a-voltage-glitch-attack-could-cripple-your-soc-or-mcu.html>
- <https://forum.newae.com/t/help-getting-lpc1765-glitched/3738>
