# Run this within gdb. First start openocd, then gdb with this script:
# openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c 'adapter speed 5000'
# gdb-multiarch --command run-glitch.py

import gdb
import time
import serial
import sys
import subprocess

# Glitch mode must be one of these constants. This determines how a glitch is
# checked.
GLITCH_MODE_FIND_LENGTH = 'find-length'
GLITCH_MODE_FIND_DELAY = 'find-delay'
GLITCH_MODE_FINAL = 'final'

mode = GLITCH_MODE_FIND_LENGTH

do_trace = True
do_trace = False
if do_trace:
	trace = print
else:
	trace = lambda *args: None

def start_ocd():
	'''Attempt to start the on-chip debugger'''
	raw = True
	if raw:
		return
	trace('start ocd')
	global ocd
	global glitched
	# For programming: openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c "adapter speed 5000" -c "program test-lpc.elf verify reset exit"
	# For debugging: openocd -f interface/cmsis-dap.cfg -f target/lpc11xx.cfg -c 'adapter speed 5000'
	ocd = subprocess.Popen(('openocd', '-f', 'interface/cmsis-dap.cfg',
			 '-f', 'target/lpc11xx.cfg', '-c',
			 'adapter speed 5000'),
			shell = False, stdin = subprocess.PIPE,
			stdout = subprocess.PIPE, stderr = subprocess.PIPE,
			close_fds = True)
	for line in ocd.stderr:
		if b'Listening on port 3333' in line:
			trace('ocd started')
			if mode == GLITCH_MODE_FINAL:
				glitched = True
			return
		trace('OCD: %s' % line)
	if mode == GLITCH_MODE_FINAL:
		glitched = False
		return
	print('ocd not started')
	for line in ocd.stderr:
		print(line)
	sys.exit(1)

def init():
	global avr
	start_ocd()
	avr = serial.Serial('/dev/ttyUSB0', 115200)
	gdb.set_parameter(name='pagination', value='off')
	gdb.execute('file ../src-lpc/test-lpc.elf')
	gdb.execute('target extended-remote :3333')

def pre_glitch_setup():
	try:
		gdb.execute('monitor reset run')
		gdb.execute('monitor halt', to_string=True)
		gdb.execute('monitor set_reg {pc 0x8f0}')
		gdb.execute('monitor resume')
	except gdb.error as err:
		print('GDB error ', err)
		sys.exit(42)

def glitch(post_reset_delay, glitch_duration):
	'''Attempt a glitch through the AVR.'''
	global avr
	trace('start glitch attempt')
	if True:
		command = b'G%d,%d\n' % (post_reset_delay, glitch_duration)
		avr.write(command)
		#print('avr write: %s' % command)
		for line in avr:
			if b'DONE' in line:
				break
			trace(b'avr: ' + line)
	trace('glitch attempt done')

def check_glitch():
	gdb.execute('monitor halt 0', to_string=True)
	try:
		output = gdb.execute('monitor get_reg -force pc', to_string=True)
	except gdb.error as err:
		## LPC has reset, which does not count as a successful glitch.
		return 'r'
	
	lines = output.split('\n')
	for line in lines:
		if not line.startswith('pc 0x'):
			continue
		addr = int(line.split()[1], 0)
		if 0x8f0 <= addr <= 0x906:
			# Nothing happened.
			return '.'
		if addr == 0x908:
			# Glitch happened.
			return '!'
		if addr == 0x8ee:
			# PC is at start of code:
			# force-pc failed or reset happened.
			return '0'
		print('addr: %x' % addr)
	
	print('Unknown program counter encountered. line=' + line)
	return '?'

def main_iteration(post_reset_delay, glitch_duration):
	pre_glitch_setup()
	glitch(post_reset_delay, glitch_duration)
	return check_glitch()

def main():
	init()
	while True:
		for post_reset_delay in range(9900, 9909):
			sys.stdout.write('post-reset delay %4d: ' % post_reset_delay)
			for glitch_duration in range(135, 265):
				status = main_iteration(post_reset_delay, glitch_duration)
				sys.stdout.write(status)
				sys.stdout.flush()
			print()

if __name__ == '__main__':
	main()
