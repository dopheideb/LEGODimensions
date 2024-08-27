#include <avr/io.h>

#include "asm-defines.h"

#if 0
.global TIMER1_OVF_vect
TIMER1_OVF_vect:
	reti
#endif



// This interrupt routine is called when timer 4 overflows. For us, this 
// means that the voltage glitch has completed.
// 
// Execution time: 6 CPU cycles
// 
// Clobbered registers: NONE
.global TIMER4_OVF_vect
.extern global__glitch_stage_not_done
TIMER4_OVF_vect:
	// [1 CPU cycle] TCCR4B = 0
	// 
	// If we do nothing, timer 4 will continue to count and another 
	// compare match and overflow will be triggered, i.e. another 
	// voltage glitch stage. We must prevent that from happening.
	// 
	// Tl;dr: stop counting.
	// 
	// We want to stop counting as fast as possible, so simply set 
	// ALL bits to 0, instead of ONLY the clock select bits (which 
	// would require reading TCCR4B first).
        sts TCCR4B,__zero_reg__
	
	// [1 CPU cycle] global__glitch_stage_not_done = 0
	// 
	// Mark that the glitching stage is over, so main() can test 
	// this variable (boolean) to see in which phase we are.
        sts global__glitch_stage_not_done,__zero_reg__
	
	// [4 CPU cycles]
	// 
	// This is an interrupt routine, so use reti not ret.
        reti
// END OF TIMER4_OVF_vect
