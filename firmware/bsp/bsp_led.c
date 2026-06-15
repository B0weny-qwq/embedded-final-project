#include "bsp_led.h"

#include "app.h"

static TIM_HandleTypeDef htim3;

#define EF_GPIO_CRL_CFG(pin_index, mode_bits, cnf_bits) \
    (((uint32_t)(mode_bits) | ((uint32_t)(cnf_bits) << 2U)) << ((pin_index) * 4U))

static uint32_t scale(uint8_t brightness)
{
    return (uint32_t)brightness * 10U;
}

static void set_channel(uint32_t channel, bool enabled, uint8_t brightness)
{
    uint32_t compare = enabled ? scale(255U - brightness) : 2550U;
    __HAL_TIM_SET_COMPARE(&htim3, channel, compare);
}

static bool pins_are_initialized(void)
{
    const uint32_t pb0_af_pp_low = EF_GPIO_CRL_CFG(0U, 2U, 2U);
    const uint32_t pb1_af_pp_low = EF_GPIO_CRL_CFG(1U, 2U, 2U);
    const uint32_t pb5_af_pp_low = EF_GPIO_CRL_CFG(5U, 2U, 2U);
    const uint32_t crl_mask = GPIO_CRL_CNF0 | GPIO_CRL_MODE0 |
                              GPIO_CRL_CNF1 | GPIO_CRL_MODE1 |
                              GPIO_CRL_CNF5 | GPIO_CRL_MODE5;
    const uint32_t crl_expect = pb0_af_pp_low | pb1_af_pp_low | pb5_af_pp_low;

    if ((RCC->APB2ENR & RCC_APB2ENR_IOPBEN) == 0U ||
        (RCC->APB2ENR & RCC_APB2ENR_AFIOEN) == 0U ||
        (RCC->APB1ENR & RCC_APB1ENR_TIM3EN) == 0U) {
        return false;
    }
    if ((AFIO->MAPR & AFIO_MAPR_TIM3_REMAP) != AFIO_MAPR_TIM3_REMAP_PARTIALREMAP) {
        return false;
    }
    return (GPIOB->CRL & crl_mask) == crl_expect;
}

void BspLed_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_TIM3_PARTIAL();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = LED_RED_Pin | LED_GREEN_Pin | LED_BLUE_Pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 71;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 2550;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    TIM_OC_InitTypeDef config = {0};
    config.OCMode = TIM_OCMODE_PWM1;
    config.Pulse = 2550;
    config.OCPolarity = TIM_OCPOLARITY_HIGH;
    config.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &config, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&htim3, &config, TIM_CHANNEL_2) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&htim3, &config, TIM_CHANNEL_3) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&htim3, &config, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
    BspLed_SetColorRaw(0, 0, 0);
    if (!pins_are_initialized()) {
        Error_Handler();
    }
}

void BspLed_SetColorRaw(uint8_t red, uint8_t green, uint8_t blue)
{
    set_channel(TIM_CHANNEL_2, red > 0, red);
    set_channel(TIM_CHANNEL_3, green > 0, green);
    set_channel(TIM_CHANNEL_4, blue > 0, blue);
}

void BspLed_ApplyMode(LedMode mode, uint8_t brightness)
{
    switch (mode) {
    case LED_MODE_OFF:
        BspLed_SetColorRaw(0, 0, 0);
        break;
    case LED_MODE_RED:
        BspLed_SetColorRaw(brightness, 0, 0);
        break;
    case LED_MODE_GREEN:
        BspLed_SetColorRaw(0, brightness, 0);
        break;
    case LED_MODE_BLUE:
        BspLed_SetColorRaw(0, 0, brightness);
        break;
    case LED_MODE_WHITE:
        BspLed_SetColorRaw(brightness, brightness, brightness);
        break;
    case LED_MODE_AUTO:
    default:
        BspLed_SetColorRaw(0, brightness, 0);
        break;
    }
}
