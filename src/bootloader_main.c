#include "bootloader_main.h"

/* Parallax Bootloader Development Notes
 *
 *
 * Releases:
 * v1. In progress...
 *
 * Future Releases:
 * - Add Flash read and write protection (should be easy to implement, maybe make a v1 feature)
 * - Add USART support (maybe make a v1 or v2 feature depending on how ambitious I feel)
 * - Add OTP user data (Use FLASH_ProgramWord/FLASH_ProgramHalfWord/FLASH_ProgramByte (should be easy)
 * - Add a CRC to FLASH_USER_DATA region? (gotta think about this one)
 * - Add proper support for stm32f4xx chips with 1MB flash, dual bank (probably not necessary, should work as is but we would just ignore the extra memory)
 */

typedef void (*jump_function)(void);

// NOTE: This function is called before ANY peripherals or the SWP is initialized
extern void __start(void)
{
	/* Enable PWR peripheral clock */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

	/* Allow access to RTC BKP Domain */
	PWR_BackupAccessCmd(ENABLE);

	RTC_ReadBackupRegisters();

	if (RtcUserData.boot_flag == (uint16_t)BOOT_FLAG_STM32_BOOTLOADER) {
		RtcUserData.boot_flag = BOOT_FLAG_PARALLAX_BOOTLOADER;
		RTC_WriteBackupRegisters();
		__asm(
			"LDR     R0, =0x40023844 ; RCC_APB2ENR (0x40023800 | pg 266)\n\t"
			"LDR     R1, =0x00004000 ; enable SYSCFG clock\n\t"
			"STR     R1, [R0, #0]    ; set register\n\t"
			"LDR     R0, =0x40013800 ; SYSCFG_MEMRMP (0x40013800 | pg 294)\n\t"
			"LDR     R1, =0x00000001 ; remap ROM at zero\n\t"
			"STR     R1, [R0, #0]    ; set register\n\t"
			"LDR     R0, =0x1FFF0000 ; load ROM base\n\t"
			"LDR     SP,[R0, #0]     ; assign main stack pointer\n\t"
			"LDR     R0,[R0, #4]     ; load bootloader address (offset by 4)\n\t"
			"BX      R0"
		);
	}
}

int main(void)
{
	// if we get to this point, we aren't loading the stm32 bootloader

	// before updating our clocks, modify the flash latency as per en.DM00031020-RM0090-STM32F4xx_EVAL, pg. 81
	FLASH_SetLatency(FLASH_Latency_5);

	SystemInit();
	SystemCoreClockUpdate();

	// initialize core bootloader peripherals
	// required peripherals at this point are sys and rcc
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);

	pincfg_gpio_init();
	pincfg_sys_init();
	pincfg_rcc_init();

	GPIO_SetBits(LED_POWER_PORT, LED_POWER_PINS);
	GPIO_SetBits(LED_STATUS_PORT, LED_STATUS_PINS);

	GPIO_ResetBits(LED_POWER_PORT, LED_POWER_RED);
	GPIO_ResetBits(LED_STATUS_PORT, LED_STATUS_RED);

	RCC_ReadStatusRegister();

	RTC_ReadBackupRegisters();

	if (RtcUserData.magic != (uint32_t)RTC_USER_DATA_MAGIC) {
		RtcUserData.magic = RTC_USER_DATA_MAGIC;
		RtcUserData.reset_flag = RESET_TYPE_UNKNOWN;
		RtcUserData.boot_flag = BOOT_FLAG_APPLICATION;
		RtcUserData.can_baud_rate = CAN_BAUD_RATE_DEFAULT;
		RtcUserData.can_settings = 0;
		RtcUserData.counter = 0;
		RtcUserData.system_mode = 0;
	}

	RtcUserData.reset_flag = RESET_FLAG;
	RTC_WriteBackupRegisters();

	// read flash user data sector
	FLASH_ReadUserData();

	if (FlashUserData.magic != (uint32_t)FLASH_USER_DATA_MAGIC) {
		FlashUserData.magic = FLASH_USER_DATA_MAGIC;
		FlashUserData.board = 0;
		FlashUserData.node = 0;
		FlashUserData.revision = 0;
		FlashUserData.part_number = 0;
		FlashUserData.serial_number = 0;
		FlashUserData.manufacture_date[0] = 0;
		FlashUserData.manufacture_date[1] = 0;
		FlashUserData.manufacture_date[2] = 0;
		FLASH_WriteUserData();
		RtcUserData.boot_flag = BOOT_FLAG_PARALLAX_BOOTLOADER;
	}

	BOARD_ID = FlashUserData.board;
	NODE_ID = FlashUserData.node;

	// read application data to attempt to verify app
	FLASH_ReadApplicationData();

	// if we are loading the application, verify it and then execute it
	if (RtcUserData.boot_flag == (uint16_t)BOOT_FLAG_APPLICATION) {
		if (verify_application()) {
			execute_application();
		}
	}

	// application verification failed or bootflag is set for bootloader, initialize all peripherals used by the bootloader
	pincfg_usart3_init();
	pincfg_can2_init();

	// setup default usart and can parameters
	usart_initialize(USART_BAUD_RATE_DEFAULT);

	can_initialize(CAN_BAUD_RATE_DEFAULT);

	can_tx.RTR = CAN_RTR_Data;
	can_tx.IDE = CAN_ID_EXT;

	// detect the interface we are using for the bootloader
	while (1) {
		// TODO v2 probe for usart bootloader flag

		if (usart_receive(&usart_rx, false)) {
			if (usart_rx == BOOTLOADER_ACK) {
				usart_ack(BOOTLOADER_ACK);
				CAN_DeInit(CAN2);
				usart_bootloader();
			}
		}

		// probe for can bootloader flag
		if (can_receive(&can_rx, false)) {
			if (CAN_COMMAND == BOOTLOADER_ACK) {
				can_ack(BOOTLOADER_ACK);
				USART_DeInit(USART3);
				can_bootloader();
			}
		}

	}
}

