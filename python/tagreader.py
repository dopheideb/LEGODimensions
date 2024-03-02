import legodimensions
import logging
import nfc		## pip install nfcpy



logging.basicConfig(level=logging.INFO)
#logging.basicConfig(level=logging.DEBUG)

clf = nfc.ContactlessFrontend('usb')
logging.debug(f"Found NFC device: {clf}")

logging.info("Waiting for a tag.")
nfctag = clf.connect(rdwr={'on-connect': lambda nfctag: False})

logging.info(f"Basic tag information: {nfctag}")

ldtag = legodimensions.Tag(uid=nfctag.identifier)
logging.debug(f"ldtag.password={ldtag.password.hex()}")

password_ack = b"\xAA\x55"
authenticated = nfctag.authenticate(ldtag.password + password_ack)

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
        logging.info(f"Vehicle with id {vehicle_id}.")
    else:
        character_doubled = ldtag.decrypt(page0x24 + page0x25)
        character1 = character_doubled[0:4]
        character2 = character_doubled[4:8]
        assert character1 == character2, "Characters differ"
        character_id = int.from_bytes(character1, byteorder='big')
        logging.info(f"Character with id {character_id}.")
