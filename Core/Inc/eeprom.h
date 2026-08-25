/*
 * eeprom.h
 *
 *  Created on: Oct 8, 2024
 *      Author: ondrejspilka
 */


#ifndef INC_EEPROM_H_
#define INC_EEPROM_H_


typedef struct sSettings
{
	float WindDirOffset;
} tdfSettings;

extern tdfSettings g_Settings;

void writeSettings();
tdfSettings readSettings();

#endif /* INC_EEPROM_H_ */
