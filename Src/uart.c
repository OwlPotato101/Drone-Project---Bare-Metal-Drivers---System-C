#include "uart.h"

// Internal state for the interrupt "Mailbox"
static volatile char last_command = 0;
static volatile bool command_ready = false;

// From your sysclk.c, PCLK1 is 24MHz
#define PCLK1_FREQ 24000000

void UART2_Init(uint32_t baud_rate) {
    // 1. Enable Clock for USART2 and GPIOA
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // 2. Configure PA2 (TX) and PA3 (RX) for Alternate Function 7
    GPIOA->MODER &= ~((3U << (2 * 2)) | (3U << (3 * 2)));
    GPIOA->MODER |=  ((2U << (2 * 2)) | (2U << (3 * 2)));

    GPIOA->AFR[0] &= ~((0xFU << (2 * 4)) | (0xFU << (3 * 4)));
    GPIOA->AFR[0] |=  ((7U << (2 * 4)) | (7U << (3 * 4)));

    // 3. Configure USART2
    USART2->CR1 &= ~USART_CR1_UE; // Disable for config
    USART2->CR1 &= ~(USART_CR1_M | USART_CR1_PCE); // 8-bit, No Parity
    USART2->CR2 &= ~USART_CR2_STOP; // 1 Stop bit

    // 4. Set Baud Rate (Rounding included)
    USART2->BRR = (PCLK1_FREQ + (baud_rate / 2)) / baud_rate;

    // 5. Enable TX, RX, and the Module
    USART2->CR1 |= (USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

void UART2_EnableInterrupt(void) {
    // Enable the RX Not Empty Interrupt
    USART2->CR1 |= USART_CR1_RXNEIE;

    // Enable in the NVIC
    NVIC_SetPriority(USART2_IRQn, 1);
    NVIC_EnableIRQ(USART2_IRQn);
}

void UART2_SendChar(char c) {
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = (c & 0xFF);
}

void UART2_SendString(const char *str) {
    while (*str) {
        UART2_SendChar(*str++);
    }
}

bool UART2_HasCommand(void) {
    return command_ready;
}

char UART2_GetCommand(void) {
    command_ready = false;
    return last_command;
}

void USART2_IRQHandler(void) {
    if (USART2->SR & USART_SR_RXNE) {
        last_command = (char)(USART2->DR & 0xFF);
        command_ready = true;
    }
}
