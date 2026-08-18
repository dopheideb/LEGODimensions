#!/usr/bin/env python3

import argparse
import logging
from   legodimensions import Tag
import unittest

tags =\
{
	'Wyldstyle':
	{
		'NAME': 'Wyldstyle',
		'IS_CHARACTER': True,
		'GENUINE': True,
		'ID': 3,
		'UID': bytes.fromhex('04:13:BB:1A:99:40:80'.replace(':', '')),
		'PAGE_24_25': bytes.fromhex('01 39 ED 60   E4 BE 30 7C'),
		'AUTH_PASSWORD': bytes.fromhex('4b ef 36 21'),
		'TEA_KEY': bytes.fromhex('23 82 ef 33   2f 08 56 3a   7c 6c f0 78   10 37 6c 24'),
	},

	'BMO':
	{
		'NAME': 'BMO',
		'IS_CHARACTER': False,
		'GENUINE': True,
		'ID': 1173,
		'UID': bytes.fromhex('04:D9:C8:DA:A2:40:80'.replace(':', '')),
		'PAGE_24_25': bytes.fromhex('95 04 00 00   00 00 00 00'),
		'AUTH_PASSWORD': bytes.fromhex('51 91 01 d0'),
		'TEA_KEY': bytes.fromhex('fc f2 3a 66   cb 96 0f 64   58 9a 08 d3   6d 7d 81 4d'),
	},

	'Supergirl':
	{
		'NAME': 'Supergirl',
		'IS_CHARACTER': True,
		'GENUINE': False,
		'ID': 46,
		'UID': bytes.fromhex('04:58:E4:52:25:20:91'.replace(':', '')),
		'PAGE_24_25': bytes.fromhex('4B 63 B1 08   8F 63 8A 8D'),
		'AUTH_PASSWORD': bytes.fromhex('85 8d 20 9b'),
		'TEA_KEY': bytes.fromhex('40 00 f1 a1   29 19 6f 54   d3 5e e6 a9   ce ad 3b 04'),
	},
}

class TestLEGODimensions(unittest.TestCase):
	def test_nfc_access_password1(self):
		for tag in tags.values():
			uid    = tag['UID']
			answer = tag['AUTH_PASSWORD']
			ldtag = Tag(uid=uid)
			self.assertEqual(ldtag.password.hex(':'), answer.hex(':'))

	def test_tea_key1(self):
		for tag in tags.values():
			uid    = tag['UID']
			answer = tag['TEA_KEY']
			ldtag = Tag(uid=uid)
			self.assertEqual(ldtag.tea_key.hex(':'), answer.hex(':'))

	def test_encrypt_decrypt1(self):
		for tag in tags.values():
			if not tag['IS_CHARACTER']:
				continue
			id     = tag['ID']
			uid    = tag['UID']
			answer = tag['PAGE_24_25']
			ldtag = Tag(uid=uid)
			encrypted = ldtag.encrypt(lego_dimesions_id=id)
			self.assertEqual(encrypted.hex(':'), answer.hex(':'))

			## Check if the message is the same after encrypting+decrypting.
			decrypted = ldtag.decrypt(encrypted)
			self.assertEqual(
				decrypted.hex(':'),
				(id.to_bytes(length=4, byteorder='little') * 2).hex(':'),
			)



if __name__ == "__main__":
	parser = argparse.ArgumentParser(
		prog='Unit tester for the legodimenions module',
		#description='',
	)
	parser.add_argument('--verbose', '-v', action='store_true', help='Be verbose during testing.')
	args = parser.parse_args()

	if args.verbose:
		logging.basicConfig(level=logging.DEBUG)
	else:
		logging.basicConfig(level=logging.INFO)
	unittest.main()
