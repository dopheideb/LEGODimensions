#include <avr/common.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "usart.hpp"
#include "glitcher.hpp"

/*
     _____ _              _     _
    |_   _| |__   ___    (_) __| | ___  __ _
      | | | '_ \ / _ \   | |/ _` |/ _ \/ _` |
      | | | | | |  __/   | | (_| |  __/ (_| |
      |_| |_| |_|\___|   |_|\__,_|\___|\__,_|
    
    The LPC11U35 has a boot ROM in which it checks fuses. We want to 
    change the outcome of 1 particular fuse check: the CRP level check. 
    "CRP" stands for "Code Read Protection". There are only 4 special 
    values:
    
        0x4E697370 "NO_ISP"
        0x12345678 "CRP1"
        0x87654321 "CRP2"
        0x43218765 "CRP3"
    
    ALL the other 4294967292 values mean "no code protection 
    whatsoever". The LEGO Dimensions toypad is programmed with Code Read 
    Protection level 3 (the worst from our point of view), which does 
    not allow us to read the firmware.
    
    The fuse is stored at firmware position 0x2FC according to the 
    LPC11U35 datasheet. We cannot modify the firmware. So, we need to 
    wait until 0x2FC is loaded into memory and is checked against the 
    CRP3/NO_ISP. (See UM10462, chapter 20.10 Boot process flowchart.)
    
    We want to glitch/change either the appropriate compares, or 
    glitch/change the register containing the value of ROM address 
    0x2FC. The latter is easier since we need to glitch only once.
    
    The LPC11U35 runs at 12 MHz during the boot ROM phase. The boot ROM 
    is 16 kB long. The Arduino Leonardo runs as 16 MHz. If we use an 
    8-bit timer, we have a maximum count of 255. That would equate to 
    some 191 CPU cycles on the LPC11U35. If the boot ROM hasn't checked 
    address 0x2FC, then an 8-bit counter is simply not enough. We could 
    use the overflow interrupt to keep count of the number of times we 
    have overflowed. However, we opted for something a bit simpler.
    
    Long story short: use a regular 16-bit timer to get "close" to where 
    we want to glitch, then change to a high speed 8-bit timer (or 
    10-bit timer) for the final stretch.
    
    The duration of the brownout is probably extremely short. Perhaps 
    even a single high speed timer tick. So do that part with timer 4 
    only, don't use the CPU, don't busy wait.
*/

// We want to spend as little time as possible in the timer interrupts. 
// Since our only goal of the timer interrupts is to WAKE the CPU, we 
// can simply issue "reti" (and a "nop" for alignment).
#define ISR_ELEMENT_SIZE 4
#define ISR_NUM_ELEMENTS (_VECTORS_SIZE / ISR_ELEMENT_SIZE)
#define JMP_00AC {0x0c, 0x94, (_VECTORS_SIZE >> 1), 0x00}
#define JMP_0000 {0x0c, 0x94, 0x00, 0x00}
#define RETI_NOP { 0b00011000, 0b10010101, 0x00, 0x00 }
uint8_t vector[ISR_NUM_ELEMENTS][ISR_ELEMENT_SIZE]
__attribute__((section (".vectors")))
__attribute__((__used__))
=
{
	// RESET
	[ 0] = JMP_00AC,
			[ 1] = JMP_0000,[ 2] = JMP_0000,[ 3] = JMP_0000,
	[ 4] = JMP_0000,[ 5] = JMP_0000,[ 6] = JMP_0000,[ 7] = JMP_0000,
	[ 8] = JMP_0000,[ 9] = JMP_0000,[10] = JMP_0000,[11] = JMP_0000,
	[12] = JMP_0000,[13] = JMP_0000,[14] = JMP_0000,[15] = JMP_0000,
	[16] = JMP_0000,[17] = JMP_0000,[18] = JMP_0000,[19] = JMP_0000,
	// TIMER1 OVF
	[20] = RETI_NOP,
			[21] = JMP_0000,[22] = JMP_0000,[23] = JMP_0000,
	[24] = JMP_0000,[25] = JMP_0000,[26] = JMP_0000,[27] = JMP_0000,
	[28] = JMP_0000,[29] = JMP_0000,[30] = JMP_0000,[31] = JMP_0000,
	[32] = JMP_0000,[33] = JMP_0000,[34] = JMP_0000,[35] = JMP_0000,
	[36] = JMP_0000,[37] = JMP_0000,[38] = JMP_0000,
	// TIMER4 COMPB
							[39] = RETI_NOP,
	[40] = JMP_0000,[41] = JMP_0000,[42] = JMP_0000,
};



