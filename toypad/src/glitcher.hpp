#ifndef _INCLUDED_GLITCHER_HPP_

//#define SERIAL_BAUDRATE (9600L)
#define SERIAL_BAUDRATE (500L * 1000L)

// We only want to glitch once, i.e. we have to prevent doing a glitch 
// twice, so we have to be ABLE to stop timer 4 in the interrupt 
// routine. That means enough ticks between the initial value of 
// timer/counter4 and the overflow. 64 ticks should be more than enough.
#define MIN_TIMER4_TICKS_BEFORE_OVERFLOW 64

// [5] "If an interrupt occurs when the MCU is in sleep mode, the 
//     interrupt execution response time is increased by five clock 
//     cycles."
// 
// [5] "The interrupt execution response for all the enabled AVR 
//     interrupts is five clock cycles minimum. After five clock cycles 
//     the program vector address for the actual interrupt handling 
//     routine is executed. During these five clock cycle period, the 
//     Program Counter is pushed onto the Stack."
// 
// [5] "A return from an interrupt handling routine takes five clock 
//     cycles. During these five clock cycles, the Program Counter 
//     (three bytes) is popped back from the Stack, the Stack Pointer is 
//     incremented by three, and the I-bit in SREG is set."
#define TIMER1_INTERRUPT_CLOCK_CYCLES_BEFORE_STARTING_TIMER4 15

#define TIMER1_RUNS_ULTRA_SLOW
#define TIMER4_RUNS_ULTRA_SLOW

#endif // ifndef _INCLUDED_GLITCHER_HPP_
