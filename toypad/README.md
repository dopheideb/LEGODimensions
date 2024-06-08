# Introduction

We think that the LEGO Dimensions Toy Pad contains the software 
(firmware) to generate the correct password to read the actual LEGO 
Dimension NFC toy tags. This is based upon hints in 
http://www.proxmark.org/forum/viewtopic.php?id=12016 and 
http://www.proxmark.org/forum/viewtopic.php?id=2657.

We were unable to find the firmware online. However, we would love to 
have access to the firmware to confirm the above assumption.

# Opening the toy pad

Beware: It is quite easy to break a plastic clamps while trying to open 
the toy pad. 
https://documents.cdn.ifixit.com/pdf/ifixit/guide_100155_en.pdf has some 
pointers though.

# Microcontrollers inside the toy pad

There are only a few microcontrollers inside the LEGO Dimensions toy 
pad. The USB cable is attached to an NXP LPC11U35. Since that 
microcontroller is a general purpose ARM processor, it is a very good 
candidate to have the firmware to generate the correct key for an NFC 
tag.

# NXP LPC11U35

## Datasheet

Datasheet: https://www.nxp.com/docs/en/data-sheet/LPC11U3X.pdf

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
https://en.wikipedia.org/wiki/Quad_flat_package

## Serial wire debug (SWD)

We had no luck in accessing the LPC11U35 via Serial wire debug. We 
reckon LEGO has disabled SWD via "Code Read Protection" (CRP).

## CRP levels

There are three levels of Code Read Protection

## Circumventing Code Read Protection

Chris Gerlinsky presented his work on Recon Brussels 2017. The 
presentation was named "Breaking Code Read Protection on the NXP LPC 
family Microcontroller". The video is online at https://www.youtube.com/watch?v=98eqp4WmHoQ

His sources are available at 
https://github.com/akacastor/xplain-glitcher

https://www.recon.cx/2017/brussels/talks/breaking_crp_on_nxp.html 
states:
> As glitch attacks go, this is a simple and ‘beginner-level’ attack 
> which should be easily reproducible. The talk will include hardware 
> and software design, including schematics and source code, for a 
> glitcher able to bypass CRP.

## Power supply: 3V3

The datasheet states:
```
Single 3.3 V power supply (1.8 V to 3.6 V)
```
