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
// Since our only goal of the interrupts is to WAKE the CPU, we can 
// simply issue "reti" (and a "nop" for alignment).
asm(
	".section .vectors"
	"\n" ".global __vectors"
	"\n\t" ".type __vectors, @function"
	"\n" "__vectors:"
	// Note: first address after jump table is 0x00AC.
					// Note: Datasheet starts counting at 1.
					//
					// Vector No. | Address | Source       | Interrupt Definition
					// -----------+---------+--------------+---------------------
	"\n\t" "jmp 0x00ac"		//         1  |  0x0000 | RESET        | External Pin, Power-on Reset, Brown-out Reset, Watchdog Reset, and JTAG AVR Reset
	"\n\t" "jmp __vector_default"	//         2  |  0x0004 | INT0         | External Interrupt Request 0
	"\n\t" "jmp __vector_default"	//         3  |  0x0008 | INT1         | External Interrupt Request 1
	"\n\t" "jmp __vector_default"	//         4  |  0x000C | INT2         | External Interrupt Request 2
	"\n\t" "jmp __vector_default"	//         5  |  0x0010 | INT3         | External Interrupt Request 3
	"\n\t" "jmp __vector_default"	//         6  |  0x0014 | Reserved     | Reserved
	"\n\t" "jmp __vector_default"	//         7  |  0x0018 | Reserved     | Reserved
	"\n\t" "jmp __vector_default"	//         8  |  0x001C | INT6         | External Interrupt Request 6
	"\n\t" "jmp __vector_default"	//         9  |  0x0020 | Reserved     | Reserved
	"\n\t" "jmp __vector_default"	//        10  |  0x0024 | PCINT0       | Pin Change Interrupt Request 0
	"\n\t" "jmp __vector_default"	//        11  |  0x0028 | USB General  | USB General Interrupt request
	"\n\t" "jmp __vector_default"	//        12  |  0x002C | USB Endpoint | USB Endpoint Interrupt request
	"\n\t" "jmp __vector_default"	//        13  |  0x0030 | WDT          | Watchdog Time-out Interrupt
	"\n\t" "jmp __vector_default"	//        14  |  0x0034 | Reserved     | Reserved
	"\n\t" "jmp __vector_default"	//        15  |  0x0038 | Reserved     | Reserved
	"\n\t" "jmp __vector_default"	//        16  |  0x003C | Reserved     | Reserved
	"\n\t" "jmp __vector_default"	//        17  |  0x0040 | TIMER1 CAPT  | Timer/Counter1 Capture Event
	"\n\t" "jmp __vector_default"	//        18  |  0x0044 | TIMER1 COMPA | Timer/Counter1 Compare Match A
	"\n\t" "jmp __vector_default"	//        19  |  0x0048 | TIMER1 COMPB | Timer/Counter1 Compare Match B
	"\n\t" "jmp __vector_default"	//        20  |  0x004C | TIMER1 COMPC | Timer/Counter1 Compare Match C
	"\n\t" "reti"			//        21  |  0x0050 | TIMER1 OVF   | Timer/Counter1 Overflow
	"\n\t" "nop"
	"\n\t" "jmp __vector_default"	//        22  |  0x0054 | TIMER0 COMPA | Timer/Counter0 Compare Match A
	"\n\t" "jmp __vector_default"	//        23  |  0x0058 | TIMER0 COMPB | Timer/Counter0 Compare match B
	"\n\t" "jmp __vector_default"	//        24  |  0x005C | TIMER0 OVF   | Timer/Counter0 Overflow
	"\n\t" "jmp __vector_default"	//        25  |  0x0060 | SPI (STC)    | SPI Serial Transfer Complete
	"\n\t" "jmp __vector_default"	//        26  |  0x0064 | USART1 RX    | USART1 Rx Complete
	"\n\t" "jmp __vector_default"	//        27  |  0x0068 | USART1 UDR   | EUSART1 Data Register Empty
	"\n\t" "jmp __vector_default"	//        28  |  0x006C | USART1TX     | USART1 Tx Complete
	"\n\t" "jmp __vector_default"	//        29  |  0x0070 | ANALOG COMP  | Analog Comparator
	"\n\t" "jmp __vector_default"	//        30  |  0x0074 | ADC          | ADC Conversion Complete
	"\n\t" "jmp __vector_default"	//        31  |  0x0078 | EE READY     | EEPROM Ready
	"\n\t" "jmp __vector_default"	//        32  |  0x007C | TIMER3 CAPT  | Timer/Counter3 Capture Event
	"\n\t" "jmp __vector_default"	//        33  |  0x0080 | TIMER3 COMPA | Timer/Counter3 Compare Match A
	"\n\t" "jmp __vector_default"	//        34  |  0x0084 | TIMER3 COMPB | Timer/Counter3 Compare Match B
	"\n\t" "jmp __vector_default"	//        35  |  0x0088 | TIMER3 COMPC | Timer/Counter3 Compare Match C
	"\n\t" "jmp __vector_default"	//        36  |  0x008C | TIMER3 OVF   | Timer/Counter3 Overflow
	"\n\t" "jmp __vector_default"	//        37  |  0x0090 | TWI          | 2-wire Serial Interface
	"\n\t" "jmp __vector_default"	//        38  |  0x0094 | SPM READY    | Store Program Memory Ready
	"\n\t" "jmp __vector_default"	//        39  |  0x0098 | TIMER4 COMPA | Timer/Counter4 Compare Match A
	"\n\t" "reti"			//        40  |  0x009C | TIMER4 COMPB | Timer/Counter4 Compare Match B
	"\n\t" "nop"
	"\n\t" "jmp __vector_default"	//        41  |  0x00A0 | TIMER4 COMPD | Timer/Counter4 Compare Match D
	"\n\t" "jmp __vector_default"	//        42  |  0x00A4 | TIMER4 OVF   | Timer/Counter4 Overflow
	"\n\t" "jmp __vector_default"	//        43  |  0x00A8 | TIMER4 FPF   | Timer/Counter4 Fault Protection Interrupt
					//               0x00AC --> First address after interrupt table.
	
	
	
	// Import section ".init2" manually from gcrt1.S
	"\n" ".section .init2"
	"\n\t" "clr __zero_reg__"		// clr	__zero_reg__
	
	// SREG = 0, disables interrupts. "cli" would have done the job 
	// as well...
	"\n\t" "out 0x3f, __zero_reg__"		// out	AVR_STATUS_ADDR, __zero_reg__
	
	// ATmega32u4 has 0xB00 bytes of RAM. Initialize the stack 
	// pointer to the highest address.
	// 
	// 0x3e == SPH, stack pointer, high byte
	// 0x3d == SPL, stack pointer, low byte
	"\n\t" ".set __stack, 0xAFF"
	"\n\t" "ldi r28, lo8(__stack)"		// ldi	r28,lo8(__stack)
	"\n\t" "ldi r29, hi8(__stack)"		// ldi	r29,hi8(__stack)
	"\n\t" "out 0x3e, r29"			// out	AVR_STACK_POINTER_HI_ADDR, r29
	"\n\t" "out 0x3d, r28"			// out	AVR_STACK_POINTER_HI_ADDR, r29
	
	
	
	// Import section ".init9" manually from gcrt1.S
	"\n" ".section .init9"
	"\n\t" "call main"
	"\n\t" "jmp exit"
	
	
	
	"\n" ".section .text"
);
ISR(BADISR_vect, ISR_NAKED) { asm("jmp 0x0000"); }



