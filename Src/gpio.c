#include <stddef.h>
#include <stdbool.h>
#include "gpio.h"

// --------------------------------------------------------
// Initialization
// --------------------------------------------------------

void GPIO_Enable(Pin_t pin) {
	GPIO_PortEnable(pin.port);
}

void GPIO_PortEnable(GPIO_TypeDef *port) {
	// Enable GPIO clock via AHB1
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN << GPIO_PORT_NUM(port);
}

void GPIO_Mode(Pin_t pin, PinMode_t mode) {
	pin.port->MODER = (pin.port->MODER & ~(0x3 << (2 * pin.bit)))
			| (mode << (2 * pin.bit));
}

void GPIO_Config(Pin_t pin, PinType_t ot, PinSpeed_t osp, PinPUPD_t pupd) {
	pin.port->OTYPER = (pin.port->OTYPER & ~(0b1 << (pin.bit)))
			| ot << (pin.bit);
	pin.port->OSPEEDR = (pin.port->OSPEEDR & ~(0b11 << (pin.bit * 2)))
			| osp << (pin.bit * 2);
	pin.port->PUPDR = (pin.port->PUPDR & ~(0b11 << (pin.bit * 2)))
			| pupd << (pin.bit * 2);
}

void GPIO_AltFunc(Pin_t pin, int af) {
	if (pin.bit < 8) {
		pin.port->AFR[0] = (pin.port->AFR[0] & ~(0xF << (4 * pin.bit)))
				| (af << (4 * pin.bit));
	} else {
		int offset = pin.bit - 8;
		pin.port->AFR[1] = (pin.port->AFR[1] & ~(0xF << (4 * offset)))
				| (af << (4 * offset));
	}
}

// --------------------------------------------------------
// Pin Observation and Control
// --------------------------------------------------------

PinState_t GPIO_Input(const Pin_t pin) {
	return (pin.port->IDR & (1 << pin.bit)) ? HIGH : LOW;
}

uint16_t GPIO_PortInput(GPIO_TypeDef *port) {
	return (uint16_t) port->IDR;
}

void GPIO_Output(Pin_t pin, const PinState_t state) {
	if (state == HIGH)
		pin.port->BSRR = (1 << pin.bit);
	else
		pin.port->BSRR = (1 << (pin.bit + 16));
}

void GPIO_PortOutput(GPIO_TypeDef *port, uint16_t states) {
	port->ODR = states;
}

void GPIO_Toggle(Pin_t pin) {
	if (pin.port->IDR & (1 << pin.bit))
		pin.port->BSRR = (1 << (pin.bit + 16)); // Reset
	else
		pin.port->BSRR = (1 << pin.bit);        // Set
}

// ---------------------------------------------------
// Interrupt handling
// ---------------------------------------------------

// Below the DSHOT DMA interrupt (0), which must never wait on a button
#define GPIO_IRQ_PRIORITY 2

static void (*callbacks[16][2])(void);
static GPIO_TypeDef *linePort[16]; // Port routed to each EXTI line, to read its level

void GPIO_Callback(Pin_t pin, void (*func)(void), PinEdge_t edge) {
	callbacks[pin.bit][edge] = func;
	linePort[pin.bit] = pin.port;

	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

	if (edge == RISE)
		EXTI->RTSR |= (1 << pin.bit);
	else
		EXTI->FTSR |= (1 << pin.bit);

	SYSCFG->EXTICR[pin.bit / 4] &= ~(0xF << (4 * (pin.bit % 4)));
	SYSCFG->EXTICR[pin.bit / 4] |= GPIO_PORT_NUM(pin.port)
			<< (4 * (pin.bit % 4));

	EXTI->IMR |= (1 << pin.bit);

	IRQn_Type IRQn;
	if (pin.bit <= 4)
		IRQn = (IRQn_Type) (EXTI0_IRQn + pin.bit);
	else if (pin.bit <= 9)
		IRQn = EXTI9_5_IRQn;
	else
		IRQn = EXTI15_10_IRQn;

	NVIC_SetPriority(IRQn, GPIO_IRQ_PRIORITY);
	NVIC_EnableIRQ(IRQn);
}

void GPIO_IRQHandler(int i) {
	if (EXTI->PR & (1 << i)) {
		EXTI->PR = (1 << i);

		void (*rise)(void) = callbacks[i][RISE];
		void (*fall)(void) = callbacks[i][FALL];

		// EXTI only reports that an edge happened, not which one. With both
		// edges armed, the pin level right after the edge tells them apart.
		if (rise && fall) {
			if (linePort[i]->IDR & (1 << i))
				rise();
			else
				fall();
		} else if (rise) {
			rise();
		} else if (fall) {
			fall();
		}
	}
}

void EXTI0_IRQHandler(void) {
	GPIO_IRQHandler(0);
}
void EXTI1_IRQHandler(void) {
	GPIO_IRQHandler(1);
}
void EXTI2_IRQHandler(void) {
	GPIO_IRQHandler(2);
}
void EXTI3_IRQHandler(void) {
	GPIO_IRQHandler(3);
}
void EXTI4_IRQHandler(void) {
	GPIO_IRQHandler(4);
}

void EXTI9_5_IRQHandler(void) {
	for (int i = 5; i <= 9; i++) {
		if (EXTI->PR & (1 << i))
			GPIO_IRQHandler(i);
	}
}

void EXTI15_10_IRQHandler(void) {
	for (int i = 10; i <= 15; i++) {
		if (EXTI->PR & (1 << i))
			GPIO_IRQHandler(i);
	}
}
