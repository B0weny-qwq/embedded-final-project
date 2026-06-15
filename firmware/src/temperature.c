#include "temperature.h"

int32_t Temperature_FromAdcCx100(uint32_t adc)
{
    const int32_t vdda_mv = 3300;
    const int32_t v25_mv = 1430;
    const int32_t avg_slope_uv_per_c = 4300;
    int32_t vsense_uv = (int32_t)((adc * vdda_mv * 1000L) / 4095L);
    return 2500 + ((v25_mv * 1000L - vsense_uv) * 100) / avg_slope_uv_per_c;
}