void setup_clocks();
void setup_timer1(uint16_t timer_ticks_wanted_before_overflow);
void setup_timer4(
    uint8_t timer_ticks_before_compare_match,
    uint8_t timer_ticks_before_overflow
);

/* The downside of using linker flag "-nostartfiles" is that section 
 .init2 and .init9 are not linked in. Hence, we must borrow code from 
 avr-libc/crt1/gcrt1.S */
inline void _init()
{
	asm(
		   "clr __zero_reg__"
		// Disable interrupts.
		NT "cli"
		// Setup the stack pointer.
		NT "out %[sp_l], %[sp_l_val]"
		NT "out %[sp_h], %[sp_h_val]"
		:
		:
			[sp_l] "i" (0x3d),
			[sp_h] "i" (0x3e),
			[sp_l_val] "r" (RAMEND & 0xff),
			[sp_h_val] "r" (RAMEND >> 8)
	);
}

int main()
{
  _init();
  
  // Disable all USB interrupts. Otherwise an ATMega32U4 may lockup. 
  // (USB_GEN_vect() is still called.)
  cli();
  USBCON = 0;
  UDIEN = 0;
  UEIENX = 0;
  UEINT = 0;
  
  // It is safe to enable interrupts again.
  sei();
  
  // Set needed pins as output.
  GLITCHER_DDR = _BV(GLITCHER_RESET_PIN)
               | _BV(GLITCHER_VSS_REGULAR_PIN)
               | _BV(GLITCHER_VSS_BAD_PIN)
               ;
  
  usart_init(SERIAL_BAUDRATE);
  setup_clocks();
  // See datasheet chapter 7.1. Timers/counters ARE available in idle 
  // mode (but not in any other sleep mode).
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  
  while (1)
  {
    // The port has 8 pins, but only our 3 pins are in use. In other 
    // words: we can simply write 0 to the other pins.
    // 
    // Don't reset the LPC11U35 yet: the previous loop may have been a 
    // successful glitch. So postpone reset until a new command is given 
    // by the user/operator.
    GLITCHER_PORT = GLITCHER_PORT_STATE_RUN_LPC11U35_WITH_REGULAR_VOLTAGE;
    
    usart_transmit_string("READY\r\n");
    
    uint32_t idle_counter = 0;
    // Wait for RX to be completed.
    while (!(UCSR1A & _BV(RXC1)))
    {
      _delay_us(8.0 * 1000 * 1000 / SERIAL_BAUDRATE);
      if (++idle_counter * 8 >= SERIAL_BAUDRATE)
      {
        idle_counter = 0;
        // Indicate we are still alive.
        usart_transmit_char('.');
      }
    }
    
    // RX has completed; we can read a byte. Expect the 'G', which 
    // stands for "glitch".
    char magic_byte = UDR1;
    if (magic_byte != 'G')    {
      usart_transmit_string("FAIL\r\n");
      continue;
    }
#ifdef USART_ECHO
    usart_transmit_string("\r\n");
    usart_transmit_char(magic_byte);
#endif
    
    uint16_t post_reset_ticks_at_96MHz = usart_receive_uint16();
    uint16_t glitch_ticks_at_96MHz     = usart_receive_uint16();
#ifdef USART_ECHO
    usart_transmit_string("\r\n");
#endif
    // Assert enough ticks are available for the post reset.
    if (glitch_ticks_at_96MHz > 150)
    {
      usart_transmit_string("FAIL: GLITCH TOO LONG\r\n");
      continue;
    }
    
    uint16_t timer1_ticks;
    uint8_t  timer4_ticks;

    if (post_reset_ticks_at_96MHz <= MIN_TIMER4_TICKS_BEFORE_OVERFLOW)
    {
      usart_transmit_string("FAIL\r\n");
      continue;
    }
    
    // We subtract ticks here so they won't be part of coarse timer 1. 
    // We will add these ticks to timer 4 later.
    uint16_t post_reset_ticks =
      post_reset_ticks_at_96MHz - MIN_TIMER4_TICKS_BEFORE_OVERFLOW;
    
    // The CPU runs at 16 MHz (or more precise: F_CPU). But the high 
    // speed timer 4 runs at 96 MHz (or more precise: 48 MHz times the 
    // PLL postscalar).
    uint8_t frequency_ratio = 6;
    timer1_ticks =  post_reset_ticks / frequency_ratio;
    timer4_ticks = (post_reset_ticks % frequency_ratio)
                 + MIN_TIMER4_TICKS_BEFORE_OVERFLOW;
    
    // We will start timer 1 precisely 3 CPU cycle later than starting 
    // the LPC11U35, compensate.
    timer1_ticks += 3;
    
    // We need at least 1 CPU cycle for the timer to fire.
    if (timer1_ticks <= TIMER1_INTERRUPT_CPU_CYCLES_BEFORE_STARTING_TIMER4)
    {
      usart_transmit_string("FAIL: POST RESET TOO SHORT\r\n");
      continue;
    }
    timer1_ticks -= TIMER1_INTERRUPT_CPU_CYCLES_BEFORE_STARTING_TIMER4;
    
    setup_timer1(timer1_ticks);
    setup_timer4(timer4_ticks, glitch_ticks_at_96MHz);
    
    
    
    // From the datasheet of the LPC11U35:
    // 
    //   A LOW-going pulse as short as 50 ns on this pin resets the 
    //   device, causing I/O ports and peripherals to take on their 
    //   default states and processor execution to begin at address 0.
    GLITCHER_PORT = GLITCHER_PORT_STATE_RESET_LPC11U35_WITH_REGULAR_VOLTAGE;
    _delay_us(100);
    
    
    // We fancy a very precise glitch. Therefore, we must be able to 
    // count cycles/instructions. And the only way to actually know 
    // which instructions are executed, is to write assembly.
    asm volatile(
      NT ASM_FILE_LINE
      NT";; Start the LPC11U35. (Stop resetting.)"
      NT";; "
      NT";; PORTB = regular voltage for LPC11U35, don't reset."
      NT"out %[port], %[regular_voltage_no_reset]"
      NT
      NT
      NT
      // Note: TCCR registers are outside the reach of the faster OUT 
      // instruction.
      NT ASM_FILE_LINE
      NT";; Start low speed timer."
      NT";;   0x1: divide clock source by    1 (don't divide)"
      NT";;   0x2: divide clock source by    8"
      NT";;   0x3: divide clock source by   64"
      NT";;   0x4: divide clock source by  256"
      NT";;   0x5: divide clock source by 1024"
      NT"sts %[tccr_low_speed], %[cs1_divider]"	// 2 CPU cycles
      NT
      NT
      NT
      // We want the tick count to be as precise as possible. However, 
      // the datasheet says: "If an interrupt occurs during execution of 
      // a multi-cycle instruction, this instruction is completed before 
      // the interrupt is served."
      // 
      // A jump takes at least 2 cycles (RJMP/IJMP/EIJMP). A conditional 
      // branch takes 2 instructions. So a while(!done) loop contains at 
      // least one multi-cycle instruction :-/.
      // 
      // However, the datasheet also states: "If an interrupt occurs 
      // when the MCU is in sleep mode, the interrupt execution response 
      // time is increased by five clock cycles." This is a FIXED number 
      // of cycles :-).
      NT";; Will wake up after servicing timer 1 overflow interrupt."
      NT"sleep"
      NT
      NT
      NT
      // Note: TCCR registers are outside the reach of the faster OUT 
      // instruction.
      NT ASM_FILE_LINE
      NT";; Start the high speed timer (96 MHz)."
      NT";;   0x1: divide clock source by     1 (96 MHz)"
      NT";;   0x1: divide clock source by     2 (48 MHz)"
      NT";;   (...)"
      NT";;   0xE: divide clock source by  8192 (11.718 kHz)"
      NT";;   0xF: divide clock source by 16384 ( 5.859 kHz)"
      NT"sts %[tccr_high_speed], %[cs4_divider]"	// 2 CPU cycles
      NT
      NT
      NT
      NT";; Stop low speed timer."
      NT"sts %[tccr_low_speed], __zero_reg__"
      NT
      NT
      NT
      NT";; Will wake up after servicing timer 4 output compare B interrupt."
      NT"sleep"
      NT
      NT
      NT
      NT";; Stop high speed timer."
      NT"sts %[tccr_high_speed], __zero_reg__"
      //
      //
      // Output operands
      :
      // Input operands
      :
        // I: Constant greater than −1, less than 64
        [port] "I" _SFR_IO_ADDR(GLITCHER_PORT),
        
        // d: Registers from r16 to r31
        // 
        // Without the cast to byte/char/uint8, the compiler assumes a 
        // 16-bit value and uses 2 registers.
        [cs1_divider] "d" ((uint8_t) CS1_DIVIDER),
        [cs4_divider] "d" ((uint8_t) CS4_DIVIDER),
        [regular_voltage_no_reset] "d" ((uint8_t) GLITCHER_PORT_STATE_RUN_LPC11U35_WITH_REGULAR_VOLTAGE),
        
        // M: Constant that fits in 8 bits
        [tccr_low_speed]  "M" (_SFR_MEM_ADDR(TCCR1B)),
        [tccr_high_speed] "M" (_SFR_MEM_ADDR(TCCR4B))
      // Clobbered registers.
    );
    
    usart_transmit_string("DONE\r\n");
  }
}



