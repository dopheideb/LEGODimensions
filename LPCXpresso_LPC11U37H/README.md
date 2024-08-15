# Introduction

The LEGO Dimensions toypad uses the following microcontroller: NXP 
LPC11U35. We would like to have the boot ROM of such a chip, so we can 
try to manipulate the boot process of the LEGO Dimensions toypad by 
means of glitching.

The LPCXpresso LPC11U37H is a development board with a chip very similar 
to the LPC11U35.

# Assumption(s)

We assume that the boot ROM of the LPC11U35 and LPC11U37H are the same. 
If that assumption turns out to be false, we will assume the boot ROM is 
quit similar in the sense that code read protection checks are done 
after roughly the same number of instructions.

# Acquiring the boot ROM from the LPCXpresso LPC11U37H via USB

The User Manual (UM10462) tells in chapter 20.10 (boot process 
flowchart) that the boot ROM can enumerate the USB device as a Mass 
Storage device, so it can be used for ISP (in system programming). We 
hope we can use this route to read firmware (instead of writing). 
Furthermore, we hope we can read the boot ROM via this USB path.

The LPC11U37H is connected one of the micro USB connectors: the one that 
is closest to the SWD header.

The User Manual also tells us we must set the following pins:
* PIO0_1 = LOW, this means "Enter ISP mode"
* PIO0_3 = HIGH, this means "Enumerate as MSC device to PC"

The schematic of the LPCXpresso LPC11U37H shows switch "SW2" governing 
PIO0_1. The same schematic shows that PIO0_3 is connected to USB_VBUS.

So, in order to a USB debugging, the procedure is:
1. Hook the correct micro USB connector to the laptop/PC/USB master. This ensures PIO0_3 is high.
2. Hold down SW2 (also labeled as "ISP").
3. Temporarily press SW3 (also labeled as "Reset").
4. Release SW2.

Output of the Linux kernel:
```
[Thu Aug 15 21:33:36 2024] usb 3-4: new full-speed USB device number 18 using xhci_hcd
[Thu Aug 15 21:33:36 2024] usb 3-4: New USB device found, idVendor=1fc9, idProduct=000f, bcdDevice= 7.02
[Thu Aug 15 21:33:36 2024] usb 3-4: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[Thu Aug 15 21:33:36 2024] usb 3-4: Product: LPC1XXX IFLASH
[Thu Aug 15 21:33:36 2024] usb 3-4: Manufacturer: NXP
[Thu Aug 15 21:33:36 2024] usb 3-4: SerialNumber: ISP
[Thu Aug 15 21:33:36 2024] usb-storage 3-4:1.0: USB Mass Storage device detected
[Thu Aug 15 21:33:36 2024] scsi host1: usb-storage 3-4:1.0
[Thu Aug 15 21:33:37 2024] scsi 1:0:0:0: Direct-Access     NXP      LPC1XXX IFLASH   1.0  PQ: 0 ANSI: 0 CCS
[Thu Aug 15 21:33:37 2024] sd 1:0:0:0: Attached scsi generic sg2 type 0
[Thu Aug 15 21:33:37 2024] sd 1:0:0:0: [sdc] 260 512-byte logical blocks: (133 kB/130 KiB)
[Thu Aug 15 21:33:37 2024] sd 1:0:0:0: [sdc] Write Protect is off
[Thu Aug 15 21:33:37 2024] sd 1:0:0:0: [sdc] Mode Sense: 03 00 00 00
[Thu Aug 15 21:33:37 2024] sd 1:0:0:0: [sdc] No Caching mode page found
[Thu Aug 15 21:33:37 2024] sd 1:0:0:0: [sdc] Assuming drive cache: write through
[Thu Aug 15 21:33:37 2024]  sdc:
[Thu Aug 15 21:33:37 2024] sd 1:0:0:0: [sdc] Attached SCSI removable disk
```

We DO see a mass storage device! The contents of the disk:
```
-rwx------ 1 root root 131072 Feb  6  2009 firmware.bin
```

That is _exactly_ 128 kiB, the size of the flash of LPC11U37. In other 
words, no boot ROM via this route :-(.
