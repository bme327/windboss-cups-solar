/*
 * peripherals.h
 *
 *  Created on: Oct 6, 2024
 *      Author: ondrejspilka
 */

#ifndef INC_PERIPHERALS_H_
#define INC_PERIPHERALS_H_

extern ADC_HandleTypeDef hadc;

extern I2C_HandleTypeDef hi2c1;

extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

#endif /* INC_PERIPHERALS_H_ */
