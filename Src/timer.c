/*
 * timer.c
 *
 * Generic Timer Peripheral Driver for STM32F446
 */

#include "timer.h"
#include <stddef.h>

// --------------------------------------------------------
// Initialization
// --------------------------------------------------------

// Force an update of timer registers after configuration changes
static void UpdateTimerRegisters(TIM_TypeDef *TIM) {
    TIM->EGR = TIM_EGR_UG;        // Generate update event (EGR is write-only)
    while (!(TIM->SR & TIM_SR_UIF)) {} // Wait for UIF flag
    TIM->SR = ~TIM_SR_UIF;        // Clear flag (rc_w0: a read-modify-write could clear other flags)
}

// Enable the timer peripheral clock + configure GPIO pin
void TimerEnable(TimerIO_t timer) {
    TIM_TypeDef *TIM = timer.iface;
    int timer_num = TIMER_NUM(TIM);

    // ----------------------------
    // ENABLE TIMER PERIPHERAL CLOCK
    // ----------------------------
    if (timer_num == 1 || (timer_num >= 8 && timer_num <= 11)) {
        if (timer_num == 1) RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
        else if (timer_num == 8) RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;
        else if (timer_num == 9) RCC->APB2ENR |= RCC_APB2ENR_TIM9EN;
        else if (timer_num == 10) RCC->APB2ENR |= RCC_APB2ENR_TIM10EN;
        else if (timer_num == 11) RCC->APB2ENR |= RCC_APB2ENR_TIM11EN;
    } else {
        // TIM2 to TIM7 map to bits 0 to 5 on APB1
        if (timer_num >= 2 && timer_num <= 7) {
            RCC->APB1ENR |= (1 << (timer_num - 2));
        }
        // TIM12 to TIM14 map to bits 6 to 8 on APB1
        else if (timer_num >= 12 && timer_num <= 14) {
            RCC->APB1ENR |= (1 << (timer_num - 6));
        }
    }

    // Enable preload
    TIM->CR1 |= TIM_CR1_ARPE;

    // ----------------------------
    // CONFIGURE GPIO PIN FOR TIMER CHANNEL
    // ----------------------------
    GPIO_Enable(timer.pin);
    GPIO_Mode(timer.pin, ALTFUNC);
    GPIO_Config(timer.pin, PP, S3, NOPUPD);
    GPIO_AltFunc(timer.pin, timer.af);
}

// --------------------------------------------------------
// Timer periods
// --------------------------------------------------------
void TimerPeriod(TimerIO_t tio, uint32_t psc, uint32_t arr, uint16_t rcr) {
    TIM_TypeDef *TIM = tio.iface;

    TIM->PSC = psc;   // Prescaler
    TIM->ARR = arr;   // Auto-reload

    // Only Advanced Timers TIM1 and TIM8 possess a Repetition Counter Register (RCR)
    if (TIM == TIM1 || TIM == TIM8) {
        TIM->RCR = rcr;
    }

    TIM->CNT = 0;     // Reset counter
    UpdateTimerRegisters(TIM);
}

