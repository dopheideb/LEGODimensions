#!/usr/bin/env python3

import logging
import math
import numpy as np
import struct
import sys
from   typing import Self



'''
TEA stands for "Tiny Encryption Algorithm". 
https://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm (on 2024-02-13) 
writes this about TEA:

    In cryptography, the Tiny Encryption Algorithm (TEA) is a block 
    cipher notable for its simplicity of description and implementation, 
    typically a few lines of code. It was designed by David Wheeler and 
    Roger Needham of the Cambridge Computer Laboratory; it was first 
    presented at the Fast Software Encryption workshop in Leuven in 
    1994, and first published in the proceedings of that workshop.[4]

    The cipher is not subject to any patents.

TEA is used in LEGO Dimensions, amongst others. The usage of TEA in LEGO 
Dimensions motivated us to implement TEA in Python.

The reference code (also on Wikipedia):

    #include <stdint.h>
    
    void encrypt (uint32_t v[2], const uint32_t k[4]) {
        uint32_t v0=v[0], v1=v[1], sum=0, i;           /* set up */
        uint32_t delta=0x9E3779B9;                     /* a key schedule constant */
        uint32_t k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */
        for (i=0; i<32; i++) {                         /* basic cycle start */
            sum += delta;
            v0 += ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
            v1 += ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
        }                                              /* end cycle */
        v[0]=v0; v[1]=v1;
    }
    
    void decrypt (uint32_t v[2], const uint32_t k[4]) {
        uint32_t v0=v[0], v1=v[1], sum=0xC6EF3720, i;  /* set up; sum is (delta << 5) & 0xFFFFFFFF */
        uint32_t delta=0x9E3779B9;                     /* a key schedule constant */
        uint32_t k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */
        for (i=0; i<32; i++) {                         /* basic cycle start */
            v1 -= ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
            v0 -= ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
            sum -= delta;
        }                                              /* end cycle */
        v[0]=v0; v[1]=v1;
    }

'''
class TEA:
    """ TEA - Tiny Encryption Algorithm
    
    This is an implementation of the Tiny Encryption Algorithm.
    
    Note that the actual TEA has no binary key nor does the 
    encryption/decryption operate on binary messages. TEA uses 32-bit 
    integers, and is therefore byteorder indepent.
    
    This module does, however, use the bytes type for the TEA key and 
    for the input of encrypt()/decrypt(). Why? Implementation is easier.
    
    If you need to convert a TEA key to bytes, try this:
        import struct
        key = (0xdeadbeef, 0xcafebabe, 0xb00cc0de, 0xfeedacdc)

        ## Little endian version:
        tea_LE = TEA(key=struct.pack('<4I', *key), byteorder='little')

        ## Big endian version:
        tea_BE = TEA(key=struct.pack('>4I', *key), byteorder='big')
    """

    def __init__(
            self: Self,
            key: bytes=bytes(16),
            byteorder: str=sys.byteorder,
    ):
        """ Initialize self.
        
        Keyword arguments:
            - key: the 16 bytes TEA key to use for encryption and/or 
              decryption.
            - byteorder: Set the byteorder of the key. Either 'little' 
              or 'big'.
        """
        self._byteorder = byteorder
        self.key = key
        
        ## Golden ratio. See https://en.wikipedia.org/wiki/Golden_ratio.
        phi = (1 + math.sqrt(5)) / 2
        
        ## Magic constant, but it is a nothing-up-my-sleeve number.
        self._delta = int(2**32 / phi)
    
    @property
    def key(self: Self) -> bytes:
        return self._key
    
    @key.setter
    def key(self: Self, bytes: bytes) -> None:
        if len(bytes) != 16:
            raise ValueError(f"The TEA key must be 128 bit (16 bytes), not {len(bytes)} bytes.")
        self._key = bytes
    
    @property
    def delta(self: Self) -> int:
        return self._delta
    
    def encrypt(self: Self, block: bytes, rounds: int=32):
        if len(block) != 8:
            raise ValueError(f"The block to encrypt must be exactly 64 bit (8 bytes), not {len(block)} bytes.")
        
        v0 = int.from_bytes(bytes=block[0:4], byteorder=self._byteorder)
        v1 = int.from_bytes(bytes=block[4:8], byteorder=self._byteorder)
        sum = 0
        
        k0 = int.from_bytes(bytes=self._key[0:4], byteorder=self._byteorder)
        k1 = int.from_bytes(bytes=self._key[4:8], byteorder=self._byteorder)
        k2 = int.from_bytes(bytes=self._key[8:12], byteorder=self._byteorder)
        k3 = int.from_bytes(bytes=self._key[12:16], byteorder=self._byteorder)
        
        mask32bit = (1 << 32) - 1
        for _ in range(rounds):
            logging.debug(f"_={_:02x} sum={sum:08x} v0={v0:08x} v1={v1:08x} k0={k0:08x} k1={k1:08x} k2={k2:08x} k3={k3:08x}")
            sum += self._delta
            sum &= mask32bit

            v0 = v0 + (((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1))
            v0 &= mask32bit

            v1  = v1 + (((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3))
            v1 &= mask32bit
        logging.debug(f"_={_+1:02x} sum={sum:08x} v0={v0:08x} v1={v1:08x} k0={k0:08x} k1={k1:08x} k2={k2:08x} k3={k3:08x}")
        
        v0_bytes = v0.to_bytes(length=4, byteorder=self._byteorder)
        v1_bytes = v1.to_bytes(length=4, byteorder=self._byteorder)
        return v0_bytes + v1_bytes
    
    def decrypt(self: Self, block: bytes, rounds: int=32):
        if len(block) != 8:
            raise ValueError(f"The block to decrypt must be exactly 64 bit (8 bytes), not {len(block)} bytes.")
        
        def bytes_to_uint32(bytes):
            return np.uint32(int.from_bytes(bytes=bytes, byteorder=self._byteorder))
        def uint32_to_bytes(uint32):
            return uint32.to_bytes(length=4, byteorder=self._byteorder)
        
        v0 = bytes_to_uint32(bytes=block[0:4])
        v1 = bytes_to_uint32(bytes=block[4:8])
        sum = np.array(self._delta * rounds).astype(dtype=np.uint32)
        k0 = bytes_to_uint32(bytes=self._key[0:4])
        k1 = bytes_to_uint32(bytes=self._key[4:8])
        k2 = bytes_to_uint32(bytes=self._key[8:12])
        k3 = bytes_to_uint32(bytes=self._key[12:16])
        
        for _ in range(rounds):
            logging.debug(f"_={_:02x} sum={sum:08x} v0={v0:08x} v1={v1:08x} k0={k0:08x} k1={k1:08x} k2={k2:08x} k3={k3:08x}")
            v1  = np.array(v1 - (((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3))).astype(dtype=np.uint32)
            v0  = np.array(v0 - (((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1))).astype(dtype=np.uint32)
            
            sum = np.array(sum - self._delta).astype(dtype=np.uint32)
        logging.debug(f"_={_+1:02x} sum={sum:08x} v0={v0:08x} v1={v1:08x} k0={k0:08x} k1={k1:08x} k2={k2:08x} k3={k3:08x}")
        
        v0_bytes = uint32_to_bytes(int(v0))
        v1_bytes = uint32_to_bytes(int(v1))
        return v0_bytes + v1_bytes



if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    
    byteorder = 'little'
    ## This is the actual TEA key for NFC tag with serial number 
    ## 04:13:BB:1A:99:40:80, a tag for LEGO Dimensions's Wyldstyle (ID 
    ## 3).
    tea_key = struct.pack('<4I', 0x33ef8223, 0x3a56082f, 0x78f06c7c, 0x246c3710)
    
    tea = TEA(key=tea_key, byteorder='little')
    block_uncrypted = struct.pack('<I', 3) * 2
    print(f"block_uncrypted={block_uncrypted.hex()}")
    block_encrypted = tea.encrypt(block_uncrypted)
    print(f"block_encrypted={block_encrypted.hex()}")
    
    block_decrypted = tea.decrypt(block_encrypted)
    print(f"block_decrypted={block_decrypted.hex()}")
