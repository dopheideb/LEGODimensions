#!/usr/bin/env python3

## From UM10462 (LPC11Uxx User Manual):
## 
## The reserved ARM Cortex-M0 exception vector location 7 (offset 
## 0x0000001C in the vector table) should contain the 2’s complement of 
## the check-sum of table entries 0 through 6. This causes the checksum 
## of the first 8 table entries to be 0. The bootloader code checksums 
## the first 8 locations in sector 0 of the flash. If the result is 0, 
## then execution control is transferred to the user code.

import sys

if len(sys.argv) != 2:
	print(f'Usage: {sys.argv[0]} cortex_bin_file')
	sys.exit(1)

checksum = 0
filename = sys.argv[1]
with open(filename, "r+b") as f:
	## Compute the checksum.
	for _ in range(7):
		raw = f.read(4)
		val = int.from_bytes(raw, byteorder='little', signed=False)
		checksum += val

	## Compute the 2's complement.
	checksum_2sc = -checksum & 0xFFFFFFFF
	print(f'Computed checksum: 0x{checksum_2sc:08x}')

	## Write to file. Since we have read 7 times 4 bytes, we are 
	## already at offset 0x1C (28 decimal).
	checksum_bytes = checksum_2sc.to_bytes(4, byteorder='little', signed=False)
	f.write(checksum_bytes)

