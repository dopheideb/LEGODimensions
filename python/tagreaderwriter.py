#!/usr/bin/env python3

import argparse
import json
import legodimensions
import logging
import nfc		## pip install nfcpy, source ~/venv/bin/activate
import re
import sys


parser = argparse.ArgumentParser(
    prog='LEGO Dimensions tag reader/writer',
    description='This toolbox aids in changing a LEGO Dimensions vehicle into any other.',
)
parser.add_argument('--list-ids', action='store_true', help='List all known IDs.')
parser.add_argument('--list-names', action='store_true', help='List all known names.')
parser.add_argument('--write', type=str, help='The ID or name of the vehicle.')
parser.add_argument('--verbose', '-v', action='store_true', help='The ID of the character (1 to 999), or the ID of the vehicle/token (1000 and beyond).')

args = parser.parse_args()

if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)
console_handler = logging.StreamHandler()
logfmt = logging.Formatter(fmt='[%(asctime)s %(filename)s->%(funcName)s:%(lineno)s %(message)s')
logging.getLogger().handlers[0].setFormatter(logfmt)

with open('../json/taglist.json') as f:
    taglist = json.load(f)

if args.list_ids:
    for key, value in taglist.items():
        if re.match('^[0-9]+$', key):
            print(f"{key}\t{value['name']}")
    sys.exit(0)

if args.list_names:
    for key, value in taglist.items():
        if not re.match('^[0-9]+$', key):
            print(f"{key}\t{value['id']}")
    sys.exit(0)



clf = nfc.ContactlessFrontend('usb')
logging.debug(f"Found NFC device: {clf}")

logging.info("Waiting for a tag.")
nfctag = clf.connect(rdwr={'on-connect': lambda nfctag: False})

logging.info(f"Basic tag information: {nfctag}")

ldtag = legodimensions.Tag(uid=nfctag.identifier)
password = ldtag.password
logging.debug(f"ldtag.password={password.hex()}")

password_ack = b"\xAA\x55"
authenticated = nfctag.authenticate(password + password_ack)

logging.debug(f"Dump ({'' if authenticated else 'UN'}authenticaed):")
for line in nfctag.dump():
    logging.debug(line)

if authenticated:
    page = nfctag.read(0x24)
    page0x24 = page[0:4]
    page0x25 = page[4:8]
    page0x26 = page[8:12]
    page0x27 = page[12:16]
    
    if page0x26 == b"\x00\x01\x00\x00":
        word = page0x24[0:2]
        vehicle_id = int.from_bytes(word, byteorder='little')
        logging.info(f"Vehicle with id {vehicle_id} ({taglist[str(vehicle_id)]['name']}).")
    else:
        character_doubled = ldtag.decrypt(page0x24 + page0x25)
        character1 = character_doubled[0:4]
        character2 = character_doubled[4:8]
        assert character1 == character2, "Characters differ"
        character_id = int.from_bytes(character1, byteorder='big')
        logging.info(f"Character with id {character_id} ({taglist[str(character_id)]['name']}).")
    
    if args.write:
        tag = taglist[args.write]
        
        if tag['type'] == 'character':
            logging.error('No support for writing characters. Sorry.')
        elif tag['type'] == 'token':
            logging.info(f"Converting this tag to ID {tag['id']} ({tag['name']}).")
            logging.info(f"Writing to page 0x2B (the password): {password} ({password.hex()})")
            nfctag.write(0x2B, password)
            
            password_ack_bytes = password_ack + b"\x00\x00"
            logging.info(f"Writing to page 0x2C (the password ack): {password_ack_bytes} ({password_ack_bytes.hex()})")
            nfctag.write(0x2C, password_ack_bytes)
            
            id_bytes = int.to_bytes(tag['id'], length=4, byteorder='little')
            logging.info(f"Writing to page 0x24 (the ID): {id_bytes} ({id_bytes.hex()})")
            nfctag.write(0x24, id_bytes)
            
            logging.info(f"Writing to page 0x25: clearing with zeroes")
            nfctag.write(0x25, b"\x00\x00\x00\x00")
            
            ## A genuine LEGO NFC tag is write protected. Such a 
            ## token/vehicle tag has page 0x26 already set to 
            ## b"\x00\x01\x00\x00" and can't be overwritten.
            #type_bytes = b"\x00\x01\x00\x00"
            #logging.info(f"Writing to page 0x26 (the type): {type_bytes} ({type_bytes.hex()})")
            #nfctag.write(0x26, b"\x00\x01\x00\x00")
clf.close()
