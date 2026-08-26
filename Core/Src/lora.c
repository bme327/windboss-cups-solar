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
// long enough to cover confirmed uplink retransmissions at high spreading factors
#define LORA_SEND_TMO 	25000
#define JOIN_TMO 		25000
// one attempt only, the send failure counter retries across cycles without blocking the wind sampling
#define LORA_INIT_RETRIES 1

typedef enum {
	LORA_OK = 1,
	LORA_ERR_AT = 2
} LoraStatus;

void clearTxBuffer(){
	memset(LoraBuffer,0, sizeof(LoraBuffer));
}
void clearRecvBuffer(){
	// no memset, the line is copied out by length and NUL terminated by the caller
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

void clearJoinStatus() {
	loraStatus &= ~(LORA_STATUS_JOIN_OK | LORA_STATUS_JOIN_ERROR | LORA_STATUS_JOIN_DONE);
}

/// @brief Main LoRa status parser, stores status in loraStatus
/// @param buf Char buffer of single line modem message
/// @note Runs in the UART ISR, at 9600 baud on a 6 MHz core the budget is ~6250 cycles per byte
void parseRecvStatus(char* buf) {
	if ( strchr(buf, '+') != NULL ) {
		clearLoraBusy();
	}

	while ( *buf == '\r' || *buf == '\n' || *buf == ' ' )
		buf++;

	// status lines are tagged at the start, prefix compare avoids scanning the whole line
	if ( strncmp(buf, "+JOIN", 5) == 0 ) {
		// "busy" is not matched here, it means a join is already running, let the timeout decide
		if ( strstr(buf, "Join failed") != NULL ) {
			loraStatus &= ~LORA_STATUS_JOIN_OK;
			loraStatus |= LORA_STATUS_JOIN_ERROR;
			return;
		}
		// the modem prints "+JOIN: Done" after a failed attempt too, only these mean joined
		if ( strstr(buf, "NetID") != NULL ||
			strstr(buf, "Joined") != NULL ||
			strstr(buf, "joined") != NULL ) {
			loraStatus &= ~LORA_STATUS_JOIN_ERROR;
			loraStatus |= LORA_STATUS_JOIN_OK;
			return;
		}
		if ( strstr(buf, "Done") != NULL ) {
			loraStatus |= LORA_STATUS_JOIN_DONE;
			return;
		}
	}
	else if ( strncmp(buf, "+CMSG", 5) == 0 ) {
		// common outcomes first, each line carries only one of these so order is free
		if ( strstr(buf, "Done") != NULL ) {
			loraStatus |= LORA_STATUS_SENT_DONE;
			return;
		}
		if ( strstr(buf, "Received") != NULL )
		{
			loraStatus |= LORA_STATUS_ACK_RECEIVED;
			return;
		}
		if ( strstr(buf, "Please join") != NULL ) {
			loraStatus &= ~(LORA_STATUS_JOIN_OK | LORA_STATUS_SENT_DONE);
			loraStatus |= LORA_STATUS_JOIN_ERROR | LORA_STATUS_SENT_ERROR;
			return;
		}
		if ( strstr(buf, "error") != NULL ||
			strstr(buf, "busy") != NULL ) {
			loraStatus &= ~LORA_STATUS_SENT_DONE;
			loraStatus |= LORA_STATUS_SENT_ERROR;
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

void startLoraRead()
{
	HAL_UART_Receive_IT(&huart1, UartRxChar, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if ( huart != &huart1 )
		return;

	debug("Lora UART Error, %lu\r\n", (unsigned long)huart->ErrorCode);
	loraStatus |= LORA_STATUS_CRITICAL_ERROR;

	__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF | UART_CLEAR_PEF);
	huart->ErrorCode = HAL_UART_ERROR_NONE;

	// HAL leaves reception disabled after a blocking error, without this the modem is never heard again
	clearRecvBuffer();
	startLoraRead();
}

/// @brief UART interrupt handler, appends and eventually calls parsing of received data
/// @param huart
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if ( huart == &huart1)
	{
		// only this ISR touches it, keeps 200 bytes off the interrupt stack
		static char lineBuffer[sizeof(LoraRecvBuffer)];
		static uint8_t lineTruncated = 0;
		uint8_t received = UartRxChar[0];

		if ( LoraRecvBufferPos < sizeof(LoraRecvBuffer) - 1 )
			LoraRecvBuffer[LoraRecvBufferPos++] = received;
		else
			lineTruncated = 1;

#ifdef DEBUG_LORA
		HAL_UART_Transmit(&huart2, UartRxChar, 1, 100);
#endif
		if ( received == '\n' )
		{
			uint16_t len = LoraRecvBufferPos;
			uint8_t parse = ( disableParse == 0 && lineTruncated == 0 );

			if ( parse )
			{
				memcpy(lineBuffer, (const char*)LoraRecvBuffer, len);
				lineBuffer[len] = '\0';
			}
			lineTruncated = 0;
			clearRecvBuffer();
			startLoraRead();

			if ( parse )
				parseRecvStatus(lineBuffer);
			return;
		}
		startLoraRead();
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
	if ( g_LoraStatus != HAL_OK )
		loraStatus |= LORA_STATUS_CRITICAL_ERROR;

	uint32_t tick = HAL_GetTick();
	while ( HAL_GetTick() - tick < COMMAND_TMO &&
			loraStatus & LORA_STATUS_BUSY )
	{
		watchdogRefresh();
		HAL_Delay(100);
	}
	clearLoraBusy();
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

/// @brief Blocks until the modem reports a successful join or the timeout expires
/// @return 1 when joined, 0 otherwise
static int waitForJoin(uint32_t timeout){
	uint32_t tick = HAL_GetTick();
	while ( HAL_GetTick() - tick < timeout )
	{
		if ( loraStatus & LORA_STATUS_JOIN_OK )
			return 1;
		if ( loraStatus & LORA_STATUS_JOIN_ERROR )
			return 0;
		watchdogRefresh();
		HAL_Delay(100);
	}
	return (loraStatus & LORA_STATUS_JOIN_OK) != 0;
}

/// @brief Internal setup and JOIN
/// @return LORA_OK when the modem joined the network, LORA_ERR_AT otherwise
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

	snprintf((char*)LoraBuffer, sizeof(LoraBuffer), "AT+ID=AppEui, \"%s\"\n", APP_EUI);
	command((const char*)LoraBuffer);

	snprintf((char*)LoraBuffer, sizeof(LoraBuffer), "AT+ID=DevEui, \"%s\"\n", DEVICE_EUI);
	command((const char*)LoraBuffer);

	snprintf((char*)LoraBuffer, sizeof(LoraBuffer), "AT+KEY=APPKEY, \"%s\"\n", APP_KEY);
	command((const char*)LoraBuffer);

	command("AT+DR=EU868\n");
	command("AT+CH=NUM,0-2\n");
	command("AT+PORT=1\n");
	command("AT+ADR=ON\n");
	command("AT+MODE=LWOTAA\n");
	command("AT+LW=LEN\n");
	blink(BLINK_RED,2);

	clearJoinStatus();
	loraForceJoin();
	LoraStatus result = waitForJoin(JOIN_TMO) ? LORA_OK : LORA_ERR_AT;

	// background retries in case the join failed or the link drops between send cycles
	loraAutoJoin();

	return result;
}

/// @brief Confirmed send, blocking.
/// @param message Hex coded message sent by CMSGHEX
/// @return LORA_SENT_OK on success, LORA_SENT_ERR on failure
int loraSend(char* message) {

	clearSendStatus();
	// DEBUG - disableParse=1;
	snprintf((char*)LoraBuffer, sizeof(LoraBuffer), "AT+CMSGHEX=\"%s\"\r\n", message);
	command((const char*)LoraBuffer);

	uint32_t tick = HAL_GetTick();
	while ( HAL_GetTick() - tick < LORA_SEND_TMO ) {
		if ( loraStatus & (LORA_STATUS_SENT_DONE | LORA_STATUS_SENT_ERROR) )
			break;
		watchdogRefresh();
	}
	// DEBUG - disableParse=0;

	// CMSGHEX is a confirmed uplink, without the ACK the frame never reached the network
	if ( (loraStatus & LORA_STATUS_ACK_RECEIVED) == 0 ||
		 (loraStatus & LORA_STATUS_SENT_ERROR) != 0 )
	{
		return LORA_SENT_ERR;
	}
	return LORA_SENT_OK;
}

/// @brief Sends modem to lowpower
void loraLowpower() {
	command("AT+LOWPOWER\n");
}

/// @brief Wakes up modem
void loraWakeup() {
	commandNoWait("A");
	HAL_Delay(100);
	command("AT+ID\n");
}

void loraHWReset()
{
	watchdogRefresh();
	HAL_GPIO_WritePin(E5RST_PORT, E5RST_PIN, GPIO_PIN_SET);
	HAL_Delay(1000);
	watchdogRefresh();
	HAL_GPIO_WritePin(E5RST_PORT, E5RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(1000);
	watchdogRefresh();
	HAL_GPIO_WritePin(E5RST_PORT, E5RST_PIN, GPIO_PIN_SET);
	HAL_Delay(1000);
	watchdogRefresh();
}

void loraInit() {

	for ( int attempt = 0; attempt < LORA_INIT_RETRIES; attempt++ )
	{
		loraHWReset();
		if ( loraSetup() == LORA_OK )
			break;
		debug("[ERROR] LoRa join failed, attempt %d\r\n", attempt + 1);
	}
	// always cleared, otherwise tasksLoop would re-enter loraInit forever
	clearLoraCiritcalError();
}
