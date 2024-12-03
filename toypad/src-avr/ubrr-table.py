#!/usr/bin/env python3

#from tabulate import tabulate
baud_rates = [
	2400,	4800,	9600,	14400,	19200,	28800,	38400,
	57600,	76800,	115200,	230400,	250000,	500000,	1000000]
oscillator_frequencies_mhz_all = [
	1.0000,		1.8432,		2.0000,
	3.6864,		4.0000,		7.3728,
	8.0000,		11.0592,	14.7456,
	16.0000]

def chunks(list, n):
	for i in range(0, len(list), n):
		yield list[i:i+n]

def get_nearest_ubrr(f_osc, u2x, requested_baud_rate):
	clock_divider = 8 if u2x else 16
	ubrr_float = f_osc / (clock_divider * requested_baud_rate) - 1
	return int(ubrr_float + 0.5)

def get_baud_rate(f_osc, u2x, ubrr):
	clock_divider = 8 if u2x else 16
	return f_osc / (clock_divider * (ubrr + 1))

def baud_rate2str(baud_rate):
	if baud_rate < 10000:
		baud_rate_str = str(baud_rate)
		baud_rate_unit = ''
	elif baud_rate <= 250000:
		baud_rate_str = f"{baud_rate/1000:.1f}"
		baud_rate_unit = 'k'
	else:
		baud_rate_str = f"{baud_rate/1000000:.1f}"
		baud_rate_unit = 'M'
	return [f"{baud_rate_str:>5s}", baud_rate_unit]

for oscillator_frequencies_mhz in chunks(oscillator_frequencies_mhz_all, 3):

	col_values = [ f"{'Baud':^6s}" ]
	for oscillator_frequency_mhz in oscillator_frequencies_mhz:
		f_osc_str = f"f_osc = {oscillator_frequency_mhz:.4f}MHz"
		col_values.append(f"{f_osc_str:^29s}")
	print(' | '.join(col_values), '|')
	
	col_values = [ f"{'Rate':^6s}" ]
	for oscillator_frequency_mhz in oscillator_frequencies_mhz:
		for u2x in [0, 1]:
			u2x_str = f"U2Xn = {u2x}"
			col_values.append(f"{u2x_str:^13s}")
	print(' | '.join(col_values), '|')
	
	col_values = [ f"{'[bps]':^6s}" ]
	for oscillator_frequency_mhz in oscillator_frequencies_mhz:
		for u2x in [0, 1]:
			col_values.append(f"{'UBRR':^4s}")
			col_values.append(f"{'Error':^6s}")
	print(' | '.join(col_values), '|')
	
	print('-------+', end='')
	for oscillator_frequency_mhz in oscillator_frequencies_mhz:
		for u2x in [0, 1]:
			print('------+--------+', end='')
	print('')
	
	for requested_baud_rate in baud_rates:
		[requested_baud_rate_str, baud_rate_unit] = baud_rate2str(requested_baud_rate)
		col_values = [f"{requested_baud_rate_str:>5s}{baud_rate_unit:1s}"]
		
		for oscillator_frequency_mhz in oscillator_frequencies_mhz:
			oscillator_frequency = oscillator_frequency_mhz * 1000000
			for u2x in [0, 1]:
				max_baud_rate = get_baud_rate(oscillator_frequency, u2x, ubrr=0)
				
				ubrr = get_nearest_ubrr(oscillator_frequency, u2x, requested_baud_rate)
				
				actual_baud_rate = get_baud_rate(oscillator_frequency, u2x, ubrr)
				error = actual_baud_rate / requested_baud_rate - 1
				if (requested_baud_rate >= max_baud_rate + 10
						or abs(error) > 0.25):
					col_values.append(f"{'-':>4s}")
					col_values.append(f"{'-':^6s}")
					continue
				col_values.append(f"{ubrr:4d}")
				col_values.append(f"{error*100:+5.1f}%")
		print(' | '.join(col_values), '|')
	
	col_values = [ f"{'Max':^6s}" ]
	for oscillator_frequency_mhz in oscillator_frequencies_mhz:
		oscillator_frequency = oscillator_frequency_mhz * 1000000
		for u2x in [0, 1]:
			max_baud_rate = get_baud_rate(oscillator_frequency, u2x, ubrr=0)
			[max_baud_rate_str, baud_rate_unit] = baud_rate2str(max_baud_rate)
			col_values.append(f"{f'{max_baud_rate_str}{baud_rate_unit}':^13}")
	print(' | '.join(col_values), '|')
