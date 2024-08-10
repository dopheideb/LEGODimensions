#include <avr/io.h>
#include <avr/avr_mcu_section.h>
#include <util/delay.h>

//#define SERIAL_BAUDRATE (9600L)
#define SERIAL_BAUDRATE (500L * 1000L)

// PC7 is the user LED on the Arduino Leonardo.
#define GLITCHER_RESET_PORT	PORTC
#define GLITCHER_RESET_DDR	DDRC
#define GLITCHER_RESET_PIN	PINC7

#define GLITCHER_VSS_DDR	DDRD
#define GLITCHER_VSS_PORT	PORTD
// PD7 == OC4D (Arduino Leonardo: pin D6)
#define GLITCHER_VSS_NORMAL_PIN		PIND7
// PD6 == not(OC4D) (Arduino Leonardo: pin D12)
#define GLITCHER_VSS_BAD_PIN		PIND6

#define CS4_DIV_1	(        0 |         0 |         0 | _BV(CS40))
#define CS4_DIV_16384	(_BV(CS43) | _BV(CS42) | _BV(CS41) | _BV(CS40))

#define PLL_POSTSCALER_HIGH_SPEED_TIMER_0	(0)
#define PLL_POSTSCALER_HIGH_SPEED_TIMER_1	(_BV(PLLTM0))
#define PLL_POSTSCALER_HIGH_SPEED_TIMER_15	(_BV(PLLTM1))
#define PLL_POSTSCALER_HIGH_SPEED_TIMER_2	(_BV(PLLTM1) | _BV(PLLTM0))

#define USART_ECHO



void usart_init(uint32_t baudrate=SERIAL_BAUDRATE)
{
  // Set the baudrate.
  // 
  // The datasheet says:
  // 
  // This clock is the baud rate generator clock output
  //   (= f osc / (16(UBRRn+1))).
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
  
  // 0x80: Bit 7 – RXCIEn: RX Complete Interrupt Enable n
  // 0x10: Bit 4 – RXENn: Receiver Enable n
  // 0x08: Bit 3 – TXENn: Transmitter Enable n
  //
  // Note:
  //
  // Bit 2 – UCSZn2: Character Size n: we want 8 bit words, so this 
  // bit must be 0. (UCSZ1 must be 0x3 for 8 bit words.)
  UCSR1B = _BV(RXCIE1)	// 0x80: RX Complete Interrupt Enable 1
         | _BV(RXEN1)	// 0x10: Receiver Enable 1
         | _BV(TXEN1)	// 0x08: Transmitter Enable 1
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



int main(void)
{
  // Disable all USB interrupts. Otherwise an ATMega32U4 may lockup.
  USBCON = 0;
  UDIEN = 0;
  UEIENX = 0;
  UEINT = 0;
  
  // Set PLL postscaler to 2. This implies timer4 uses ansynchronous 
  // mode. See datasheet 15.3.1 "Counter Initialization for Asynchronous 
  // Mode".
  // 
  // PLL operates at 48 MHz, so timer 4 operates at 96 MHz.
  //PLLFRQ |= PLL_POSTSCALER_HIGH_SPEED_TIMER_0;
  //PLLFRQ |= PLL_POSTSCALER_HIGH_SPEED_TIMER_1;
  //PLLFRQ |= PLL_POSTSCALER_HIGH_SPEED_TIMER_15;
  PLLFRQ |= PLL_POSTSCALER_HIGH_SPEED_TIMER_2;
  
  // Set needed pins as output.
  GLITCHER_RESET_DDR |= _BV(GLITCHER_RESET_PIN);
  GLITCHER_VSS_DDR |= _BV(GLITCHER_VSS_NORMAL_PIN)
                   |  _BV(GLITCHER_VSS_BAD_PIN)
                   ;
  
  usart_init();
  
  while (1)
  {
    // 0 on reset on LP11U35 means: RESET (active high)
    GLITCHER_RESET_PORT &= ~_BV(GLITCHER_RESET_PIN);
    
    // DISABLE bad voltage supply.
    GLITCHER_VSS_PORT &= ~_BV(GLITCHER_VSS_BAD_PIN);
    
    // Power the LPC11U35 (and keep resetting).
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
    usart_transmit_char(magic_byte);
#endif
    
    uint16_t post_reset_delay_cycles  = usart_receive_uint16();
    uint16_t brownout_duration_cycles = usart_receive_uint16();
#ifdef USART_ECHO
    usart_transmit_string("\r\n");
#endif
    
    // TCCR == Timer/Counter Control Register
    // 
    // See table 15-16 for COM4D1..0
    //   01:
    //     Cleared on Compare Match
    //     Set when TCNT4 = 0x000
    //     OC4D Pin: Connected
    //     not(OC4D) Pin: Connected
    TCCR4C = _BV(COM4D0)	// Cleared on Compare Match.
           | _BV(PWM4D)		// Enables PWM mode based on comparator OCR4D.
           ;
    // Fast PWM is also known as "single slope".
    TCCR4D = (0 << WGM41)	// Necessary for fast PWM mode.
           | (0 << WGM40)	// Necessary for fast PWM mode.
           ;
    OCR4C = 0xFF;				// Set TOP for fast PWM.
    OCR4D = post_reset_delay_cycles & 0xFF;
    
    // Reset counter.
    TCNT4 = 0;
    
    // End the reset of the LPC11U35 on the LEGO Dimensions toypad. 
    // Remember: the RESET is active low, so high means not resetting.
    GLITCHER_RESET_PORT |= _BV(GLITCHER_RESET_PIN);
    
    // Start counting. CS = Clock Select.
    //TCCR4B = CS4_DIV_16384;
    TCCR4B = CS4_DIV_1;
    
    // Wait until the post reset delay is done.
    while ((TIFR4 & _BV(OCF4D)) == 0)
    {}
    // Post reset delay is done.
    // Also:
    //   Pin OC4D is cleared.
    
    // Start the brownout phase.
    TCCR4B = 0;	// No CS, stops counter.
    OCR4D = brownout_duration_cycles & 0xFF;
    TIFR4 |= _BV(OCF4D);	// Clear overflow flag.
    TCNT4 = 0;			// Reset counter.
    TCCR4B = _BV(PWM4X)		// PWM Inversion Mode.
           //| CS4_DIV_16384;	// Start counting.
           | CS4_DIV_1;		// Start counting.
    
    while ((TIFR4 & _BV(OCF4D)) == 0)
    {}
    TCCR4B = 0;			// Stop counter.
    TIFR4 |= _BV(OCF4D);	// Clear overflow.
    usart_transmit_string("DONE\r\n");
  }
}
