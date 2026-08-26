#ifndef __TMAG_5273_H__
#define __TMAG_5273_H__

#include "math.h"

#define HALL_DEVICE_CONFIG_1 0x00
#define HALL_DEVICE_CONFIG_2 0x01
#define HALL_SENSOR_CONFIG_1 0x02
#define HALL_SENSOR_CONFIG_2 0x03
#define HALL_X_THR_CONFIG 0x04
#define HALL_Y_THR_CONFIG 0x05
#define HALL_Z_THR_CONFIG 0x06
#define HALL_T_CONFIG 0x07
#define HALL_INT_CONFIG_1 0x08
#define HALL_MAG_GAIN_CONFIG 0x09
#define HALL_MAG_OFFSET_CONFIG_1 0x0a
#define HALL_MAG_OFFSET_CONFIG_2 0x0b
#define HALL_I2C_ADDRESS 0x0c
#define HALL_DEVICE_ID 0x0d
#define HALL_MANUFACTURER_ID_LSB 0x0e
#define HALL_MANUFACTURER_ID_MSB 0x0f
#define HALL_T_MSB_RESULT 0x10
#define HALL_T_LSB_RESULT 0x11
#define HALL_X_MSB_RESULT 0x12
#define HALL_X_LSB_RESULT 0x13
#define HALL_Y_MSB_RESULT 0x14
#define HALL_Y_LSB_RESULT 0x15
#define HALL_Z_MSB_RESULT 0x16
#define HALL_Z_LSB_RESULT 0x17
#define HALL_CONV_STATUS 0x18
#define HALL_ANGLE_RESULT_MSB 0x19
#define HALL_ANGLE_RESULT_LSB 0x1a
#define HALL_MAGNITUDE_RESULT 0x1b

#define HALL_HW_ADDR ( 0x35 << 1 )
#define HALL_PRI_ADDR ( 0x35 << 1 )
#define HALL_SEC_ADDR ( 0x37 << 1 )


#define TS0 25
#define TADC0 17508
#define TADCRES 60.1

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

extern uint32_t g_i2cError;


typedef struct windData {
    float directionAngle;
    int directionAngleAvg;
    float RPS;
    float RPSAvg;
    float RPSGust;
    float minDir;
    float maxDir;
    float xAvg;
    float yAvg;
    uint32_t counter;
    float Pressure;
    float Temperature;
    int Humidity;
    long Pulses;
    long LastPulses;
    long Battery;
    long Solar;
    int BatteryAct;
    int SolarAct;
} windData_t;


extern uint8_t hallBuffer[];
extern I2C_HandleTypeDef hi2c1;
extern windData_t g_data;

void resetHALLStatistics();
void initHALL(I2C_HandleTypeDef* port);
void readHALL(I2C_HandleTypeDef* port);
float getX();
float getY();
float getZ();
float getTemp();
int getAngle();
windData_t* getWindData();
HAL_I2C_StateTypeDef getI2CStatus();
void getAndCalculateDirection();
void calculateRPS(uint32_t period);


#endif