uint8_t usart_initialize(enum UsartBaudRate baud) {
	// setup usart3
	USART_DeInit(USART3);

	// USART3 clock enable
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	USART_InitTypeDef USART_InitStructure;
	USART_StructInit(&USART_InitStructure);

	/* USART3 configuration */
	/* X baud, window 8bits, one stop bit, no parity, no hw control, rx/tx enabled */
	// TODO v2 implement support for USART baud rates other than 115200
	if (baud == USART_115200_bps) USART_InitStructure.USART_BaudRate = 115200;
	else if (baud == USART_230400_bps) USART_InitStructure.USART_BaudRate = 230400;
	else if (baud == USART_460800_bps) USART_InitStructure.USART_BaudRate = 460800;
	else if (baud == USART_921600_bps) USART_InitStructure.USART_BaudRate = 921600;
	else return 0;

	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl =
			USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);

	USART_Cmd(USART3, ENABLE);

	return 1;
}

void usart_bootloader(void) {
	while (1) {
        uint16_t usart_command;
		usart_receive(&usart_command, true);   // waiting to receive a command

		switch (usart_command) {
		case BOOTLOADER_ACK:
		{
			//return ack
			usart_ack(BOOTLOADER_ACK);
			break;
		}
		case BOOTLOADER_VERSION:
		{
			//return ack/nack and Build Version
			usart_ack(BOOTLOADER_VERSION);       // command and ack
			usart_send(BUILD_VERSION);            // send version
			break;
		}
		case BOOTLOADER_SPEED:
		{
			usart_receive(&usart_rx, true);// receive baud rate and initialize
			if ( usart_rx <= USART_115200_bps && usart_rx >= USART_921600_bps)
			{
				usart_ack(BOOTLOADER_SPEED);
			}
			else
			{
				usart_ack(BOOTLOADER_SPEED);
				break;
			}

			delay(7000);
			usart_initialize(usart_rx);
			break;
		}
		case BOOTLOADER_ERASE:
		{
			usart_receive(&usart_rx, true);
            uint16_t flash_region = usart_rx;
            FLASH_Status status = erase(flash_region & 0xFF);

            if(flash_region == FLASH_REGION_USER_DATA || flash_region == FLASH_REGION_APPLICATION)
            {
            	usart_ack(BOOTLOADER_ERASE);
            }
            else
            {
            	usart_nack(BOOTLOADER_ERASE);
            	break;
            }

            if (status == FLASH_COMPLETE) usart_ack(BOOTLOADER_ERASE);
            else if (status != FLASH_COMPLETE) usart_nack(BOOTLOADER_ERASE);
			break;
		}
		case BOOTLOADER_READ:
		{
		 usart_receive(&usart_rx, true);
         uint16_t flash_region = usart_rx;
         uint32_t length = 0;
         uint32_t address;

         //check and verify
         if (flash_region == FLASH_REGION_BOOTLOADER ||
        	 flash_region == FLASH_REGION_USER_DATA ||
			 flash_region == FLASH_REGION_APPLICATION)
         {
         		if (flash_region == FLASH_REGION_BOOTLOADER)
         		        {
         					address = BOOTLOADER_ADDRESS;
         					length = BOOTLOADER_SIZE;
         					usart_ack(BOOTLOADER_READ);
         				}
         		else if (flash_region == FLASH_REGION_USER_DATA)
         		        {
         					address = USER_DATA_ADDRESS;
         					length = sizeof(FlashUserData);
         					usart_ack(BOOTLOADER_READ);
         				}
         		else if (flash_region == FLASH_REGION_APPLICATION)
         		        {
         					address = APPLICATION_ADDRESS;
         					uint32_t crc = FlashApplicationData.crc;
         					length = FlashApplicationData.length;
         					if (crc == 0 || crc == 0xFFFFFFFF || length == 0 || length == 0xFFFFFFFF)
         					{
         					length = APPLICATION_SIZE;
         				    }
         		            usart_ack(BOOTLOADER_READ);
         				}
         		else
         				{
         					usart_nack(BOOTLOADER_READ);
         					break;
         				}
         }
         else {
         	usart_nack(BOOTLOADER_READ);
         	break;
         	  }

         // send total length
         usart_send_32(length);
         	while (length > 0) {
         		memcpy(&usart_tx, ((uint8_t *)(address)), 1);
         		address++;
         		usart_send(usart_tx);
         		length--;
         	}
                usart_ack(BOOTLOADER_READ); // ack command after finished
			break;
		}
		case BOOTLOADER_WRITE:
		{
			usart_receive(&usart_rx, true);
			uint16_t flash_region = usart_rx;
			uint16_t current_command;

			uint32_t length = usart_receive_32();
		    uint32_t address;

		    if (flash_region == FLASH_REGION_USER_DATA || flash_region == FLASH_REGION_APPLICATION)
		    {
		    	if (flash_region == FLASH_REGION_USER_DATA && length <= USER_DATA_SIZE) {
		    		address = USER_DATA_ADDRESS;
		    		usart_ack(BOOTLOADER_WRITE);
		    	} else if (flash_region == FLASH_REGION_APPLICATION && length <= APPLICATION_SIZE) {
		    		address = APPLICATION_ADDRESS;
		    		usart_ack(BOOTLOADER_WRITE);
		    	} else {
		    		usart_nack(BOOTLOADER_WRITE);
		    		break;
		    			}
		    }
		    else {
		    		usart_nack(BOOTLOADER_WRITE);
		    		break;
		    	  }

		    // receive , buffer and write data part
		    int frames;
		    uint32_t index;
		    uint8_t size = 0;
		    FLASH_Status status = FLASH_COMPLETE;

		    while (length > 0)
		    {
		    			if (length >= BOOTLOADER_SEGMENT_SIZE) frames = 1024;
		    			else frames = (uint8_t)length;

		    			flash_buffer_index = 0;
                        memset(&flash_buffer, 0xFF, BOOTLOADER_SEGMENT_SIZE);

		    			// receive and buffer data
		    			while (frames > 0)
		    			    {
		    				usart_receive(&current_command, true); // receive the write_segment command
		    				usart_receive(&usart_rx, true);
		    				if (current_command == BOOTLOADER_NACK) break;
		    				if (current_command != BOOTLOADER_WRITE_SEGMENT) continue;
		    				memcpy(&flash_buffer[flash_buffer_index], &usart_rx, 1);
		    				flash_buffer_index += 1;
		    				frames--;
		    				}

		    				// write data
		    				index = 0;
		    				FLASH_Unlock();
		    			while (index < flash_buffer_index)
		    			 {
		    				if (flash_buffer_index - index >= sizeof(uint32_t)) size = sizeof(uint32_t);
		    				else if (flash_buffer_index - index >= sizeof(uint16_t)) size = sizeof(uint16_t);
		    				else if (flash_buffer_index - index >= sizeof(uint8_t)) size = sizeof(uint8_t);

		    				if (size == sizeof(uint32_t)) status = FLASH_ProgramWord(address, *((uint32_t *)(&flash_buffer[index])));
		    				else if (size == sizeof(uint16_t)) status = FLASH_ProgramHalfWord(address, *((uint16_t *)(&flash_buffer[index])));
		    				else if (size == sizeof(uint8_t)) status = FLASH_ProgramByte(address, *((uint8_t *)(&flash_buffer[index])));

		    				if (status != FLASH_COMPLETE)
		    				    {
		    					break;
		    					}
		    				index += size;
		    				address += size;
		                  }
		    				FLASH_Lock();

		    				delay(10000);
		    				// ack/nack 1k blocks
		    				if (status == FLASH_COMPLETE) {
		    					usart_ack(BOOTLOADER_WRITE_SEGMENT);
		    					delay(7000);
		    				} else if (status != FLASH_COMPLETE) {
		    					usart_nack(BOOTLOADER_WRITE_SEGMENT);
		    					break;
		    				}
		    				length -= index;
		    }

		     // finished writing to flash, checks and send acks
		    if (usart_command == BOOTLOADER_NACK) break;

            delay(500000);
		    // ack/nack the entire operation
		    if (status == FLASH_COMPLETE)
		    {
		    	usart_ack(BOOTLOADER_WRITE);
		    	delay(10000);
		    }
		    else if (status != FLASH_COMPLETE)
		    {
		    	usart_nack(BOOTLOADER_WRITE);
		    	break;
		     }

		    delay(500000);
		    // verify the application
		    if (flash_region == FLASH_REGION_APPLICATION)
		    {
		    	FLASH_ReadApplicationData();
                if (verify_application()) {
		    	  usart_ack(BOOTLOADER_WRITE);
		    	} else {
		   		  usart_nack(BOOTLOADER_WRITE);
		    		   }
		    }
           break;
		}
		case BOOTLOADER_VERIFY:
		{
			// return ack/nack for command parameters, return ack/nack for application verification
			usart_ack(BOOTLOADER_VERIFY);
			// verify the app in flash
			if (verify_application())
			{
				usart_ack(BOOTLOADER_VERIFY);
			} else
			{
				usart_nack(BOOTLOADER_VERIFY);
			}
			break;
		}

		case BOOTLOADER_EXECUTE:
		{
			usart_ack(BOOTLOADER_EXECUTE);
			delay(7000);

			RtcUserData.boot_flag = BOOT_FLAG_APPLICATION;
			RTC_WriteBackupRegisters();
			NVIC_SystemReset();
			break;
		}
		case BOOTLOADER_SECURE:
		{
            // TODO
			usart_ack(BOOTLOADER_SECURE);
		    break;
		}
		case BOOTLOADER_READ_KEY:
		{
			usart_receive(&usart_rx, true);
			uint8_t key = usart_rx &0xFF;
			uint32_t data;

			if (key >= 0x01 && key <= 0xFF) {
				if (key == KEY_BOARD) {
					memcpy(&usart_tx, &FlashUserData.board, sizeof(FlashUserData.board));
				} else if (key == KEY_NODE) {
					memcpy(&usart_tx, &FlashUserData.node, sizeof(FlashUserData.node));
				} else if (key == KEY_BOARD_REVISION) {
					memcpy(&usart_tx, &FlashUserData.revision, sizeof(FlashUserData.revision));
				} else if (key == KEY_BOARD_PART_NUMBER) {
					memcpy(&data, &FlashUserData.part_number, sizeof(FlashUserData.part_number));
				} else if (key == KEY_BOARD_SERIAL_NUMBER) {
					memcpy(&data, &FlashUserData.serial_number, sizeof(FlashUserData.serial_number));
				} else if (key == KEY_BOARD_MANUFACTURE_DATE) {
					memcpy(&data, &FlashUserData.manufacture_date, sizeof(FlashUserData.manufacture_date));
				} else {
					usart_nack(BOOTLOADER_READ_KEY);
					break;
				}
			} else {
				usart_nack(BOOTLOADER_READ_KEY);
			}
			usart_ack(BOOTLOADER_READ_KEY);   // sending total of 8 bytes response...
			usart_send(key);
			usart_send(usart_tx);        // for key 1,2,3, 1 byte data
			usart_send_32(data);         // for key 4,5,6, 4 byte data
			break;
		}
		case BOOTLOADER_WRITE_KEY:
		{
			usart_receive(&usart_rx, true); // receive key
			uint8_t key = usart_rx & 0xFF;

			if (key >= 0x01 && key <= 0xFF) {
				// write value received to memory base on keys...
					if (key == KEY_BOARD) {
						usart_receive(&usart_rx, true);
						FlashUserData.board = usart_rx & 0xFF;
					} else if (key == KEY_NODE) {
						usart_receive(&usart_rx, true);
						FlashUserData.node = usart_rx & 0xFF;
					} else if (key == KEY_BOARD_REVISION) {
						usart_receive(&usart_rx, true);
						FlashUserData.revision = usart_rx & 0xFF;
					} else if (key == KEY_BOARD_PART_NUMBER) {
						FlashUserData.part_number = usart_receive_32();
					} else if (key == KEY_BOARD_SERIAL_NUMBER) {
						FlashUserData.serial_number = usart_receive_32();
					} else if (key == KEY_BOARD_MANUFACTURE_DATE) {
						usart_receive(&usart_rx, true);
						FlashUserData.manufacture_date[0] = usart_rx & 0xFF;
						usart_receive(&usart_rx, true);
						FlashUserData.manufacture_date[1] = usart_rx & 0xFF;
						usart_receive(&usart_rx, true);
						FlashUserData.manufacture_date[2] = usart_rx & 0xFF;
					} else {
						usart_nack(BOOTLOADER_WRITE_KEY);
						break;
					}
				} else {
					usart_nack(BOOTLOADER_WRITE_KEY);
					break;
				}
			usart_ack(BOOTLOADER_WRITE_KEY);
			break;
		}
		case BOOTLOADER_SAVE_KEYS:
		{
			FLASH_Status status = FLASH_WriteUserData();
			usart_ack(BOOTLOADER_SAVE_KEYS);

			if(status == FLASH_COMPLETE)
				usart_ack(BOOTLOADER_SAVE_KEYS);
			else if(status != FLASH_COMPLETE)
				usart_nack(BOOTLOADER_SAVE_KEYS);

			break;
		}
		case BOOTLOADER_RESET:
		{
			usart_receive(&usart_rx, true);
			uint16_t type = usart_rx;

			if(type == RESET_TYPE_PARALLAX_BOOTLOADER)
			{
				RtcUserData.boot_flag = BOOT_FLAG_PARALLAX_BOOTLOADER;
			}
			else if(type == RESET_TYPE_STM32_BOOTLOADER)
			{
				RtcUserData.boot_flag = BOOT_FLAG_STM32_BOOTLOADER;
			}
			else
			{
				usart_nack(BOOTLOADER_RESET);
				continue;
			}
			usart_ack(BOOTLOADER_RESET);
			delay(7000);

			RTC_WriteBackupRegisters();
			NVIC_SystemReset();
			break;
		}
		default:
		{
			usart_nack(usart_command & 0xFF);
			break;
		}
		}
	}
}

