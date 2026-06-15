#include "bsp_adc_temp.h"

#include "app.h"

static ADC_HandleTypeDef hadc1;

void BspAdcTemp_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    ADC_ChannelConfTypeDef channel = {0};
    channel.Channel = ADC_CHANNEL_TEMPSENSOR;
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK) {
        Error_Handler();
    }

    HAL_ADCEx_Calibration_Start(&hadc1);
}

uint32_t BspAdcTemp_ReadRaw(void)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return 0;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }
    uint32_t value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return value;
}
