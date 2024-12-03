#!/usr/bin/env python3

import tabulate as tab
tab.PRESERVE_WHITESPACE = True

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
	elif baud_rate < 1000000 and baud_rate not in (500000,):
		baud_rate_str = f"{baud_rate/1000:g}"
		baud_rate_unit = 'k'
	else:
		baud_rate_str = f"{baud_rate/1000000:g}"
		baud_rate_unit = 'M'
	return [f"{baud_rate_str:>5s}", baud_rate_unit]

for oscillator_frequencies_mhz in chunks(oscillator_frequencies_mhz_all, 3):
	hdr_baud_rates = [ "Baud\nRate\n[bps]" ]
	dat_baud_rates = [[''.join(baud_rate2str(baud_rate))] for baud_rate in baud_rates]
	dat_baud_rates.append(["Max."])
	col_baud_rates = tab.tabulate(dat_baud_rates, headers=hdr_baud_rates, tablefmt="presto")
	columns = [col_baud_rates]
	
	for oscillator_frequency_mhz in oscillator_frequencies_mhz:
		oscillator_frequency = oscillator_frequency_mhz * 1000000
		
		max_baud_rates = [None] * 2
		for u2x in [0, 1]:
			max_baud_rate = get_baud_rate(oscillator_frequency, u2x, ubrr=0)
			[max_baud_rate_str, baud_rate_unit] = baud_rate2str(max_baud_rate)
			max_baud_rates[u2x] = ''.join([max_baud_rate_str, baud_rate_unit]) + "bps"
		
		u2x_data = [[], []]
		for requested_baud_rate in baud_rates:
			[requested_baud_rate_str, baud_rate_unit] = baud_rate2str(requested_baud_rate)
			
			for u2x in [0, 1]:
				max_baud_rate = get_baud_rate(oscillator_frequency, u2x, ubrr=0)
				
				ubrr = get_nearest_ubrr(oscillator_frequency, u2x, requested_baud_rate)
				
				actual_baud_rate = get_baud_rate(oscillator_frequency, u2x, ubrr)
				error = actual_baud_rate / requested_baud_rate - 1
				if (requested_baud_rate >= max_baud_rate + 10
						or abs(error) > 0.25):
					ubrr_val = '-'
					error_pct = '-'
				else:
					ubrr_val = ubrr
					error_pct = f"{error * 100:+.1f}%"
				u2x_data[u2x].append([ubrr_val, error_pct])
		col_u2x0 = tab.tabulate(u2x_data[0], headers=["UBRR", "Error"], tablefmt="presto", colalign=("right", "right"))
		col_u2x1 = tab.tabulate(u2x_data[1], headers=["UBRR", "Error"], tablefmt="presto")
		col_u2x = tab.tabulate([["U2Xn = 0", "U2Xn = 1"], [col_u2x0, col_u2x1], max_baud_rates], tablefmt="plain")
		col_f_osc = tab.tabulate([[f"f_osc {oscillator_frequency_mhz:.4f}MHz"], [col_u2x]], tablefmt="plain")
		columns.append(col_f_osc)
	
	print(tab.tabulate([columns], tablefmt="presto"))
