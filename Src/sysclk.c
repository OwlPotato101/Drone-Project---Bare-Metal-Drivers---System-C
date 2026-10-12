#include <stdbool.h>
#include "sysclk.h"

static bool done = false;

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

    // For 48MHz at 3.3V, 1 Wait State (2 CPU cycles) is required [191, Table 5]
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) | FLASH_ACR_LATENCY_1WS;

    // 3. Configure the main PLL factors
    // PLLM = 8: 16MHz / 8 = 2MHz VCO input (recommended 1-2MHz)
    // PLLN = 96: 2MHz * 96 = 192MHz VCO frequency
    // PLLP = 4: 192MHz / 4 = 48MHz Main System Clock
    // PLLSRC = 0: Select HSI as PLL entry clock source
    RCC->PLLCFGR = (8  << RCC_PLLCFGR_PLLM_Pos) | // for the VCO Input, must divide clock by 8 to reach 2MHz range to avoid jitter (Must be between 1-2 MHz)
                   (96 << RCC_PLLCFGR_PLLN_Pos) | // for the VCO Output, must mulitply by 96 (has to be a multiple of 4) to reach 192 MHz. allowed range is 100-432 MHz
                   (0b01 << RCC_PLLCFGR_PLLP_Pos) | // System clock. Once we have our VCO value, we can divide it by the PLL to get the desired system clk. 0b01 maps to Div 4
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

