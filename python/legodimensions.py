import logging
import struct
from   tea import TEA
from   typing import Self
import utils


'''
Credits:

All contributors in http://www.proxmark.org/forum/viewtopic.php?id=2657

ags131 for code on/in https://github.com/AlinaNova21/node-ld/

'''

class Tag:
    def __init__(self: Self, uid: bytes) -> None:
        self.uid = uid
    
    @property
    def uid(self: Self) -> bytes:
        return self._uid
    
    @uid.setter
    def uid(self: Self, uid: bytes) -> None:
        len_uid = len(uid)
        assert len_uid == 7, f"The UID is always 7 bytes, not {len_uid}."
        
        self._uid = uid
    
    def _shuffle_bits_and_derive_4byte_password(self: Self, base: bytes, rounds: int) -> bytes:
        show_bits = False
        password = 0
        for n in range(rounds):
            prev_password = password
            rot7  = utils.rotate_left_dword(password, 7)
            rot22 = utils.rotate_left_dword(password, 22)
            b = int.from_bytes(base[n * 4 : (n + 1) * 4], byteorder='little', signed=False)
            password = (b + rot7 + rot22 - password) & 0xFFFFFFFF
            logging.debug(f"n={n} prev_password={prev_password:08x} rot7={rot7:08x} rot22={rot22:08x} b={b:08x} current_password={password:08x}")
            
            if show_bits:
                logging.debug(f"n={n} prev_password={prev_password >> 25:07b}.{prev_password & 0x1ffffff:025b} rot7 ={rot7 >> 7:025b}.{rot7 & 0x7F:07b}")
                logging.debug(f"n={n} prev_password={prev_password >> 10:022b}.{prev_password & 0x3ff:010b} rot22={rot22 >> 22:010b}.{rot22 & 0x3FFFFF:022b}")
        return password.to_bytes(length=4, byteorder='little')

    @property
    def password(self: Self) -> bytes:
        """ Compute the NFC tag password.
        
        Every genuine LEGO Dimensions NFC tag has a password. The 
        password is unique for each tag, as it it based on the UID.
        
        This password is needed to access the character ID, or vehicle 
        ID of the toy tag.
        """
        ## The password is derived from 32 bytes, consisting of:
        ## 
        ##   1. [7] The 7-byte UUID.
        ##   2. [22] The magic copyright string.
        ##   3. [2] Two trailing bytes (alternating bit pattern 0xAA).
        ##
	##                       1         2
        ##             01234567890123456789012
        base = (self._uid
            + "(c) Copyright LEGO 2014".encode('utf-8')
            + bytes([0xaa, 0xaa])
        )
        for n in range(8):
            logging.debug(f"n={n} base[{n * 4:2d}:{(n+1)*4:2d}]={base[n * 4 : (n + 1) * 4].hex()} little_endian={int.from_bytes(base[n * 4 : (n + 1) * 4], byteorder='little', signed=False):08x}")
        
        return self._shuffle_bits_and_derive_4byte_password(base=base, rounds=8)

    @property
    def tea_key(self: Self) -> bytes:
        s3 = self.scramble(3)
        s4 = self.scramble(4)
        s5 = self.scramble(5)
        s6 = self.scramble(6)
        logging.debug(f"s3={s3.hex()} s4={s4.hex()} s5={s5.hex()} s6={s6.hex()}")

        ## Note: for two bytes objects, "+" means concatenate.
        return s3 + s4 + s5 + s6

    def scramble(self: Self, rounds: int) -> bytes:
        ## The firmware contains 16 bytes of random bytes.
        ## 
        ##                                  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
        static_randomness = bytes.fromhex("b7 d5 d7 e6 e7 ba 3c a8 d8 75 47 68 cf 23 e9 fe")
        padding_byte = bytes.fromhex("aa")

        base = self._uid + static_randomness[0:4 * (rounds - 2)] + padding_byte
        logging.debug(f"rounds={rounds}, uid={self.uid.hex(':')} base={base.hex(':')}")
        
        return self._shuffle_bits_and_derive_4byte_password(base=base, rounds=rounds)

    ## Characters start at 1.
    ## Vehicles/tokens start at 1000.
    def encrypt(self: Self, lego_dimesions_id: int) -> bytes:
        logging.debug(f"tea_key={self.tea_key.hex()}")
        tea = TEA(self.tea_key, byteorder='little')
        block = lego_dimesions_id.to_bytes(length=4, byteorder='little') * 2
        logging.debug(f"block to encrypt={block}")
        
        return tea.encrypt(block=block, rounds=32)
    
    def decrypt(self: Self, data: bytes) -> bytes:
        logging.debug(f"tea_key={self.tea_key.hex()}")
        tea = TEA(self.tea_key, byteorder='little')
        return tea.decrypt(block=data, rounds=32)
