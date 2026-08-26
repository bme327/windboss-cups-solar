#ifndef __ADC__H__
#define __ADC__H__

#define SENSE_DIVIDER_RATIO 4.0f
#define BATTERY_NOMINAL 3.7f
#define SOLAR_MAX_V 6.0f
#define VREFINT_CAL_ADDR ((uint16_t *)0x1FF80078U)
#define VREFINT_CAL_VOLT 3.0f

uint16_t *ADC_ReadAllChannels(void);

#endif