void setup_clocks();
void setup_timer1(uint16_t timer_ticks_wanted_before_overflow);
void setup_timer4(
    uint8_t timer_ticks_before_compare_match,
    uint8_t timer_ticks_before_overflow
);


int main()
{
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
  GLITCHER_DDR = _BV(GLITCHER_NOT_RESET_PIN)
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
    GLITCHER_PORT = GLITCHER_PORT_STATE_RESET_LPC11U35_WITH_REGULAR_VOLTAGE;
    
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
    if (glitch_ticks_at_96MHz > 15)
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
    
    
    
    // Start the LPC11U35.
    GLITCHER_PORT = GLITCHER_PORT_STATE_RUN_LPC11U35_WITH_REGULAR_VOLTAGE;
    
    register uint8_t tmpreg;
#define NT "\n\t"
    asm volatile(
      // Start timer 1, this is the coarse counter.
      //
      // Note: 0x81 is outside the reach of the faster OUT instruction.
      NT";; TCCR1B = CS1_DIVIDER"
      NT"ldi %[tmpreg], " STRINGIFY(CS1_DIVIDER)	// 1 CPU cycle
      NT"sts 0x81, %[tmpreg]"				// 2 CPU cycles
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
      // Start timer 4, this is the high speed timer (96 MHz).
      //
      // Note: 0xC1 is outside the reach of the faster OUT instruction.
      NT";; TCCR4B = CS4_DIVIDER"
      NT"ldi %[tmpreg], " STRINGIFY(CS4_DIVIDER)	// 1 CPU cycle
      NT"sts 0xC1, %[tmpreg]"				// 2 CPU cycles
      NT
      NT
      NT
      // Stop timer 1.
      NT";; TCCR1B = 0"
      NT"sts 0x81, __zero_reg__"
      NT
      NT
      NT
      NT";; Will wake up after servicing timer 4 output compare B interrupt."
      NT"sleep"
      NT
      NT
      NT
      // Stop timer 4.
      NT";; TCCR4B = 0"
      NT"sts 0xC1, __zero_reg__"
      //
      //
      // Output operands
      : [tmpreg] "=r" (tmpreg)
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
