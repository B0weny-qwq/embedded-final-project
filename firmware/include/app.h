#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board.h"

void App_Init(void);
void App_Tick(void);

void App_SetThreshold(int32_t threshold_cx100);
void App_SetLedMode(LedMode mode);
void App_SetBeepMode(BeepMode mode);
void App_AckAlarm(void);

void Error_Handler(void);
