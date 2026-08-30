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
import asyncio
import legodimensions
import logging
import random
import struct
import sys
from   tea import TEA
import time
from   typing import Dict, Final, List, Self
import usb.core
import usb.util
import utils



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
## The following TEA key was found in the Xbox 360 toypad firmware, 
## offset 0x928.
SEED_TEA_KEY = bytes.fromhex('55 fe f6 30   62 bf 0b c1   c9 b3 7c 34   97 3e 29 fb')

class command:
    @property
    def CHALLENGE(self: Self) -> int:
        return 0xb3
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
    def INIT_SEED(self: Self) -> int:
        return 0xb1
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

        self.reader_task = None
        self.rng_state = bytes(16)

        ## Example of events: placing a tag, removing a tag.
        self.events = asyncio.Queue()

        self.response_futures = {}

    def find(self: Self, poll=None, **kwargs) -> None:
        while True:
            dev = usb.core.find(**kwargs)
            if dev:
                self.dev = dev

                ## We need to "try" the device, because libusb may have 
                ## uses cached data during usb.core.find, i.e. it may 
                ## have returned an already disconnected device.
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

    async def _reader(self: Self) -> None:
        timeout_ms = 500
        while True:
            logging.debug("Alive")
            message = None
            try:
                message = bytes(await asyncio.to_thread(
                    self.dev.read,
                    ## Arguments:
                    endpoint=self.bEndpointAddress,
                    size_or_buffer=self.wMaxPacketSize,
                    timeout=timeout_ms,
                ))
                logging.debug(f"message={message.hex(':')}")
            except usb.core.USBError as e:
                if e.errno is None or e.errno == 110:
                    ## Timeout.
                    message = None
                else:
                    raise

            if message is None:
                continue

            ## The toypad sometimes returns an empty packet.
            if len(message) == 0:
                return None

            if not self._is_xbox_version:
                return message
            ## This is an Xbox version. It has 2 additional bytes, at 
            ## the start. Check them, remove them.
            assert message[0:2] == b"\x0b\x16"
            message = message[2:]

            if message[0] == 0x56:
                ## This is an event message, not a reply to a command.
                await self.events.put(message)
                continue

            if message[0] == 0x55:
                ## This is a reply to a command message.
                message_id = message[2]
                future = self.response_futures[message_id]
                if not future.done():
                    future.set_result(message)
                continue

            logging.error(f"Unhandled message received: {message.hex(':')}")



    async def start(self: Self, task_group: asyncio.TaskGroup) -> asyncio.Task:
        if self.reader_task is not None:
            raise RuntimeError("Toypad is already started.")

        ## Start reading the IN endpoint.
        self.reader_task = task_group.create_task(
            self._reader(),
            name="Toypad reader",
        )

        logging.info(f"Sending start command.")
        message_id = 0x00
        reply = await self.send_command(
            command=COMMAND.START,
            message_id=message_id,
            payload=bytes('(c) LEGO 2014', 'utf-8'),
        )

        logging.debug(f"reply={bytes(reply).hex(':')}")
        if reply[0] != 0x55:
            raise ValueError(f"Wrong start byte received. Expected 0x55, got 0x{reply[0]:02x}.")
        if reply[1] != 0x19:
            raise ValueError(f"Wrong count byte received. Expected 0x19, got 0x{reply[1]:02x}.")
        if reply[2] != message_id:
            raise ValueError(f"Wrong message id byte received. Expected 0x{message_id:02x}, got 0x{reply[2]:02x}.")
        logging.debug(f"Static bytes: {bytes(reply[5:11]).hex(':')}")
        logging.debug(f"LPC device UID (part 0): {bytes(reply[23:27]).hex(':')}")
        logging.debug(f"LPC device UID (part 1): {bytes(reply[19:23]).hex(':')}")
        logging.debug(f"LPC device UID (part 2): {bytes(reply[15:19]).hex(':')}")
        logging.debug(f"LPC device UID (part 3): {bytes(reply[11:15]).hex(':')}")
        if reply[3] != 0x00:
            raise ValueError(f"Toypad says our start command was wrong.")

        return self.reader_task

    async def stop(self: Self):
        if self.reader_task is None:
            return

        self.reader_task.cancel()
        await asyncio.gather(
            self.reader_task,
            return_exceptions=True,
        )
        self.reader_task = None



    async def send_command(self: Self, command: int, payload: bytes, message_id: int|None=None) -> bytes:
        if message_id is None:
            message_id = random.randrange(256)
        logging.debug(f"Sending command={command:#04x}, message_id={message_id:04x}, payload={payload.hex(':')}.")

        command_id_payload = bytes([command, message_id]) + payload
        length = len(command_id_payload)
        data = bytes([MAGIC_START_BYTE, length]) + command_id_payload

        checksum = sum(data) & 0xff
        data += bytes([checksum])

        if self.is_xbox_version:
            ## The Xbox 360 version needs 2 additional magic 
            ## bytes.
            data = bytes([11, 22]) + data

        ## Per USB standard, the toypad must respond to our (upcoming) 
        ## OUT message with an URB_COMPLETE message (no data part). The 
        ## toypad is simply not allowed to return bytes. But the toypad 
        ## wants to give feedback and does so by eventually responding 
        ## to an IN message.
        ## 
        ## However, regular events (placing/removing tag) are also IN 
        ## message. Hmm. Handles this mess with asyncio then. Just wait 
        ## for the reader saying it has found a reply for us.
        timeout = 5.0
        response_future = asyncio.get_running_loop().create_future()
        self.response_futures[message_id] = response_future

        self.write(raw_message=data)
        try:
            reply = await asyncio.wait_for(response_future, timeout=timeout)
        finally:
            del self.response_futures[message_id]
        return reply



    def write(self: Self, raw_message) -> int:
        """ Write a raw message.

        Write a raw message to the toypad. The message will be zero 
        padded automatically and then be send to endpoint 1 of the toypad.
        """
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

    async def init_rng(self: Self, seed: bytes, encrypted=False) -> None:
        if len(seed) != 8:
            raise ValueError("The seed must be 8 bytes, whether encrypted or not.")

        tea = TEA(SEED_TEA_KEY, byteorder='little')
        message_id = random.randrange(256)

        if encrypted:
            decrypted_seed = tea.decrypt(seed)
            encrypted_seed = seed
        else:
            decrypted_seed = seed
            encrypted_seed = tea.encrypt(seed)

        reply = await self.send_command(
            command=COMMAND.INIT_SEED,
            message_id=message_id,
            payload=encrypted_seed,
        )
        reply_payload = bytes(reply[3:11])
        decrypted = tea.decrypt(reply_payload)
        logging.debug(f"reply_payload={reply_payload.hex(':')}")
        logging.debug(f"decrypted={decrypted.hex(':')}")
        if decrypted[0:4] != decrypted_seed[4:8]:
            raise ValueError(f"Toypad did not decrypt and encrypt our message correctly.")
        logging.debug(f"Decrypted message matches the input.")

    async def read_page(self: Self, index: int, page: int) -> array:
        reply = await self.send_command(
            command=COMMAND.READ_PAGE,
            message_id=random.randrange(256),
            payload=bytes([index, page]),
        )
        logging.debug(f"reply={bytes(reply).hex(':')}")
        page = reply[4:4+16]
        return page



