#!/usr/bin/env python3

## Inspired by:
## * https://wasabifan.github.io/ev3dev.github.io/docs/tutorials/using-lego-dimensions-toy-pad/
## * https://github.com/woodenphone/lego_dimensions_protocol/blob/master/command%20notes/Special/b0.py
## * https://github.com/Ellerbach/LegoDimensions/blob/main/LegoDimensionsProtocol.md

import array
import sys
import time
from   typing import Dict, Final, List, Self
import usb.core
import usb.util

MAGIC_START_BYTE: Final[int] = 0x55

class command:
	@property
	def START(self: Self) -> int:
		return 0xb0
	@property
	def CHANGE_COLOR(self: Self) -> int:
		return 0xb0
	@property
	def CHANGE_COLORS(self: Self) -> int:
		return 0xc8
## Instantiate a command object.
COMMAND = command()

class pad:
	@property
	def ALL(self: Self) -> int:
		return 0
	@property
	def CENTER(self: Self) -> int:
		return 1
	@property
	def LEFT(self: Self) -> int:
		return 2
	@property
	def RIGHT(self: Self) -> int:
		return 3
## Instantiate a pad object.
PAD = pad()

vid_pids =\
[
	## Xbox 360 version.
	##   iManufacturer : "Warner Bros."
	##   iProduct      : "LEGO(R) DIMENSIONS(TM)"
	{
		'idVendor':	0x24c6,
		'idProduct':	0xfa01,
	},

	## Non Xbox 360 version:
	##   iManufacturer : "PDP LIMITED."
	##   iProduct      : "Logic3 LEGO READER V2.10"
	{
		'idVendor':	0x0e6f,
		'idProduct':	0x0241,
	},
]



class Toypad:
	def __init__(self: Self) -> None:
		self.dev = None
		self._is_xbox_version = None
		self.bEndpointAddress = 0x81
		self.wMaxPacketSize = 0x20

	def find(self: Self, poll=None, **kwargs) -> None:
		while True:
			dev = usb.core.find(**kwargs)
			if dev:
				self.dev = dev
				return

			if poll is None:
				raise FileNotFoundError("Could not found toypad.")
			time.sleep(poll)

	@property
	def is_xbox_version(self: Self) -> bool:
		return self._is_xbox_version

	def init(self: Self) -> None:
		if self.dev.is_kernel_driver_active(0):
			## Linux: xpad module somehow claims the Xbox 360 toypad.
			self.dev.detach_kernel_driver(0)
			self.dev.set_configuration()


		## Note: The Xbox 360 version has 4(!) interfaces, all 
		## vendor specific. The PS4/PS4/Wii version has just 1 
		## interface.

		self._must_claim_interface = False
		self._is_xbox_version = False
		for configuration in self.dev:
			for interface in configuration:
				idx = interface.iInterface
				if self._must_claim_interface:
					usb.util.claim_interface(self.dev, interface.bInterfaceNumber)
				str = usb.util.get_string(self.dev, idx)
				if str is None:
					continue

				if str.startswith('Xbox Security Method 3'):
					self._is_xbox_version = True

				for ep_candidate in interface:
					addr = ep_candidate.bEndpointAddress
					if addr == self.bEndpointAddress:
						assert self.wMaxPacketSize == ep.wMaxPacketSize

		self.send_command(
			command=COMMAND.START,
			message_id=0x01,
			payload=bytes('(c) LEGO 2014', 'utf-8')
		)


	def read(self: Self, timeout_ms=None):
		data = None
		try:
			#self.dev.clear_halt(ep.bEndpointAddress)
			data = self.dev.read(
				endpoint=self.bEndpointAddress,
				size_or_buffer=self.wMaxPacketSize,
				timeout=timeout_ms
			)
		except usb.core.USBError as e:
			if e.errno is None or e.errno == 110:
				## Timeout.
				pass
			else:
				raise

		return data

	def send_command(self: Self, command: int, message_id: int, payload: bytes) -> int:
		command_id_payload = bytes([command, message_id]) + payload
		length = len(command_id_payload)
		data = bytes([MAGIC_START_BYTE, length]) + command_id_payload

		checksum = sum(data) & 0xff
		data += bytes([checksum])

		if self.is_xbox_version:
			## The Xbox 360 version needs 2 additional magic 
			## bytes.
			data = bytes([11, 22]) + data

		return self.write(raw_message=data)


	def write(self: Self, raw_message) -> int:
		msg = bytes(raw_message)
		padded_msg = msg.ljust(32, b'\x00')
		return self.dev.write(endpoint=0x01, data=padded_msg)


	def change_color(self: Self, message_id: int, color: List[int], pad: int) -> int:
		return self.send_command(
			command=COMMAND.CHANGE_COLOR,
			message_id=message_id,
			payload=bytes((pad,) + color)
		)

	def change_colors(self: Self, message_id: int, colors: List[List[int]]) -> int:
		(center, left, right) = colors

		payload = bytes(
			((0, 0, 0, 0) if center is None else (1, *center)) +
			((0, 0, 0, 0) if left   is None else (1, *left)) +
			((0, 0, 0, 0) if right  is None else (1, *right))
		)

		return self.send_command(
			command=COMMAND.CHANGE_COLORS,
			message_id=message_id,
			payload=payload,
		)


def main():
	toypad = Toypad()
	print('Waiting for LEGO Dimensions toypad.')
	found = False
	while not found:
		for item in vid_pids:
			try:
				toypad.find(poll=None, **item)
			except FileNotFoundError:
				time.sleep(0.1)
				continue
			found = True

	toypad.init()
	print('Connected!')
	print(f"This toypad is {'' if toypad.is_xbox_version else 'NOT '}an Xbox 360 version.")

	toypad.change_color(
		message_id=42,
		color=(0x08,0x08,0x08),
		pad=PAD.ALL
	)

	count = 0
	while True:
		data = toypad.read(timeout_ms=1000);
		if data is not None:
			count += 1
			print(f"Received data from toypad (EP1IN): {bytes(data).hex(':')}")

			if count == 5:
				center = (0x00, 0x08, 0x00)
				left   = (0x08, 0x00, 0x00)
				right  = (0x00, 0x00, 0x08)
				toypad.change_colors(
					message_id=0x42,
					colors=(center, left, right),
				)


if __name__ == '__main__':
	main()
