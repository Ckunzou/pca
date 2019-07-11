/*
 * bootloader_common.c
 *
 *  Created on: Apr 4, 2018
 *      Author: RRiggott
 */

#include "bootloader_common.h"

extern void FLASH_ReadUserData(void) {
	uint8_t data[sizeof(FlashUserData)];
	for (uint32_t i = 0; i < sizeof(FlashUserData); i++) {
		data[i] = *((uint8_t *)(USER_DATA_ADDRESS + i));
	}
	memcpy(&FlashUserData, &data, sizeof(FlashUserData));
}

extern FLASH_Status FLASH_WriteUserData(void) {
	FLASH_Status status = FLASH_COMPLETE;

	FLASH_Unlock();
	if ((status = FLASH_EraseSector(USER_DATA_FLASH_SECTOR, VoltageRange_3)) != FLASH_COMPLETE) {
		FLASH_Lock();
		return status;
	}
	FLASH_Lock();

	uint32_t address = USER_DATA_ADDRESS;
	uint32_t length = sizeof(FlashUserData);
	uint32_t index = 0;
	uint8_t size = 0;

	uint8_t data[sizeof(FlashUserData)];
	memcpy(&data, &FlashUserData, sizeof(FlashUserData));

	FLASH_Unlock();
	while (length != 0) {
		if (length >= sizeof(uint32_t)) size = sizeof(uint32_t);
		else if (length >= sizeof(uint16_t)) size = sizeof(uint16_t);
		else if (length >= sizeof(uint8_t)) size = sizeof(uint8_t);

		if (size == sizeof(uint32_t)) status = FLASH_ProgramWord(address, *((uint32_t *)(&data[index])));
		else if (size == sizeof(uint16_t)) status = FLASH_ProgramHalfWord(address, *((uint16_t *)(&data[index])));
		else if (size == sizeof(uint8_t)) status = FLASH_ProgramByte(address, *((uint8_t *)(&data[index])));

		if (status != FLASH_COMPLETE) break;

		address += size;
		length -= size;
		index += size;
	}
	FLASH_Lock();

	return status;
}

extern void FLASH_ReadApplicationData(void) {
	uint8_t data[sizeof(FlashApplicationData)];
	for (uint32_t i = 0; i < sizeof(FlashApplicationData); i++) {
		data[i] = *((uint8_t *)(APPLICATION_ADDRESS + i));
	}
	memcpy(&FlashApplicationData, &data, sizeof(FlashApplicationData));
}

extern FLASH_Status FLASH_EraseSectors(uint16_t sector, uint16_t end_sector, uint8_t VoltageRange) {
	assert_param(IS_FLASH_SECTOR(sector));
	assert_param(IS_FLASH_SECTOR(end_sector));

	FLASH_Status status = FLASH_COMPLETE;

	FLASH_Unlock();
	while (sector <= end_sector) {
		status = FLASH_EraseSector(sector, VoltageRange);
		if (status != FLASH_COMPLETE) return status;
		sector += 0x8;
	}
	FLASH_Lock();

	return status;
}

extern uint16_t FLASH_GetSector(uint32_t address) {
	assert_param(IS_FLASH_ADDRESS(address));

	if (address >= FLASH_SECTOR_0_ADDRESS && address < FLASH_SECTOR_1_ADDRESS) {
		return FLASH_Sector_0;
	} else if (address >= FLASH_SECTOR_1_ADDRESS && address < FLASH_SECTOR_2_ADDRESS) {
		return FLASH_Sector_1;
	} else if (address >= FLASH_SECTOR_2_ADDRESS && address < FLASH_SECTOR_3_ADDRESS) {
		return FLASH_Sector_2;
	} else if (address >= FLASH_SECTOR_3_ADDRESS && address < FLASH_SECTOR_4_ADDRESS) {
		return FLASH_Sector_3;
	} else if (address >= FLASH_SECTOR_4_ADDRESS && address < FLASH_SECTOR_5_ADDRESS) {
		return FLASH_Sector_4;
	} else if (address >= FLASH_SECTOR_5_ADDRESS && address < FLASH_SECTOR_6_ADDRESS) {
		return FLASH_Sector_5;
	} else if (address >= FLASH_SECTOR_6_ADDRESS && address < FLASH_SECTOR_7_ADDRESS) {
		return FLASH_Sector_6;
	} else if (address >= FLASH_SECTOR_7_ADDRESS && address < FLASH_SECTOR_8_ADDRESS) {
		return FLASH_Sector_7;
	} else if (address >= FLASH_SECTOR_8_ADDRESS && address < FLASH_SECTOR_9_ADDRESS) {
		return FLASH_Sector_8;
	} else if (address >= FLASH_SECTOR_9_ADDRESS && address < FLASH_SECTOR_10_ADDRESS) {
		return FLASH_Sector_9;
	} else if (address >= FLASH_SECTOR_10_ADDRESS && address < FLASH_SECTOR_11_ADDRESS) {
		return FLASH_Sector_10;
	} else { //if (address >= FLASH_SECTOR_11_ADDRESS && address < FLASH_REGION_END) {
		return FLASH_Sector_11;
	}
}

extern void RTC_ReadBackupRegisters(void) {
	// read in words
	uint32_t size = sizeof(RtcUserData) / 4;
	uint32_t data[size];
	for (uint8_t i = 0; i < size; i++) {
		data[i] = RTC_ReadBackupRegister(i);
	}
	memcpy(&RtcUserData, &data, sizeof(RtcUserData));
}

extern void RTC_WriteBackupRegisters(void) {
	// write in words
	uint32_t size = sizeof(RtcUserData) / 4;
	uint32_t data[size];
	memcpy(&data, &RtcUserData, sizeof(RtcUserData));
	for (uint8_t i = 0; i < size; i++) {
		RTC_WriteBackupRegister(i, data[i]);
	}
}

extern void RCC_ReadStatusRegister(void) {
	RESET_FLAG_REGISTER = RCC->CSR;
	if (READ_BIT(RESET_FLAG_REGISTER, RCC_CSR_BORRSTF) || READ_BIT(RESET_FLAG_REGISTER, RCC_CSR_PORRSTF) || READ_BIT(RESET_FLAG_REGISTER, RCC_CSR_LPWRRSTF)) {
		RESET_FLAG = RESET_TYPE_POWER;
	}
	else if (READ_BIT(RESET_FLAG_REGISTER, RCC_CSR_SFTRSTF)) {
		RESET_FLAG = RESET_TYPE_SOFTWARE;
	}
	else if (READ_BIT(RESET_FLAG_REGISTER, RCC_CSR_WDGRSTF) || READ_BIT(RESET_FLAG_REGISTER, RCC_CSR_WWDGRSTF)) {
		RESET_FLAG = RESET_TYPE_WATCHDOG;
	}
	else if (READ_BIT(RESET_FLAG_REGISTER, RCC_CSR_PADRSTF)) {
		RESET_FLAG = RESET_TYPE_HARDWARE;
	}
	SET_BIT(RCC->CSR, RCC_CSR_RMVF);
}
