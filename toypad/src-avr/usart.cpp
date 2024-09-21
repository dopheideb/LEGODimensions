#include "usart.hpp"
#include <avr/io.h>



// When running with F_CPU = 16 MHz with U2Xn = 0:
// 
// Wanted baudrate | Nearest UBBRn | Actual baudrate | Error
// ----------------+---------------+-----------------+-------------
//       1,000,000 |             0 |       1,000,000 |  0 (perfect!)
//         500,000 |             1 |         500,000 |  0 (perfect!)
//         250,000 |             3 |         250,000 |  0 (perfect!)
//         230,400 |             3 |         250,000 | +8.5%
//         200,000 |             4 |         200,000 |  0 (perfect!)
//         125,000 |             7 |         125,000 |  0 (perfect!)
//         115,200 |             8 |         111,111 | -3.5%
//         100,000 |             9 |         100,000 |  0 (perfect!)
//          57,600 |            16 |          58,823 | +2.1%
//          38,400 |            25 |          38,461 | +0.2%
//          19,200 |            51 |          19,230 | +0.2%
//           9,600 |           103 |           9,615 | +0.2%
void usart_init(uint32_t baudrate)
{
  // Set the baudrate. The datasheet says in table 18-1 for U2Xn = 0:
  // 
  //   UBBRn = f_{OSC} / (16 * BAUD) - 1
  // 
  // Version that implicitly floors the answer:
  //uint16_t ubrr = F_CPU / (16 * baudrate) - 1;
  // Version that rounds the answer:
  //uint16_t ubrr = (((F_CPU << 1) + 1) / (16 * baudrate) - 1) >> 1;

  // Using U2X never makes it less accurate, and usually makes it more
  // accurate. So we use that.
  // The datasheet says: UBRRn = f_{OSC} / (8 * BAUD) - 1
  // The division must be rounded for the best value.
  uint16_t ubrr = (F_CPU + 4 * baudrate) / (8 * baudrate) - 1;
  UBRR1H = (ubrr >> 8);
  UBRR1L = (ubrr & 0xff);

  
  // The only writable bits of UCSR1A are:
  // 
  // Bit 6 – TXCn: USART Transmit Complete. "or it can be cleared by 
  // writing a one to its bit location." We don't need that.
  // 
  // Bit 1 – U2Xn: Double the USART Transmission Speed. We do need 
  // that.
  // 
  // Bit 0 – MPCMn: Multi-processor Communication Mode. We definitely 
  // don't need that feature.
  UCSR1A = (_BV(TXC1) * 0)	// 0x60: clear pending transmit interrupt 1
	  | (_BV(U2X1) * 1)	// 0x02: double transmission speed 1
	  | (_BV(MPCM1) * 0)	// 0x01: multi processor communication mode 1
	  ;
  
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
