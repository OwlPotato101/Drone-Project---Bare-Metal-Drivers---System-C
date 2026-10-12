/*
 * dshot.c
 *
 *  Created on: Sep 30, 2026
 *      Author: PC
 */


#include "dshot.h"
#include "stm32f446xx.h"

// The DMA array. 72 elements total.
// Memory layout: [CH1_bit0, CH2_bit0, CH3_bit0, CH4_bit0, CH1_bit1, CH2_bit1...]
static uint32_t dshot_buffer[DSHOT_BUFFER_SIZE] = {0};

// Hardware definition for TIM1 controlling all 4 channels
static const TimerIO_t Tim1_Motor = { TIM1, { GPIOA, 8 }, 1, 1 }; // Base config, pins handled separately

// Internal CRC Calculation for DSHOT protocol
static uint16_t CalculateCRC(uint16_t throttle, bool telemetry) {
    uint16_t packet = (throttle << 1) | (telemetry ? 1 : 0);
    uint16_t csum = (packet ^ (packet >> 4) ^ (packet >> 8)) & 0x0F;
    return (packet << 4) | csum;
}



void DSHOT_Init(void) {
    // 1. Define the 4 motor pins (AF1 for TIM1)
    TimerIO_t m1 = { TIM1, { GPIOA, 8 }, 1, 1 };
    TimerIO_t m2 = { TIM1, { GPIOA, 9 }, 1, 2 };
    TimerIO_t m3 = { TIM1, { GPIOA, 10 }, 1, 3 };
    TimerIO_t m4 = { TIM1, { GPIOA, 11 }, 1, 4 };

    // 2. Initialize TIM1 and route it to the GPIO pins automatically
    // This utilizes your existing TimerEnable driver from timer_4.c
    TimerEnable(m1);
    TimerEnable(m2);
    TimerEnable(m3);
    TimerEnable(m4);

    // 3. Set Timer Period for 300kHz
    TimerPeriod(m1, 0, DSHOT_ARR, 0);

    // 4. Set all 4 channels to PWM Mode 1
    TimerMode(m1, OUTCMP, PWM1);
    TimerMode(m2, OUTCMP, PWM1);
    TimerMode(m3, OUTCMP, PWM1);
    TimerMode(m4, OUTCMP, PWM1);

    // 5. Configure DMA Burst on TIM1
    // DBA = 13 (0x0D) starts the burst at CCR1.
    // DBL = 4 (passed as 4, internal driver does 4-1=3) transfers 4 registers: CCR1, CCR2, CCR3, CCR4
    TimerConfigDMABurst(m1, 0x0D, 4);

    // 6. Configure DMA2 Stream 5 Channel 6 (STM32F446 Mapping for TIM1_UP)
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    // Ensure DMA is disabled before configuration
    DMA2_Stream5->CR &= ~DMA_SxCR_EN;
    while(DMA2_Stream5->CR & DMA_SxCR_EN);

    // Set Peripheral Address to the Timer's Burst Address Register (DMAR)
    DMA2_Stream5->PAR = (uint32_t)&TIM1->DMAR;

    // Set Memory Address to our data buffer
    DMA2_Stream5->M0AR = (uint32_t)dshot_buffer;

    // DMA Configuration:
    // CHSEL = 6 (Channel 6)
    // DIR = 1 (Memory-to-Peripheral)
    // MSIZE = 2 (32-bit word memory size)
    // PSIZE = 2 (32-bit word peripheral size)
    // MINC = 1 (Increment memory pointer)
    // PINC = 0 (Do NOT increment peripheral pointer, DMAR handles internal routing!)
    // CIRC = 0 (Normal mode, NOT circular)
    DMA2_Stream5->CR = (6U << 25) | (1U << 6) | (2U << 13) | (2U << 11) | (1U << 10);

    // Enable DMA triggers on Timer Update
    TimerEnableDMARequest(m1);
}


void DSHOT_PrepareFrame(uint16_t m1, uint16_t m2, uint16_t m3, uint16_t m4, bool telemetry) {
    // Combine throttle, telemetry, and CRC into standard 16-bit DSHOT frames
    uint16_t packet1 = CalculateCRC(m1, telemetry);
    uint16_t packet2 = CalculateCRC(m2, telemetry);
    uint16_t packet3 = CalculateCRC(m3, telemetry);
    uint16_t packet4 = CalculateCRC(m4, telemetry);

    // Fill the buffer: 16 bits of data
    for (int i = 0; i < 16; i++) {
        // Evaluate from MSB (bit 15) down to LSB (bit 0)
        dshot_buffer[i * 4 + 0] = (packet1 & (0x8000 >> i)) ? DSHOT_1 : DSHOT_0;
        dshot_buffer[i * 4 + 1] = (packet2 & (0x8000 >> i)) ? DSHOT_1 : DSHOT_0;
        dshot_buffer[i * 4 + 2] = (packet3 & (0x8000 >> i)) ? DSHOT_1 : DSHOT_0;
        dshot_buffer[i * 4 + 3] = (packet4 & (0x8000 >> i)) ? DSHOT_1 : DSHOT_0;
    }

    // Fill the remaining 2 frames (8 array slots) with 0s to reset the lines
    for (int i = 16; i < DSHOT_FRAME_LENGTH; i++) {
        dshot_buffer[i * 4 + 0] = 0;
        dshot_buffer[i * 4 + 1] = 0;
        dshot_buffer[i * 4 + 2] = 0;
        dshot_buffer[i * 4 + 3] = 0;
    }
}

void DSHOT_Send(void) {
    // 1. Disable DMA stream to reset it
    DMA2_Stream5->CR &= ~DMA_SxCR_EN;
    while(DMA2_Stream5->CR & DMA_SxCR_EN);

    // 2. Clear all DMA2 Stream 5 interrupt flags in the High Interrupt Flag Clear Register
    DMA2->HIFCR = (DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5);

    // 3. Reset the Number of Data to Transfer (18 elements * 4 channels = 72)
    DMA2_Stream5->NDTR = DSHOT_BUFFER_SIZE;

    // 4. Enable the DMA Stream
    DMA2_Stream5->CR |= DMA_SxCR_EN;

    // 5. Fire the Timer!
    TimerStart(Tim1_Motor);
}