bool usart_receive(uint16_t * data, bool wait) {
	do {
		if (USART_GetFlagStatus(USART3, USART_FLAG_RXNE) == SET) {
			*data = USART_ReceiveData(USART3);
			USART_ClearFlag(USART3, USART_FLAG_RXNE);
			return true;
		}
	} while (wait);
	return false;
}
uint32_t usart_receive_32(void){ // receives a 4 byte size value
	uint32_t data;
	usart_receive(&usart_rx, true);
	data = (uint32_t) (usart_rx<<24); // msb
	usart_receive(&usart_rx, true);
	data = data | (uint32_t)(usart_rx<<16);
	usart_receive(&usart_rx, true);
	data = data | (uint32_t)(usart_rx<<8);
	usart_receive(&usart_rx, true);
	data = data | (uint32_t)(usart_rx); // lsb
	return data;
}

void usart_send_64(uint64_t data) {
	usart_send((data >> 56) & 0xFF);
	usart_send((data >> 48) & 0xFF);
	usart_send((data >> 40) & 0xFF);
	usart_send((data >> 32) & 0xFF);
	usart_send((data >> 24) & 0xFF);
	usart_send((data >> 16) & 0xFF);
	usart_send((data >> 8) & 0xFF);
	usart_send((data >> 0) & 0xFF);
}

