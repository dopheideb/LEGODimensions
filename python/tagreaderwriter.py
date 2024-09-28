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
parser.add_argument('--write', type=str, help='The ID of the character (1 to 999), or the ID of the vehicle/token (1000 and beyond).')
parser.add_argument('--verbose', '-v', action='store_true', help='Be verbose.')

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
logging.debug(f"Found the following NFC device: {clf}")

logging.info("Waiting for a tag.")
nfctag = clf.connect(rdwr={'on-connect': lambda nfctag: False})

logging.info(f"Basic tag information: {nfctag}")

ldtag = legodimensions.Tag(uid=nfctag.identifier)
## LEGO protects the page in which the the character/vehicle ID is 
## written. However, if one buys an empty NTAG213, there is no need to 
## authenticate. Moreover, if an empty NTAG213 is written to contain a 
## valid LEGO Dimensions character or vehicle, then LEGO Dimensions does 
## not REQUIRE that the tag is protected against unwanted reads and 
## writes. So, if one does not protect an NTAG213, it is perfectly fine. 
## But then authentication fails if tried.
## 
## Just try to read page 0x24 (without prior authentication), where the 
## vehicle/character ID is stored. Only authenticate if needed.
password_ack = b"\xAA\x55"
password = ldtag.password

authentication_needed = False
is_authenticated = None
try:
    page_0x28_to_0x2B = nfctag.read(0x24)
except nfc.tag.tt2.Type2TagCommandError as e:
    if e.errno != nfc.tag.tt2.INVALID_PAGE_ERROR:
        raise
    authentication_needed = True
    logging.debug(f"ldtag.password={password.hex()}")
    is_authenticated = nfctag.authenticate(password + password_ack)

logging.debug(f"Dump ({'' if is_authenticated else 'UN'}authenticaed):")
for line in nfctag.dump():
    logging.debug(line)

if authentication_needed and not is_authenticated:
    raise f"Authentication is needed, but authentication failed."

page = nfctag.read(0x24)
page0x24 = page[0:4]
page0x25 = page[4:8]
page0x26 = page[8:12]
page0x27 = page[12:16]

## A vehicle (type 0x00010000) is handled differently from a 
## character.
current_tag_type = None
if page0x26 == b"\x00\x01\x00\x00":
    current_tag_type = 'token'
    word = page0x24[0:2]
    vehicle_id = int.from_bytes(word, byteorder='little')
    logging.info(f"Tag contains a vehicle/token with id {vehicle_id} ({taglist[str(vehicle_id)]['name']}).")
elif page0x26 == b"\x00\x00\x00\x00":
    current_tag_type = 'character'
    character_doubled = ldtag.decrypt(page0x24 + page0x25)
    character1 = character_doubled[0:4]
    character2 = character_doubled[4:8]
    if character1 != character2:
        logging.error(f"Decryption error: Character IDs differ ({character1} != ${character2}).")
    character_id = int.from_bytes(character1, byteorder='big')
    try:
        character_name = taglist[str(character_id)]['name']
        logging.info(f"Character with id {character_id} ({character_name}).")
    except KeyError:
        logging.error(f"Character ID {character_id} could not be converted to a name.")
        if character1 != character2:
            logging.error(f"Is this an empty tag perhaps?")
else:
    logging.error("The current tag is NOT a LEGO Dimensions character nor is a vehicle/token.")



if args.write:
    page_0x24 = None
    page_0x25 = None
    page_0x26 = None	## The type. 0: character, 1: vehicle/token
    
    tag = taglist[args.write]
    if tag['type'] == 'character':
        logging.info(f"Converting this tag to ID {tag['id']} ({tag['name']}).")
        page_0x26 = b"\x00\x00\x00\x00"
        
        encrypted_character = ldtag.encrypt(tag['id'])
        logging.info(f"Encrypted data to write to pages 0x24 and 0x25: {encrypted_character} ({encrypted_character.hex()})")
        
        page_0x24 = encrypted_character[0:4]
        page_0x25 = encrypted_character[4:8]
    elif tag['type'] == 'token':
        logging.info(f"Converting this tag to ID {tag['id']} ({tag['name']}).")
        type_bytes = b"\x00\x01\x00\x00"
        
        ## Page 0x24 contains the vehicle/token ID.
        page_0x24 = int.to_bytes(tag['id'], length=4, byteorder='little')
        
        ## Page 0x25 needs to be zeroed.
        page_0x25 = b"\x00\x00\x00\x00"
    
    ## We cannot switch between vehicle/token with a genuine LEGO 
    ## Dimensions tag.
    if tag['type'] != current_tag_type:
        if authentication_needed:
            raise ValueError(f"This is assumed to be a genuine LEGO Dimensions tag. We cannot alter the tag type from '{current_tag_type}' to '{tag['type']}'.")
        logging.info(f"Writing to page 0x26 (the type): {page_0x26} ({page_0x26.hex()})")
        nfctag.write(0x26, page_0x26)
    
    logging.info(f"Writing to page 0x24: {page_0x24} ({page_0x24.hex()})")
    nfctag.write(0x24, page_0x24)
    
    logging.info(f"Writing to page 0x25: {page_0x25} ({page_0x25.hex()})")
    nfctag.write(0x25, page_0x25)
    
    ## Page 0x29 consists of 4 bytes:
    ##     Byte 0+1: PACK
    ##     Byte 2:3: RFUI, must be all ones according to the docs: 
    ##     "Reserved for future use - implemented. Write all bits 
    ##     and bytes denoted as RFUI as 0b."
    page_0x2C = password_ack + b"\x00\x00"
    
    logging.info(f"Writing to page 0x2B (the password): {password} ({password.hex()})")
    nfctag.write(0x2B, password)
    logging.info(f"Writing to page 0x2C (the password ack and RFUI): {page_0x2C} ({page_0x2C.hex()})")
    nfctag.write(0x2C, page_0x2C)

clf.close()
