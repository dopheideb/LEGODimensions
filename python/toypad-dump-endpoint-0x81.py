#!/usr/bin/env python3

'''
Copyright (C) 2026 Bart Dopheide

This program is free software; you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 2 as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program; if not, see <https://www.gnu.org/licenses/>.
'''

## Inspired by:
## * https://wasabifan.github.io/ev3dev.github.io/docs/tutorials/using-lego-dimensions-toy-pad/
## * https://github.com/woodenphone/lego_dimensions_protocol/blob/master/command%20notes/Special/b0.py
## * https://github.com/Ellerbach/LegoDimensions/blob/main/LegoDimensionsProtocol.md

import argparse
import array
import logging
import sys
import time
from   typing import Dict, Final, List, Self
import usb.core
import usb.util



parser = argparse.ArgumentParser(
    prog='LEGO Dimensions toypad reader',
    description='This toolbox aids in accessing the LEGO Dimensions toypad.',
)
parser.add_argument('--verbose', '-v', action='store_true', help='Be more verbose.')
args = parser.parse_args()
if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)
console_handler = logging.StreamHandler()
logfmt = logging.Formatter(fmt='[%(asctime)s %(filename)s->%(funcName)s:%(lineno)s] %(message)s')
logging.getLogger().handlers[0].setFormatter(logfmt)


MAGIC_START_BYTE: Final[int] = 0x55

class command:
	@property
	def START(self: Self) -> int:
		return 0xb0
	@property
	def CHANGE_COLOR(self: Self) -> int:
		return 0xc0
	@property
	def CHANGE_COLORS(self: Self) -> int:
		return 0xc8
	@property
	def READ_PAGE(self: Self) -> int:
		return 0xd2
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

				## We need to "try" the device, because 
				## libusb may have uses cached data 
				## during usb.core.find, i.e. it may 
				## have returned an already disconnected 
				## device.
				if self.is_alive():
					return

			if poll is None:
				raise FileNotFoundError("Could not found toypad.")
			time.sleep(poll)

	def is_alive(self: Self, timeout_ms: int=100) -> bool:
		try:
			self.dev.ctrl_transfer(0x80, 6, (1 << 8), 0, 1, timeout=timeout_ms)
		except usb.core.USBError as e:
			##  5: "[Errno 5] Input/Output Error"
			## 19: "[Errno 19] No such device (it may have been disconnected)"
			## 32: "[Errno 32] Pipe error"
			if e.errno in [5, 19, 32]:
				logging.debug(f"The device is not alive. Error: {e}")
				return False

			## Unknown error.
			raise
		return True

	@property
	def is_xbox_version(self: Self) -> bool:
		return self._is_xbox_version

	def init(self: Self) -> None:
		if self.dev.is_kernel_driver_active(0):
			## Linux: xpad module somehow claims the Xbox 360 toypad.
			self.dev.detach_kernel_driver(0)

		self.dev.reset()
		self.dev.set_configuration()
		logging.debug(
			"iManufacturer=" +
			usb.util.get_string(self.dev, self.dev.iManufacturer)
		)
		logging.debug(
			"iProduct=" +
			usb.util.get_string(self.dev, self.dev.iProduct)
		)
		logging.debug(
			"iSerial=" +
			usb.util.get_string(self.dev, self.dev.iSerialNumber)
		)

		## Note: The Xbox 360 version has 4(!) interfaces, all 
		## vendor specific. The PS4/PS4/Wii version has just 1 
		## interface.

		self._must_claim_interface = False
		self._is_xbox_version = False
		for configuration in self.dev:
			for interface in configuration:
				for ep_candidate in interface:
					addr = ep_candidate.bEndpointAddress
					if addr == self.bEndpointAddress:
						assert self.wMaxPacketSize == ep_candidate.wMaxPacketSize

				idx = interface.iInterface
				if self._must_claim_interface:
					usb.util.claim_interface(self.dev, interface.bInterfaceNumber)

				str = usb.util.get_string(self.dev, idx)
				if str is None:
					continue

				logging.debug("iInterface=" + str)
				if str.startswith('Xbox Security Method 3'):
					self._is_xbox_version = True

		logging.info(f"Sending start command.")
		self.send_command(
			command=COMMAND.START,
			message_id=0x01,
			payload=bytes('(c) LEGO 2014', 'utf-8')
		)


	def read(self: Self, timeout_ms=None):
		try:
			data = self.dev.read(
				endpoint=self.bEndpointAddress,
				size_or_buffer=self.wMaxPacketSize,
				timeout=timeout_ms
			)
		except usb.core.USBError as e:
			if e.errno is None or e.errno == 110:
				## Timeout.
				return None
			else:
				raise
		if not self._is_xbox_version:
			return data
		assert data[0] == 11
		assert data[1] == 22
		return data[2:]

	def send_command(self: Self, command: int, message_id: int, payload: bytes) -> int:
		logging.debug(f"Sending command={command:#04x}, message_id={message_id:#04x}, payload={payload.hex(':')}.")

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
		logging.debug(f"Sending {padded_msg.hex(':')}")
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

	def read_page(self: Self, message_id: int, index: int, page: int) -> array:
		self.send_command(
			command=COMMAND.READ_PAGE,
			message_id=message_id,
			payload=bytes([index, page]),
		)
		data = self.read(timeout_ms=1000);
		logging.debug(f"data={bytes(data).hex(':')}")
		page = data[4:4+16]
		return page



