/*
 * test.c
 *
 * DSHOT300 steady-state testing application
 */

#include "test.h"
#include "dshot.h"
#include "systick.h" // Assuming this contains TimeNow() and TimePassed()
#include <stdbool.h>

// 48 is the absolute minimum valid throttle value in the DSHOT protocol to spin the motors.
// Values 1-47 are reserved for telemetry and ESC commands (like beeping).
static const uint16_t IDLE_THROTTLE = 48;

// Timer to keep the DSHOT packets streaming continuously
static Time_t lastDshotTime = 0;

void Test_Init(void) {
	// Initialize the TIM1 and DMA2 peripherals
	DSHOT_Init();

	lastDshotTime = TimeNow();
}

void Test_Task(void) {
	// Stream the DSHOT packets continuously (Every 2ms / 500Hz).
	// ESCs require a constant stream of packets or they will disarm for safety.
	if (TimePassed(lastDshotTime) >= 2) {
		lastDshotTime = TimeNow();

		// Apply the steady idle throttle to all 4 motors.
		// Telemetry bit is set to 'false'.
		DSHOT_PrepareFrame(IDLE_THROTTLE, IDLE_THROTTLE, IDLE_THROTTLE, IDLE_THROTTLE, false);

		// Fire the DMA burst - this runs completely in the background
		DSHOT_Send();
	}
}
