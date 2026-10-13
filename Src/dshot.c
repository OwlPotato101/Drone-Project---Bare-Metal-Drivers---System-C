/*
 * dshot.c
 *
 *  Created on: Sep 30, 2026
 *      Author: PC
 */


#include <stddef.h>
#include "dshot.h"
#include "stm32f446xx.h"

// The DMA array. 72 elements total.
// Memory layout: [CH1_bit0, CH2_bit0, CH3_bit0, CH4_bit0, CH1_bit1, CH2_bit1...]
static uint32_t dshot_buffer[DSHOT_BUFFER_SIZE] = {0};

// DMA transfer errors since boot, counted in the stream interrupt
static volatile uint32_t dshot_errors = 0;

// TIM1 drives all 4 motors: PA8-PA11 are TIM1 CH1-CH4 on AF1
static const TimerIO_t Tim1_Motor[DSHOT_MOTOR_COUNT] = {
    { TIM1, { GPIOA, 8 },  1, 1 },
    { TIM1, { GPIOA, 9 },  1, 2 },
    { TIM1, { GPIOA, 10 }, 1, 3 },
    { TIM1, { GPIOA, 11 }, 1, 4 },
};

// DBA counts 32-bit registers from TIMx_CR1, so CCR1 (offset 0x34) is 13 (0x0D)
#define TIM_DBA_CCR1 (offsetof(TIM_TypeDef, CCR1) / 4)

// Every DMA2 Stream 5 flag in the High Interrupt Flag Clear Register
#define DMA_STREAM5_FLAGS (DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5)

// Internal CRC Calculation for DSHOT protocol
static uint16_t CalculateCRC(uint16_t throttle, bool telemetry) {
    uint16_t packet = (throttle << 1) | (telemetry ? 1 : 0);
    uint16_t csum = (packet ^ (packet >> 4) ^ (packet >> 8)) & 0x0F;
    return (packet << 4) | csum;
}



void DSHOT_Init(void) {
    // 1. Initialize TIM1 and route it to the 4 motor pins (AF1)
    for (int m = 0; m < DSHOT_MOTOR_COUNT; m++) {
        TimerEnable(Tim1_Motor[m]);
    }

    // 2. Set all 4 channels to PWM Mode 1 with a compare value of 0, so the lines idle low
    for (int m = 0; m < DSHOT_MOTOR_COUNT; m++) {
        TimerMode(Tim1_Motor[m], OUTCMP, PWM1);
        TimerOutput(Tim1_Motor[m], 0);
    }

    // 3. Set Timer Period for 300kHz. Keep update DMA requests off: the UG inside
    // TimerPeriod would otherwise leave a request pending for the stream
    TIM1->DIER &= ~TIM_DIER_UDE;
    TimerPeriod(Tim1_Motor[0], 0, DSHOT_ARR, 0);

    // 4. Configure DMA Burst on TIM1
    // DBA = 13 (0x0D) starts the burst at CCR1.
    // DBL = 4 (passed as 4, internal driver does 4-1=3) transfers 4 registers: CCR1, CCR2, CCR3, CCR4
    TimerConfigDMABurst(Tim1_Motor[0], TIM_DBA_CCR1, DSHOT_MOTOR_COUNT);

    // 5. Configure DMA2 Stream 5 Channel 6 (STM32F446 Mapping for TIM1_UP, RM0390 Table 29)
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    // Ensure DMA is disabled before configuration
    DMA2_Stream5->CR &= ~DMA_SxCR_EN;
    while(DMA2_Stream5->CR & DMA_SxCR_EN);
    DMA2->HIFCR = DMA_STREAM5_FLAGS;

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
    // PL = 2 (High priority)
    // TCIE, TEIE = 1 (Interrupt at the end of the frame and on a transfer error)
    DMA2_Stream5->CR = (6U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_DIR_0 | DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1
                     | DMA_SxCR_MINC | DMA_SxCR_PL_1 | DMA_SxCR_TCIE | DMA_SxCR_TEIE;

    // The transfer-complete interrupt ends every frame and has to run within one
    // bit period (3.3 us), so nothing may block it: highest priority in the system
    NVIC_SetPriority(DMA2_Stream5_IRQn, 0);
    NVIC_EnableIRQ(DMA2_Stream5_IRQn);

    // 6. Let TIM1 free-run. The lines stay low until DSHOT_Send enables DMA requests
    TimerStart(Tim1_Motor[0]);
}


bool DSHOT_IsBusy(void) {
    // UDE is set by DSHOT_Send and cleared by the transfer-complete interrupt
    return (TIM1->DIER & TIM_DIER_UDE) || (DMA2_Stream5->CR & DMA_SxCR_EN);
}


bool DSHOT_PrepareFrame(uint16_t m1, uint16_t m2, uint16_t m3, uint16_t m4, bool telemetry) {
    // The DMA is still reading the previous frame out of the buffer
    if (DSHOT_IsBusy()) {
        return false;
    }

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
    return true;
}

bool DSHOT_Send(void) {
    // 1. Previous frame still going out
    if (DSHOT_IsBusy()) {
        return false;
    }

    // 2. Clear all DMA2 Stream 5 interrupt flags in the High Interrupt Flag Clear Register
    // (required before the stream can be re-enabled, RM0390 9.3.18)
    DMA2->HIFCR = DMA_STREAM5_FLAGS;

    // 3. Reset the Number of Data to Transfer (18 elements * 4 channels = 72)
    DMA2_Stream5->NDTR = DSHOT_BUFFER_SIZE;

    // 4. Make sure the buffer writes have landed before the DMA starts reading
    __DMB();

    // 5. Enable the DMA Stream. UDE is still off, so no stale TIM1 request can start it early
    DMA2_Stream5->CR |= DMA_SxCR_EN;

    // 6. Fire! The first burst waits for the next update event, so every
    // 4-register burst lands at the start of a bit period
    TIM1->DIER |= TIM_DIER_UDE;
    return true;
}

uint32_t DSHOT_ErrorCount(void) {
    return dshot_errors;
}

// End of frame. The stream has just written the last burst. Turn off update DMA
// requests before the next update event (one bit period away), or TIM1 latches a
// request that fires the next frame's first burst the moment the stream is
// re-enabled, at a random point in the bit period ("As soon as the stream is
// enabled, it can serve any DMA request from the peripheral", RM0390 9.3.18)
void DMA2_Stream5_IRQHandler(void) {
    TIM1->DIER &= ~TIM_DIER_UDE;

    uint32_t flags = DMA2->HISR;
    DMA2->HIFCR = DMA_STREAM5_FLAGS;

    if (flags & DMA_HISR_TEIF5) {
        DMA2_Stream5->CR &= ~DMA_SxCR_EN;
        dshot_errors++;
    }
}
