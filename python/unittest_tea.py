#!/usr/bin/env python3

import logging
import random
import struct
from   tea import TEA
import unittest



def swap_endianness_per_4_bytes(data: bytes) -> bytes:
    num = len(data) // 4
    return struct.pack(f'>{num}I', *struct.unpack(f'<{num}I', data))

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
        encrypted_block = t.encrypt(block, byteorder='big')
        answer = bytes.fromhex('f9 3f 19 64 00 ab e7 59')
        self.assertEqual(encrypted_block, answer)

    def test_encrypt_big_or_little_endian_does_not_matter(self):
        ## We want reproducable results.
        random.seed(42)

        key_be = random.randbytes(16)
        key_le = swap_endianness_per_4_bytes(key_be)
        
        t_be = TEA(key_be)
        t_le = TEA(key_le)
	
        block_be = random.randbytes(8)
        block_le = swap_endianness_per_4_bytes(block_be)
        #key = bytes.fromhex("ef be ad de   be ba fe ca   ed fe 0b b0   dc ac de c0")

        encrypted_block_le = t_le.encrypt(block_le, byteorder='little')
        encrypted_block_be = t_be.encrypt(block_be, byteorder='big')
        self.assertEqual(encrypted_block_le, swap_endianness_per_4_bytes(encrypted_block_be))



if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    unittest.main()
