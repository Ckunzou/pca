/*
 * bootloader.h
 *
 *  Created on: Apr 4, 2018
 *      Author: RRiggott
 */

#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include "bootloader_defines.h"

#include "stm32f4xx_flash.h"

#define USART_BAUD_RATE_DEFAULT			USART_115200_bps
#define CAN_BAUD_RATE_DEFAULT			CAN_500_kbps

extern uint8_t CAN_BAUD_RATE_TIMING_MAP[8][4];

// note: to achieve maximum usart speeds, the microcontroller needs to be configured accordingly
enum UsartBaudRate { USART_Custom = 0, USART_921600_bps, USART_460800_bps, USART_230400_bps, USART_115200_bps };
enum CanBaudRate { CAN_Custom = 0, CAN_1000_kbps, CAN_500_kbps, CAN_250_kbps, CAN_125_kbps };

enum RESET_FLAGS { RESET_TYPE_UNKNOWN = 0, RESET_TYPE_POWER = 1, RESET_TYPE_HARDWARE = 2, RESET_TYPE_WATCHDOG = 3, RESET_TYPE_SOFTWARE = 4 };

extern RtcUserData_TypeDef RtcUserData;
extern FlashUserData_TypeDef FlashUserData;
extern FlashApplicationData_TypeDef FlashApplicationData;

extern uint32_t RESET_FLAG_REGISTER;
extern char * RESET_FLAG_NAMES[5];
extern uint8_t RESET_FLAG;
extern char * RESET_FLAG_NAME;

extern void FLASH_ReadUserData(void);
extern FLASH_Status FLASH_WriteUserData(void);

extern void FLASH_ReadApplicationData(void);

extern FLASH_Status FLASH_EraseSectors(uint16_t sector, uint16_t end_sector, uint8_t VoltageRange);
extern uint16_t FLASH_GetSector(uint32_t address);

extern void RTC_ReadBackupRegisters(void);
extern void RTC_WriteBackupRegisters(void);

extern void RCC_ReadStatusRegister(void);

#endif /* BOOTLOADER_H_ */
