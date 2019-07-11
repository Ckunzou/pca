/*
 * bootloader_defines.h
 *
 *  Created on: Apr 4, 2018
 *      Author: RRiggott
 */

#ifndef BOOTLOADER_DEFINES_H_
#define BOOTLOADER_DEFINES_H_

#include <ctype.h>

#include "stm32f4xx_flash.h"

// flash layout
#define STM32_BOOTLOADER_ADDRESS			0x1FFF0000

#define BOOTLOADER_ADDRESS					0x08000000
#define BOOTLOADER_SIZE						(16 * 1024)
#define BOOTLOADER_FLASH_SECTOR				FLASH_Sector_0

#define USER_DATA_ADDRESS					0x08004000
#define USER_DATA_SIZE						(16 * 1024)
#define USER_DATA_FLASH_SECTOR				FLASH_Sector_1

#define APPLICATION_ADDRESS					0x08008000
#define APPLICATION_SIZE					(480 * 1024)
#define APPLICATION_FLASH_SECTOR			FLASH_Sector_2
#define APPLICATION_HEADER_SIZE				(1 * 1024)
#define APPLICATION_ENTRY_POINT_ADDRESS		(APPLICATION_ADDRESS + APPLICATION_HEADER_SIZE)

// note: vector table must be aligned within flash based on the number of interrupt handlers (http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0553a/BABIFJFG.html)
// alignment is by 0x200 I believe
// SCB->VTOR = APPLICATION_ENTRY_POINT_ADDRESS
#define APPLICATION_VECTOR_TABLE_OFFSET		APPLICATION_HEADER_SIZE

#define FLASH_REGION_16K			(1024 * 16)
#define FLASH_REGION_64K			(1024 * 64)
#define FLASH_REGION_128K			(1024 * 128)

#define FLASH_SECTOR_0_ADDRESS		0x08000000 // 16 Kbytes
#define FLASH_SECTOR_1_ADDRESS		0x08004000 // 16 Kbytes
#define FLASH_SECTOR_2_ADDRESS		0x08008000 // 16 Kbytes
#define FLASH_SECTOR_3_ADDRESS		0x0800C000 // 16 Kbytes
#define FLASH_SECTOR_4_ADDRESS		0x08010000 // 64 Kbytes
#define FLASH_SECTOR_5_ADDRESS		0x08020000 // 128 Kbytes
#define FLASH_SECTOR_6_ADDRESS		0x08040000 // 128 Kbytes
#define FLASH_SECTOR_7_ADDRESS		0x08060000 // 128 Kbytes
// these flash sectors are only available on devices with 1 MB of memory
#define FLASH_SECTOR_8_ADDRESS		0x08080000 // 128 Kbytes
#define FLASH_SECTOR_9_ADDRESS		0x080A0000 // 128 Kbytes
#define FLASH_SECTOR_10_ADDRESS		0x080C0000 // 128 Kbytes
#define FLASH_SECTOR_11_ADDRESS		0x080E0000 // 128 Kbytes

#define FLASH_REGION_START			FLASH_SECTOR_0_ADDRESS
#define FLASH_REGION_END			(FLASH_SECTOR_8_ADDRESS - 1)

#define SYSTEM_MEMORY_ADDRESS		0x1FFF0000 // 30 Kbytes
// OTP bytes - en.DM00031020-RM0090-STM32F4xx_EVAL, pg. 97
// ref lib: https://stm32f4-discovery.net/2015/01/library-49-one-time-programmable-otp-bytes-stm32f4xx/
#define OTP_MEMORY_ADDRESS			0x1FFF7800 // 528 bytes (512 OTP bytes + 16 OTP lock bytes)
#define OPTION_BYTES_MEMORY_ADDRESS	0x1FFFC000 // 16 bytes

// CAN Extended ID Format & Definitions
#define CAN_COMMAND_TYPE_MASK		0x1
#define CAN_COMMAND_MASK			0xFF
#define CAN_ADDRESS_MASK			0xF
#define CAN_ADDRESS_NODE_MASK		0xF
#define CAN_ADDRESS_BOARD_MASK		0xF
#define CAN_BROADCAST_FLAG_MASK		0x3
#define CAN_PRIORITY_MASK			0x3

#define CAN_PRIORITY_VERY_HIGH 		0 // alarms/faults
#define CAN_PRIORITY_HIGH			1 // control commands
#define CAN_PRIORITY_MEDIUM			2 // default priority
#define CAN_PRIORITY_LOW			3 // data collection/monitoring

#define CAN_PRIORITY_DEFAULT		CAN_PRIORITY_MEDIUM

#define CAN_BROADCAST_FLAG_NONE		0x0
#define CAN_BROADCAST_FLAG_BOARD	0x1
#define CAN_BROADCAST_FLAG_NODE		0x2

#define CAN_BROADCAST_BOARD_ID		0x0
#define CAN_BROADCAST_NODE_ID		0x0

#define CAN_COMMAND_TYPE_REQUEST	0x0
#define CAN_COMMAND_TYPE_RESPONSE	0x1

#define RTC_USER_DATA_MAGIC				0xDEADBEEF
#define FLASH_USER_DATA_MAGIC       	0xCAFEBABE
#define FLASH_APPLICATION_DATA_MAGIC	0xDEADBABE

#define BOOT_FLAG_STM32_BOOTLOADER		0xCAFE
#define BOOT_FLAG_PARALLAX_BOOTLOADER	0xBABE
#define BOOT_FLAG_APPLICATION			0xBEEF

#define LED_POWER_PORT				GPIOC
#define LED_POWER_RED				GPIO_Pin_13
#define LED_POWER_GREEN				GPIO_Pin_15
#define LED_POWER_PINS				(LED_POWER_RED | LED_POWER_GREEN)

#define LED_STATUS_PORT				GPIOC
#define LED_STATUS_RED				GPIO_Pin_0
#define LED_STATUS_GREEN			GPIO_Pin_12
#define LED_STATUS_PINS				(LED_STATUS_RED | LED_STATUS_GREEN)

// structure needs to be word (32 bit) aligned as backup registers are words
typedef struct {
 uint32_t magic;
 uint8_t reset_flag;
 uint16_t boot_flag;
 uint8_t can_baud_rate;
 uint32_t can_settings;
 uint8_t counter;
 uint64_t system_mode;
} __attribute__((aligned(8))) RtcUserData_TypeDef;

typedef struct {
	uint32_t magic;
	uint8_t board;
	uint8_t node;
	uint8_t revision;
	uint32_t part_number;
	uint32_t serial_number;
	uint8_t manufacture_date[3]; // month, day, year
} __attribute__((aligned(4))) FlashUserData_TypeDef;

typedef struct {
	uint32_t magic;
	uint32_t crc;
	uint32_t length;
	uint8_t version[8];
} __attribute__((aligned(4))) FlashApplicationData_TypeDef;

#endif /* BOOTLOADER_DEFINES_H_ */
