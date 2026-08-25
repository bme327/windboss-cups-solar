/*
 * lora.h
 *
 *  Created on: Dec 11, 2023
 *      Author: ondrejspilka
 */

#ifndef INC_LORA_H_
#define INC_LORA_H_

#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"

extern UART_HandleTypeDef huart1;

#define LORA_STATUS_SENT_ERROR  0x00000001
#define LORA_STATUS_SENT_DONE     0x00000002
#define LORA_STATUS_JOIN_OK     0x00000004
#define LORA_STATUS_JOIN_ERROR  0x00000008
#define LORA_STATUS_BUSY        0x00000010
#define LORA_STATUS_ACK_RECEIVED   0x00000020
#define LORA_STATUS_CRITICAL_ERROR  0x00000040
#define LORA_SENT_OK   0x00
#define LORA_SENT_ERR   0x01

void loraInit();
int loraSend(char* message);
void loraLowpower();
void loraWakeup();
uint32_t getLoraRxTxStatus();
void clearLoraRxTxStatus();
void clearSendStatus();
void clearLastAck();
uint32_t getLastAck(uint32_t defaultWhenNoLastAck);
void initTerminal();
void clearLoraCiritcalError();


#endif /* INC_LORA_H_ */