void usart_send_32(uint32_t data) {
	usart_send((data >> 24) & 0xFF);
	usart_send((data >> 16) & 0xFF);
	usart_send((data >> 8) & 0xFF);
	usart_send((data >> 0) & 0xFF);
}

void usart_send_16(uint16_t data) {
	usart_send((data >> 8) & 0xFF);
	usart_send((data >> 0) & 0xFF);
}

void usart_send(uint8_t data) {
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	USART_SendData(USART3, data);
}

void usart_ack(uint8_t command) {
	usart_send(command);
	usart_send(BOOTLOADER_ACK);
}

void usart_nack(uint8_t command) {
	usart_send(command);
	usart_send(BOOTLOADER_NACK);
}
void delay(int ticks){
	for(int i = 0; i<ticks ; i++);
}

uint8_t can_initialize(enum CanBaudRate baud) {
	// setup can2
	CAN_InitTypeDef CAN_InitStructure;
	CAN_StructInit(&CAN_InitStructure);

	CAN_InitStructure.CAN_TTCM = DISABLE;
	CAN_InitStructure.CAN_ABOM = DISABLE;
	CAN_InitStructure.CAN_AWUM = DISABLE;
	CAN_InitStructure.CAN_NART = DISABLE;
	CAN_InitStructure.CAN_RFLM = DISABLE;
	CAN_InitStructure.CAN_TXFP = DISABLE;
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;

	CAN_DeInit(CAN2);

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1 | RCC_APB1Periph_CAN2, ENABLE);

	CAN_InitStructure.CAN_SJW = CAN_BAUD_RATE_TIMING_MAP[baud][0];
	CAN_InitStructure.CAN_Prescaler = CAN_BAUD_RATE_TIMING_MAP[baud][1];
	CAN_InitStructure.CAN_BS1 = CAN_BAUD_RATE_TIMING_MAP[baud][2];
	CAN_InitStructure.CAN_BS2 = CAN_BAUD_RATE_TIMING_MAP[baud][3];

	if (CAN_Init(CAN2, &CAN_InitStructure) != CAN_InitStatus_Success) {
		return 0;
	}

	CAN_FilterInitTypeDef CAN_FilterInitStructure;
	memset(&CAN_FilterInitStructure, 0, sizeof(CAN_FilterInitTypeDef));
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
	CAN_FilterInitStructure.CAN_FilterActivation = DISABLE;
	uint32_t filter_id, filter_mask;
	uint16_t filter_fifo;

	for (uint8_t i = 14; i < 17; i++) {
		CAN_FilterInitStructure.CAN_FilterNumber = i;
		if (i == 14) {
			// board/node id filter
			filter_id = ((CAN_COMMAND_TYPE_REQUEST & CAN_COMMAND_TYPE_MASK) << 0) | ((BOARD_ID & CAN_ADDRESS_BOARD_MASK) << 9) | ((NODE_ID & CAN_ADDRESS_NODE_MASK) << 13) | ((CAN_BROADCAST_FLAG_NONE & CAN_BROADCAST_FLAG_MASK) << 25);
			filter_mask = (CAN_COMMAND_TYPE_MASK << 0) | (CAN_ADDRESS_MASK << 9) | (CAN_ADDRESS_MASK << 13) | (CAN_BROADCAST_FLAG_MASK << 25);
			filter_fifo = CAN_Filter_FIFO0;
			CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
		} else if (i == 15) {
			// board broadcast filter
			filter_id = ((CAN_COMMAND_TYPE_REQUEST & CAN_COMMAND_TYPE_MASK) << 0) | ((BOARD_ID & CAN_ADDRESS_BOARD_MASK) << 9) | ((CAN_BROADCAST_FLAG_BOARD & CAN_BROADCAST_FLAG_MASK) << 25);
			filter_mask = (CAN_COMMAND_TYPE_MASK << 0) | (CAN_ADDRESS_MASK << 9) | (CAN_BROADCAST_FLAG_MASK << 25);
			filter_fifo = CAN_Filter_FIFO0;
			CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
		} else if (i == 16) {
			// global broadcast filter
			filter_id = ((CAN_COMMAND_TYPE_REQUEST & CAN_COMMAND_TYPE_MASK) << 0) | ((CAN_BROADCAST_FLAG_NODE & CAN_BROADCAST_FLAG_MASK) << 25);
			filter_mask = (CAN_COMMAND_TYPE_MASK << 0) | (CAN_BROADCAST_FLAG_MASK << 25);
			filter_fifo = CAN_Filter_FIFO0;
			CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
		} else {
			filter_id = 0;
			filter_mask = 0;
			filter_fifo = CAN_Filter_FIFO0;
			CAN_FilterInitStructure.CAN_FilterActivation = DISABLE;
		}

		// filter ignores 3 lsb's
		filter_id = filter_id << 3;
		filter_mask = filter_mask << 3;

		CAN_FilterInitStructure.CAN_FilterIdHigh = (uint16_t)((filter_id & 0xFFFF0000) >> 16);
		CAN_FilterInitStructure.CAN_FilterIdLow = (uint16_t)((filter_id & 0x0000FFFF) >> 0);
		CAN_FilterInitStructure.CAN_FilterMaskIdHigh = (uint16_t)((filter_mask & 0xFFFF0000) >> 16);
		CAN_FilterInitStructure.CAN_FilterMaskIdLow = (uint16_t)((filter_mask & 0x0000FFFF) >> 0);
		CAN_FilterInitStructure.CAN_FilterFIFOAssignment = filter_fifo;
		CAN_FilterInit(&CAN_FilterInitStructure);
	}

	return 1;
}

