#pragma once

#include <stdint.h>

typedef struct {
    int32_t value_cx100;
    uint8_t initialized;
} EwmaFilter;

void Filter_Init(EwmaFilter *filter);
int32_t Filter_Update(EwmaFilter *filter, int32_t sample_cx100);
