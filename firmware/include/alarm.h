#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t threshold_cx100;
    int32_t recovery_cx100;
    bool active;
    bool silenced;
} AlarmState;

void Alarm_Init(AlarmState *alarm, int32_t threshold_cx100, int32_t recovery_cx100);
void Alarm_Update(AlarmState *alarm, int32_t filtered_cx100);
void Alarm_Ack(AlarmState *alarm);
void Alarm_SetThreshold(AlarmState *alarm, int32_t threshold_cx100);
