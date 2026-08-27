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
#include "adc.h"

uint32_t g_lastMeasureTicks = 0;
uint32_t g_lastSendTicks = 0;
uint32_t g_lastSuccessTicks = 0;
uint8_t g_sendFailures = 0;
char g_term[16];
uint16_t g_termPos = 0;

// no confirmed uplink for this long means software recovery failed, stop kicking the watchdog
#define MAX_SILENCE_TICKS 3600000UL
// consecutive failed uplinks before the modem is reset and rejoined
#define MAX_SEND_FAILURES 3
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
#ifdef DEBUG_SLEEP_ENABLED
    // DBGMCU is clock gated on L0, without this the DBG_SLEEP write is discarded
    __HAL_RCC_DBGMCU_CLK_ENABLE();
    HAL_DBGMCU_EnableDBGSleepMode();
#endif
    clearLoraCiritcalError();
    watchdogInit();
    HAL_SetTickFreq(HAL_TICK_FREQ_10HZ);
    blinkSetFreq();
	blinkAll(2);
	startPulseTimer();
	resetHALLStatistics();
	initHALL(&hi2c1);
	init_BME280();
#ifdef CALIB_RUN
	float dir = 0; //expects steady value 
	for(int i = 0; i < CALIB_RUN_COUNTS; i++){
		readHALL(&hi2c1);
		HAL_Delay(100);
		dir += getAngle(); 
	}
	
	g_Settings.WindDirOffset = 180 - dir / CALIB_RUN_COUNTS;
	writeSettings();
#endif
	readSettings();
	loraInit();
	//initTerminal();
	g_lastSuccessTicks = HAL_GetTick();
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

void getAndProcessVoltages(void)
{
	uint16_t *values = ADC_ReadAllChannels();
	if (values[2] == 0)
		return;

	float vdd = (float)(*VREFINT_CAL_ADDR) * VREFINT_CAL_VOLT / (float)values[2];
	float adcToVoltage = SENSE_DIVIDER_RATIO * vdd / 4096.0f;
	g_data.BatteryAct = (int)(100.0f * (float)values[1] * adcToVoltage / BATTERY_NOMINAL);
	g_data.SolarAct = (int)(100.0f * (float)values[0] * adcToVoltage / SOLAR_MAX_V);
	g_data.Battery += g_data.BatteryAct;
	g_data.Solar += g_data.SolarAct;

	debug("Battery: %d, Solar: %d\r\n", g_data.BatteryAct, g_data.SolarAct);
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
	g_data.Battery /= g_data.counter;
	g_data.Solar /= g_data.counter;
	CayenneLppReset();
	int channel = 0;
	CayenneLppAddDigitalInput(channel++, 101 ); 							// protocol version 1.01
	// 2 decimals matches the analog field resolution; at 1 decimal a light-wind average
	// over ~60 measure cycles collapses to 0.0
	CayenneLppAddAnalogInput( channel++, roundRadix(g_data.RPSAvg,2) );
	CayenneLppAddAnalogInput( channel++, roundRadix(g_data.RPSGust,2) );
	// analog input is int16 x0.01 and saturates at 327.67, so direction is pre-scaled:
	// the raw field carries whole degrees, decoder multiplies analog_in_3 by 100
	CayenneLppAddAnalogInput( channel++, (lround(g_data.directionAngleAvg) % 360) / 100.0f );
	CayenneLppAddRelativeHumidity(channel++, roundRadix(g_data.Humidity,0));
	CayenneLppAddTemperature( channel++, roundRadix(g_data.Temperature,1) );
	CayenneLppAddBarometricPressure( channel++, roundRadix(g_data.Pressure,1));
	CayenneLppAddAnalogInput(channel++, (float)g_data.Battery);
	CayenneLppAddAnalogInput(channel++, (float)g_data.Solar);
	memset(message, 0, sizeof(message));
	uint8_t* buf = CayenneLppGetBuffer();
	int sz  = CayenneLppGetSize();
	// the Cayenne cursor may reach 242 but message holds two hex chars per byte plus a terminator
	if ( sz > (int)(sizeof(message) - 1) / 2 )
		sz = (int)(sizeof(message) - 1) / 2;
	// hex coded
	for(int idx = 0; idx < sz; idx++) {
	   message[2*idx] = nibbleToChar(buf[idx] >> 4);
	   message[2*idx+1] = nibbleToChar(buf[idx] & 0x0F);
	}
	resetHALLStatistics();

	// ensure modem is awake
	loraWakeup();

	// blocking send
	int sendStatus = loraSend(message);
	if ( sendStatus == LORA_SENT_OK ){
		blink(BLINK_GREEN, 3);
		g_sendFailures = 0;
		g_lastSuccessTicks = HAL_GetTick();
		// all good, send modem to lowPower
		loraLowpower();
		return;
	}

	blink(BLINK_BLUE, 2);
	g_sendFailures++;
	// a single rejection is usually a busy modem or duty cycle, only a run of them means the link is gone
	if ( g_sendFailures < MAX_SEND_FAILURES ){
		loraLowpower();
		return;
	}

	g_sendFailures = 0;
	loraInit();
}

void tasksLoop()
{
	uint32_t tick = HAL_GetTick();

	if ( (tick - g_lastSuccessTicks) < MAX_SILENCE_TICKS )
		watchdogRefresh();

	if ( (getLoraRxTxStatus() & LORA_STATUS_CRITICAL_ERROR) != 0 )
	{
		debug("[ERROR] Critical LoRa");
		loraInit();
		g_sendFailures = 0;
		return;
	}

	// unsigned arithmetic already wraps correctly, an explicit tick < last guard would latch forever
	if ( tick - g_lastMeasureTicks > MEASURE_CYCLE )
	{
		getAndProcessWind(tick - g_lastMeasureTicks);		// wind direction and strength
		getAndProcessVoltages();
		g_lastMeasureTicks = tick;							// remember last timestamp for HALLs = wind measurement

		if ( (tick - g_lastSendTicks) > SEND_CYCLE )
		{	// about to send
			g_lastSendTicks = tick;

			getAndProcessTPH(); 	// temperature, pressure, humidity
			prepareAndSendLora();	// prepare Cayenne frame and send via LoRa
		}
	}


	// Put device into low power for 100ms
 	HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}
