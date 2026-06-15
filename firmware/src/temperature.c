#include "temperature.h"

int32_t Temperature_FromAdcCx100(uint32_t adc)
{
    const int64_t vdda_mv = 3300;
    const int64_t v25_uv = 1430000;
    const int64_t avg_slope_uv_per_c = 4300;
    int64_t vsense_uv = ((int64_t)adc * vdda_mv * 1000) / 4095;
    return (int32_t)(2500 + ((v25_uv - vsense_uv) * 100) / avg_slope_uv_per_c);
}
