#include "stm32f446xx.h"
#include "systick.h"
#include "led_app.h"
#include "uart.h"
#include "test.h"

int main(void) {
    // Debugger maintenance: keeps debugging active in low power modes
    DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY;

    // 1. Hardware Initializations
    StartSysTick();      // 1ms tick and 48MHz SysClk


    // 2. Peripheral Initializations
    UART2_Init(115200);
    UART2_EnableInterrupt();
    UART2_SendString("\r\n--- ROVER BOOT SUCCESS ---\r\n");


    // Init_GreenLED();     // App LEDs/Pins (Heartbeat). Moved off the motor pins, rewire before enabling


    // DSHOT bench test. Owns DSHOT_Init() and the motor arming state machine
    Test_Init();



    while (1) {
        // App Tasks
        // Task_GreenLED();     // Keeps the LED blinking to prove CPU is alive
    	Test_Task();
        // Wait for next 1ms tick to save power
        WaitForSysTick();
    }
}
