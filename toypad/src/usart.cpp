#include "usart.hpp"
#include <avr/io.h>



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