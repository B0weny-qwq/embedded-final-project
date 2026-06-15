#include "bsp_beep.h"

#include "board.h"

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
}

void BspBeep_Set(bool on)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
