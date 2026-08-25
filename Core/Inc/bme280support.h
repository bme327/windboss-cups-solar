/*
 * bme280support.h
 *
 *  Created on: Jan 6, 2025
 *      Author: ondrejspilka
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "main.h"
#include "bme280_defs.h"
#include "bme280.h"


#ifndef INC_BME280SUPPORT_H_
#define INC_BME280SUPPORT_H_

extern struct bme280_dev g_bme_dev;
extern struct bme280_data g_bme_data;
extern struct bme280_data g_bme_comp_data;

typedef struct bme280_data tdf_bme280_data;

BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
void init_BME280();
tdf_bme280_data* measure_BME280();
tdf_bme280_data* getMeasurement_BME280();

#endif /* INC_BME280SUPPORT_H_ */
