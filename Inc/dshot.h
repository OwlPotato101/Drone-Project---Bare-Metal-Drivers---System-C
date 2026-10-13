/*
 * dshot.h
 *
 *  Created on: Sep 30, 2026
 *      Author: PC
 */

#ifndef DSHOT_H_
#define DSHOT_H_

#include <stdint.h>
#include <stdbool.h>
#include "timer.h"
#include "sysclk.h"

// DSHOT300 runs at 300 kHz (3.33 us per bit). A '1' is high for 75% of the bit
// (2.50 us) and a '0' for 37.5% (1.25 us). All values below are TIM1 ticks and
// follow the clock in sysclk.h: at 48 MHz that is 160 ticks per bit.
#define DSHOT_BITRATE   300000UL
#define DSHOT_TIM_FREQ  TIM_APB2_FREQ                     // TIM1 sits on APB2
#define DSHOT_PERIOD    (DSHOT_TIM_FREQ / DSHOT_BITRATE)  // 160 ticks per bit
#define DSHOT_ARR       (DSHOT_PERIOD - 1)                // Counter runs 0..ARR, so ARR + 1 ticks
#define DSHOT_0         ((DSHOT_PERIOD * 3) / 8)          // DSHOT 0: 37.5% duty, 60 ticks
#define DSHOT_1         ((DSHOT_PERIOD * 3) / 4)          // DSHOT 1: 75.0% duty, 120 ticks

_Static_assert(DSHOT_TIM_FREQ % DSHOT_BITRATE == 0, "Timer clock is not a whole multiple of the DSHOT bit rate");
_Static_assert(DSHOT_ARR <= 0xFFFF, "TIM1 is a 16-bit timer");

// Frame values: 0 = motor stop, 1-47 = ESC commands (beep, spin direction...),
// 48-2047 = throttle from 0% to 100%
#define DSHOT_CMD_MOTOR_STOP 0
#define DSHOT_THROTTLE_MIN   48
#define DSHOT_THROTTLE_MAX   2047

// 16 bits of data + 2 bits of reset (zeroes) to end the frame = 18 elements per motor
#define DSHOT_FRAME_LENGTH 18
#define DSHOT_MOTOR_COUNT  4
#define DSHOT_BUFFER_SIZE  (DSHOT_FRAME_LENGTH * DSHOT_MOTOR_COUNT)

// Function Prototypes
void DSHOT_Init(void);

// A frame is on the wire (about 60 us after DSHOT_Send). The buffer can't be
// touched until it finishes, so PrepareFrame and Send return false meanwhile.
bool DSHOT_IsBusy(void);
bool DSHOT_PrepareFrame(uint16_t m1, uint16_t m2, uint16_t m3, uint16_t m4, bool telemetry);
bool DSHOT_Send(void);

// DMA transfer errors since boot
uint32_t DSHOT_ErrorCount(void);

#endif /* DSHOT_H_ */
