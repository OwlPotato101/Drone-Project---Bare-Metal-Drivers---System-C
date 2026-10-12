#include "systick.h"
#include "sysclk.h"
#include "stm32f446xx.h"

#define SYSTICKS (SYSCLK_FREQ / 1000)
static volatile Time_t sysTime = 0;

void StartSysTick() {
	ConfigureSystemClock();
	sysTime = 0;
	SysTick->LOAD = (uint32_t) (SYSTICKS - 1);
	NVIC_SetPriority(SysTick_IRQn, 7);
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk
			| SysTick_CTRL_ENABLE_Msk;
}

// Ensure this is the ONLY definition of SysTick_Handler in the project
void SysTick_Handler(void) {
	sysTime++;
}

void WaitForSysTick(void) {
	Time_t wasTime = sysTime;
	while (sysTime == wasTime) {
		__asm volatile ("wfi");
	}
}

Time_t TimeNow(void) {
	return sysTime;
}

Time_t TimePassed(Time_t since) {
	Time_t now = sysTime;
	if (now >= since)
		return now - since;
	// Roll-over protection using the defined TIME_MAX
	return (TIME_MAX - since) + now + 1;
}