void can_bootloader(void) {
	while (1) {
		can_receive(&can_rx, true);

		switch (CAN_COMMAND) {
		case BOOTLOADER_ACK:
		{
			// return ack
			can_ack(BOOTLOADER_ACK);
			break;
		}
		case BOOTLOADER_VERSION:
		{
			// return ack/nack and BUILD_VERSION
			can_tx.Data[0] = BOOTLOADER_ACK;
			can_tx.Data[1] = BUILD_VERSION;
			can_send(&can_tx, BOOTLOADER_VERSION, 2);
			break;
		}
		case BOOTLOADER_SPEED:
		{
			// verify parameter is within range and set, return ack/nack
			uint8_t can_speed = can_rx.Data[0];

			if (can_speed >= CAN_1000_kbps && can_speed <= CAN_125_kbps) {
				can_ack(BOOTLOADER_SPEED);
			} else {
				can_nack(BOOTLOADER_SPEED);
				break;
			}

			can_initialize(can_speed);

			break;
		}
		case BOOTLOADER_ERASE:
		{
			// verify parameter, send ack/nack, handle erase operation, send ack/nack
			uint8_t flash_region = can_rx.Data[0];

			if (flash_region == FLASH_REGION_USER_DATA || flash_region == FLASH_REGION_APPLICATION) {
				can_ack(BOOTLOADER_ERASE);
			} else {
				can_nack(BOOTLOADER_ERASE);
				break;
			}

			FLASH_Status status = erase(flash_region);

			if (status == FLASH_COMPLETE) can_ack(BOOTLOADER_ERASE);
			else if (status != FLASH_COMPLETE) can_nack(BOOTLOADER_ERASE);

			break;
		}
		case BOOTLOADER_READ:
		{
			uint8_t flash_region = can_rx.Data[0];
			uint32_t length = 0;
			uint32_t address;

			// check and verify parameters
			if (flash_region == FLASH_REGION_BOOTLOADER || flash_region == FLASH_REGION_USER_DATA || flash_region == FLASH_REGION_APPLICATION) {
				if (flash_region == FLASH_REGION_BOOTLOADER) {
					address = BOOTLOADER_ADDRESS;
					length = BOOTLOADER_SIZE;
					can_ack(BOOTLOADER_READ);
				} else if (flash_region == FLASH_REGION_USER_DATA) {
					address = USER_DATA_ADDRESS;
					length = sizeof(FlashUserData);
					can_ack(BOOTLOADER_READ);
				} else if (flash_region == FLASH_REGION_APPLICATION) {
					address = APPLICATION_ADDRESS;
					uint32_t crc = FlashApplicationData.crc;
					length = FlashApplicationData.length;
					if (crc == 0 || crc == 0xFFFFFFFF || length == 0 || length == 0xFFFFFFFF) {
						length = APPLICATION_SIZE;
					}
					can_ack(BOOTLOADER_READ);
				} else {
					can_nack(BOOTLOADER_READ);
					break;
				}
			} else {
				can_nack(BOOTLOADER_READ);
				break;
			}

			// vomit out the data over CAN
			uint8_t dlc;
			while (length != 0) {
				if (length > 8) dlc = 8;
				else dlc = (uint8_t)length;
				memcpy(&can_tx.Data, ((uint8_t *)(address)), dlc);
				address += 8;
				can_send(&can_tx, BOOTLOADER_READ_SEGMENT, dlc);
				length -= dlc;
			}

			can_ack(BOOTLOADER_READ);

			break;
		}
		case BOOTLOADER_WRITE:
		{
			uint8_t flash_region = can_rx.Data[0];
			uint32_t length = *((uint32_t *)&can_rx.Data[1]);
			uint32_t address;

			// check and verify parameters
			if (flash_region == FLASH_REGION_USER_DATA || flash_region == FLASH_REGION_APPLICATION) {
				if (flash_region == FLASH_REGION_USER_DATA && length < USER_DATA_SIZE) {
					address = USER_DATA_ADDRESS;
					can_ack(BOOTLOADER_WRITE);
				} else if (flash_region == FLASH_REGION_APPLICATION && length < APPLICATION_SIZE) {
					address = APPLICATION_ADDRESS;
					can_ack(BOOTLOADER_WRITE);
				} else {
					can_nack(BOOTLOADER_WRITE);
					break;
				}
			} else {
				can_nack(BOOTLOADER_WRITE);
				break;
			}

			// receive, buffer, and write data
			uint8_t frames;
			uint32_t index;
			uint8_t size = 0;
			FLASH_Status status = FLASH_COMPLETE;
			while (length > 0) {
				if (length >= BOOTLOADER_SEGMENT_SIZE) frames = 128;
				else frames = (uint8_t)ceil(length/8.0f);

				flash_buffer_index = 0;

				memset(&flash_buffer, 0xFF, BOOTLOADER_SEGMENT_SIZE);

				// receive and buffer data
				while (frames > 0) {
					can_receive(&can_rx, true);
					if (CAN_COMMAND == BOOTLOADER_NACK) break;
					if (CAN_COMMAND != BOOTLOADER_WRITE_SEGMENT) continue;
					memcpy(&flash_buffer[flash_buffer_index], &can_rx.Data, can_rx.DLC);
					flash_buffer_index += can_rx.DLC;
					frames--;
				}

				if (CAN_COMMAND == BOOTLOADER_NACK) break;

				can_ack(BOOTLOADER_WRITE_SEGMENT);

				// write data
				index = 0;
				FLASH_Unlock();
				while (index < flash_buffer_index) {
					if (flash_buffer_index - index >= sizeof(uint32_t)) size = sizeof(uint32_t);
					else if (flash_buffer_index - index >= sizeof(uint16_t)) size = sizeof(uint16_t);
					else if (flash_buffer_index - index >= sizeof(uint8_t)) size = sizeof(uint8_t);

					if (size == sizeof(uint32_t)) status = FLASH_ProgramWord(address, *((uint32_t *)(&flash_buffer[index])));
					else if (size == sizeof(uint16_t)) status = FLASH_ProgramHalfWord(address, *((uint16_t *)(&flash_buffer[index])));
					else if (size == sizeof(uint8_t)) status = FLASH_ProgramByte(address, *((uint8_t *)(&flash_buffer[index])));

					if (status != FLASH_COMPLETE) {
						break;
					}
					index += size;
					address += size;
				}
				FLASH_Lock();

				// ack/nack 1k blocks
				if (status == FLASH_COMPLETE) {
					can_ack(BOOTLOADER_WRITE_SEGMENT);
				} else if (status != FLASH_COMPLETE) {
					can_nack(BOOTLOADER_WRITE_SEGMENT);
					break;
				}

				length -= index;
			}

			if (CAN_COMMAND == BOOTLOADER_NACK) break;

			// ack/nack the entire operation
			if (status == FLASH_COMPLETE) {
				can_ack(BOOTLOADER_WRITE);
			} else if (status != FLASH_COMPLETE) {
				can_nack(BOOTLOADER_WRITE);
				break;
			}

			if (flash_region == FLASH_REGION_APPLICATION) {
				// verify the application
				FLASH_ReadApplicationData();

				if (verify_application()) {
					can_ack(BOOTLOADER_WRITE);
				} else {
					can_nack(BOOTLOADER_WRITE);
				}
			}

			break;
		}
		case BOOTLOADER_VERIFY:
		{
			// return ack/nack for command parameters, return ack/nack for application verification
			can_ack(BOOTLOADER_VERIFY);

			// verify the app in flash
			if (verify_application()) {
				can_ack(BOOTLOADER_VERIFY);
			} else {
				can_nack(BOOTLOADER_VERIFY);
			}

			break;
		}
		case BOOTLOADER_EXECUTE:
		{
			// return ack, set boot flag, save and reset
			can_ack(BOOTLOADER_EXECUTE);

			RtcUserData.boot_flag = BOOT_FLAG_APPLICATION;
			RTC_WriteBackupRegisters();

			NVIC_SystemReset();
			break;
		}
		case BOOTLOADER_SECURE:
		{
			// TODO v2 Implement secure boot commands
			uint8_t flash_region = can_rx.Data[0];
			uint8_t secure_type = can_rx.Data[1];
			uint8_t secure_access_type = can_rx.Data[2];

			can_ack(BOOTLOADER_SECURE);

			break;
		}
		case BOOTLOADER_READ_KEY:
		{
			uint8_t key = can_rx.Data[0];
			if (key >= 0x01 && key <= 0xFF) {
				if (key == KEY_BOARD) {
					memcpy(&can_tx.Data[2], &FlashUserData.board, sizeof(FlashUserData.board));
				} else if (key == KEY_NODE) {
					memcpy(&can_tx.Data[2], &FlashUserData.node, sizeof(FlashUserData.node));
				} else if (key == KEY_BOARD_REVISION) {
					memcpy(&can_tx.Data[2], &FlashUserData.revision, sizeof(FlashUserData.revision));
				} else if (key == KEY_BOARD_PART_NUMBER) {
					memcpy(&can_tx.Data[2], &FlashUserData.part_number, sizeof(FlashUserData.part_number));
				} else if (key == KEY_BOARD_SERIAL_NUMBER) {
					memcpy(&can_tx.Data[2], &FlashUserData.serial_number, sizeof(FlashUserData.serial_number));
				} else if (key == KEY_BOARD_MANUFACTURE_DATE) {
					memcpy(&can_tx.Data[2], &FlashUserData.manufacture_date, sizeof(FlashUserData.manufacture_date));
				} else {
					can_nack(BOOTLOADER_READ_KEY);
					break;
				}
			} else {
				can_nack(BOOTLOADER_READ_KEY);
			}

			can_tx.Data[0] = BOOTLOADER_ACK;
			can_tx.Data[1] = key;
			can_send(&can_tx, BOOTLOADER_READ_KEY, 8);

			break;
		}
		case BOOTLOADER_WRITE_KEY:
		{
			uint8_t key = can_rx.Data[0];
			if (key >= 0x01 && key <= 0xFF) {
				if (key == KEY_BOARD) {
					FlashUserData.board = can_rx.Data[1];
				} else if (key == KEY_NODE) {
					FlashUserData.node = can_rx.Data[1];
				} else if (key == KEY_BOARD_REVISION) {
					FlashUserData.revision = can_rx.Data[1];
				} else if (key == KEY_BOARD_PART_NUMBER) {
					FlashUserData.part_number = *((uint32_t *)(&can_rx.Data[1]));
				} else if (key == KEY_BOARD_SERIAL_NUMBER) {
					FlashUserData.serial_number = *((uint32_t *)(&can_rx.Data[1]));
				} else if (key == KEY_BOARD_MANUFACTURE_DATE) {
					FlashUserData.manufacture_date[0] = can_rx.Data[1];
					FlashUserData.manufacture_date[1] = can_rx.Data[2];
					FlashUserData.manufacture_date[2] = can_rx.Data[3];
				} else {
					can_nack(BOOTLOADER_WRITE_KEY);
					break;
				}
			} else {
				can_nack(BOOTLOADER_WRITE_KEY);
				break;
			}

			can_ack(BOOTLOADER_WRITE_KEY);

			break;
		}
		case BOOTLOADER_SAVE_KEYS:
		{
			can_ack(BOOTLOADER_SAVE_KEYS);

			FLASH_Status status = FLASH_WriteUserData();

			if (status == FLASH_COMPLETE) can_ack(BOOTLOADER_SAVE_KEYS);
			else if (status != FLASH_COMPLETE) can_nack(BOOTLOADER_SAVE_KEYS);

			break;
		}
		case BOOTLOADER_RESET:
		{
			uint8_t type = can_rx.Data[0];

			if (type == RESET_TYPE_PARALLAX_BOOTLOADER) {
				RtcUserData.boot_flag = BOOT_FLAG_PARALLAX_BOOTLOADER;
			} else if (type == RESET_TYPE_STM32_BOOTLOADER) {
				RtcUserData.boot_flag = BOOT_FLAG_STM32_BOOTLOADER;
			} else {
				can_nack(BOOTLOADER_RESET);
				continue;
			}

			can_ack(BOOTLOADER_RESET);

			RTC_WriteBackupRegisters();

			NVIC_SystemReset();
			break;
		}
		}
	}
}

