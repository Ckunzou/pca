/*
 * bootloader.h
 *
 *  Created on: Mar 23, 2018
 *      Author: RRiggott
 */

#ifndef BOOTLOADER_COMMON_H_
#define BOOTLOADER_COMMON_H_

#include <stdint.h>
#include <string.h>

#include "bootloader_defines.h"
#include "bootloader.h"

#include "stm32f4xx_can.h"
#include "stm32f4xx_flash.h"

// SJW [SYNC], BRP [PROP], BS1, BS2
extern uint8_t CAN_BAUD_RATE_TIMING_MAP[8][4] = {
	{ CAN_SJW_1tq, 0, 0, 0 }, // Custom
	{ CAN_SJW_1tq, 3, CAN_BS1_11tq, CAN_BS2_2tq }, // 1000
	{ CAN_SJW_1tq, 6, CAN_BS1_12tq, CAN_BS2_1tq }, // 500
	{ CAN_SJW_1tq, 12, CAN_BS1_12tq, CAN_BS2_1tq }, // 250
	{ CAN_SJW_1tq, 21, CAN_BS1_14tq, CAN_BS2_1tq }, // 125
	{ CAN_SJW_1tq, 21, CAN_BS1_16tq, CAN_BS2_3tq }, // 100
	{ CAN_SJW_1tq, 42, CAN_BS1_16tq, CAN_BS2_3tq }, // 50
	{ CAN_SJW_1tq, 210, CAN_BS1_16tq, CAN_BS2_3tq }, // 10
};

extern uint32_t RESET_FLAG_REGISTER = 0;
extern char * RESET_FLAG_NAMES[5] = { "UNKNOWN", "POWER", "HARDWARE", "WATCHDOG", "SOFTWARE" };
extern uint8_t RESET_FLAG = RESET_TYPE_UNKNOWN;
extern char * RESET_FLAG_NAME = "UNKNOWN";

extern RtcUserData_TypeDef RtcUserData = { RTC_USER_DATA_MAGIC, RESET_TYPE_UNKNOWN, BOOT_FLAG_APPLICATION, CAN_BAUD_RATE_DEFAULT, 0, 0};
extern FlashUserData_TypeDef FlashUserData = { FLASH_USER_DATA_MAGIC, 0, 0, 0, 0, 0, {0} };
extern FlashApplicationData_TypeDef FlashApplicationData = { FLASH_APPLICATION_DATA_MAGIC, 0, 0, {0} };

extern const uint8_t DEFAULT_FRAME_DATA[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

extern void FLASH_ReadUserData(void);
extern FLASH_Status FLASH_WriteUserData(void);

extern void FLASH_ReadApplicationData(void);

extern FLASH_Status FLASH_EraseSectors(uint16_t sector, uint16_t end_sector, uint8_t VoltageRange);
extern uint16_t FLASH_GetSector(uint32_t address);

extern void RTC_ReadBackupRegisters(void);
extern void RTC_WriteBackupRegisters(void);

extern void RCC_ReadStatusRegister(void);

#endif /* BOOTLOADER_COMMON_H_ */
