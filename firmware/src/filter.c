#include "filter.h"

#include "app_config.h"

void Filter_Init(EwmaFilter *filter)
{
    filter->value_cx100 = 0;
    filter->initialized = 0;
}

int32_t Filter_Update(EwmaFilter *filter, int32_t sample_cx100)
{
    if (!filter->initialized) {
        filter->value_cx100 = sample_cx100;
        filter->initialized = 1;
        return filter->value_cx100;
    }

    filter->value_cx100 += ((sample_cx100 - filter->value_cx100) *
                            APP_FILTER_ALPHA_NUM) /
                           APP_FILTER_ALPHA_DEN;
    return filter->value_cx100;
}
