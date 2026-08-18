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
        t = TEA(key=key, byteorder='big')
        
        ##             12345678
        block = bytes("a phrase", "utf-8")
        encrypted_block = t.encrypt(block)
        answer = bytes.fromhex('f9 3f 19 64 00 ab e7 59')
        self.assertEqual(encrypted_block, answer)

    def test_encrypt_big_or_little_endian_does_not_matter(self):
        ## We want reproducable results.
        random.seed(42)
        
        ## Create a TEA key: 4 uint32_t values.
        key = [random.randrange(0, 2**32) for _ in range(4)]
        tea_be = TEA(key=struct.pack('>4I', *key), byteorder='big')
        tea_le = TEA(key=struct.pack('<4I', *key), byteorder='little')
	
        msg = [random.randrange(0, 2**32) for _ in range(2)]
        msg_bin_be = struct.pack('>2I', *msg)
        msg_bin_le = struct.pack('<2I', *msg)
        
        msg_enc_be = struct.unpack('>2I', tea_be.encrypt(msg_bin_be))
        msg_enc_le = struct.unpack('<2I', tea_le.encrypt(msg_bin_le))
        self.assertEqual(msg_enc_be, msg_enc_le)



if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    unittest.main()
