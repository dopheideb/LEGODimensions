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



seed = bytes(16)
TEA_KEY_in_firmware = bytes.fromhex("55 fe f6 30   62 bf 0b c1   c9 b3 7c 34   97 3e 29 fb")
TEA_KEY             = bytes.fromhex("30 f6 fe 55   c1 0b bf 62   34 7c b3 c9   fb 29 3e 97")

def main():
	global seed
	print(f"[main] seed={seed.hex(':')}")

	## https://github.com/Ellerbach/LegoDimensions/blob/main/LegoDimensionsProtocol.md 
	## sends this packet to the toypad:
	## 
	##     55:02:B3:03:0D:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
	## 
	## The reply from the toypad is (when seed hasn't been changed by the host):
	## 
	##    55:09:03:55:0E:B8:F6:64:71:FC:5D:A0:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00
	challenge_str = '55:02:B3:03:0D:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00'

	## Extract the payloads.
	challenge = bytes.fromhex(challenge_str.replace(':', ''))
	challenge_payload = challenge[4:4+8]
	print(f"challenge_payload={challenge_payload.hex(':')}")

	reply_str = None
	if challenge_payload == bytes.fromhex("0d:00:00:00:00:00:00:00".replace(':', '')):
		if seed == bytes(16):
			reply_str = '55:09:03:55:0E:B8:F6:64:71:FC:5D:A0:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00'
		elif seed == bytes.fromhex("0f:ea:1d:29:7e:da:84:b2:29:b2:b5:7b:35:39:ab:c9".replace(':', '')):
			reply_str = '55:09:03:e1:0d:9c:20:c1:6f:1f:91:eb:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00'
	if reply_str is None:
		raise NotImplementedError("Unknown challenge-seed combination")

	reply = bytes.fromhex(reply_str.replace(':', ''))
	reply_payload = reply[3:3+8]
	print(f"reply_payload={reply_payload.hex(':')}")


	## 
	## Redo what the toypad does/computes.
	## 
	## Set up TEA.
	global TEA_KEY
	T = tea.TEA(TEA_KEY)

	## Decrypt the challenge.
	challenge_payload_byteswapped = swap_endianness_per_4_bytes(challenge_payload)
	decrypted_challenge = T.decrypt(challenge_payload_byteswapped)
	decrypted_challenge_byteswapped = swap_endianness_per_4_bytes(decrypted_challenge)

	seed_excerpt = toypad_shuffle(seed)[12:16]

	v = bytearray(8)
	v[4:8] = decrypted_challenge_byteswapped[0:4]
	v[0:4] = seed_excerpt

	v_swapped = swap_endianness_per_4_bytes(v)
	reply_payload_computed = T.encrypt(v_swapped, byteorder='big')
	reply_payload_computed_byteswapped = swap_endianness_per_4_bytes(reply_payload_computed)
	print(f"reply_payload_computed_byteswapped={reply_payload_computed_byteswapped.hex(':')}")

	print(f"Are the computed reply payload and the actual reply payload the same? {reply_payload_computed_byteswapped == reply_payload}")



def swap_endianness_per_4_bytes(data: bytes) -> bytes:
	num = len(data) // 4
	return struct.pack(f'>{num}I', *struct.unpack(f'<{num}I', data))

def toypad_shuffle(data: bytes) -> bytes:
	## The firmware uses the data as 4 dword, and ARM is little 
	## endian.
	dword = list(struct.unpack('<4I', data))

	## Ghidra says a cast to 16 bit is used (ushort), assembly shows 
	## ldrh (load register half word), but only for computing the 
	## dword[1] << 21 part. Well, the upper 16 bits are shifted away 
	## anyway, so it seems a compiler optimization. We'll use a 
	## proper rotate left for readability.
	temp = 0xffffffff & (
		dword[0] - (_rotate_left_dword(dword[1], 21))
	)
	dword[0] = 0xffffffff & (
		_rotate_left_dword(dword[2], 19)
		^
		dword[1]
	)
	dword[1] = 0xffffffff & (_rotate_left_dword(dword[3], 6) + dword[2])
	dword[2] = 0xffffffff & (dword[3] + temp)
	dword[3] = 0xffffffff & (dword[0] + temp)

	return struct.pack('<4I', *dword)

def toypad_scramble(data: bytes) -> bytes:
	result = bytearray(16)
	## "f1ea 5eed" is in the firmware.
	result[ 0: 0+4] = struct.pack('<I', 0xf1ea5eed)
	result[ 4: 4+4] = data
	result[ 8: 8+4] = data
	result[12:12+4] = data
	print(f"[toypad_scramble] start_data={result.hex(':')}")

	for n in range(42):
		result = toypad_shuffle(result)
		print(f"[toypad_scramble] result={result.hex(':')} (n={n})")

	return result

def _rotate_left(n: int, rotations: int=1, width: int=8) -> int:
	part1 = n << (rotations % width)
	part2 = n >> ((width - rotations) % width)
	return (part1 | part2) & ((1 << width) - 1)

def _rotate_left_dword(dword: int, rotations: int) -> int:
	dword_size = 32
	return _rotate_left(dword, rotations, width=dword_size)

def set_seed(payload: bytes) -> None:
	global seed

	print(f"[set_seed] payload={payload.hex(':')}")
	## The host encrypted the payload with the common TEA key, so 
	## decrypt the message.
	v = swap_endianness_per_4_bytes(payload)
	T = tea.TEA(TEA_KEY)
	decrypted = T.decrypt(v)
	v0 = swap_endianness_per_4_bytes(decrypted[0:4])
	v1 = swap_endianness_per_4_bytes(decrypted[4:8])
	print(f"[set_seed] decrypted v[0]={v0.hex(':')}")
	print(f"[set_seed] decrypted v[1]={v1.hex(':')}")

	scrambled = toypad_scramble(v0)
	#scrambled = toypad_scramble(decrypted[0:4])
	seed = scrambled

	if payload == bytes.fromhex("de ad be ef   ca fe b0 0b"):
		assert seed == bytes.fromhex("0f ea 1d 29   7e da 84 b2   29 b2 b5 7b   35 39 ab c9")
		print(f"[set_seed] seed confirmed to be GOOD.")

	reply_unencrypted = scrambled[4:8] + bytes(4)
	reply_encrypted = T.encrypt(reply_unencrypted)



if __name__ == '__main__':
	#if True:
	#	set_seed(bytes.fromhex("de ad be ef   ca fe b0 0b"))
	main()
