#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/avr_mcu_section.h>
#include <util/delay.h>

//#define SERIAL_BAUDRATE (9600L)
#define SERIAL_BAUDRATE (500L * 1000L)

// We need 3 pins to communicate to the glitcher and the LPC11U35:
// 
//   1. To the RESET port of the LPC11U35. (Note: Active low.)
//   2. To the MOSFET supplying the normal voltage to the LPC11U35.
//   3. To the MOSFET supplying the bad voltage to the LPC11U35.
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
// The obvious choice is to use OC4B, since it is available on both 
// boards.
// 
// The choice for the RESET port is based on aesthetics; just use a 
// neighbouring pin. D8 is available on both boards (D11 is not). D8 is 
// connected to ATmega pin PB4.

#define GLITCHER_RESET_PORT	PORTB
#define GLITCHER_RESET_DDR	DDRB
#define GLITCHER_RESET_PIN	PINB4

#define GLITCHER_VSS_DDR	DDRB
#define GLITCHER_VSS_PORT	PORTB
#define GLITCHER_VSS_NORMAL_PIN	PINB6
#define GLITCHER_VSS_BAD_PIN	PINB5

#define CS1_STOP		(        0 |         0 |         0)
#define CS1_DIVIDER_1		(        0 |         0 | _BV(CS10))
#define CS1_DIVIDER_8		(        0 | _BV(CS11) |        0 )
#define CS1_DIVIDER_64		(        0 | _BV(CS11) | _BV(CS10))
#define CS1_DIVIDER_256		(_BV(CS12) |         0 |        0 )
#define CS1_DIVIDER_1024	(_BV(CS12) |         0 | _BV(CS10))

#define CS4_STOP		(        0 |         0 |         0 |         0)
#define CS4_DIVIDER_1		(        0 |         0 |         0 | _BV(CS40))
#define CS4_DIVIDER_2		(        0 |         0 | _BV(CS41) |         0)
#define CS4_DIVIDER_8192	(_BV(CS43) | _BV(CS42) | _BV(CS41) |         0)
#define CS4_DIVIDER_16384	(_BV(CS43) | _BV(CS42) | _BV(CS41) | _BV(CS40))

#define USART_ECHO



void usart_init(uint32_t baudrate=SERIAL_BAUDRATE);
void usart_transmit_char(char ch);
void usart_transmit_string(char const ch[]);
uint16_t usart_receive_uint16();
void usart_dump_byte(uint8_t byte);

void setup_timer1(uint16_t timer_ticks_wanted_before_overflow);
void setup_timer4(
    uint8_t timer_ticks_before_compare_match,
    uint8_t timer_ticks_before_overflow
);
volatile bool global__glitch_stage_not_done;



// This routine serves 2 goals:
// 
//   Main goal: Let high speed timer 4 operate at the maximum frequency 
//   of 96 MHz. I.e. clk_TMR must be 96 MHz.
//   
//   Side goal: The USB must still be usable. I.e. clk_USB must be 48 
//   MHz.
void setup_clocks()
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



