#ifndef _INCLUDED_GLITCHER_HPP_

//#define SERIAL_BAUDRATE (9600L)
#define SERIAL_BAUDRATE (500L * 1000L)

#define _CONCAT3(x,y,z) x##y##z
#define CONCAT3(x,y,z) _CONCAT3(x,y,z)
#define STRINGIFY(s) _STR(s)
#define _STR(s) #s
#define NX "\n"
#define NT "\n\t"
#define ASM_FILE_LINE ";; " __FILE__ ":" STRINGIFY(__LINE__)

// We need 3 pins to communicate to the glitcher and the 
// toypad/LPC11U35:
// 
//   1. [LPC11U35] To the RESET port of the LPC11U35. (Note: Active low.)
//   2. [Glitcher] To the MOSFET supplying the normal voltage to the LPC11U35.
//   3. [Glitcher] To the MOSFET supplying the bad voltage to the LPC11U35.
// 
// 
// 
// The hardware we already own:
// 
//   A. Arduino Leonardo   (ATmega32U4)
//   B. SparkFun Pro Micro (ATmega32U4)
// 
// We will use timer 4 for high precision glitching/switching, with the 
// hardware turning on/off the signal to the MOSFETs (OC4x, output 
// compare).
// 
//   OC4X            | ATmega32U4 | Arduino Leonardo | SparkFun Pro Micro |
//   ----------------+------------+------------------+--------------------+
//   OC4A (regular)  |     PC7    |        D13       |    NOT AVAILABLE   |
//   OC4A (inverted) |     PC6    |        D5        |           5        |
//   ----------------+------------+------------------+--------------------+
//   OC4B (regular)  |     PB6    |        D10       |          10        | <--- Normal voltage
//   OC4B (inverted) |     PB5    |        D9        |           9        | <--- Bad voltage
//   ----------------+------------+------------------+--------------------+
//   OC4D (regular)  |     PD7    |        D6        |           6        |
//   OC4D (inverted) |     PD6    |        D12       |    NOT AVAILABLE   |
//   ----------------+------------+------------------+--------------------+
// 
// The obvious choice is to use OC4B, since it is available on both our 
// boards.
// 
// The choice for the RESET port is based on aesthetics; just use a 
// neighbouring pin. D8 is available on both boards (D11 is not). D8 is 
// connected to ATmega pin PB4.

#define GLITCHER_PORT_GROUP		B
#define GLITCHER_PORT			CONCAT3(PORT, GLITCHER_PORT_GROUP, )
#define GLITCHER_DDR			CONCAT3(DDR, GLITCHER_PORT_GROUP, )
#define GLITCHER_NOT_RESET_PIN		CONCAT3(PIN, GLITCHER_PORT_GROUP, 4)
#define GLITCHER_VSS_REGULAR_PIN	CONCAT3(PIN, GLITCHER_PORT_GROUP, 5)
#define GLITCHER_VSS_BAD_PIN		CONCAT3(PIN, GLITCHER_PORT_GROUP, 6)

// Note: the RESET pin of LPC11U35 is active low, so:
// 
//   GLITCHER_NOT_RESET_PIN | Result on LPC11U35
//   -----------------------+-------------------
//   1 / high / on          | Not resetting, i.e. run normally.
//   0 / low / off          | Resetting, i.e. does not run.
#define GLITCHER_PORT_STATE_RESET_LPC11U35_WITH_REGULAR_VOLTAGE	(                          0 | _BV(GLITCHER_VSS_REGULAR_PIN))
#define GLITCHER_PORT_STATE_RUN_LPC11U35_WITH_REGULAR_VOLTAGE	(_BV(GLITCHER_NOT_RESET_PIN) | _BV(GLITCHER_VSS_REGULAR_PIN))


#define CS1_STOP		(        0 |         0 |         0)
#define CS1_DIVIDER_1		(        0 |         0 | _BV(CS10))
#define CS1_DIVIDER_8		(        0 | _BV(CS11) |        0 )
#define CS1_DIVIDER_64		(        0 | _BV(CS11) | _BV(CS10))
#define CS1_DIVIDER_256		(_BV(CS12) |         0 |        0 )
#define CS1_DIVIDER_1024	(_BV(CS12) |         0 | _BV(CS10))

#define CS4_STOP		(        0 |         0 |         0 |         0)
#define CS4_DIVIDER_1		(        0 |         0 |         0 | _BV(CS40))
#define CS4_DIVIDER_16384	(_BV(CS43) | _BV(CS42) | _BV(CS41) | _BV(CS40))

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
// 
// [2] sts TCCR4B, rXX
#define TIMER1_INTERRUPT_CPU_CYCLES_BEFORE_STARTING_TIMER4 (5+5+5+2)

#if 0
#define TIMER1_RUNS_ULTRA_SLOW
#define TIMER4_RUNS_ULTRA_SLOW
#endif

#ifdef TIMER1_RUNS_ULTRA_SLOW
  #define CS1_DIVIDER CS1_DIVIDER_1024
#else
  #define CS1_DIVIDER CS1_DIVIDER_1
#endif

#ifdef TIMER4_RUNS_ULTRA_SLOW
  #define CS4_DIVIDER CS4_DIVIDER_16384
#else
  #define CS4_DIVIDER CS4_DIVIDER_1
#endif

#endif // ifndef _INCLUDED_GLITCHER_HPP_
