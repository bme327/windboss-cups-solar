#include "hw.h"
#include "blink.h"
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"

#define MAX_COLORS          3
volatile uint8_t blinkCounter[MAX_COLORS];
uint16_t pins[MAX_COLORS] = {LED1_PIN, LED2_PIN, LED3_PIN};

void blinkAll(uint8_t count) {
    for(uint8_t n = 0 ; n < MAX_COLORS; n++)
    {
        blinkCounter[n] = count*2;
    }
}

void blinkAlternate(uint8_t count) {
    for(uint8_t n = 0 ; n < MAX_COLORS; n++)
    {
        blinkCounter[n] = count*2 + n%2;
    }
}

void blink(uint8_t color, uint8_t count) {
    if ( color >= MAX_COLORS ) return;
    blinkCounter[color] = count*2;
}

uint32_t tickFr = 100;
void blinkSetFreq()
{
	if( HAL_GetTickFreq()== HAL_TICK_FREQ_10HZ )
		tickFr = 1;
}

uint8_t _blinkSlowDown = 0;
void blinkHandler() {

	if ( tickFr != 1 &&
		_blinkSlowDown++ % tickFr != 0 )
	{
		return;
	}
    for(uint8_t n = 0 ; n < MAX_COLORS; n++)
    {
        if( blinkCounter[n] == 0) continue;
        HAL_GPIO_WritePin(LED_PORT, pins[n], blinkCounter[n] % 2 == 0 ? 1 : 0);
        blinkCounter[n]--;
    }

}