// --------------------------------------------------------
// Timer operating modes
// --------------------------------------------------------
void TimerMode(TimerIO_t tio, TimerMode_t mode, TimerSelect_t sel) {
    TIM_TypeDef *TIM = tio.iface;

    if (tio.chan < 1 || tio.chan > 4) return; // Bounds check

    volatile uint32_t *CCMR = (tio.chan <= 2) ? &TIM->CCMR1 : &TIM->CCMR2;
    uint32_t shift = ((tio.chan - 1) % 2) * 8;

    if (mode == OUTCMP) {
        // Output compare (PWM)
        uint32_t pwmMode = (sel == PWM1 ? 0b110 : 0b111);

        *CCMR &= ~(0x7 << (TIM_CCMR1_OC1M_Pos + shift));      // Clear mode bits
        *CCMR |= (pwmMode << TIM_CCMR1_OC1M_Pos) << shift;    // Set new mode bits
        *CCMR |= (TIM_CCMR1_OC1PE << shift);                  // Preload enable

        // Explicitly set PWM polarity to active-high (Clear CCxP bit)
        TIM->CCER &= ~(TIM_CCER_CC1P << ((tio.chan - 1) * 4));

        // Enable capture/compare output on the pin
        TIM->CCER |= (TIM_CCER_CC1E << ((tio.chan - 1) * 4));

        // For advanced control timers, Main Output Enable (MOE) must be set
        if (TIM == TIM1 || TIM == TIM8) {
            TIM->BDTR |= TIM_BDTR_MOE;
        }

    } else if (mode == INCAP) {
        // Input capture mode
        uint32_t inputSel = (sel == TISEC ? 0b10 : 0b01);

        *CCMR &= ~(0x3 << (TIM_CCMR1_CC1S_Pos + shift));
        *CCMR |= (inputSel << TIM_CCMR1_CC1S_Pos) << shift;

        // Enable capture on the pin
        TIM->CCER |= (TIM_CCER_CC1E << ((tio.chan - 1) * 4));
    }
}

// --------------------------------------------------------
// Start timer
// --------------------------------------------------------
void TimerStart(TimerIO_t tio) {
    TIM_TypeDef *TIM = tio.iface;
    TIM->CR1 |= TIM_CR1_CEN; // Counter Enable
}

// --------------------------------------------------------
// Observation and control
// --------------------------------------------------------
void TimerOutput(TimerIO_t timer, uint32_t thresh) {
    if (timer.chan < 1 || timer.chan > 4) return; // Bounds check

    // Hardware protection: Prevent thresh from exceeding the auto-reload value
    if (thresh > timer.iface->ARR) {
        thresh = timer.iface->ARR;
    }

    volatile uint32_t *CCR = (&timer.iface->CCR1 + (timer.chan - 1));
    *CCR = thresh;
}

uint32_t TimerInput(TimerIO_t timer) {
    if (timer.chan < 1 || timer.chan > 4) return 0; // Bounds check
    volatile uint32_t *CCR = (&timer.iface->CCR1 + (timer.chan - 1));
    return *CCR;
}

// Enable timer for the DMA controller and trigger timers on overflow
// --------------------------------------------------------
// DMA Burst Configuration
// --------------------------------------------------------
void TimerConfigDMABurst(TimerIO_t tio, uint8_t base_address_offset, uint8_t burst_length) {
    TIM_TypeDef *TIM = tio.iface;

    // Clear out old DBL and DBA bits (Bits 0-4 and Bits 8-12)
    TIM->DCR &= ~((0x1F << 8) | 0x1F);

    uint32_t dbl_val = (burst_length - 1) & 0x1F;
    uint32_t dba_val = base_address_offset & 0x1F;

    TIM->DCR |= (dbl_val << 8) | dba_val;
}

void TimerEnableDMARequest(TimerIO_t tio) {
    tio.iface->DIER |= TIM_DIER_UDE;
}

// --------------------------------------------------------
// Interrupt handling
// --------------------------------------------------------

static void (*callbacks[14][5])(void) = {0};

