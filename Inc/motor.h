/*
 * motor.h
 *
 * Motor arming and failsafe on top of the DSHOT driver.
 *
 * Disarmed, every motor gets throttle 0 on every frame. ESCs only arm after a
 * stretch of zero-throttle frames, so arming is refused until MOTOR_ARM_ZERO_MS
 * of zeros have gone out. Disarming (kill) or any fault returns to zero at once,
 * and a fault stays latched until reset.
 */

#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "dshot.h"

#define MOTOR_COUNT DSHOT_MOTOR_COUNT

// Zero throttle the ESCs must see before Motor_Arm() is accepted
#define MOTOR_ARM_ZERO_MS 2000

typedef enum {
    MOTOR_DISARMED, // Throttle 0 on every motor, whatever was commanded
    MOTOR_ARMED     // Commanded throttle goes out
} MotorState_t;

typedef enum {
    MOTOR_FAULT_NONE,
    MOTOR_FAULT_DMA,  // DMA transfer error
    MOTOR_FAULT_STUCK // Frames stopped completing
} MotorFault_t;

void Motor_Init(void);

// Sends one frame. Call once per 1 ms tick.
void Motor_Task(void);

// Returns false (and stays disarmed) until MOTOR_ARM_ZERO_MS of zero throttle
// has gone out, or while a fault is latched. Arming starts every motor at 0.
bool Motor_Arm(void);
bool Motor_CanArm(void);
void Motor_Disarm(void);

// Throttle in DSHOT units: 0 = stop, 48-2047 = 0-100%. 1-47 are ESC commands,
// so anything below 48 is sent as 0. Ignored while disarmed.
void Motor_SetThrottle(int motor, uint16_t throttle);
void Motor_SetAll(uint16_t throttle);

MotorState_t Motor_GetState(void);
MotorFault_t Motor_GetFault(void);

#endif /* MOTOR_H_ */