int main(void)
{
  // Disable all USB interrupts. Otherwise an ATMega32U4 may lockup.
  cli();
  USBCON = 0;
  UDIEN = 0;
  UEIENX = 0;
  UEINT = 0;
  
  // It is safe to enable interrupts again.
  sei();
  
  // Set needed pins as output.
  GLITCHER_RESET_DDR |= _BV(GLITCHER_RESET_PIN);
  GLITCHER_VSS_DDR |= _BV(GLITCHER_VSS_NORMAL_PIN)
                   |  _BV(GLITCHER_VSS_BAD_PIN)
                   ;
  
  usart_init();
  
  setup_clocks();
  
  while (1)
  {
    // 0 on reset on LP11U35 means: RESET (active low)
    GLITCHER_RESET_PORT &= ~_BV(GLITCHER_RESET_PIN);
    
    // DISABLE bad voltage supply.
    GLITCHER_VSS_PORT &= ~_BV(GLITCHER_VSS_BAD_PIN);
    
    // Power on the LPC11U35 (and keep resetting).
    GLITCHER_VSS_PORT |= _BV(GLITCHER_VSS_NORMAL_PIN);
    
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
    if (magic_byte != 'G')
    {
      usart_transmit_string("FAIL\r\n");
      continue;
    }
#ifdef USART_ECHO
    usart_transmit_string("\r\n");
    usart_transmit_char(magic_byte);
#endif
    
/*
    This is the idea:

    The LPC11U35 has a boot ROM in which it checks fuses. We want to 
    change the outcome of 1 particular fuse check: the CRP level check. 
    "CRP" stands for "Code Read Protection". There are only 4 special 
    values:
    
        0x4E697370 "NO_ISP"
        0x12345678 "CRP1"
        0x87654321 "CRP2"
        0x43218765 "CRP3"
    
    ALL the other values mean "no code protection whatsoever". The LEGO 
    Dimensions toypad is programmed with Code Read Protection level 3 
    (the worst from our point of view), which does not allow us to read 
    the firmware.
    
    The fuse is stored at firmware position 0x2FC according to the 
    datasheet. We cannot modify the firmware. So, we need to wait until 
    0x2FC is loaded into memory and is checked against the CRP3/NO_ISP. 
    (See UM10462, chapter 20.10 Boot process flowchart.)
    
    We want to glitch/change either the appropriate compares, or 
    glitch/change the register containing the value of ROM address 
    0x2FC. The latter is easier since we need to glitch only once.
    
    The LPC11U35 runs at 12 MHz during the boot ROM phase. The boot ROM 
    is 16 kB long. The Arduino Leonardo runs as 16 MHz. If we use an 8 
    bit timer, we have a maximum count of 255. That would equate to some 
    191 CPU cycles on the LPC11U35. If the boot ROM hasn't check address 
    0x2FC, then an 8-bit counter is simply not enough. We could use the 
    overflow interrupt to keep count of the number of times we have 
    overflowed. However we opted for something a bit simpler.
    
    Long story short: use a regular 16-bit timer to get "close" to where 
    we want to glitch, then change to a high speed 8-bit timer (or 
    10-bit timer) for the final stretch.
    
    The duration of the brownout is probably extremely short. Perhaps 
    even a single high speed timer tick. So do that part with timer 4 
    only, don't use the CPU, don't busy wait.
*/
    uint16_t post_reset_ticks_at_96MHz = usart_receive_uint16();
    uint16_t glitch_ticks_at_96MHz = usart_receive_uint16();
#ifdef USART_ECHO
    usart_transmit_string("\r\n");
#endif
    // Assert enough ticks are available for the post reset.
    if (glitch_ticks_at_96MHz > 15)
    {
      usart_transmit_string("FAIL: GLITCH TOO LONG\r\n");
      continue;
    }
    
    // The interrupt routine of timer 4 switches this boolean to false 
    // when the glitching stage is over.
    global__glitch_stage_not_done = true;
    
    uint16_t timer1_ticks;
    uint8_t  timer4_ticks;

    // We only want to glitch once, i.e. we have to prevent doing a 
    // glitch twice, so we have to be ABLE to stop timer 4 in the 
    // interrupt routine. That means enough ticks between the initial 
    // value of timer/counter4 and the compare match. 64 ticks should 
    // be more than enough.
#define MIN_TIMER4_TICKS_BEFORE_COMPARE_MATCH 64
    if (post_reset_ticks_at_96MHz <= 64)
    {
      usart_transmit_string("FAIL\r\n");
      continue;
    }
    uint16_t post_reset_ticks =
      post_reset_ticks_at_96MHz - MIN_TIMER4_TICKS_BEFORE_COMPARE_MATCH;
    
    // The CPU runs at 16 MHz (or more precise: F_CPU). But the high 
    // speed timer 4 runs at 96 MHz (or more precise: 48 MHz times the 
    // PLL postscalar).
    uint8_t frequency_ratio = 6;
    timer1_ticks =  post_reset_ticks / frequency_ratio;
    timer4_ticks = (post_reset_ticks % frequency_ratio) + MIN_TIMER4_TICKS_BEFORE_COMPARE_MATCH;
    
#define TIMER1_INTERRUPT_TICKS_BEFORE_STARTING_TIMER4 10
    if (timer1_ticks <= TIMER1_INTERRUPT_TICKS_BEFORE_STARTING_TIMER4)
    {
      usart_transmit_string("FAIL: POST RESET TOO SHORT\r\n");
      continue;
    }
    timer1_ticks -= TIMER1_INTERRUPT_TICKS_BEFORE_STARTING_TIMER4;
    
    setup_timer1(timer1_ticks);
    setup_timer4(timer4_ticks, glitch_ticks_at_96MHz);
    
    // Start the LPC11U35.
    //
    // Note: the RESET pin of LPC11U35 is active low, so:
    // 
    //   GLITCHER_RESET_PIN | Result on LPC11U35
    //   -------------------+-------------------
    //   1 / high / on      | Not resetting, i.e. run normally.
    //   1 / low / off      | Resetting, i.e. does not run.
    GLITCHER_RESET_PORT |= _BV(GLITCHER_RESET_PIN);
    
    // Start the coarse counter. Note: the coarse counter starts high 
    // speed timer 4.
    //TCCR1B = CS1_DIVIDER_1024;
    TCCR1B = CS1_DIVIDER_1;
    
    // Just busy wait for completion.
    while (global__glitch_stage_not_done)
    {}
    usart_transmit_string("DONE\r\n");
  }
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
  uint16_t timer_ticks_left = -timer_ticks_wanted_before_overflow;
  TCNT1 = timer_ticks_left;
  
  // Enable the overflow interrupt of timer 1. See ISR(TIMER1_OVF_vect, 
  // ISR_NAKED) for the actual overflow routine.
  // 
  // "TIMSK" means "Timer/Counter1 Interrupt Mask Register"
  // "TOIE1" means "Timer/Counter1 Overflow Interrupt Enable"
  TIMSK1 |= _BV(TOIE1);
}

