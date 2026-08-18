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
		'TEA_KEY': bytes.fromhex('33 ef 82 23 3a 56 08 2f 78 f0 6c 7c 24 6c 37 10'),
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
		'TEA_KEY': bytes.fromhex('66 3a f2 fc 64 0f 96 cb d3 08 9a 58 4d 81 7d 6d'),
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
		'TEA_KEY': bytes.fromhex('a1 f1 00 40 54 6f 19 29 a9 e6 5e d3 04 3b ad ce'),
	},
}

class TestTEA(unittest.TestCase):
	def test_nfc_access_password1(self):
		for tag in tags.values():
			print(tag)
			uid    = tag['UID']
			answer = tag['AUTH_PASSWORD']
			self.assertEqual(Tag(uid=uid).password.hex(':'), answer.hex(':'))

	def test_tea_key1(self):
		for tag in tags.values():
			uid    = tag['UID']
			answer = tag['TEA_KEY']
			self.assertEqual(Tag(uid=uid).tea_key.hex(':'),	answer.hex(':'))

	def test_encrypt1(self):
		for tag in tags.values():
			if not tag['IS_CHARACTER']:
				continue
			id     = tag['ID']
			uid    = tag['UID']
			answer = tag['PAGE_24_25']
			message = Tag(uid=uid).encrypt(lego_dimesions_id=id)
			self.assertEqual(message.hex(':'), answer.hex(':'))



if __name__ == "__main__":
	parser = argparse.ArgumentParser(
		prog='Unit tester for the module legodimenions',
		#description='',
	)
	parser.add_argument('--verbose', '-v', action='store_true', help='Be verbose during testing.')
	args = parser.parse_args()

	if args.verbose:
		logging.basicConfig(level=logging.DEBUG)
	else:
		logging.basicConfig(level=logging.INFO)
	unittest.main()
