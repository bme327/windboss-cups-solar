/*
 * lora.c
 *
 *  Created on: Dec 11, 2023
 *      Author: ondrejspilka
 */

#include "hw.h"
#include "lora.h"
#include "loraProvisioning.h"
#include <stdio.h>
#include <string.h>
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "utils.h"
#include "blink.h"
#include "peripherals.h"
#include "tasks.h"

uint8_t LoraBuffer[200];
volatile uint8_t LoraRecvBuffer[200];
volatile uint8_t UartRxChar[4];
volatile uint8_t TermRxChar[1];
HAL_StatusTypeDef g_LoraStatus;
volatile uint16_t LoraRecvBufferPos = 0;
volatile uint32_t loraStatus = 0;
volatile int disableParse = 0;

#define COMMAND_TMO 	5000
#define TX_TMO 			10000
#define LORA_SEND_TMO 	15000

typedef enum {
	LORA_OK = 1,
	LORA_ERR_AT = 2
} LoraStatus;

void clearTxBuffer(){
	memset(LoraBuffer,0, sizeof(LoraBuffer));
}
void clearRecvBuffer(){
	memset(LoraRecvBuffer,0, sizeof(LoraRecvBuffer));
	LoraRecvBufferPos = 0;
}


uint32_t getLoraRxTxStatus(){
	return loraStatus;
}

void clearLoraRxTxStatus() {
	loraStatus = 0;
}

void setLoraBusy() {
	loraStatus |= LORA_STATUS_BUSY;
}


void clearLoraCiritcalError()
{
	loraStatus &= ~LORA_STATUS_CRITICAL_ERROR;
}


void clearLoraBusy() {
	loraStatus &= ~LORA_STATUS_BUSY;
}

void clearSendStatus() {
	loraStatus &= ~(LORA_STATUS_SENT_ERROR | LORA_STATUS_SENT_DONE | LORA_STATUS_ACK_RECEIVED);
}

/// @brief Main LoRa status parser, stores status in loraStatus
/// @param buf Char buffer of single line modem message
void parseRecvStatus(char* buf) {
	if ( strchr(buf, '+') != NULL ) {
		clearLoraBusy();
	}
	if ( strstr(buf, "+JOIN") != NULL ) {
		if ( strstr(buf, "Join failed") != NULL ) {
			loraStatus = LORA_STATUS_JOIN_ERROR;
			return;
		}
		if ( strstr(buf, "Done") != NULL ||
			strstr(buf, "Joined") != NULL ) {
			loraStatus = LORA_STATUS_JOIN_OK;
			return;
		}
	}
	else if ( strstr(buf, "+CMSG") != NULL ) {
		if ( strstr(buf, "Please join") != NULL ) {
			loraStatus = LORA_STATUS_JOIN_ERROR | LORA_STATUS_SENT_ERROR;
			return;
		}
		if ( strstr(buf, "error") != NULL ) {
			loraStatus &= ~LORA_STATUS_SENT_DONE;
			loraStatus |= LORA_STATUS_SENT_ERROR;
			return;
		}
		if ( strstr(buf, "Received") != NULL )
		{
			loraStatus |= LORA_STATUS_ACK_RECEIVED;
			return;
		}
		if ( strstr(buf, "Done") != NULL ) {
			loraStatus &= ~LORA_STATUS_SENT_ERROR;
			loraStatus |= LORA_STATUS_SENT_DONE;
			return;
		}
	}
}

void initTerminal()
{
	debug("Terminal activated...\r\n");
	HAL_UART_Receive_IT(&huart2, (unsigned char*) TermRxChar, 1);
}

void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef *huart)
{
	debug("Lora UART Abort");
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	debug("Lora UART Error, %d\r\n", huart->ErrorCode);
	g_LoraStatus |= LORA_STATUS_CRITICAL_ERROR;
}

void startLoraRead()
{
	memset(UartRxChar, 0, sizeof(UartRxChar));
	HAL_UART_Receive_IT(&huart1, UartRxChar, 1);
}

/// @brief UART interrupt handler, appends and eventually calls parsing of received data
/// @param huart
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if ( huart == &huart1)
	{
		LoraRecvBuffer[LoraRecvBufferPos++] = UartRxChar[0];
		if ( LoraRecvBufferPos >= sizeof(LoraRecvBuffer) )
			LoraRecvBufferPos = 0;

#ifdef DEBUG_LORA
		HAL_UART_Transmit(&huart2, UartRxChar, 1, 100);
#endif
		if( disableParse == 0 &&  UartRxChar[0] == '\n' )
		{
			char buf[200];
			strcpy(buf, LoraRecvBuffer);
			clearRecvBuffer();
			startLoraRead();
			parseRecvStatus(buf);
		}else{
			startLoraRead();
		}
	}else if ( huart == &huart2)
	{
		terminal(TermRxChar[0]);
		HAL_UART_Receive_IT(&huart2, (unsigned char*) TermRxChar, 1);
	}
}

