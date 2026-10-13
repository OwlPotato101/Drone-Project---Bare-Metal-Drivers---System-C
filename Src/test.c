/*
 * test.c
 *
 * DSHOT300 bench test. PROPS OFF.
 *
 * Boots disarmed and streams zero throttle at 1 kHz so the ESCs can arm.
 * Driven from the ST-LINK virtual COM port (115200 8N1) or the blue button:
 *
 *   a          arm (accepted after 2 s of zero throttle), spins at TEST_THROTTLE_START
 *   + / -      throttle up / down, capped at TEST_THROTTLE_LIMIT
 *   k / space  kill: back to zero throttle
 *   ?          status
 *   B1 button  arm when disarmed, kill when armed
 */

#include <stdio.h>
#include "test.h"
#include "motor.h"
#include "gpio.h"
#include "uart.h"
#include "systick.h" // Assuming this contains TimeNow() and TimePassed()

// Throttle in DSHOT units (48 = 0%, 2047 = 100%).
// About 5% on arming: enough to see an unloaded motor turn.
#define TEST_THROTTLE_START 150
#define TEST_THROTTLE_STEP  25
// Bench cap, about 17.6%
#define TEST_THROTTLE_LIMIT 400

#define BUTTON_DEBOUNCE_MS 250

static const Pin_t Button = { GPIOC, 13 }; // B1, low when pressed

static uint16_t testThrottle = 0;
static volatile bool buttonPressed = false;
static Time_t buttonTime = 0;
static MotorFault_t reportedFault = MOTOR_FAULT_NONE;

static void CallbackButtonPress(void) {
	buttonPressed = true;
}

static void Print(const char *str) {
	UART2_SendString(str);
}

static void PrintThrottle(void) {
	char line[48];
	// DSHOT 48-2047 maps to 0-100%; show tenths of a percent
	unsigned permille = (testThrottle - DSHOT_THROTTLE_MIN) * 1000u
			/ (DSHOT_THROTTLE_MAX - DSHOT_THROTTLE_MIN);
	snprintf(line, sizeof line, "Throttle %u (%u.%u%%)\r\n", testThrottle,
			permille / 10, permille % 10);
	Print(line);
}

static void Arm(void) {
	if (Motor_GetState() == MOTOR_ARMED) {
		return;
	}
	if (!Motor_Arm()) {
		Print(Motor_GetFault() != MOTOR_FAULT_NONE ?
				"Fault latched, reset the board\r\n" :
				"Not yet: needs 2 s of zero throttle first\r\n");
		return;
	}
	testThrottle = TEST_THROTTLE_START;
	Print("ARMED. ");
	PrintThrottle();
}

static void Kill(void) {
	Motor_Disarm();
	testThrottle = 0;
	Print("DISARMED\r\n");
}

static void ChangeThrottle(int step) {
	if (Motor_GetState() != MOTOR_ARMED) {
		Print("Arm first (a)\r\n");
		return;
	}
	int t = testThrottle + step;
	if (t < DSHOT_THROTTLE_MIN)
		t = DSHOT_THROTTLE_MIN;
	if (t > TEST_THROTTLE_LIMIT)
		t = TEST_THROTTLE_LIMIT;
	testThrottle = t;
	PrintThrottle();
}

static void PrintStatus(void) {
	if (Motor_GetState() == MOTOR_ARMED) {
		Print("ARMED. ");
		PrintThrottle();
	} else if (Motor_GetFault() != MOTOR_FAULT_NONE) {
		Print("DISARMED, fault latched\r\n");
	} else {
		Print(Motor_CanArm() ? "DISARMED, ready to arm\r\n" :
				"DISARMED, sending zero throttle\r\n");
	}
}

void Test_Init(void) {
	// Blue button as arm/kill switch
	GPIO_Enable(Button);
	GPIO_Mode(Button, INPUT);
	GPIO_Callback(Button, CallbackButtonPress, FALL);

	// Initialize the TIM1 and DMA2 peripherals and start streaming zero throttle
	Motor_Init();

	Print("DSHOT300 bench test. PROPS OFF.\r\n"
			"a=arm  +/-=throttle  k/space=kill  ?=status  B1=arm/kill\r\n");
}

void Test_Task(void) {
	if (buttonPressed) {
		buttonPressed = false;
		if (TimePassed(buttonTime) >= BUTTON_DEBOUNCE_MS) {
			buttonTime = TimeNow();
			if (Motor_GetState() == MOTOR_ARMED)
				Kill();
			else
				Arm();
		}
	}

	if (UART2_HasCommand()) {
		switch (UART2_GetCommand()) {
		case 'a':
		case 'A':
			Arm();
			break;
		case '+':
		case '=':
			ChangeThrottle(TEST_THROTTLE_STEP);
			break;
		case '-':
		case '_':
			ChangeThrottle(-TEST_THROTTLE_STEP);
			break;
		case 'k':
		case 'K':
		case ' ':
			Kill();
			break;
		case '?':
			PrintStatus();
			break;
		}
	}

	// The motor layer disarms on its own when it sees a fault
	if (Motor_GetFault() != reportedFault) {
		reportedFault = Motor_GetFault();
		testThrottle = 0;
		Print(reportedFault == MOTOR_FAULT_DMA ? "FAULT: DMA error, motors stopped\r\n" :
				"FAULT: DSHOT frames stuck, motors stopped\r\n");
	}

	Motor_SetAll(testThrottle);

	// Stream the DSHOT packets continuously (every 1 ms tick, 1 kHz).
	// ESCs require a constant stream of packets or they will disarm for safety.
	Motor_Task();
}
