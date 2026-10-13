#include <stdbool.h>
#include "sysclk.h"

static bool done = false;

// Called by Reset_Handler before .data/.bss are set up and before main(), so it
// must not touch global variables. The startup file only declares SystemInit
// weak; without this definition the call is linked as a NOP.
void SystemInit(void) {
    // Full access to CP10/CP11 (the FPU). The build uses -mfloat-abi=hard, so the
    // first float instruction raises a NOCP UsageFault until this is set
    // (PM0214 4.6.1, 4.6.6)
    SCB->CPACR |= (3UL << (10 * 2)) | (3UL << (11 * 2));
    __DSB();
    __ISB();
}

void ConfigureSystemClock(void) {
    if (done) {
        return; // SYSCLK already configured [1]
    }
    // Enable Power Controller clock
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    // 1. Enable Internal High-Speed oscillator (HSI)
    RCC->CR |= RCC_CR_HSION; // Enable HSI
    while (!(RCC->CR & RCC_CR_HSIRDY)); // Wait until HSI is stable to turn on the clk, HSIRDY must be at 1 to be ready

    // 2. Configure Flash Latency

    // For 48MHz at 3.3V, 1 Wait State (2 CPU cycles) is required [RM0390 3.4.1, Table 5]
    // Prefetch and the instruction/data caches are off after reset; turn them on too
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) | FLASH_ACR_LATENCY_1WS
               | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    // The new latency must be in effect before the clock goes up [RM0390 3.4.1]
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != FLASH_ACR_LATENCY_1WS);

    // 3. Configure the main PLL factors
    // PLLM = 8: 16MHz / 8 = 2MHz VCO input (recommended 1-2MHz)
    // PLLN = 96: 2MHz * 96 = 192MHz VCO frequency
    // PLLP = 4: 192MHz / 4 = 48MHz Main System Clock
    // PLLQ = 4: 192MHz / 4 = 48MHz for USB/SDIO
    // PLLR = 2: 192MHz / 2 = 96MHz for I2S/SAI
    // PLLSRC = 0: Select HSI as PLL entry clock source
    RCC->PLLCFGR = (8  << RCC_PLLCFGR_PLLM_Pos) | // for the VCO Input, must divide clock by 8 to reach 2MHz range to avoid jitter (Must be between 1-2 MHz)
                   (96 << RCC_PLLCFGR_PLLN_Pos) | // for the VCO Output, must mulitply by 96 (has to be a multiple of 4) to reach 192 MHz. allowed range is 100-432 MHz
                   (0b01 << RCC_PLLCFGR_PLLP_Pos) | // System clock. Once we have our VCO value, we can divide it by the PLL to get the desired system clk. 0b01 maps to Div 4
                   (4  << RCC_PLLCFGR_PLLQ_Pos) | // 0 and 1 are "wrong configuration" [RM0390 6.3.2], so never leave Q or R at 0
                   (2  << RCC_PLLCFGR_PLLR_Pos) |
                   (0  << RCC_PLLCFGR_PLLSRC_Pos);

    // 4. Enable the PLL and wait for lock
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // 5. Configure Bus Prescalers
    // HCLK (AHB) = 48MHz (Max 180MHz) -> Div 1
    // PCLK1 (APB1) = 24MHz (Max 45MHz) -> Div 2
    // PCLK2 (APB2) = 48MHz (Max 90MHz) -> Div 1
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE_Msk)  | RCC_CFGR_HPRE_DIV1;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE1_Msk) | RCC_CFGR_PPRE1_DIV2;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PPRE2_Msk) | RCC_CFGR_PPRE2_DIV1;

    // 6. Switch System Clock to PLL [20]
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL); // Confirm PLL is used

    done = true;
}
