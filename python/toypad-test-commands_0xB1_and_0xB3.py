#!/usr/bin/env python3

import argparse
import asyncio
import legodimensions
import logging
import random
from   tea import TEA
import time
import toypad as _toypad
import usb

NUM_CHALLENGES_PER_SEED = 25

parser = argparse.ArgumentParser(
	prog='LEGO Dimensions toypad command 0xB1 and 0xB3 tester',
	description='This program demonstrates the B1 and B3 commands, and implements what the toypad firmware does with those commands.',
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



async def application_loop(toypad: _toypad.Toypad):
	byteorder = 'little'
	while True:
		tea = TEA(key=_toypad.RNG.TEA_KEY, byteorder=byteorder)
		rng = _toypad.RNG()

		encrypted_seed = random.randbytes(8)
		logging.debug(f"encrypted_seed={encrypted_seed.hex(':')}")

		logging.info("(Re)initializing the toypad's RNG.")
		await toypad.init_rng(
			seed=encrypted_seed,
			encrypted=True,
		)
		rng.seed(seed=encrypted_seed)
		logging.debug(f"rng.state={rng.state.hex(':')}")

		for n in range(NUM_CHALLENGES_PER_SEED):
			challenge_message_id = random.randrange(256)
			challenge_payload = random.randbytes(8)
			challenge_reply = await toypad.send_command(
				command=_toypad.COMMAND.CHALLENGE,
				message_id=challenge_message_id,
				payload=challenge_payload,
			)
			logging.debug(f"challenge_reply={challenge_reply.hex(':')}")

			actual_reply_payload = challenge_reply[3:3+8]

			## Redo what the firmware does.
			decrypted = tea.decrypt(challenge_payload)
			computed_reply_payload_unencrypted =\
				rng.random() + decrypted[0:4]
			logging.debug(f"computed_reply_payload_unencrypted={computed_reply_payload_unencrypted.hex(':')}")
			computed_reply_payload =\
				tea.encrypt(computed_reply_payload_unencrypted)

			logging.debug(f"actual_reply_payload={actual_reply_payload.hex(':')}")
			logging.debug(f"computed_reply_payload={computed_reply_payload.hex(':')}")
			assert actual_reply_payload.hex(':') == computed_reply_payload.hex(':')
			logging.info(f"[{n}/{NUM_CHALLENGES_PER_SEED}] Computed and actual response match.")



async def main():
	## We want reproducable result, so fix the seed.
	random.seed(a=42)

	toypad = _toypad.Toypad()

	logging.info('Waiting for LEGO Dimensions toypad.')
	found = False
	while not found:
		for item in _toypad.vid_pids:
			try:
				toypad.find(poll=None, **item)
			except FileNotFoundError:
				time.sleep(0.1)
				continue
			found = True
	logging.info('Connected.')

	toypad.init()
	logging.info('Initialized.')
	try:
		async with asyncio.TaskGroup() as group:
			reader_task = await toypad.start(group)
			application_task = group.create_task(
				application_loop(toypad),
				name="application",
			)
			await reader_task
	finally:
		await toypad.stop()


if __name__ == '__main__':
	while True:
		try:
			asyncio.run(main())
		except* usb.core.USBError as error:
			logging.info(f'USB error received. Assuming toypad got disconnected. Error: "{error}"')
		time.sleep(0.1)
