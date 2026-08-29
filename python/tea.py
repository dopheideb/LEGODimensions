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

            v0 += ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1)
            v0 &= mask32bit

            v1 += ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3)
            v1 &= mask32bit
        logging.debug(f"_={_+1:02x} sum={sum:08x} v0={v0:08x} v1={v1:08x} k0={k0:08x} k1={k1:08x} k2={k2:08x} k3={k3:08x}")
        
        v0_bytes = v0.to_bytes(length=4, byteorder=self._byteorder)
        v1_bytes = v1.to_bytes(length=4, byteorder=self._byteorder)
        return v0_bytes + v1_bytes
    
    def decrypt(self: Self, block: bytes, rounds: int=32):
        if len(block) != 8:
            raise ValueError(f"The block to decrypt must be exactly 64 bit (8 bytes), not {len(block)} bytes.")
        
        mask32bit = (1 << 32) - 1
        
        v0 = int.from_bytes(bytes=block[0:4], byteorder=self._byteorder)
        v1 = int.from_bytes(bytes=block[4:8], byteorder=self._byteorder)
        sum = self._delta * rounds
        sum &= mask32bit

        k0 = int.from_bytes(bytes=self._key[0:4], byteorder=self._byteorder)
        k1 = int.from_bytes(bytes=self._key[4:8], byteorder=self._byteorder)
        k2 = int.from_bytes(bytes=self._key[8:12], byteorder=self._byteorder)
        k3 = int.from_bytes(bytes=self._key[12:16], byteorder=self._byteorder)
        
        for _ in range(rounds):
            logging.debug(f"_={_:02x} sum={sum:08x} v0={v0:08x} v1={v1:08x} k0={k0:08x} k1={k1:08x} k2={k2:08x} k3={k3:08x}")
            v1 -= ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3)
            v1 &= mask32bit
            
            v0 -= ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1)
            v0 &= mask32bit
            
            sum -= self._delta
            sum &= mask32bit
        logging.debug(f"_={_+1:02x} sum={sum:08x} v0={v0:08x} v1={v1:08x} k0={k0:08x} k1={k1:08x} k2={k2:08x} k3={k3:08x}")
        
        v0_bytes = v0.to_bytes(length=4, byteorder=self._byteorder)
        v1_bytes = v1.to_bytes(length=4, byteorder=self._byteorder)
        return v0_bytes + v1_bytes



if __name__ == "__main__":
    import argparse
    import re

    class Formatter(
        argparse.ArgumentDefaultsHelpFormatter,
        argparse.RawDescriptionHelpFormatter,
    ):
        pass

    epilog = '''
Examples:

    The following two commands yield the same outcome, because they both 
    use the same key and message. The difference is input format: big or 
    little endian.

        %(prog)s --key='23:82:ef:33   2f:08:56:3a   7c:6c:f0:78   10:37:6c:24' --message='03:00:00:00 03:00:00:00' --byteorder='little'
        %(prog)s --key='33:ef:82:23   3a:56:08:2f   78:f0:6c:7c   24:6c:37:10' --message='00:00:00:03 00:00:00:03' --byteorder='big'

    Decryption:

        %(prog)s --decrypt --key='23:82:ef:33   2f:08:56:3a   7c:6c:f0:78   10:37:6c:24' --message='01:39:ed:60 e4:be:30:7c' --byteorder='little'
        %(prog)s --decrypt --key='33:ef:82:23   3a:56:08:2f   78:f0:6c:7c   24:6c:37:10' --message='60:ed:39:01 7c:30:be:e4' --byteorder='big'

'''
    parser = argparse.ArgumentParser(
        description='This program can encrypt and decrypt with TEA, Tiny Encryption Algorithm.',
        epilog=epilog,
        #formatter_class=argparse.RawDescriptionHelpFormatter,
        #formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        formatter_class=Formatter,
    )
    parser.add_argument('--verbose', '-v', action='store_true', help='Be more verbose.')

    parser.add_argument('--key', '-k', required=True, help='The TEA key, in hex format.')

    crypt_group = parser.add_mutually_exclusive_group()
    crypt_group.add_argument('--decrypt', '-d', action='store_true', help='Decrypt instead of encrypt.')

    parser.add_argument('--byteorder', '-b', help='The byte order. Either "little", or "big".')
    parser.set_defaults(byteorder='little')

    parser.add_argument('--message', '-m', required=True, help='The message to encrypt/decrypt, in hex format.')
    parser.set_defaults(message=None)

    args = parser.parse_args()
    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)
    logging.debug(f"byteorder={args.byteorder}")
    logging.debug(f"decrypt={args.decrypt}")
    logging.debug(f"encrypt={not args.decrypt}")

    logging.debug(f"key={args.key}")
    stripped_key = re.sub('[^0-9A-Fa-f]', '', args.key)
    logging.debug(f"stripped_key={stripped_key}")
    key = bytes.fromhex(stripped_key)
    logging.debug(f"key={key.hex(':')}")

    logging.debug(f"message={args.message}")
    stripped_message = re.sub('[^0-9A-Fa-f]', '', args.message)
    logging.debug(f"stripped_message={stripped_message}")
    message = bytes.fromhex(stripped_message)
    logging.debug(f"message={message.hex(':')}")

    tea = TEA(key=key, byteorder=args.byteorder)
    if args.decrypt:
        result = tea.decrypt(message)
    else:
        result = tea.encrypt(message)
    print(f"hex={result.hex(':')}")
    print(f"v0=0x{int.from_bytes(result[0:4], byteorder=args.byteorder):08x}")
    print(f"v1=0x{int.from_bytes(result[4:8], byteorder=args.byteorder):08x}")
