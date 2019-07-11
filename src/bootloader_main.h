/*
 * main.h
 *
 *  Created on: Mar 7, 2018
 *      Author: RRiggott
 */

#ifndef BOOTLOADER_MAIN_H_
#define BOOTLOADER_MAIN_H_

#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include<stdio.h>

#include <swplatform.h>

#include "bootloader/bootloader.h"

#include "stm32_chip_config.h"
#include "system_stm32f4xx.h"

#include "stm32f4xx.h"
#include "stm32f4xx_pwr.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_flash.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_can.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_crc.h"

#define BUILD_VERSION					0x1

// commands
#define BOOTLOADER_NACK					0x00
#define BOOTLOADER_VERSION				0x01 // N/A <=> ack/nack, version
#define BOOTLOADER_SPEED				0x02 // 1 byte speed <=> ack/nack, ack/nack?
#define BOOTLOADER_ERASE				0x03 // 1 byte flash region <=> ack/nack, ack/nack
#define BOOTLOADER_READ					0x04 // 1 byte flash region, 4 byte length <=> ack/nack, X bytes, ack/nack
#define BOOTLOADER_READ_SEGMENT			0x05 // N/A <=> 8 bytes data
#define BOOTLOADER_WRITE				0x06 // 1 byte flash region, 4 byte length <=> ack/nack, X ack/nacks, ack/nack
#define BOOTLOADER_WRITE_SEGMENT		0x07 // N/A <=> ack/nack
#define BOOTLOADER_VERIFY				0x08 // <=> ack/nack, ack/nack
#define BOOTLOADER_EXECUTE				0x09 // <=> ack/nack
#define BOOTLOADER_SECURE				0x0A // 1 byte flash region, 1 byte secure type, 1 byte secure access type
#define BOOTLOADER_READ_KEY				0x0B // 1 byte key <=> ack/nack, 1 byte key, X byte value
#define BOOTLOADER_WRITE_KEY			0x0C // 1 byte key, X byte value <=> ack/nack
#define BOOTLOADER_SAVE_KEYS			0x0D // <=> ack
#define BOOTLOADER_RESET				0x0E // 1 byte reset type <=> ack/nack
#define BOOTLOADER_ACK					0x0F // bootloader: ack <=> ack, firmware: ack <=> nack

// amount of memory to buffer before writing to flash
#define BOOTLOADER_SEGMENT_SIZE			1024

#define FLASH_REGION_BOOTLOADER			0x01
#define FLASH_REGION_USER_DATA			0x02
#define FLASH_REGION_APPLICATION		0x03

#define SECURE_TYPE_PROTECT				0x01
#define SECURE_TYPE_UNPROTECT			0x02

#define SECURE_ACCESS_TYPE_READ			0x01
#define SECURE_ACCESS_TYPE_WRITE		0x02

#define RESET_TYPE_PARALLAX_BOOTLOADER	0x01
#define RESET_TYPE_STM32_BOOTLOADER		0x02

#define KEY_BOARD						0x01
#define KEY_NODE						0x02
#define KEY_BOARD_REVISION				0x03
#define KEY_BOARD_PART_NUMBER			0x04
#define KEY_BOARD_SERIAL_NUMBER			0x05
#define KEY_BOARD_MANUFACTURE_DATE		0x06

uint8_t CAN_COMMAND_TYPE;
uint8_t CAN_COMMAND;
uint8_t CAN_DESTINATION;
uint8_t CAN_DESTINATION_BOARD;
uint8_t CAN_DESTINATION_NODE;
uint8_t CAN_SOURCE;
uint8_t CAN_SOURCE_BOARD;
uint8_t CAN_SOURCE_NODE;
uint8_t CAN_BROADCAST_FLAG;
uint8_t CAN_PRIORITY;

uint8_t BOARD_ID = 0;
uint8_t NODE_ID = 0;

uint16_t usart_rx;
uint8_t usart_tx;
CanRxMsg can_rx;
CanTxMsg can_tx;

uint8_t flash_buffer[BOOTLOADER_SEGMENT_SIZE];
uint16_t flash_buffer_index = 0;



int main(void);

uint8_t usart_initialize(enum UsartBaudRate baud);
bool usart_receive(uint16_t * data, bool wait);
uint32_t usart_receive_32(void);
void usart_send_64(uint64_t data);
void usart_send_32(uint32_t data);
void usart_send_16(uint16_t data);
void usart_send(uint8_t data);
void usart_bootloader(void);
void usart_ack(uint8_t command);
void usart_nack(uint8_t command);
void delay(int ticks);

uint8_t can_initialize(enum CanBaudRate baud);
bool can_receive(CanRxMsg * msg, bool wait);
void can_send(CanTxMsg * msg, uint8_t command, uint8_t dlc);
void can_bootloader(void);
void can_ack(uint8_t command);
void can_nack(uint8_t command);

FLASH_Status erase(uint8_t type);
bool verify_application(void);
void execute_application(void);

#endif /* BOOTLOADER_MAIN_H_ */
