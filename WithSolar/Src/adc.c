/*
 * adc.c
 *
 *  Created on: May 11, 2025
 *      Author: ondrejspilka
 */

#include <string.h>
#include "main.h"
#include "adc.h"

#define ADC_CHANNEL_COUNT 3
#define ADC_AVERAGE 4

uint16_t g_values[ADC_CHANNEL_COUNT];
volatile uint16_t adc_buffer[ADC_CHANNEL_COUNT];
volatile int g_DMA_done = 0;

uint16_t* ADC_GetValues()
{
	return g_values;
}

/* ADC has 160cycles since we have weak source */
uint16_t* ADC_ReadAllChannels()
{
	/*
	HAL_ADC_Start(&hadc);
	for(int n = 0; n < ADC_CHANNEL_COUNT; n++)
	{
		HAL_ADC_PollForConversion(&hadc, 10);  // take last of 10, since weak source
		g_values[n] = HAL_ADC_GetValue(&hadc);
    }
    HAL_ADC_Stop(&hadc);
    return g_values;*/


	memset(g_values, 0, sizeof(g_values));

	for(int n = 0; n < ADC_AVERAGE; n++)
	{
		memset(adc_buffer, 0, sizeof(adc_buffer));
		g_DMA_done = 0;
		HAL_ADC_Start_DMA(&hadc, (uint32_t*)adc_buffer, ADC_CHANNEL_COUNT);
		while(g_DMA_done == 0)
			HAL_Delay(1);
		for(int chan = 0; chan < ADC_CHANNEL_COUNT; chan++)
		{
			g_values[chan] += adc_buffer[chan];
		}
		HAL_ADC_Stop_DMA(&hadc);
	}
	for(int chan = 0; chan < ADC_CHANNEL_COUNT; chan++)
	{
		g_values[chan] /= ADC_AVERAGE;
	}
	return g_values;
}


void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
  g_DMA_done = 1;
}