bool can_receive(CanRxMsg * msg, bool wait) {
	do {
		if (CAN_GetFlagStatus(CAN2, CAN_FLAG_FMP0) == SET) {
			CAN_Receive(CAN2, CAN_FIFO0, msg);

			CAN_COMMAND_TYPE = (msg->ExtId >> 0) & CAN_COMMAND_TYPE_MASK;
			CAN_COMMAND	= (msg->ExtId >> 1) & CAN_COMMAND_MASK;
			CAN_DESTINATION = (msg->ExtId >> 9) & CAN_ADDRESS_MASK;
			CAN_DESTINATION_BOARD = (msg->ExtId >> 9) & CAN_ADDRESS_BOARD_MASK;
			CAN_DESTINATION_NODE = (msg->ExtId >> 13) & CAN_ADDRESS_NODE_MASK;
			CAN_SOURCE = (msg->ExtId >> 17) & CAN_ADDRESS_MASK;
			CAN_SOURCE_BOARD = (msg->ExtId >> 17) & CAN_ADDRESS_BOARD_MASK;
			CAN_SOURCE_NODE	= (msg->ExtId >> 21) & CAN_ADDRESS_NODE_MASK;
			CAN_BROADCAST_FLAG = (msg->ExtId >> 25) & CAN_BROADCAST_FLAG_MASK;
			CAN_PRIORITY = (msg->ExtId >> 27) & CAN_PRIORITY_MASK;

			CAN_ClearFlag(CAN2, CAN_FLAG_FMP0);
			return true;
		}
	} while (wait);
	return false;
}