/// @brief Sends command to LoRa modem and wait for first command reply
/// @param cmd String command
void command(const char* cmd){
	setLoraBusy();
	g_LoraStatus = HAL_UART_Transmit(&huart1, (const uint8_t*)cmd, strlen(cmd), TX_TMO);

	long tick = HAL_GetTick();
	while ( HAL_GetTick() - tick < COMMAND_TMO &&
			loraStatus & LORA_STATUS_BUSY )
	{
		HAL_Delay(1000);
	}
}

/// @brief Sends data and doesn't wait
/// @param cmd
void commandNoWait(const char* cmd){
	g_LoraStatus = HAL_UART_Transmit(&huart1, (const uint8_t*)cmd, strlen(cmd), TX_TMO);
}

/// @brief LoRa forced join AT+JOIN=FORCE
void loraForceJoin(){
	command("AT+JOIN=FORCE\n");
}

void loraAutoJoin(){
	command("AT+JOIN=120\n");
}

/// @brief Internal setup and JOIN
/// @return Always LORA_OK
LoraStatus loraSetup() {

	clearTxBuffer();
	clearRecvBuffer();
	clearLoraRxTxStatus();

	blink(BLINK_RED,1);
	startLoraRead();

	command("AT+RESET\n");
	command("AT+LOG=DEBUG\n");
	command("AT+JOIN=0\n");
	command("AT+POWER=30\n");

	strcpy((char*)LoraBuffer,"AT+ID=AppEui, \"");
	strcat((char*)LoraBuffer,APP_EUI);
	strcat((char*)LoraBuffer,"\"\n");
	command((const char*)LoraBuffer);

	strcpy((char*)LoraBuffer,"AT+ID=DevEui, \"");
	strcat((char*)LoraBuffer,DEVICE_EUI);
	strcat((char*)LoraBuffer,"\"\n");
	command((const char*)LoraBuffer);

	strcpy((char*)LoraBuffer,"AT+KEY=APPKEY, \"");
	strcat((char*)LoraBuffer,APP_KEY);
	strcat((char*)LoraBuffer,"\"\n");
	command((const char*)LoraBuffer);

	command("AT+DR=EU868\n");
	command("AT+CH=NUM,0-2\n");
	command("AT+PORT=1\n");
	command("AT+ADR=ON\n");
	command("AT+MODE=LWOTAA\n");
	command("AT+LW=LEN\n");
	blink(BLINK_RED,2);

	loraAutoJoin();

	return LORA_OK;
}

/// @brief Confirmed send, blocking.
/// @param message Hex coded message sent by CMSGHEX
/// @return 1 on success, 0 on failure
int loraSend(char* message) {

	uint32_t tick = HAL_GetTick();
	uint32_t currentTick = HAL_GetTick();
	clearSendStatus();
	// DEBUG - disableParse=1;
	strcpy((char*)LoraBuffer,"AT+CMSGHEX=\"");
	strcat((char*)LoraBuffer,message);
	strcat((char*)LoraBuffer,"\"\r\n");
	command((const char*)LoraBuffer);

	// wait for command finished
	while( (currentTick - tick) < LORA_SEND_TMO ) {
		if ( (loraStatus & LORA_STATUS_SENT_DONE) > 0 ) {
			break;
		}
		currentTick = HAL_GetTick();
	}
	tick = currentTick - tick;
	// DEBUG - disableParse=0;

	// check if ACK was received
	if ( (loraStatus & LORA_STATUS_JOIN_ERROR) > 0 ||
	 	 (loraStatus & LORA_STATUS_SENT_ERROR) > 0)
	{
		return LORA_SENT_ERR;
	}
	return LORA_SENT_OK;
}

/// @brief Sends modem to lowpower
void loraLowpower() {
	command("AT+LOWPOWER");
}

/// @brief Wakes up modem
void loraWakeup() {
	commandNoWait("A");
	command("AT+ID");
	command("AT+ID");
}

void loraHWReset()
{
	HAL_GPIO_WritePin(E5RST_PORT, E5RST_PIN, GPIO_PIN_SET);
	HAL_Delay(1000);
	HAL_GPIO_WritePin(E5RST_PORT, E5RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(1000);
	HAL_GPIO_WritePin(E5RST_PORT, E5RST_PIN, GPIO_PIN_SET);
	HAL_Delay(1000);
}

void loraInit() {

	loraHWReset();
	loraSetup();
}
