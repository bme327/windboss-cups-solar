#ifndef __ADC__H__
#define __ADC__H__

// calib constant for battery divider
#define SENSE_DIVIDER_RATIO 4 // external 1:4 divider
#define BATTERY_NOMINAL 3.7
#define SOLAR_MAX_V 6
#define VREFINT_CAL_ADDR  ((uint16_t*) ((uint32_t) 0x1FF80078))
#define VREFINT_CAL_VOLT  3.0f

uint16_t* ADC_GetValues();
uint16_t* ADC_ReadAllChannels();


#endif
