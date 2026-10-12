#ifndef LED_APP_H_
#define LED_APP_H_

/**
 * @brief Initializes the GPIO settings for the Blue LED.
 */
void Init_GreenLED(void);
void Init_PWM(void);

/**
 * @brief The task function that maintains the LED state.
 */
void Task_GreenLED(void);
void Task_PWM(void);
#endif
