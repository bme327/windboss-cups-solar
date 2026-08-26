#include <string.h>
#include "main.h"
#include "peripherals.h"
#include "adc.h"

#define ADC_CHANNEL_COUNT 3
#define ADC_AVERAGE 4
#define ADC_TIMEOUT 100U

static uint16_t g_values[ADC_CHANNEL_COUNT];
static volatile uint16_t g_adcBuffer[ADC_CHANNEL_COUNT];
static volatile int g_adcDone = 0;

uint16_t *ADC_ReadAllChannels(void)
{
	memset(g_values, 0, sizeof(g_values));

	for (int sample = 0; sample < ADC_AVERAGE; sample++)
	{
		memset((void *)g_adcBuffer, 0, sizeof(g_adcBuffer));
		g_adcDone = 0;
		if (HAL_ADC_Start_DMA(&hadc, (uint32_t *)g_adcBuffer, ADC_CHANNEL_COUNT) != HAL_OK)
			continue;

		// the whole 3-channel sequence takes ~130us, and HAL_Delay(1) would burn ~200ms at the 10Hz tick
		uint32_t startTick = HAL_GetTick();
		while (g_adcDone == 0 && HAL_GetTick() - startTick < ADC_TIMEOUT)
			;
		if (g_adcDone != 0)
		{
			for (int channel = 0; channel < ADC_CHANNEL_COUNT; channel++)
				g_values[channel] += g_adcBuffer[channel];
		}
		HAL_ADC_Stop_DMA(&hadc);
	}

	for (int channel = 0; channel < ADC_CHANNEL_COUNT; channel++)
		g_values[channel] /= ADC_AVERAGE;

	return g_values;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadcHandle)
{
	if (hadcHandle == &hadc)
		g_adcDone = 1;
}