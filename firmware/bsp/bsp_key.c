#include "bsp_key.h"

#include "app.h"
#include "board.h"

static bool last_k1;

#define EF_GPIO_CRL_CFG(pin_index, mode_bits, cnf_bits) \
    (((uint32_t)(mode_bits) | ((uint32_t)(cnf_bits) << 2U)) << ((pin_index) * 4U))

#define EF_GPIO_CRH_CFG(pin_index, mode_bits, cnf_bits) \
    (((uint32_t)(mode_bits) | ((uint32_t)(cnf_bits) << 2U)) << (((pin_index) - 8U) * 4U))

static bool pins_are_initialized(void)
{
    const uint32_t pa0_input_pupd = EF_GPIO_CRL_CFG(0U, 0U, 2U);
    const uint32_t pc13_input_floating = EF_GPIO_CRH_CFG(13U, 0U, 1U);

    if ((RCC->APB2ENR & RCC_APB2ENR_IOPAEN) == 0U ||
        (RCC->APB2ENR & RCC_APB2ENR_IOPCEN) == 0U) {
        return false;
    }
    if ((GPIOA->CRL & (GPIO_CRL_CNF0 | GPIO_CRL_MODE0)) != pa0_input_pupd) {
        return false;
    }
    if ((GPIOA->ODR & KEY_K1_Pin) != 0U) {
        return false;
    }
    return (GPIOC->CRH & (GPIO_CRH_CNF13 | GPIO_CRH_MODE13)) == pc13_input_floating;
}

void BspKey_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = KEY_K1_Pin;
    HAL_GPIO_Init(KEY_K1_GPIO_Port, &gpio);

    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = KEY_K2_Pin;
    HAL_GPIO_Init(KEY_K2_GPIO_Port, &gpio);

    last_k1 = HAL_GPIO_ReadPin(KEY_K1_GPIO_Port, KEY_K1_Pin) == GPIO_PIN_SET;
    if (!pins_are_initialized()) {
        Error_Handler();
    }
}

bool BspKey_K1PressedEdge(void)
{
    bool now = HAL_GPIO_ReadPin(KEY_K1_GPIO_Port, KEY_K1_Pin) == GPIO_PIN_SET;
    bool edge = now && !last_k1;
    last_k1 = now;
    return edge;
}

bool BspKey_K2Pressed(void)
{
    return HAL_GPIO_ReadPin(KEY_K2_GPIO_Port, KEY_K2_Pin) == GPIO_PIN_SET;
}
