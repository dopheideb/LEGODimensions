#!/usr/bin/env python3

import argparse
import logging
from   legodimensions import Tag
import unittest

class TestTEA(unittest.TestCase):
	def test_nfc_access_password1(self):
		uid = bytes.fromhex("04 13 BB 1A 99 40 80")
		password = bytes.fromhex("4b ef 36 21")
		self.assertEqual(Tag(uid=uid).password, password)

	def test_nfc_access_password2(self):
		uid = bytes.fromhex("00 11 22 33 44 55 66")
		password = bytes.fromhex("94 28 cc fc")
		self.assertEqual(Tag(uid=uid).password, password)

	def test_nfc_access_password3(self):
		uid = bytes.fromhex("00 00 00 00 00 00 00")
		password = bytes.fromhex("0b ac db dd")
		self.assertEqual(Tag(uid=uid).password, password)

	def test_tea_key1(self):
		uid = bytes.fromhex("04 13 BB 1A 99 40 80")
		tea_key = bytes.fromhex("33 ef 82 23 3a 56 08 2f 78 f0 6c 7c 24 6c 37 10")
		self.assertEqual(Tag(uid=uid).tea_key, tea_key)

	def test_tea_key2(self):
		uid = bytes.fromhex("00 11 22 33 44 55 66")
		tea_key = bytes.fromhex("3a a9 12 01 87 e6 15 b8 55 ba 87 9c 74 a6 de 65")
		self.assertEqual(Tag(uid=uid).tea_key, tea_key)

	def test_tea_key3(self):
		uid = bytes.fromhex("00 00 00 00 00 00 00")
		tea_key = bytes.fromhex("74 14 98 30 90 0c 02 08 74 8c cc cc 0a fe e5 d6")
		self.assertEqual(Tag(uid=uid).tea_key, tea_key)


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