// This routine serves 2 goals:
// 
//   Main goal: Let high speed timer 4 operate at the maximum frequency 
//   of 96 MHz. I.e. clk_TMR must be 96 MHz.
//   
//   Side goal: The USB must still be usable. I.e. clk_USB must be 48 
//   MHz. (We currently do not use USB, but we could use it as a serial 
//   port.)
inline void setup_clocks()
{
  // The datasheet contains figure 6-1, called "Clock Distribution", 
  // page 27. It shows which clock is connected to which prescaler, 
  // postscaler, multiplexer etc.
  // 
  // The datasheet contains figure 6-6, called "PLL Clocking System", 
  // page 36. It shows which register bits influences which 
  // stage/frequency.
  
  // Bit 3:0 – PDIV3:0 PLL Lock Frequency
  // 
  // 1010 == 96 MHz, the highest allowed PLL Lock Frequency.
  uint8_t pdiv30 = (_BV(PDIV3) * 1)	// PLLFRQ bit 3
                 | (_BV(PDIV2) * 0)	// PLLFRQ bit 2
                 | (_BV(PDIV1) * 1)	// PLLFRQ bit 1
                 | (_BV(PDIV0) * 0)	// PLLFRQ bit 0
                 ;
  // Bit 5:4 – PLLTM1:0: PLL Postcaler for High Speed Timer
  // 
  // 10 == PLL postscaler factor is 1, i.e. clk_TMR is 96 MHz
  uint8_t plltm10 = (_BV(PLLTM1) * 1)	// PLLFRQ bit 5
                  | (_BV(PLLTM0) * 0)	// PLLFRQ bit 4
                  ;
  
  PLLFRQ = (_BV(PINMUX) * 1)	// Bit 7: Use internal RC oscillator
         | (_BV(PLLUSB) * 1)	// Bit 6: Set PLL Postscaler to 2 since PLL output is 96 MHz.
         | plltm10		// Bits 5:4
         | pdiv30		// Bits 3:0
         ;
}



