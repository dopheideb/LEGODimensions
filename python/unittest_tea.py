#!/usr/bin/env python3

import logging
from   tea import TEA
import unittest

class TestTEA(unittest.TestCase):
    def test_magic_constants(self):
        self.assertEqual(TEA().delta, 0x9E3779B9)
    
    def test_short_key(self):
        for n in range(16):
            self.assertRaises(ValueError, TEA, bytes(n))
    
    def test_long_key(self):
        for n in range(17, 32):
            self.assertRaises(ValueError, TEA, bytes(n))
    
    def test_read_key(self):
        key = bytes.fromhex("deadbeefcafebabeb00bfeedc0deacdc")
        self.assertEqual(key, TEA(key).key)
    
    def test_encrypt(self):
        key = bytes.fromhex("deadbeefcafebabeb00bfeedc0deacdc")
        t = TEA(key)
        
        ##             12345678
        block = bytes("a phrase", "utf-8")
        encrypted_block = t.encrypt(block)
        self.assertEqual(encrypted_block, b'\xf9?\x19d\x00\xab\xe7Y')



if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    unittest.main()
