#!/usr/bin/env python3

import argparse
import array
import legodimensions
import logging
import struct
import sys
import tea
import time
from   typing import Dict, Final, List, Self
import usb.core
import usb.util



def main():
	## https://github.com/Ellerbach/LegoDimensions/blob/main/LegoDimensionsProtocol.md 
	## sends this packet to the toypad:
	## 
	##     55:02:B3:03:0D:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
	## 
	## The reply from the toypad is (when seed hasn't been altered):
	## 
	##    55:09:03:55:0E:B8:F6:64:71:FC:5D:A0:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
	challenge_str = '55:02:B3:03:0D:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00'
	reply_str     = '55:09:03:55:0E:B8:F6:64:71:FC:5D:A0:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00'

	## Extract the payloads.
	challenge = bytes.fromhex(challenge_str.replace(':', ''))
	challenge_payload = challenge[4:4+8]
	print(f"challenge_payload={challenge_payload.hex(':')}")

	reply = bytes.fromhex(reply_str.replace(':', ''))
	reply_payload = reply[3:3+8]
	print(f"reply_payload={reply_payload.hex(':')}")



	## Redo what the toypad does/computes.

	## Set up TEA (i.e. set the key).
	TEA_key_in_firmware = bytes.fromhex("55 fe f6 30   62 bf 0b c1   c9 b3 7c 34   97 3e 29 fb")
	TEA_key_byteswapped = swap_endianness_per_4_bytes(TEA_key_in_firmware)
	T = tea.TEA(TEA_key_byteswapped)

	## Decrypt the challenge.
	challenge_payload_byteswapped = swap_endianness_per_4_bytes(challenge_payload)
	decrypted_challenge = T.decrypt(challenge_payload_byteswapped)
	decrypted_challenge_byteswapped = swap_endianness_per_4_bytes(decrypted_challenge)

	## FIXME: must allow non-zero seed (and shuffle bits like the 
	## firmware does).
	seed = bytes(4)
	seed_bitshuffled = seed

	v = bytearray(8)
	v[4:8] = decrypted_challenge_byteswapped[0:4]
	v[0:4] = seed_bitshuffled

	v_swapped = swap_endianness_per_4_bytes(v)
	reply_payload_computed = T.encrypt(v_swapped, byteorder='big')
	reply_payload_computed_byteswapped = swap_endianness_per_4_bytes(reply_payload_computed)
	print(f"reply_payload_computed_byteswapped={reply_payload_computed_byteswapped.hex(':')}")



def swap_endianness_per_4_bytes(data: bytes) -> bytes:
	num = len(data) // 4
	return struct.pack(f'>{num}I', *struct.unpack(f'<{num}I', data))


if __name__ == '__main__':
	main()
