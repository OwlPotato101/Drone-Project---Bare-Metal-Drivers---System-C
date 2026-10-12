#include "stm32f446xx.h"
#include "uart.h" // Include your new UART driver

int __io_putchar(int c) {
    UART2_SendChar((char)c);
    return c;
}
