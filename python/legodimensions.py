import logging
import struct
from   tea import TEA



'''
Credits:

All contributors in http://www.proxmark.org/forum/viewtopic.php?id=2657

ags131 for code on/in https://github.com/AlinaNova21/node-ld/

'''


def _rotate_right(n, rotations=1, width=8):
    part1 = n >> rotations
    
    ## Rotating to the right is almost the same as rotating to 
    ## the left.
    part2 = n << (width - rotations)
    return (part1 | part2) & ((1 << width) - 1)
    
def _rotate_right_dword(dword, rotations):
    dword_size = 32
    return _rotate_right(dword, rotations, width=dword_size)



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
    
    def __init__(self, uid=None):
        self._uid = uid
    
    @property
    def uid(self):
        return self._uid
    
    @uid.setter
    def uid(self, uid):
        self._uid = uid
    
    @property
    def password(self):
        base = bytearray(Tag.password_base)
        base[0:7] = self._uid.to_bytes(length=7, byteorder='big')
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
        
        ## Convert v2 to little endian.
        return int.from_bytes(struct.pack('>I', v2), byteorder='little', signed=False)
    
    @property
    def tea_key(self):
        s3 = self.scramble(3)
        s4 = self.scramble(4)
        s5 = self.scramble(5)
        s6 = self.scramble(6)
        logging.debug(f"s3={s3:#010x} s4={s4:#010x} s5={s5:#010x} s6={s6:#010x}")
        return (
            s3 << 96
            |
            s4 << 64
            |
            s5 << 32
            |
            s6 <<  0
        )
    
    def scramble(self, cnt):
        base = bytearray(Tag.scramble_base)
        base[0:7] = self._uid.to_bytes(length=7, byteorder='big')
        if cnt <= 0:
            return 0
        base[(cnt * 4) - 1] = 0xAA
        logging.debug(f"cnt={cnt}, uid={self._uid:014x} base={base}")
        
        v2 = 0
        for n in range(cnt):
            v4 = _rotate_right_dword(v2, 25)
            v5 = _rotate_right_dword(v2, 10)
            b = int.from_bytes(base[n * 4 : (n + 1) * 4], byteorder='little', signed=False)
            v2 = (b + v4 + v5 - v2) & 0xFFFFFFFF
            logging.debug(f"n={n} v4={v4:08x} v5={v5:08x} b={b:08x} v2={v2:08x}")
        
        return v2
        ## Convert v2 to little endian.
        #return int.from_bytes(struct.pack('>I', v2), byteorder='little', signed=False)
    
    ## Characters start at 1.
    ## Vehicles/tokens start at 1000.
    def encrypt(self, lego_dimesions_id):
        tea_key = int.to_bytes(self.tea_key, 16)
        logging.debug(f"tea_key={self.tea_key:#010x}")
        logging.debug(f"tea_key={tea_key}")
        tea = TEA(tea_key)
        block = bytearray(8)
        ## Weird, x86_64 is little Endian.
        block[0:4] = int.to_bytes(lego_dimesions_id, length=4, byteorder='big', signed=False)
        block[4:8] = block[0:4]
        logging.debug(f"block to encrypt={block}")
        
        encrypted_block = tea.encrypt(block=block, rounds=32, byteorder='big')
        
        ## Note: for two bytes objects, "+" means concatenate.
        shuffled_encrypted_block = Tag.swap_endianness_32(encrypted_block[0:4]) \
                                 + Tag.swap_endianness_32(encrypted_block[4:8])
        return shuffled_encrypted_block
    
    def decrypt(self, data):
        tea_key = int.to_bytes(self.tea_key, 16)
        logging.debug(f"tea_key={self.tea_key:#010x}")
        logging.debug(f"tea_key={tea_key}")
        tea = TEA(tea_key)
        
        ## Note: for two bytes objects, "+" means concatenate.
        shuffled_data = Tag.swap_endianness_32(data[0:4]) \
                      + Tag.swap_endianness_32(data[4:8])
        
        buf = tea.decrypt(block=shuffled_data, rounds=32, byteorder='big')
        return buf
        
    def swap_endianness_32(data32):
        return int.from_bytes(bytes=data32, byteorder='little').to_bytes(length=4, byteorder='big', signed=False)
