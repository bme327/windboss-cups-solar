/*
 * timers.h
 *
 *  Created on: Oct 8, 2024
 *      Author: ondrejspilka
 */

#ifndef INC_TIMERS_H_
#define INC_TIMERS_H_

#include "tasks.h"
#include <stdint.h>

void startPulseTimer();
uint16_t getPulses();

#endif /* INC_TIMERS_H_ */
