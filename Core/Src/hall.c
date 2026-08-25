#include <math.h>
#include <string.h>
#include "main.h"
#include "hall.h"
#include "timers.h"
#include "eeprom.h"

uint8_t HALL_BUFFER_START =  HALL_T_MSB_RESULT;
uint8_t hallBuffer[] = {0,0,0,0,0,0,0,0,0,0,0,0};
HAL_StatusTypeDef g_Status;
windData_t g_data;

long g_usClock = 0;
long g_lastEval = 0;
extern unsigned long HAL_GetTick();

uint32_t g_i2cError = 0;

void resetHALLStatistics(){
    g_data.directionAngle = 0.0f;
    g_data.directionAngleAvg = 0.0f;
    g_data.minDir = 361;
    g_data.maxDir = -1;
    g_data.RPS = 0.0f;
    g_data.RPSAvg = 0.0f;
    g_data.RPSGust = 0.0f;
    g_data.xAvg = 0.0;
    g_data.yAvg = 0.0;
    g_data.counter = 0;
    g_data.Pulses = 0;
}

void calculateRPS(uint32_t period)
{
	g_data.Pulses = (long)getPulses();				// get incremental sensor pulses

	long diff = g_data.Pulses - g_data.LastPulses;
	if ( diff < 0 )
	{	// handle overflow of counter
		diff = 65535 - g_data.LastPulses + g_data.Pulses;
	}
	const float K_ANEMO = 1; // since circle length is 0.38 and K is 2.4 ~ 1
	float time = (float)period;
	time /= 1000.0;									// milliseconds
	g_data.RPS = (float)diff * K_ANEMO / time;
	g_data.RPSAvg += g_data.RPS;
	if ( g_data.RPS > g_data.RPSGust){
		g_data.RPSGust = g_data.RPS;
	}
	g_data.LastPulses = g_data.Pulses;				// mark last pulses
	g_data.counter++;
}

void aggregateRPS()
{
	g_data.RPSAvg /= g_data.counter;
}


void getAndCalculateDirection() {

    float dir = getAngle();
    const float DIR_FILTER = 0.9;

    dir -= g_Settings.WindDirOffset;
	if ( dir > 360 )
		dir -= 360;
	if ( dir < 0 )
		dir += 360;

	// inverse, config?
	dir = 360.0 - dir;

    if ( dir < g_data.minDir)
        g_data.minDir = dir;
    if ( dir > g_data.maxDir)
        g_data.maxDir = dir;
    g_data.directionAngle = dir;
    //average of direction via vector
    float ang = M_PI * dir / 180;
    float x = cos(ang);
    float y = sin(ang);
    g_data.xAvg = (x + DIR_FILTER * g_data.xAvg)/(1.0 + DIR_FILTER);
    g_data.yAvg = (y + DIR_FILTER * g_data.yAvg)/(1.0 + DIR_FILTER);

    g_data.directionAngleAvg = atan2(g_data.yAvg, g_data.xAvg) * 180 / M_PI;
    if ( g_data.directionAngleAvg < 0 )
    {
        g_data.directionAngleAvg = 360 + g_data.directionAngleAvg;
    }

}

void  initHALL(I2C_HandleTypeDef* port) {

    const uint8_t initComand[] = {
        HALL_DEVICE_CONFIG_1,
        2 << 2 ,            // 4x average, 1b read
        1 << 4 | 2,         // low noise, continuous
        3 << 4,             // XY, 1ms sleeptime
        1 << 2              // angle XY, +-133mT
        };
    const uint8_t tempComand[] = {
        HALL_T_CONFIG,
        1 ,            // temp enabled
        };

    //init 1st
    g_i2cError = 0;
    g_Status = HAL_I2C_Master_Transmit(port, HALL_PRI_ADDR, (uint8_t*)initComand, sizeof(initComand), 1000 );
    if ( g_Status == HAL_OK ) {
    	g_Status = HAL_I2C_Master_Transmit(port, HALL_PRI_ADDR, (uint8_t*)tempComand, sizeof(tempComand), 1000 );
    }else{
    	g_i2cError = HAL_I2C_GetError(port);
    }

}

void readHALL(I2C_HandleTypeDef* port)
{
    const uint8_t command[] = {
        HALL_T_MSB_RESULT,
     };

    memset(hallBuffer, 0, sizeof(hallBuffer));

    g_Status = HAL_I2C_Master_Transmit(port, HALL_PRI_ADDR, (uint8_t*)command, sizeof(command), 1000 );
    if ( g_Status == HAL_OK)
    	g_Status = HAL_I2C_Master_Receive(port, HALL_PRI_ADDR | 1, hallBuffer, sizeof(hallBuffer), 1000);
}

float calculateB(uint16_t val){

    float B = val & 0x7FFF;
    B +=  -(val & 0x8000) ;
    B = B / pow(2,16);
    B = B * 2.0 * 133.0;
    return B;
}

float calculateT(uint16_t val){

    float T = val;
    return TS0 + (T - TADC0)/TADCRES;
}

float calculateAngle(uint16_t val){

    // as per doc
    float A = (val & 0x1FF0) >> 4;
    A += (float)(val & 0x000F) / 16.0;

    A -= SOUTH;
    if ( A <= 0 )
    	A = 360 + A;
    // N->S inverse
    A = A + 180;
    if ( A >= 360 )
    	A -= 360;
    return A;
}

//set origin for indexing
void setHalBufOrigin(uint8_t registerAddr) {
    HALL_BUFFER_START = registerAddr;
}

//easily
uint8_t HALBUF(uint8_t registerAddr){
    return hallBuffer[registerAddr - HALL_BUFFER_START];
}

float getX()
{
    uint16_t val =  (uint16_t)HALBUF(HALL_X_MSB_RESULT) << 8 | (uint16_t)HALBUF(HALL_X_LSB_RESULT);
    return calculateB(val);
}

float getY()
{
    uint16_t val =  (uint16_t)HALBUF(HALL_Y_MSB_RESULT) << 8 | (uint16_t)HALBUF(HALL_Y_LSB_RESULT);
    return calculateB(val);
}

float getZ()
{
    uint16_t val =  (uint16_t)HALBUF(HALL_Z_MSB_RESULT) << 8 | (uint16_t)HALBUF(HALL_Z_LSB_RESULT);
    return calculateB(val);
}

float getTemp()
{
    uint16_t val =  (uint16_t)HALBUF(HALL_T_MSB_RESULT) << 8 | (uint16_t)HALBUF(HALL_T_LSB_RESULT);
    return calculateT(val);
}

int getAngle()
{
    uint16_t val =  (uint16_t)HALBUF(HALL_ANGLE_RESULT_MSB) << 8 | (uint16_t)HALBUF(HALL_ANGLE_RESULT_LSB);
    return (int)round(calculateAngle(val));
}

uint8_t getStatus(){
    return HALBUF(HALL_CONV_STATUS);
}

HAL_I2C_StateTypeDef getI2CStatus(){
    return (HAL_I2C_StateTypeDef)g_Status;
}
