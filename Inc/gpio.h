#ifndef GPIO_H_
#define GPIO_H_

#include "stm32f446xx.h"

#define GPIO_PORT_NUM(addr) (((unsigned)(addr) & 0xFC00) / 0x400)

typedef struct {
    GPIO_TypeDef *port;
    int           bit;
} Pin_t;

typedef enum {INPUT=0, OUTPUT=1, ALTFUNC=2, ANALOG=3} PinMode_t;
typedef enum {LOW=0, HIGH=1} PinState_t;
typedef enum {FALL=0, RISE=1} PinEdge_t;
typedef enum {PP=0, OD=1} PinType_t;            // Push-Pull or Open-Drain [3]
typedef enum {S0=0, S1=1, S2=2, S3=3} PinSpeed_t; // Output Speeds [4]
typedef enum {NOPUPD=0, PU=1, PD=2} PinPUPD_t;  // Pull-up/Pull-down [4]

void GPIO_Enable(Pin_t pin);
void GPIO_PortEnable(GPIO_TypeDef *port);
void GPIO_Mode(Pin_t pin, PinMode_t mode);

// NEW: Advanced configuration for I2C/SPI
void GPIO_Config(Pin_t pin, PinType_t ot, PinSpeed_t osp, PinPUPD_t pupd);
void GPIO_AltFunc(Pin_t pin, int af);

PinState_t GPIO_Input(const Pin_t pin);
void GPIO_Output(Pin_t pin, const PinState_t state);
void GPIO_Toggle(Pin_t pin);
void GPIO_Callback(Pin_t pin, void (*func)(void), PinEdge_t edge);

#endif