void can_send(CanTxMsg * msg, uint8_t command, uint8_t dlc) {
	uint32_t id = 0;
	id |= (CAN_COMMAND_TYPE_RESPONSE & CAN_COMMAND_TYPE_MASK) << 0;
	id |= (command & CAN_COMMAND_MASK) << 1 ;
	id |= (CAN_SOURCE_BOARD & CAN_ADDRESS_BOARD_MASK) << 9;
	id |= (CAN_SOURCE_NODE & CAN_ADDRESS_NODE_MASK) << 13;
	id |= (BOARD_ID & CAN_ADDRESS_BOARD_MASK) << 17;
	id |= (NODE_ID & CAN_ADDRESS_NODE_MASK) << 21;
	id |= (CAN_BROADCAST_FLAG_NONE & CAN_BROADCAST_FLAG_MASK) << 25;
	id |= (CAN_PRIORITY_VERY_HIGH & CAN_PRIORITY_MASK) << 27;
	msg->ExtId = id;
	msg->DLC = dlc;
	uint8_t mailbox = CAN_Transmit(CAN2, msg);
	while (CAN_TransmitStatus(CAN2, mailbox) != CAN_TxStatus_Ok);
}

void can_ack(uint8_t command) {
	can_tx.Data[0] = BOOTLOADER_ACK;
	can_send(&can_tx, command, 1);
}

