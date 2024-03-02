import logging
import struct
from   tea import TEA
from   typing import Self



'''
Credits:

All contributors in http://www.proxmark.org/forum/viewtopic.php?id=2657

ags131 for code on/in https://github.com/AlinaNova21/node-ld/

'''


def _rotate_right(n: int, rotations: int=1, width: int=8):
    part1 = n >> rotations
    
    ## Rotating to the right is almost the same as rotating to 
    ## the left.
    part2 = n << (width - rotations)
    return (part1 | part2) & ((1 << width) - 1)
    
def _rotate_right_dword(dword: int, rotations: int):
    dword_size = 32
    return _rotate_right(dword, rotations, width=dword_size)

def swap_endianness_32(data32: bytes) -> bytes:
    return int.from_bytes(bytes=data32, byteorder='little').to_bytes(length=4, byteorder='big', signed=False)



class Tag:
    password_base = (
        ## Replace "UUUUUUU" with the UID.
        b"UUUUUUU(c) Copyright LEGO 2014\xAA\xAA"
    )
    
    scramble_base = (
        b"\x00\x00\x00\x00" +
        b"\x00\x00\x00"     +
                    b"\xB7" +
        b"\xD5\xD7\xE6\xE7" +
        b"\xBA\x3C\xA8\xD8" +
        b"\x75\x47\x68\xCF" +
        b"\x23\xE9\xFE\xAA"
    )
    
    def __init__(self: Self, uid: bytes) -> None:
        self.uid = uid
    
    @property
    def uid(self: Self) -> bytes:
        return self._uid
    
    @uid.setter
    def uid(self: Self, uid: bytes) -> None:
        len_uid = len(uid)
        assert len_uid == 7, "The UID is always 7 bytes, not f{len_uid}."
        
        self._uid = uid
    
    @property
    def password(self: Self) -> bytes:
        base = bytearray(Tag.password_base)
        base[0:7] = self._uid
        for n in range(8):
            logging.debug(f"n={n} base[{n * 4:2d}:{(n+1)*4:2d}]={base[n * 4 : (n + 1) * 4].hex()} little_endian={int.from_bytes(base[n * 4 : (n + 1) * 4], byteorder='little', signed=False):08x}")
        
        v2 = 0
        for n in range(8):
            v2_old = v2
            v4 = _rotate_right_dword(v2, 25)
            v5 = _rotate_right_dword(v2, 10)
            b = int.from_bytes(base[n * 4 : (n + 1) * 4], byteorder='little', signed=False)
            v2 = (b + v4 + v5 - v2) & 0xFFFFFFFF
            logging.debug(f"n={n} v4={v4:08x} v5={v5:08x} b={b:08x} v2={v2:08x}")
            #logging.debug(f"n={n} v4=rl(v2,7)=rr({v2_old:032b},7)={v4:032b}={v4:08x} v5=rr(v2,10)=rr({v2_old:032b},10)={v5:032b}={v5:08x} b={b:08x} v2=b+v4+v5-v2={v2:08x}")
            #logging.debug(f"n={n} v4={v4:032b}")
            #logging.debug(f"n={n} v5={v5:032b}")
            #logging.debug(f"n={n}  b={b:032b}")
            #logging.debug(f"n={n} v2={v2_old:032b}")
        
        return int.to_bytes(v2, length=4, byteorder='little')
    
    @property
    def tea_key(self: Self) -> bytes:
        s3 = self.scramble(3)
        s4 = self.scramble(4)
        s5 = self.scramble(5)
        s6 = self.scramble(6)
        logging.debug(f"s3={s3.hex()} s4={s4.hex()} s5={s5.hex()} s6={s6.hex()}")
        return s3 + s4 + s5 + s6
    
    def scramble(self: Self, cnt: int) -> bytes:
        base = bytearray(Tag.scramble_base)
        base[0:7] = self._uid
        if cnt <= 0:
            return int.to_bytes(0, length=4, byteorder='big')
        base[(cnt * 4) - 1] = 0xAA
        logging.debug(f"cnt={cnt}, uid={self.uid.hex()} base={base}")
        
        v2 = 0
        for n in range(cnt):
            v4 = _rotate_right_dword(v2, 25)
            v5 = _rotate_right_dword(v2, 10)
            b = int.from_bytes(base[n * 4 : (n + 1) * 4], byteorder='little', signed=False)
            v2 = (b + v4 + v5 - v2) & 0xFFFFFFFF
            logging.debug(f"n={n} v4={v4:08x} v5={v5:08x} b={b:08x} v2={v2:08x}")
        
        return int.to_bytes(v2, length=4, byteorder='big')
    
    ## Characters start at 1.
    ## Vehicles/tokens start at 1000.
    def encrypt(self: Self, lego_dimesions_id: int) -> bytes:
        logging.debug(f"tea_key={self.tea_key.hex()}")
        tea = TEA(self.tea_key)
        block = bytearray(8)
        ## Weird, x86_64 is little Endian.
        block[0:4] = int.to_bytes(lego_dimesions_id, length=4, byteorder='big', signed=False)
        block[4:8] = block[0:4]
        logging.debug(f"block to encrypt={block}")
        
        encrypted_block = tea.encrypt(block=block, rounds=32, byteorder='big')
        
        ## Note: for two bytes objects, "+" means concatenate.
        shuffled_encrypted_block = swap_endianness_32(encrypted_block[0:4]) \
                                 + swap_endianness_32(encrypted_block[4:8])
        return shuffled_encrypted_block
    
    def decrypt(self: Self, data: bytes) -> bytes:
        logging.debug(f"tea_key={self.tea_key.hex()}")
        tea = TEA(self.tea_key)
        
        ## Note: for two bytes objects, "+" means concatenate.
        shuffled_data = swap_endianness_32(data[0:4]) \
                      + swap_endianness_32(data[4:8])
        
        buf = tea.decrypt(block=shuffled_data, rounds=32, byteorder='big')
        return buf
