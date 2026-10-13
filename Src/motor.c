/*
 * motor.c
 *
 * Motor arming and failsafe on top of the DSHOT driver. See motor.h.
 */

#include "motor.h"
#include "systick.h"

// Frames in a row that fail to go out before the output counts as stuck.
// A frame takes about 60 us, so even one miss at 1 ms ticks means trouble.
#define MOTOR_MAX_MISSED 3

static MotorState_t state = MOTOR_DISARMED;
static MotorFault_t fault = MOTOR_FAULT_NONE;
static uint16_t throttle[MOTOR_COUNT];

static Time_t zeroSince = 0;       // Start of the current unbroken run of zero frames
static uint32_t knownErrors = 0;   // DSHOT_ErrorCount() at the last check
static int missedFrames = 0;

static void Fault(MotorFault_t why) {
    Motor_Disarm();
    if (fault == MOTOR_FAULT_NONE) {
        fault = why; // Keep the first cause
    }
}

void Motor_Init(void) {
    DSHOT_Init();
    knownErrors = DSHOT_ErrorCount();
    Motor_Disarm();
}

void Motor_Task(void) {
    uint16_t out[MOTOR_COUNT] = { 0 };

    if (DSHOT_ErrorCount() != knownErrors) {
        knownErrors = DSHOT_ErrorCount();
        Fault(MOTOR_FAULT_DMA);
    }

    if (state == MOTOR_ARMED) {
        for (int m = 0; m < MOTOR_COUNT; m++) {
            out[m] = throttle[m];
        }
    }

    if (!DSHOT_PrepareFrame(out[0], out[1], out[2], out[3], false) || !DSHOT_Send()) {
        // The ESCs didn't get this frame, so the zero run is broken
        zeroSince = TimeNow();
        if (++missedFrames >= MOTOR_MAX_MISSED) {
            Fault(MOTOR_FAULT_STUCK);
        }
        return;
    }
    missedFrames = 0;
}

bool Motor_CanArm(void) {
    return state == MOTOR_DISARMED && fault == MOTOR_FAULT_NONE
            && TimePassed(zeroSince) >= MOTOR_ARM_ZERO_MS;
}

bool Motor_Arm(void) {
    if (!Motor_CanArm()) {
        return false;
    }
    for (int m = 0; m < MOTOR_COUNT; m++) {
        throttle[m] = DSHOT_CMD_MOTOR_STOP;
    }
    state = MOTOR_ARMED;
    return true;
}

void Motor_Disarm(void) {
    state = MOTOR_DISARMED;
    for (int m = 0; m < MOTOR_COUNT; m++) {
        throttle[m] = DSHOT_CMD_MOTOR_STOP;
    }
    zeroSince = TimeNow();
}

void Motor_SetThrottle(int motor, uint16_t value) {
    if (state != MOTOR_ARMED || motor < 0 || motor >= MOTOR_COUNT) {
        return;
    }
    // Never let a low throttle turn into an ESC command (1-47)
    if (value < DSHOT_THROTTLE_MIN) {
        value = DSHOT_CMD_MOTOR_STOP;
    } else if (value > DSHOT_THROTTLE_MAX) {
        value = DSHOT_THROTTLE_MAX;
    }
    throttle[motor] = value;
}

void Motor_SetAll(uint16_t value) {
    for (int m = 0; m < MOTOR_COUNT; m++) {
        Motor_SetThrottle(m, value);
    }
}

MotorState_t Motor_GetState(void) {
    return state;
}

MotorFault_t Motor_GetFault(void) {
    return fault;
}
