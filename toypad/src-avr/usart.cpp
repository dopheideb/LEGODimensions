#include "usart.hpp"
#include <avr/io.h>

//   Baud     | f_osc 16.0000MHz
//   Rate     | U2Xn = 0            U2Xn = 1
//   [bps]    |    UBRR |   Error      UBRR | Error
//  --------- | --------+---------  --------+---------
//    2400    |     416 |   -0.1%       832 | +0.0%
//    4800    |     207 |   +0.2%       416 | -0.1%
//    9600    |     103 |   +0.2%       207 | +0.2%
//    14.4k   |      68 |   +0.6%       138 | -0.1%
//    19.2k   |      51 |   +0.2%       103 | +0.2%
//    28.8k   |      34 |   -0.8%        68 | +0.6%
//    38.4k   |      25 |   +0.2%        51 | +0.2%
//    57.6k   |      16 |   +2.1%        34 | -0.8%
//    76.8k   |      12 |   +0.2%        25 | +0.2%
//   115.2k   |       8 |   -3.5%        16 | +2.1%
//   230.4k   |       3 |   +8.5%         8 | -3.5%
//     250k   |       3 |   +0.0%         7 | +0.0%
//     0.5M   |       1 |   +0.0%         3 | +0.0%
//       1M   |       0 |   +0.0%         1 | +0.0%
//   Max.     |     1Mbps               2Mbps
void usart_init(uint32_t baudrate, uint8_t u2x)
{
  // Set the baudrate.
  // 
  // The datasheet says in table 18-1 for U2Xn = 0:
  //   UBRRn = f_{OSC} / (16 * BAUD) - 1
  // 
  // The datasheet says in table 18-1 for U2Xn = 1:
  //   UBRRn = f_{OSC} / (8 * BAUD) - 1
  // 
  // The division must be rounded for the best value.
  uint16_t ubrr_u2x0 = (F_CPU + 8 * baudrate) / (16 * baudrate) - 1;
  uint16_t ubrr_u2x1 = (F_CPU + 4 * baudrate) / ( 8 * baudrate) - 1;
  
  uint32_t baudrate_actual_u2x0 = F_CPU / (16 * (ubrr_u2x0 + 1));
  uint32_t baudrate_actual_u2x1 = F_CPU / ( 8 * (ubrr_u2x1 + 1));
  
  switch (u2x)
  {
    case 0:
      break;
    case 1:
      break;
    default:
      // Select the best value for U2X automatically.
      uint32_t abs_diff_u2x0 = baudrate > baudrate_actual_u2x0 ?
        baudrate - baudrate_actual_u2x0 : baudrate_actual_u2x0 - baudrate;
      uint32_t abs_diff_u2x1 = baudrate > baudrate_actual_u2x1 ?
        baudrate - baudrate_actual_u2x1 : baudrate_actual_u2x1 - baudrate;
      u2x = abs_diff_u2x1 < abs_diff_u2x0;
  }
  uint16_t ubrr = u2x ? ubrr_u2x1 : ubrr_u2x0;
  UBRR1H = (ubrr >> 8);
  UBRR1L = (ubrr & 0xff);
  
  // The only writable bits of UCSR1A are:
  // 
  // Bit 6 – TXCn: USART Transmit Complete. "or it can be cleared by 
  // writing a one to its bit location." We don't need that.
  // 
  // Bit 1 – U2Xn: Double the USART Transmission Speed.
  // 
  // Bit 0 – MPCMn: Multi-processor Communication Mode. We definitely 
  // don't need that feature.
  UCSR1A = (_BV(TXC1) * 0)	// 0x60: clear pending transmit interrupt
	 | (_BV(U2X1) * u2x)	// 0x02: double transmission speed
	 | (_BV(MPCM1) * 0)	// 0x01: multi processor communication mode
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
