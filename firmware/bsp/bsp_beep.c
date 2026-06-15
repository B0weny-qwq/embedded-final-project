#include "bsp_beep.h"

#include "app.h"
#include "board.h"

#define EF_GPIO_CRH_CFG(pin_index, mode_bits, cnf_bits) \
    (((uint32_t)(mode_bits) | ((uint32_t)(cnf_bits) << 2U)) << (((pin_index) - 8U) * 4U))

static bool pins_are_initialized(void)
{
    const uint32_t pa8_out_pp_low = EF_GPIO_CRH_CFG(8U, 2U, 0U);
    const uint32_t crh_mask = GPIO_CRH_CNF8 | GPIO_CRH_MODE8;

    if ((RCC->APB2ENR & RCC_APB2ENR_IOPAEN) == 0U) {
        return false;
    }
    if ((GPIOA->CRH & crh_mask) != pa8_out_pp_low) {
        return false;
    }
    return (GPIOA->ODR & BEEP_Pin) == 0U;
}

void BspBeep_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = BEEP_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BEEP_GPIO_Port, &gpio);
    BspBeep_Set(false);
    if (!pins_are_initialized()) {
        Error_Handler();
    }
}

void BspBeep_Set(bool on)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