// This interrupt routine is called when timer 1 overflows. For us, this 
// means that the coarse waiting is over. Switch to timer 4 for 
// precision waiting and glitching (both at 96 MHz).
ISR(TIMER1_OVF_vect, ISR_NAKED)
{
  // Timing: https://www.gammon.com.au/interrupts: "the minimal amount 
  // of time to service an interrupt is 4 clock cycles (to push the 
  // current program counter onto the stack) followed by the code now 
  // executing at the interrupt vector location. This normally contains 
  // a jump to where the interrupt routine really is, which is another 3 
  // cycles."
  // 
  // The ATmega32U4 has an AVRe+ core.
  //
  // We have spend 7 CPU cycles so far.

  // Start timer/counter 4.
  //TCCR4B = CS4_DIVIDER_16384;
  TCCR4B = CS4_DIVIDER_1;
  // 3 CPU cycles:
  //   ldi r24,lo8(15)
  //   sts 193,r24
  
  // Total number of CPU cycles before timer/counter 4 starts:
  //   7 + 3 = 10
  
  
  
  // Stop timer/counter 1.
  TCCR1B = 0;
  // 2 CPU cycles:
  //   sts 129,__zero_reg__
  
  // This interrupt routine was marked as "NAKED", so we have to return 
  // manually.
  reti();
  // 4 CPU cycles:
  //   reti
}



inline void setup_timer4(
    uint8_t timer_ticks_before_compare_match,
    uint8_t timer_ticks_before_overflow
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
  //   Event          | OC4B pin (good VSS) | not(OC4B) pin (bad VSS) |
  //   ---------------+---------------------+-------------------------+
  //   Compare Match  | Turns OFF           | Turns ON                |
  //   Timer overflow | Turns ON            | Turns OFF               |
  TCCR4A = _BV(COM4B0)	// Cleared on Compare Match.
         | _BV(PWM4B)	// Enables PWM mode based on comparator OCR4B.
         ;
  
  // Fast PWM is also known as "single slope".
  TCCR4D = (0 << WGM41)	// Necessary for fast PWM mode.
         | (0 << WGM40)	// Necessary for fast PWM mode.
         ;

  // Set TOP for fast PWM. Make it an 8-bit counter, so we can use 8-bit 
  // arithmetic. (We don't need 10-bit precision.)
  OCR4C = 0xFF;
  
  uint8_t timer_ticks_left_before_overflow = -timer_ticks_before_overflow;
  OCR4B = timer_ticks_left_before_overflow;
  
  uint8_t timer_ticks_left_before_compare_match = timer_ticks_left_before_overflow - timer_ticks_before_compare_match;
  TCNT4 = timer_ticks_left_before_compare_match;
  // Enable the overflow interrupt of timer 4. See ISR(TIMER4_OVF_vect, 
  // ISR_NAKED) for the actual overflow routine.
  // 
  // "TIMSK" means "Timer/Counter4 Interrupt Mask Register"
  // "TOIE4" means "Timer/Counter4 Overflow Interrupt Enable"
  TIMSK4 |= _BV(TOIE4);
}