def keeping_reading_toypad_endpoint(toypad):
	count = 0
	while True:
		data = toypad.read(timeout_ms=1000);
		if data is None:
			continue

		logging.info(f"Received data from toypad: {bytes(data).hex(':')}")
		type = data[0]
		if type != 0x56:
			continue

		length = data[1]
		assert length == 0x0b

		pad = data[2]
		status = data[3]
		index = data[4]
		removed = data[5] != 0
		present = data[5] == 0
		uuid = data[6:6+7]
		logging.debug(f"pad={pad}, status={status}, index={index} present={present} uuid={bytes(uuid).hex(':')}")

		page = toypad.read_page(
			message_id=0x77,
			index=index,
			page=0x24,
		)
		logging.debug(f"page={bytes(page).hex(':')}")
		if not present:
			color = (0x08, 0x08, 0x08)
		else:
			if status != 0x00:
				## Unaccepted tag: bright red.
				color = (0xff, 0x00, 0x00)
			else:
				if page[9] == 0x00:
					## Character.
					color = (0x40, 0x00, 0x80)
				else:
					## Vehicle
					color = (0x00, 0x80, 0x40)

		toypad.change_color(
			message_id=0x42,
			color=color,
			pad=pad,
		)



def main():
	toypad = Toypad()

	while True:
		logging.info('Waiting for LEGO Dimensions toypad.')
		found = False
		while not found:
			for item in vid_pids:
				try:
					toypad.find(poll=None, **item)
				except FileNotFoundError:
					time.sleep(0.1)
					continue
				found = True
		logging.info('Connected.')

		toypad.init()
		logging.info('Initialized.')
		logging.debug(f"This toypad is {'' if toypad.is_xbox_version else 'NOT '}an Xbox 360 version.")

		toypad.change_color(
			message_id=0x01,
			color=(0x08,0x08,0x08),
			pad=PAD.ALL
		)

		keeping_reading_toypad_endpoint(toypad)



if __name__ == '__main__':
	## In order to allow a device to disconnect and reconnect, we 
	## must either try to catch all sorts of USB errors at various 
	## methods/functions, or be practical and use 1 catch all. We 
	## tried the former, but it keeps failing. So switch to being 
	## practical then.
	while True:
		try:
			main()
		except usb.core.USBError as e:
			logging.info(f'USB error received. Assuming toypad got disconnected. Error: "{e}"')
		time.sleep(0.1)
