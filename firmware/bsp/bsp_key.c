#include "bsp_key.h"

#include "board.h"

static bool last_k1;

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