// This interrupt routine is called when timer 4 overflows. For us, this 
// means that the voltage glitch has completed. (So no need to stop the 
// glitching manually.) Timer 4 will continue to count and therefore 
// will perform another voltage glitch stage. We must prevent another 
// glitch from happening. Tl;dr: stop counting.
ISR(TIMER4_OVF_vect, ISR_NAKED)
{
  // We want to stop counting as fast as possible, so simply set ALL 
  // bits to 0, instead of ONLY the clock select bits (which would 
  // require reading TCCR4B first).
  TCCR4B = 0;
  
  // Mark that the glitching stage is over, so main loop can test this 
  // boolean to see in which phase we are.
  global__glitch_stage_not_done = false;
  
  // This interrupt routine was marked as "NAKED", so we have to return 
  // manually.
  reti();
}



/*  _   _ ____    _    ____ _____
 * | | | / ___|  / \  |  _ \_   _|
 * | | | \___ \ / _ \ | |_) || |
 * | |_| |___) / ___ \|  _ < | |
 *  \___/|____/_/   \_\_| \_\|_|
 */
void usart_init(uint32_t baudrate)
{
  // Set the baudrate. The datasheet says:
  // 
  //   This clock is the baud rate generator clock output
  //     (= f osc / (16(UBRRn+1))).
  uint32_t ubrr = F_CPU / (16 * baudrate) - 1;
  UBRR1H = (ubrr >> 8);
  UBRR1L = (ubrr & 0xff);
  
  // The only writable bits of UCSR1A are:
  // 
  // Bit 6 – TXCn: USART Transmit Complete. "or it can be cleared by 
  // writing a one to its bit location." We don't need that.
  // 
  // Bit 1 – U2Xn: Double the USART Transmission Speed. We don't need 
  // that.
  // 
  // Bit 0 – MPCMn: Multi-processor Communication Mode. We definitely 
  // don't need that feature.
  UCSR1A = 0;
  
  // Note:
  //
  // Bit 2 – UCSZn2: Character Size n: we want 8 bit words, so this 
  // bit must be 0. (UCSZ1 must be 0x3 for 8 bit words.)
  UCSR1B = (_BV(RXCIE1)	* 0)	// 0x80: RX Complete Interrupt Enable 1
         | (_BV(TXCIE1) * 0)	// 0x40: TX Complete Interrupt Enable 1
         | (_BV(UDRIE1) * 0)	// 0x20: USART Data Register Empty Interrupt Enable 1
         | (_BV(RXEN1)  * 1)	// 0x10: Receiver Enable 1
         | (_BV(TXEN1)  * 1)	// 0x08: Transmitter Enable 1
         | (_BV(UCSZ12) * 0)	// 0x04: Character Size 1
         | (_BV(RXB81)  * 0)	// 0x02: Receive Data Bit 8 1
         | (_BV(TXB81)  * 0)	// 0x01: Transmit Data Bit 8 1
         ;
  
  UCSR1C = _BV(UCSZ11)
         | _BV(UCSZ10)
         ;
  
  UCSR1D = 0x00;
}



void usart_transmit_char(char ch)
{
  // UDRE = USART Data Register Empty
  while (!(UCSR1A & _BV(UDRE1)))
  {
    // No-op, spinning wait.
  }
  // Post: Data Register is empty/ready.
  
  UDR1 = ch;
}



void usart_transmit_string(char const ch[])
{
  while (*ch != '\0')
  {
    usart_transmit_char(*ch);
    ++ch;
  }
}



uint16_t usart_receive_uint16()
{
  uint16_t ret = 0;
  
  while (1)
  {
    // Wait for RX to be completed.
    while (!(UCSR1A & _BV(RXC1)))
    {}
    
    uint8_t byte = UDR1;
#ifdef USART_ECHO
    usart_transmit_char(byte);
#endif
    if (byte < '0' || byte > '9')
      return ret;
    
    ret = ret * 10 + (byte - '0');
  }
}



void usart_dump_byte(uint8_t byte)
{
  for (uint8_t u = 0; u < 8; ++u)
  {
    uint8_t v = 7 - u;
    usart_transmit_char('0' + ((byte & _BV(v)) >> v));
  }
}