void can_nack(uint8_t command) {
	can_tx.Data[0] = BOOTLOADER_NACK;
	can_send(&can_tx, command, 1);
}

FLASH_Status erase(uint8_t type) {
	FLASH_Status status = FLASH_COMPLETE;

	FLASH_Unlock();
	if (type == FLASH_REGION_USER_DATA) {
		status = FLASH_EraseSector(FLASH_GetSector(USER_DATA_ADDRESS), VoltageRange_3);
	} else if (type == FLASH_REGION_APPLICATION) {
		status = FLASH_EraseSectors(APPLICATION_FLASH_SECTOR, FLASH_GetSector(FLASH_REGION_END), VoltageRange_3);
	}
	FLASH_Lock();

	return status;
}

bool verify_application(void) {
	uint32_t magic = FlashApplicationData.magic;
	uint32_t crc = FlashApplicationData.crc;
	uint32_t length = FlashApplicationData.length;
	uint32_t address = APPLICATION_ENTRY_POINT_ADDRESS;
	uint8_t size = 0;

	if (magic != FLASH_APPLICATION_DATA_MAGIC) {
		return false;
	}

	if (crc == 0 || crc == 0xFFFFFFFF) {
		return false;
	}

	if (length == 0 || length > APPLICATION_SIZE) {
		return false;
	}

	CRC_ResetDR();

	uint32_t crc32 = 0;

	while (length != 0) {
		if (length >= sizeof(uint32_t)) size = sizeof(uint32_t);
		else if (length >= sizeof(uint16_t)) size = sizeof(uint16_t);
		else if (length >= sizeof(uint8_t)) size = sizeof(uint8_t);

		if (size == sizeof(uint32_t)) crc32 = CRC_CalcCRC(*((uint32_t *)(address)));
		else if (size == sizeof(uint16_t)) crc32 = CRC_CalcCRC(*((uint16_t *)(address)));
		else if (size == sizeof(uint8_t)) crc32 = CRC_CalcCRC(*((uint8_t *)(address)));

		address += size;
		length -= size;
	}

	crc32 = CRC_GetCRC();

	if (crc == crc32) {
		return true;
	} else {
		return false;
	}
}

void execute_application(void) {
	// deinit peripherals

	uint32_t address = APPLICATION_ENTRY_POINT_ADDRESS;
	uint32_t stack_pointer;
	jump_function app_entry;

	if (((*(__IO uint32_t*)address) & 0x2FFE0000) == 0x20000000) {

		stack_pointer = (uint32_t) *((__IO uint32_t *)address);
		app_entry = (jump_function) *((__IO uint32_t *)(address + 4));

		SCB->VTOR = address;

		__set_MSP(stack_pointer);

		app_entry();
	}
}
