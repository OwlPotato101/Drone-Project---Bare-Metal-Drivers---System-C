#include "led_app.h"
#include "gpio.h"
#include "systick.h"
#include "uart.h"
#include "timer.h" // Added for PWM timer features
#include "sysclk.h"
#include <stdbool.h>

// Define the Green LED pin (PA5, LD2 on the NUCLEO-F446RE) [1, 2]
// PA8-PA11 are the DSHOT motor outputs, so the LED matrix and the encoder
// switch moved from D2 (PA10) and D7 (PA8) to A3 and A4. Rewire before enabling.
// B1 (PC13) is also the DSHOT bench test's arm/kill button: run one app or the other.
static const Pin_t GreenLED = { GPIOA, 5 };
static const Pin_t Button = { GPIOC, 13 };
static const Pin_t D2Pin = { GPIOB, 0 }; //LED martix (A3, was D2/PA10)
static const Pin_t D4Pin = { GPIOB, 5 }; //speaker
static const Pin_t CLK_Pin = { GPIOB, 4 }; //Rotary Button Rotate
static const Pin_t DT_Pin = { GPIOB, 10 }; //Rotary Button Rotate
static const Pin_t SW_Pin = { GPIOC, 1 }; //Rotary Encoder Button (A4, was D7/PA8)


// --- NEW: PWM Timer LED Definition ---
// PA0 maps to TIM2, Channel 1, Alternate Function 1
static const TimerIO_t PwmLED = { TIM2, { GPIOA, 0 }, 1, 1 };

static void CallbackButtonPress();
static void CallbackButtonRelease();

//for the rotary and dimming lights function
static void CallbackButtonPressSW();
static void CallbackButtonReleaseSW();

static void DT();
static void CLK();

static Time_t debounceTime = 0;
static Time_t toggleTime = 0;
static Time_t speakerTime = 0;
static Time_t rotaryTime1 = 0;
static Time_t rotaryTime2 = 0;

// --- NEW: PWM state variables ---
//static Time_t pwmTimer = 0;
#define PWM_MAX 1000 // Full brightness (ARR + 1)
static int pwmValue = 10;
static int temppwmValue = 5; // Brightness to restore when the encoder switch turns the LED back on
//static int pwmStep = 1; // 1 for fading up, -1 for fading down

static const int speed[] = { 200, 500, 1000, 0 };
#define SPEED_COUNT ((int) (sizeof speed / sizeof speed[0]))
static int i = 0;
static char cmd;
static int counter1 = 0;
static int counter2 = 0;

void Init_GreenLED(void) {
	// 1. Enable the peripheral clock for Port B [5, 6]
	GPIO_Enable(GreenLED);
	GPIO_Enable(D2Pin);
	GPIO_Enable(D4Pin);
	GPIO_Enable(Button);
	GPIO_Enable(CLK_Pin);
	GPIO_Enable(SW_Pin);
	GPIO_Enable(DT_Pin);

	// 2. Set the pin mode to Output [7, 8]
	GPIO_Mode(GreenLED, OUTPUT);
	GPIO_Mode(D2Pin, OUTPUT);
	GPIO_Mode(D4Pin, OUTPUT);
	GPIO_Mode(Button, INPUT);
	GPIO_Mode(CLK_Pin, INPUT);
	GPIO_Mode(DT_Pin, INPUT);
	GPIO_Mode(SW_Pin, INPUT);


	// Suggested if the button doesn't have an external resistor
	GPIO_Config(Button, PP, S0, PU);
	GPIO_Config(SW_Pin, PP, S0, PU);
	GPIO_Config(DT_Pin, PP, S0, NOPUPD);
	GPIO_Config(CLK_Pin, PP, S0, NOPUPD);


	toggleTime = TimeNow();

	GPIO_Output(GreenLED, HIGH);
	GPIO_Output(D2Pin, HIGH);
	GPIO_Output(D4Pin, HIGH);

	//Blue button on the board
	GPIO_Callback(Button, CallbackButtonPress, FALL);
	GPIO_Callback(Button, CallbackButtonRelease, RISE);

	// Rotary encoder button
	GPIO_Callback(SW_Pin, CallbackButtonPressSW, FALL);
	GPIO_Callback(SW_Pin, CallbackButtonReleaseSW, RISE);

	GPIO_Callback(DT_Pin, DT, RISE);
	GPIO_Callback(CLK_Pin, CLK, RISE);

	// --- NEW: Initialize the PWM LED Timer ---
	TimerEnable(PwmLED);

	// TIM2 runs at TIM_APB1_FREQ (48MHz). Prescaler of 47 (48-1) = 1MHz timer clock.
	// ARR of 999 (1000-1) = 1kHz PWM frequency.
	TimerPeriod(PwmLED, (TIM_APB1_FREQ / 1000000) - 1, PWM_MAX - 1, 0);
	TimerMode(PwmLED, OUTCMP, PWM1);
	TimerStart(PwmLED);

	//pwmTimer = TimeNow();
}


