#ifndef _USART_HPP_
#define _USART_HPP_

#include <avr/common.h>

void usart_init(uint32_t baudrate);
void usart_transmit_char(char ch);
void usart_transmit_string(char const ch[]);
uint16_t usart_receive_uint16();
void usart_dump_byte(uint8_t byte);

#endif /* _USART_HPP_ */