void TimerCallback(TimerIO_t timer, void (*func)(void), TimerFlag_t flag) {
    TIM_TypeDef *TIM = timer.iface;
    int i = TIMER_NUM(TIM);
    int IRQn = 0;

    // Explicitly map IRQ numbers based on STM32F446 architecture
    switch(i) {
        case 1:  IRQn = (flag == UP) ? TIM1_UP_TIM10_IRQn : TIM1_CC_IRQn; break;
        case 2:  IRQn = TIM2_IRQn; break;
        case 3:  IRQn = TIM3_IRQn; break;
        case 4:  IRQn = TIM4_IRQn; break;
        case 5:  IRQn = TIM5_IRQn; break;
        case 6:  IRQn = TIM6_DAC_IRQn; break;
        case 7:  IRQn = TIM7_IRQn; break;
        case 8:  IRQn = (flag == UP) ? TIM8_UP_TIM13_IRQn : TIM8_CC_IRQn; break;
        case 9:  IRQn = TIM1_BRK_TIM9_IRQn; break;
        case 10: IRQn = TIM1_UP_TIM10_IRQn; break;
        case 11: IRQn = TIM1_TRG_COM_TIM11_IRQn; break;
        case 12: IRQn = TIM8_BRK_TIM12_IRQn; break;
        case 13: IRQn = TIM8_UP_TIM13_IRQn; break;
        case 14: IRQn = TIM8_TRG_COM_TIM14_IRQn; break;
        default: return;
    }

    callbacks[i - 1][flag] = func;

    TIM->DIER |= (1 << flag);
    UpdateTimerRegisters(TIM);

    NVIC->IP[IRQn] = 0;
    __COMPILER_BARRIER();
    NVIC->ISER[IRQn / 32] = 1 << (IRQn % 32);
    __COMPILER_BARRIER();
}

static void TimerIRQHandler(TIM_TypeDef *TIM, int i, int IRQn) {
    void (*fp)(void);

    NVIC->ICPR[IRQn / 32] = 1 << (IRQn % 32);

    for (int j = 0; j <= 4; j++) {
        if ((TIM->SR & (1 << j)) && (TIM->DIER & (1 << j))) {
            TIM->SR = ~(1 << j);

            fp = callbacks[i - 1][j];
            if (fp != NULL) {
                fp();
            }
        }
    }
}

// --------------------------------------------------------
// Standard STM32F446 IRQ Definitions
// --------------------------------------------------------
void TIM1_BRK_TIM9_IRQHandler(void)       { TimerIRQHandler(TIM9, 9, TIM1_BRK_TIM9_IRQn); }
void TIM1_UP_TIM10_IRQHandler(void)       {
    if (TIM1->SR & TIM_SR_UIF) TimerIRQHandler(TIM1, 1, TIM1_UP_TIM10_IRQn);
    if (TIM10->SR & TIM_SR_UIF) TimerIRQHandler(TIM10, 10, TIM1_UP_TIM10_IRQn);
}
void TIM1_TRG_COM_TIM11_IRQHandler(void)  { TimerIRQHandler(TIM11, 11, TIM1_TRG_COM_TIM11_IRQn); }
void TIM1_CC_IRQHandler(void)             { TimerIRQHandler(TIM1, 1, TIM1_CC_IRQn); }
void TIM2_IRQHandler(void)                { TimerIRQHandler(TIM2, 2, TIM2_IRQn); }
void TIM3_IRQHandler(void)                { TimerIRQHandler(TIM3, 3, TIM3_IRQn); }
void TIM4_IRQHandler(void)                { TimerIRQHandler(TIM4, 4, TIM4_IRQn); }
void TIM5_IRQHandler(void)                { TimerIRQHandler(TIM5, 5, TIM5_IRQn); }
void TIM6_DAC_IRQHandler(void)            { TimerIRQHandler(TIM6, 6, TIM6_DAC_IRQn); }
void TIM7_IRQHandler(void)                { TimerIRQHandler(TIM7, 7, TIM7_IRQn); }
void TIM8_BRK_TIM12_IRQHandler(void)      { TimerIRQHandler(TIM12, 12, TIM8_BRK_TIM12_IRQn); }
void TIM8_UP_TIM13_IRQHandler(void)       {
    if (TIM8->SR & TIM_SR_UIF) TimerIRQHandler(TIM8, 8, TIM8_UP_TIM13_IRQn);
    if (TIM13->SR & TIM_SR_UIF) TimerIRQHandler(TIM13, 13, TIM8_UP_TIM13_IRQn);
}
void TIM8_TRG_COM_TIM14_IRQHandler(void)  { TimerIRQHandler(TIM14, 14, TIM8_TRG_COM_TIM14_IRQn); }
void TIM8_CC_IRQHandler(void)             { TimerIRQHandler(TIM8, 8, TIM8_CC_IRQn); }