void Task_GreenLED(void) {
	if (speed[i] == 0) {
		GPIO_Output(D2Pin, HIGH);
		GPIO_Output(GreenLED, HIGH);
	} else if ((TimePassed(toggleTime) >= speed[i]) && (speed[i] != 0)) {
		GPIO_Toggle(GreenLED);
		GPIO_Toggle(D2Pin);
		toggleTime = TimeNow();
	}

	if (TimePassed(speakerTime) >= 1000) {
		GPIO_Output(D4Pin, LOW);
		speakerTime = 0;
	}

	if (UART2_HasCommand()) {
		cmd = UART2_GetCommand();
		if (cmd == '1') {
			i++;
			UART2_SendString("Scroll up \r\n");
			if (i >= SPEED_COUNT) {
				i = 0;
			}
		} else if (cmd == '2') {
			i--;
			UART2_SendString("Scroll down \r\n");
			if (i < 0) {
				i = SPEED_COUNT - 1;
			}
		}
	}

	// --- NEW: Non-blocking PWM dimming logic ---
	// Updates the duty cycle every 2ms.
	// 2ms * 1000 steps = 2000ms (2s) to fade up, and 2s to fade down. (4s total)
//	if (TimePassed(pwmTimer) >= 2) {
//		pwmTimer = TimeNow();
//		pwmValue += pwmStep;
//
//		if (pwmValue >= 1000) {
//			pwmValue = 1000;
//			pwmStep = -1; // Reverse direction to fade down
//		} else if (pwmValue <= 0) {
//			pwmValue = 0;
//			pwmStep = 1;  // Reverse direction to fade up
//		}
//
//		TimerOutput(PwmLED, pwmValue);
//	}

	if (counter1 > counter2){
		UART2_SendString("\r\n Brighter");
		pwmValue += 30;
		rotaryTime1 = 0;
		rotaryTime2 = 0;
		counter1 = 0;
		counter2 = 0;
	}

	else if (counter2 > counter1){
		UART2_SendString("\r\n Dimmer");
		pwmValue -= 30;
		rotaryTime1 = 0;
		rotaryTime2 = 0;
		counter1 = 0;
		counter2 = 0;
	}

//	if ((counter1 == 1) && (rotaryTime1 < rotaryTime2) && (pwmValue <= 1000)) {
//
//	}
	// TimerOutput takes an unsigned value: a negative one would wrap to full brightness
	if (pwmValue < 0)
		pwmValue = 0;
	if (pwmValue > PWM_MAX)
		pwmValue = PWM_MAX;
	TimerOutput(PwmLED, pwmValue);


}

void DT(void) {
	if (counter2 == 0){
		counter1 ++;
		rotaryTime1 = TimeNow();
	}
}

void CLK(void) {
	if (counter1 == 0){
		counter2 ++;
		rotaryTime2 = TimeNow();
	}
}

void CallbackButtonPress(void) {
	debounceTime = TimeNow();
}

void CallbackButtonRelease(void) {
	if (TimePassed(debounceTime) >= 200) {
		i++;
		UART2_SendString("\r\n Button Pressed");
		GPIO_Output(D4Pin, HIGH);
		speakerTime = TimeNow();
		if (i >= SPEED_COUNT) {
			i = 0;
		}
	}
}

//Rotary Button Logic
void CallbackButtonPressSW(void) {
	debounceTime = TimeNow();
}

void CallbackButtonReleaseSW(void) {
	if (TimePassed(debounceTime) >= 200) {
		UART2_SendString("\r\n Rotary Button Pressed");
		if (pwmValue == 0){
			pwmValue = temppwmValue; // Back on at the saved brightness
		}else{
			temppwmValue = pwmValue; // Remember it, then switch off
			pwmValue = 0;
		}
	}
}
