/*
 * tasks.c
 *
 *  Created on: Oct 6, 2024
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
#include "bme280support.h"
#include "tasks.h"
#include "peripherals.h"
#include "hw.h"
#include "blink.h"
#include "utils.h"
#include "timers.h"
#include "parameters.h"
#include "math.h"
#include "eeprom.h"
#include "hall.h"
#include "CayenneLpp.h"

uint32_t g_lastMeasureTicks = 0;
long g_lastSendTicks = 0;
char g_term[16];
uint16_t g_termPos = 0;
typedef enum {
	COMMAND_NONE = 0,
	COMMAND_CALIBRATE
} eCommands ;
eCommands g_command = COMMAND_NONE;


void terminal(char received)
{
	if( g_termPos >= sizeof(g_term)-1){
		g_termPos = 0;
	}
	g_term[g_termPos] = received;
	if ( received == '\r' || received == '\n')
	{
		g_term[g_termPos] = 0;
		if ( strcmp("cal",g_term) == 0){
			g_command = COMMAND_CALIBRATE;
		}
		else{
			g_command = COMMAND_NONE;
			debug("Invalid command: %s", g_term);
		}
		g_termPos = 0;
	}else{
		g_termPos++;
	}
}



void tasksInit()
{
    debug("Starting WB...\r\n");
    clearLoraCiritcalError();
    HAL_SetTickFreq(HAL_TICK_FREQ_10HZ);
    blinkSetFreq();
	blinkAll(2);
	startPulseTimer();
	resetHALLStatistics();
	initHALL(&hi2c1);
	init_BME280();
#ifdef CALIB_RUN
	g_Settings.WindDirOffset = CALIB_WINDDIR;
	writeSettings();
#endif
	readSettings();
	loraInit();
	//initTerminal();
    debug("Init done.\r\n");
}

/*
 * Measure wind speed and direction and add to averages
 */
void getAndProcessWind(long period)
{
	readHALL(&hi2c1);
	getAndCalculateDirection(); 				// get direction
	calculateRPS(period);	// calculate average rotations per second

	debug("RPS: %d, RPSAvg: %d, RPSGust: %d, Angle: %d, AVG Angle: %d, Pulses: %d \r\n",
			(int)(g_data.RPS*10),
			(int)(g_data.RPSAvg/g_data.counter*10.0),
			(int)(g_data.RPSGust*10),
			(int)(g_data.directionAngle),
			(int)(g_data.directionAngleAvg),
			g_data.Pulses);
	blink(BLINK_BLUE, 1);
}

/*
 * Measure temperature, pressure and humidity and store them in global g_data
 */
void getAndProcessTPH()
{
	measure_BME280();
	g_data.Temperature = getMeasurement_BME280()->temperature;
	g_data.Pressure = getMeasurement_BME280()->pressure/100.0;
	g_data.Humidity = (int)(getMeasurement_BME280()->humidity);
	debug("Temp: %d °C (*10), Press: %d hPa (*10), Hum: %d %% \r\n",
			(int)g_data.Temperature,
			(int)g_data.Pressure,
			g_data.Humidity);
}

double roundRadix(double val, int roundDigits)
{
	long base = (long)pow(10.0,roundDigits);
	return round(val*base)/base;
}

void prepareAndSendLora()
{
	char message[200];

	aggregateRPS();
	CayenneLppReset();
	int channel = 0;
	CayenneLppAddDigitalInput(channel++, 100 ); 							// protocol version 1.00
	CayenneLppAddAnalogInput( channel++, roundRadix(g_data.RPSAvg,1) );
	CayenneLppAddAnalogInput( channel++, roundRadix(g_data.RPSGust,1) );
	CayenneLppAddAnalogInput( channel++, roundRadix(g_data.directionAngleAvg,0) );
	CayenneLppAddRelativeHumidity(channel++, roundRadix(g_data.Humidity,0));
	CayenneLppAddTemperature( channel++, roundRadix(g_data.Temperature,1) );
	CayenneLppAddBarometricPressure( channel++, roundRadix(g_data.Pressure,1));
	memset(message, 0, sizeof(message));
	uint8_t* buf = CayenneLppGetBuffer();
	int sz  = CayenneLppGetSize();
	// hex coded
	for(uint8_t idx = 0; idx < sz; idx++) {
	   message[2*idx] = nibbleToChar(buf[idx] >> 4);
	   message[2*idx+1] = nibbleToChar(buf[idx] & 0x0F);
	}
	resetHALLStatistics();

	// ensure modem is awake
	loraWakeup();

	// blocking send
	int sendStatus = loraSend(message);
	if ( sendStatus != LORA_SENT_OK ){
		blink(BLINK_BLUE, 2);
		// send not complete or not ACKed
		loraInit();
	}else{
		blink(BLINK_GREEN, 3);
		// all good, send modem to lowPower
		loraLowpower();
	}
}

void tasksLoop()
{
	uint32_t tick = HAL_GetTick();

	if ( (getLoraRxTxStatus() & LORA_STATUS_CRITICAL_ERROR) != 0 )
	{
		debug("[ERROR] Critical LoRa");
		loraInit();
		return;
	}

	if ( tick - g_lastMeasureTicks > 4900 )
	{
		if ( g_lastMeasureTicks < 0 || tick < g_lastMeasureTicks)
			return; // safe for overflows

		getAndProcessWind(tick - g_lastMeasureTicks);		// wind direction and strength
		g_lastMeasureTicks = tick;							// remember last timestamp for HALLs = wind measurement

		if ( (tick - g_lastSendTicks) > 300000 )
		{	// about to send
			g_lastSendTicks = tick;

			getAndProcessTPH(); 	// temperature, pressure, humidity
			prepareAndSendLora();	// prepare Cayenne frame and send via LoRa
		}
	}


	// Put device into low power for 100ms
 	HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}