/*  _   _
 * | |_(_)_ __ ___   ___ _ __ ___
 * | __| | '_ ` _ \ / _ \ '__/ __|
 * | |_| | | | | | |  __/ |  \__ \
 *  \__|_|_| |_| |_|\___|_|  |___/
 */

// This routine initialises timer 1 to overflow are after 
// timer_ticks_wanted_before_overflow ticks/cycles. This routine does 
// NOT start the timer.
//
// Corner case: 0 actually means 65536.
inline void setup_timer1(uint16_t timer_ticks_wanted_before_overflow)
{
  uint16_t timer_ticks_left = /* 65536 */ - timer_ticks_wanted_before_overflow;
  TCNT1 = timer_ticks_left;
  
  // Enable the overflow interrupt of timer 1. See ISR(TIMER1_OVF_vect, 
  // ISR_NAKED) for the actual overflow routine.
  // 
  // "TIMSK" means "Timer/Counter1 Interrupt Mask Register"
  // "TOIE1" means "Timer/Counter1 Overflow Interrupt Enable"
  TIMSK1 |= _BV(TOIE1);
}



inline void setup_timer4(
    uint8_t timer_ticks_before_glitch,
    uint8_t timer_ticks_glitch_length
)
{
  // TCCR == Timer/Counter Control Register
  // 
  // From datasheet table 15-11 for COM4B1..0 (fast PWM mode):
  // 
  //   COM4B1 | COM4B0 | OCW4B Behavior           | OC4B Pin  | not(OC4B) Pin
  //   -------+--------+--------------------------+-----------+--------------
  //        0 |      1 | Cleared on Compare Match | Connected | Connected
  //          |        | Set when TCNT4 = 0x000   |           |
  // 
  // 
  // 
  // For us, this means the following:
  // 
  //   Event            | OC4B pin  | not(OC4B)  |
  //                    | (bad VSS) | (good VSS) |
  //   -----------------+-----------+------------+
  //   Timer overflow   | Turns ON  | Turns OFF  |
  //   Compare Match 4B | Turns OFF | Turns ON   |
  TCCR4A = _BV(COM4B0)	// Cleared on Compare Match.
         | _BV(PWM4B)	// Enables PWM mode based on comparator OCR4B.
         ;
  
  // Fast PWM is also known as "single slope".
  TCCR4D = (0 << WGM41)	// Necessary for fast PWM mode.
         | (0 << WGM40)	// Necessary for fast PWM mode.
         ;

  // Set TOP for fast PWM. Make it an 8-bit counter, so we can use 8-bit 
  // arithmetic. (We don't need 10-bit precision.)
  TC4H = 0;
  OCR4C = 0xFF;
  
  // From the datasheet, chapter 15.11 ("Accessing 10-bit Registers"):
  // 
  //   To do a 10-bit write, the high byte must be written to the TC4H 
  //   register before the low byte is written.
  TC4H = 0;
  TCNT4L = /* 256 */ - timer_ticks_before_glitch;
  OCR4B = +timer_ticks_glitch_length;
  
  // Enable the overflow interrupt of timer 4. See ISR(TIMER4_OVF_vect, 
  // ISR_NAKED) for the actual overflow routine.
  // 
  // "TIMSK" means "Timer/Counter4 Interrupt Mask Register"
  // "OCIE4B" means "Timer/Counter4 Output Compare Interrupt Enable"
  TIMSK4 |= _BV(OCIE4B);
}