class RNG:
    """ Implementation of RNG routines of the LEGO Dimensions toypad.

    The RNG is used while handling the commands 0xB1, 0xB3 and 0xB4.
    """
    ## This TEA key was found in the Xbox 360 toypad firmware, offset 
    ## 0x928.
    TEA_KEY: Final[bytes]=\
        bytes.fromhex('55 fe f6 30   62 bf 0b c1   c9 b3 7c 34   97 3e 29 fb')

    def __init__(self: Self, seed: bytes|None=None) -> None:
        self.state = bytes(16)
        self.byteorder = 'little'
        if seed is not None:
            self.seed(seed)

    @property
    def state(self: Self) -> bytes:
        """ Get the internal state.

        This member was mainly added for diagnostic purposes. It isn't 
        useful outside debugging/troubleshooting.
        """
        return self._state

    @state.setter
    def state(self: Self, state: bytes) -> None:
        """ Set the internal state directly.

        The toypay has no way to set the staate directly, so this member 
        was mainly added for diagnostic purposes. Use with caution, use 
        the seed() member if possible.
        """
        if len(state) != 16:
            raise ValueError(f"The state must be 128 bit (16 bytes), not {len(state)} bytes.")
        self._state = state
        logging.debug(f"self._state={self._state.hex(':')}")

    def seed(self: Self, seed: bytes, encrypted: bool=True) -> None:
        if not encrypted:
            if len(seed) != 4:
                raise ValueError("The unencrypted seed must be 4 bytes long.")
            ## No need to decrypt the seed.
        else:
            if len(seed) != 8:
                raise ValueError("The seed must be 8 bytes long.")

            tea = TEA(self.TEA_KEY, byteorder=self.byteorder)
            seed = tea.decrypt(seed)[0:4]

        ## "f1ea 5eed" is actually in the firmware. It is 
        ## leetspeak for "flea seed".
        length: Final[int] = 4
        s0 = 0xf1ea5eed.to_bytes(length, self.byteorder)
        s1 = seed
        s2 = seed
        s3 = seed
        self.state = s0 + s1 + s2 + s3

        for _ in range(42):
            self.random()


    def random(self: Self) -> bytes:
        """ Get 4 pseudo random bytes.

        random() update the 16 byte internal state and returns the last 
        four bytes of the updated state.

        The update uses ARX operations:
          - A: Adding (subtracting)
          - R: Rotating
          - X: Xorring
        """

        ## The firmware does all the operation using 32-bit unsigned 
        ## integers, so convert the state to uints/dwords.
        s0 = int.from_bytes(self.state[ 0: 4], self.byteorder)
        s1 = int.from_bytes(self.state[ 4: 8], self.byteorder)
        s2 = int.from_bytes(self.state[ 8:12], self.byteorder)
        s3 = int.from_bytes(self.state[12:16], self.byteorder)

        ## Rotating and adding (subtraction).
        temp = 0xffffffff & (
            s0
            -
            utils.rotate_left_dword(dword=s1, n=21)
        )

        ## Rotating and xorring.
        s0 = 0xffffffff & (
            s1
            ^
            utils.rotate_left_dword(dword=s2, n=19)
        )

        ## Rotating and adding.
        s1 = 0xffffffff & (
            s2
            +
            utils.rotate_left_dword(dword=s3, n=6)
        )

        ## Adding.
        s2 = 0xffffffff & (s3 + temp)
        s3 = 0xffffffff & (s0 + temp)

        self.state = (
            s0.to_bytes(length=4, byteorder=self.byteorder)
            +
            s1.to_bytes(length=4, byteorder=self.byteorder)
            +
            s2.to_bytes(length=4, byteorder=self.byteorder)
            +
            s3.to_bytes(length=4, byteorder=self.byteorder)
        )
        ## Return the last four bytes.
        return self.state[12:16]


