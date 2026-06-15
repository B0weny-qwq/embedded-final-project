#pragma once

#include <stdint.h>

#include "board.h"

void BspLed_Init(void);
void BspLed_SetColorRaw(uint8_t red, uint8_t green, uint8_t blue);
void BspLed_ApplyMode(LedMode mode, uint8_t brightness);
