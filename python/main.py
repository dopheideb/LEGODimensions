#!/usr/bin/env python3

import argparse
import itertools
import legodimensions
import logging
import re



parser = argparse.ArgumentParser(
    prog='LEGO Dimensions tag toolbox',
    description='Tis toolbox aids in changing an NFC213 tag a valid/other LEGO Dimensions character or vehicle.',
)
parser.add_argument('--uid', required=True, help='The UID of the NFC tag. This is also called the serial number of the tag.')
parser.add_argument('--id', type=int, help='The ID of the character (1 to 999), or the ID of the vehicle/token (1000 and beyond).')
parser.add_argument('--verbose', '-v', action='store_true', help='The ID of the character (1 to 999), or the ID of the vehicle/token (1000 and beyond).')

args = parser.parse_args()

if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)

## Validate UID.
logging.debug(f"args.uid={args.uid}")
uid = re.sub(r"^0x", '', args.uid)
uid = re.sub(r"[^0-9A-Fa-f]", '', uid)
logging.debug(f"uid={uid}")
if len(uid) == 13:
    ## People may forget the leading zero.
    uid = '0' + uid
if len(uid) > 14:
    raise ValueError(f"The UID is only 7 bytes, i.e. 14 hexdigits, not {len(uid)} (uid={uid}).")

def str2colon_separated(string: str):
    return ':'.join(re.findall('..', string))
def int2hexcolon_separated(integer: int):
    return str2colon_separated(f"{integer:08x}")

tag = legodimensions.Tag()
tag.uid = int(uid, 16)

logging.info(f"tag.uid={tag.uid:#010x} / {tag.uid:08x} / {int2hexcolon_separated(tag.uid)}")
logging.info(f"tag.password={tag.password:#010x} / {tag.password:08x} / {int2hexcolon_separated(tag.password)}")
logging.info(f"tea_key={tag.tea_key:#034x}")

if args.id is None:
    args.ids = itertools.chain(range(1, 800), range(1000, 1300))
else:
    args.ids = [ args.id ]

for id in args.ids:
    if id < 1000:
        logging.info(f"ID {id} is a character, not a vehicle/token.")
        encrypted = tag.encrypt(id)
    else:
        logging.info(f"ID {id} is a vehicle/token, not a character.")
        encrypted = int.to_bytes(id, length=8, byteorder='little')
    
    #print(f"encrypted={encrypted.hex()}")
    #print(f"encrypted for page 0x24={encrypted[0:4].hex()}")
    #print(f"encrypted for page 0x25={encrypted[4:8].hex()}")
    #print(f"NFC Tools command for page 0x24: 1B:{tag.password:08X},A2:24:{encrypted[0:4].hex().upper()}")
    #print(f"NFC Tools command for page 0x25: 1B:{tag.password:08X},A2:25:{encrypted[4:8].hex().upper()}")
    if id < 1000:
        print(f"NFC Tools command for ID={id} for pages 0x2B (set password): 1B:{tag.password:08X},A2:2B:{tag.password:08X}")
        print(f"NFC Tools command for ID={id} for pages 0x2C (set password ack to AA:55): A2:2C:AA:55:00:00")
    
    print(f"NFC Tools command for ID={id} for pages 0x24 and 0x25: 1B:{tag.password:08X},A2:24:{encrypted[0:4].hex().upper()},A2:25:{encrypted[4:8].hex().upper()}")
