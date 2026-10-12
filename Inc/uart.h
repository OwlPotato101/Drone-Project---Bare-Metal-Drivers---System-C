#ifndef UART_H_
#define UART_H_

#include "stm32f446xx.h"
#include <stdint.h>
#include <stdbool.h>

void UART2_Init(uint32_t baud_rate);
void UART2_SendChar(char c);
void UART2_SendString(const char *str);

// New: Interrupt-driven reception
void UART2_EnableInterrupt(void);
bool UART2_HasCommand(void);
char UART2_GetCommand(void);

#endif
