#pragma once

#include "stm32f1xx_hal.h"

#define LED_RED_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_5
#define LED_GREEN_GPIO_Port GPIOB
#define LED_GREEN_Pin GPIO_PIN_0
#define LED_BLUE_GPIO_Port GPIOB
#define LED_BLUE_Pin GPIO_PIN_1

#define BEEP_GPIO_Port GPIOA
#define BEEP_Pin GPIO_PIN_8

#define KEY_K1_GPIO_Port GPIOA
#define KEY_K1_Pin GPIO_PIN_0
#define KEY_K2_GPIO_Port GPIOC
#define KEY_K2_Pin GPIO_PIN_13

#define USB_DM_GPIO_Port GPIOA
#define USB_DM_Pin GPIO_PIN_11
#define USB_DP_GPIO_Port GPIOA
#define USB_DP_Pin GPIO_PIN_12
#define USB_DISCONNECT_GPIO_Port GPIOD
#define USB_DISCONNECT_Pin GPIO_PIN_6

typedef enum {
    LED_MODE_AUTO = 0,
    LED_MODE_OFF,
    LED_MODE_RED,
    LED_MODE_GREEN,
    LED_MODE_BLUE,
    LED_MODE_WHITE,
} LedMode;

typedef enum {
    BEEP_MODE_AUTO = 0,
    BEEP_MODE_ON,
    BEEP_MODE_OFF,
} BeepMode;
