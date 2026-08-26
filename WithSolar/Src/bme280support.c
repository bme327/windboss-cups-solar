#include "bme280support.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "main.h"
#include "lora.h"
#include "bme280.h"
#include "bme280support.h"
#include "tasks.h"
#include "peripherals.h"
#include "hw.h"
#include "utils.h"

#define BME280_WR 0b11101100
#define BME280_RD 0b11101101

struct bme280_dev g_bme_dev;
struct bme280_data g_bme_data;
struct bme280_data g_bme_comp_data;
struct bme280_settings g_bme_settings;
uint32_t g_bme_period;
const uint8_t BME_I2C_ADDR = BME280_I2C_ADDR_PRIM;

BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, (BME_I2C_ADDR<<1), &reg_addr, 1, 5000 );
	if ( status == HAL_OK ) {
		status = HAL_I2C_Master_Receive(&hi2c1, (BME_I2C_ADDR<<1) | 0x01, reg_data, length, 5000 );
	}
	return status == HAL_OK ? BME280_INTF_RET_SUCCESS : -1;
}

BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
	uint8_t buffer[100];
	memset(buffer,0,sizeof(buffer));
    memcpy((uint8_t*)buffer+1,reg_data,length);
    buffer[0] = reg_addr;

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, (BME_I2C_ADDR<<1), (uint8_t*)&buffer, length+1, 5000 );
    return status == HAL_OK ? BME280_INTF_RET_SUCCESS : -1;
}



void init_BME280()
{
	g_bme_dev.read = bme280_i2c_read;
	g_bme_dev.write = bme280_i2c_write;
	g_bme_dev.delay_us = delay_us;
	g_bme_dev.intf = BME280_I2C_INTF;

	bme280_init(&g_bme_dev);
	//bme280_soft_reset(&g_bme_dev);
	bme280_get_sensor_settings(&g_bme_settings, &g_bme_dev);

	g_bme_settings.filter = BME280_FILTER_COEFF_2;
	g_bme_settings.osr_h = BME280_OVERSAMPLING_4X;
	g_bme_settings.osr_p = BME280_OVERSAMPLING_4X;
	g_bme_settings.osr_t = BME280_OVERSAMPLING_4X;
	g_bme_settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

    bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &g_bme_settings, &g_bme_dev);
    bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &g_bme_dev);

}

tdf_bme280_data* measure_BME280()
{
	uint8_t status_reg = 0;
	int cnt = 0;
    bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &g_bme_dev);
	while (((status_reg & BME280_STATUS_MEAS_DONE) == 0) && cnt++ < 1000)
	{
		bme280_get_regs(BME280_REG_STATUS, &status_reg, 1, &g_bme_dev);
		delay_us(10, g_bme_dev.intf_ptr);
	}
	bme280_get_sensor_data(BME280_ALL, &g_bme_data, &g_bme_dev);

	return &g_bme_data;
}

tdf_bme280_data* getMeasurement_BME280()
{
	return &g_bme_data;
}
