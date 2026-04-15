#!/usr/bin/env python3

import logging
from   legodimensions import Tag
import unittest

class TestTEA(unittest.TestCase):
	def test_nfc_access_password(self):
		uid_int = 0x0413BB1A994080
		uid_bytes = uid_int.to_bytes(7)

		password_int = 0x4bef3621
		password_bytes = password_int.to_bytes(4)

		self.assertEqual(Tag(uid=uid_bytes).password, password_bytes)

	def test_tea_key(self):
		uid_int = 0x0413BB1A994080
		uid_bytes = uid_int.to_bytes(7)

		tea_key_int = 0x33ef82233a56082f78f06c7c246c3710
		tea_key_bytes = tea_key_int.to_bytes(16)

		self.assertEqual(Tag(uid=uid_bytes).tea_key, tea_key_bytes)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    unittest.main()
