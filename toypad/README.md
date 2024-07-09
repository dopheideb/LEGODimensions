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

Note that this is a 32-bit value. Only four values out of the possible 
4294967296 give some sort of protection. This means that any of the 
other 4294967292 values mean **NO CODE PROTECTION** at all.

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
  enable_debugging_or_not();
  
  if(WATCHDOG_FLAG_SET || crp_option == CRP_OPTION_3 || crp_option == CRP_OPTION_NO_ISP)
  {
    if(user_code_is_valid())
      execute_internal_user_code()
    else
      /* Not of interest. */
  }
  else
  {
    /* Enter ISP MODE if the correct GPIO pins have the correct logical 
    value. PIO_1 == LOW, PIO_3 == HIGH. */
  }
}
```

What if we could **manipulate** the following test:
- WATCHDOG FLAG SET?
- CRP3/NO_ISP ENABLED?

If we could do that, then we control the boot process!

## Manipulate the boot process

The value of the Code Read Protection is stored in the firmware, in 
location 0x2FC. We don't have the powers of changing the firmware. 
However, we are some knowledge about (micro)controllers, since we have 
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
This is known as a brownout. See 
<https://en.wikipedia.org/wiki/Brownout_(electricity)>.

Delivering decreased power to the LEGO Dimensions toypad seems like a 
doable task. The LPC11U35 used on the toypad is only 5mm by 5mm, too 
small to solder wires to (at least too small for *us*). However, the J2 
header looks more promising.

## The J2 header on the toypad 

The pinout is:

| Pin number  | Connected to LPC11U35 pin | Meaning of LPC11U35 pin
| ----------- | ------------------------: | -----------------------
| 1           | 33                        | `VSS` (i.e. ground)
| 2           | 02                        | not(`RESET`)/`PIO_0`
| 3           | 18                        | `PIO0_9`/**`MOSI0`**/`CT16B0_MAT1` (?but why?)
| 4           | 19                        | **`SWCLK`**/`PIO0_10`/`SCK0`/`CT16B0_MAT2`
| 5           | 25                        | **`SWDIO`**/`PIO0_15`/`AD4`/`CT32B1_MAT2`
| 6<br>&nbsp; | 06<br>29                  | `VDD`<br>&nbsp;

The pins on the header have **no resistance** to counterpart on the 
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
- Atmel AVRs
- MOSFETs (P-channel)

Lab power supplies:
- One power supply to deliver a just enough but very stable voltage.
- One power supply to deliver just too low yet very stable voltage.

2 MOSFETs which will act as switching:
- 1 MOSFET to turn the "barely enough voltage" on and off.
- 1 MOSFET to turn the "just too lowh voltage" on and off.
- We will switch the power *before* the load; we are doing high-side 
switching
- High-side switching: so choose P-channel MOSFET

Atmel AVR:
- Switching. Use the AVR to switch between both voltages, i.e. drive the 
MOSFETs.
- Communication. An AVR can communicate via serial protocol, so we can 
"talk" to the AVR, bidirectionally.
- Timing. An AVR typically runs 20 MHz. We can time the reset, the delay 
before the brownout and the length of the brownout.

## AVR firmware

We think we should build the following state machine for the AVR:
1. Start
2. Blocking: Read serial command "GLITCH \<resetdelay\> \<brownoutlength\>"
3. Reset the LPC11U35X (either via reset pin or by applying 0 voltage)
4. Stop reset procedure.
5. Start brownout delay timer (via Output Compare Register, OCRx)
6. Burn cycles until the Output Compare Flag (OCFx) is set in the TIFRn register.
7. Switch off "barely enough voltage"
8. Switch on "just too low voltage"
9. Deliberately burn a user supplied number CPU cycles
10. Switch off "just too low voltage"
11. Switch on "barely enough voltage"
12. Write serial output: "DONE"
13. Back to state 2.

Atmel AVR130 is an application note called "Setup and Use of AVR 
Timers".

# Unsorted

- <https://github.com/vekexasia/lpc_voltage_glitch_test>
- <https://arxiv.org/pdf/2108.06131.pdf>
- <https://www.design-reuse.com/articles/48553/how-a-voltage-glitch-attack-could-cripple-your-soc-or-mcu.html>
- <https://forum.newae.com/t/help-getting-lpc1765-glitched/3738>
