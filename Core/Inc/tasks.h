/*
 * tasks.h
 *
 *  Created on: Oct 6, 2024
 *      Author: ondrejspilka
 */

#ifndef INC_TASKS_H_
#define INC_TASKS_H_

void tasksInit();
void tasksLoop();
void pwmHandler();
void terminal(char received);
void calibrate();

#endif /* INC_TASKS_H_ */
