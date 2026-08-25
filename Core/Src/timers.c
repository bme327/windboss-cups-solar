/*
 * timers.c
 *
 *  Created on: Oct 8, 2024
 *      Author: ondrejspilka
 */

#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "peripherals.h"
#include "hw.h"
#include "blink.h"
#include "timers.h"
#include "parameters.h"


uint16_t getPulses()
{
	uint16_t res = __HAL_TIM_GET_COUNTER(&htim2);
	return res;
}


void startPulseTimer()
{
    HAL_TIM_Base_Start(&htim2);
}
