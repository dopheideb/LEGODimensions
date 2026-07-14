echo Connecting to remote target...\n
target extended-remote :3333
echo Connected...\n
set pagination off


##
## !! There seems to be timing involved in between writing and reading 
## !! data. So if we write debug statement during the precise sleeps, we 
## !! mess up the challenge and the chip will result zeroes...
##
## Workaround: dump data when OUT handlers sets variable 
## Xbox_security_chip_command_and_response_buffer.

hbreak *0x2f9c
commands
  echo XSM3_handle_OUT_bRequest_0x82__UsbdSecXSM3SetChallengeProtocolData\n
  x/34bx *0x10000248
  continue
end

hbreak *0x3034
commands
  echo XSM3_handle_OUT_bRequest_0x87__UsbdSecXSM3SetVerifyProtocolDataN\n
  x/22bx *0x10000248
  continue
end

continue
