/*
 * eeprom.c
 *
 *  Created on: Oct 8, 2024
 *      Author: ondrejspilka
 */

#include <stdint.h>
#include <string.h>
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "eeprom.h"

tdfSettings g_Settings = {0};

void writeSettings() {
	  uint32_t* address = (uint32_t*)DATA_EEPROM_BASE;
	  HAL_FLASHEx_DATAEEPROM_Unlock();  //Unprotect the EEPROM to allow writing

	  uint32_t* pData = (uint32_t*)&g_Settings;
	  for(uint16_t n = 0; n < sizeof(g_Settings)/sizeof(uint32_t); n++){
		  HAL_FLASHEx_DATAEEPROM_Program(TYPEPROGRAMDATA_WORD, (uint32_t)address, pData[n]);
		  address++;
	  }
	  HAL_FLASHEx_DATAEEPROM_Lock();  // Reprotect the EEPROM
}

tdfSettings readSettings() {
	memset(&g_Settings, 0, sizeof(g_Settings));
	uint32_t* address =  (__IO uint32_t*)DATA_EEPROM_BASE;
	memcpy(&g_Settings, address, sizeof(g_Settings));
	return g_Settings;
}
