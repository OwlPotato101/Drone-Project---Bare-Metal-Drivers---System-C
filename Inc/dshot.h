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

// DSHOT300 runs at 300 kHz (3.33 us per bit)
// IMPORTANT: These values assume a 16 MHz Timer Clock.
// If you configure the PLL for a higher clock (e.g., 84 MHz or 180 MHz),
// you must recalculate these based on: Ticks = (Clock / 300,000)
#define DSHOT_ARR   (160) // 16MHz / 300kHz = 53.33
#define DSHOT_0     (60) // DSHOT 0: Needs a ~37.5% duty cycle. 160 * 0.375 = 60.
#define DSHOT_1     (120) // DSHOT 1: Needs a ~75.0% duty cycle. 160 * 0.75 = 120.

// 16 bits of data + 2 bits of reset (zeroes) to end the frame = 18 elements per motor
#define DSHOT_FRAME_LENGTH 18
#define DSHOT_MOTOR_COUNT  4
#define DSHOT_BUFFER_SIZE  (DSHOT_FRAME_LENGTH * DSHOT_MOTOR_COUNT)

// Function Prototypes
void DSHOT_Init(void);
void DSHOT_PrepareFrame(uint16_t m1, uint16_t m2, uint16_t m3, uint16_t m4, bool telemetry);
void DSHOT_Send(void);

#endif /* DSHOT_H_ */
