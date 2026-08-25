/*
 * utils.c
 *
 *  Created on: Oct 7, 2024
 *      Author: ondrejspilka
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "main.h"
#include "lora.h"
#include "bme280.h"
#include "hw.h"
#include "peripherals.h"
#include "parameters.h"
#include "utils.h"
#include "stm32l0xx_hal_adc.h"

#ifdef DEBUG_SERIAL_ENABLED
void debug(const char * format, ...)
{
	char msg[200];
    va_list args;
    va_start(args, format);
	vsnprintf(msg, sizeof(msg), format, args);
	HAL_UART_Transmit(&huart2, (unsigned char*) msg, strlen(msg), 500);
}
#else
void debug(const char * format, ...)
{
}
#endif

char nibbleToChar(short int  byte) {
    return (byte < 10) ? byte + '0' : byte - 10 + 'A';
}

void Delay_Micro(uint32_t period)
{
	  __IO uint32_t waitLoopIndex = (period * (SystemCoreClock / 1000000U));

	  while (waitLoopIndex != 0U)
	  {
	    waitLoopIndex--;
	  }
}

void delay_us(uint32_t period, void *intf_ptr)
{
/*	__HAL_TIM_SET_COUNTER(&htim22,0);  // set the counter value a 0
	uint16_t cntr = __HAL_TIM_GET_COUNTER(&htim22);

	while ( cntr < period){
		cntr = __HAL_TIM_GET_COUNTER(&htim22);
	}
	cntr--;*/
	Delay_Micro(period);
}
/*
char* stristr( char* str1, const char* str2 )
{
    char* p1 = str1 ;
    const char* p2 = str2 ;
    const char* r = *p2 == 0 ? str1 : 0 ;

    while( *p1 != 0  )
    {
        while( *p2 != 0 )
        {
            if ( tolower( (unsigned char)*p1 ) != tolower( (unsigned char)*p2 ) )
                break;
            p2++;
        }
        if ( *p2 == 0 )
            return p1;

        p1++ ;
    }

    return *p1 == 0 ? NULL : p1;
}
*/