def keeping_reading_toypad_endpoint(toypad):
    count = 0
    while True:
        data = toypad.read(timeout_ms=1000)
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

        #def swap_endianness_32(data32: bytes) -> bytes:
        #	return int.from_bytes(bytes=data32, byteorder='little').to_bytes(length=4, byteorder='big', signed=False)
        def swap_endianness_per_4_bytes(data: bytes) -> bytes:
            num = len(data) // 4
            return struct.pack(f'>{num}I', *struct.unpack(f'<{num}I', data))


        ## Set seed.
        payload=bytes.fromhex("de ad be ef   ca fe b0 0b")
        logging.warning(f"Setting seed with payload={payload.hex(':')}")
        toypad.send_command(
            command=0xb1,
            message_id=0x04,
            payload=payload,
        )
        reply = toypad.read(timeout_ms=500);
        logging.warning(f"reply={bytes(reply).hex(':')}")



        TEA_key_in_firmware = bytes.fromhex("55 fe f6 30   62 bf 0b c1   c9 b3 7c 34   97 3e 29 fb")
        TEA_key_byteswapped = swap_endianness_per_4_bytes(TEA_key_in_firmware)
        T = tea.TEA(TEA_key_byteswapped)
        data = toypad.read(timeout_ms=25);

        payload=bytes.fromhex("0d 00 00 00   00 00 00 00")
        logging.warning(f"Sending payload={payload.hex(':')}")
        toypad.send_command(
            command=0xb3,
            message_id=0x03,
            payload=payload,
        )
        reply = toypad.read(timeout_ms=500);
        logging.warning(f"reply={bytes(reply).hex(':')}")

        #reply = bytes.fromhex("55 09 03 55 0e b8 f6 64 71 fc 5d a0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00")
        #enc = reply[3:3+8]

        enc = swap_endianness_per_4_bytes(payload)
        logging.warning(f"enc={enc.hex(':')}")

        dec = T.decrypt(enc, byteorder='big')
        x = swap_endianness_per_4_bytes(dec)
        logging.warning(f"dec (swapped)={x.hex(':')}")
        v = bytearray(8)
        v[4:8] = x[0:4]
        v[0:4] = bytes(4)	## FIXME: must use non-zero seed and shuffle bits.
        logging.warning(f"v={v.hex(':')}")
        v_swapped = swap_endianness_per_4_bytes(v)
        logging.warning(f"v (swapped)={v_swapped.hex(':')}")
        result = T.encrypt(v_swapped, byteorder='big')
        logging.warning(f"result={result.hex(':')}")
        logging.warning(f"result (swapped)={swap_endianness_per_4_bytes(result).hex(':')}")

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
