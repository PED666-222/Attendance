#include "allhead.h"

#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base address of Sector 0, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base address of Sector 1, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Base address of Sector 2, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Base address of Sector 3, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Base address of Sector 4, 64 Kbytes   */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Base address of Sector 5, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Base address of Sector 6, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Base address of Sector 7, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Base address of Sector 8, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Base address of Sector 9, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Base address of Sector 10, 128 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Base address of Sector 11, 128 Kbytes */


uint32_t flash_write_record(char *pbuf,uint32_t record_count)
{
	
	uint32_t addr_start=ADDR_FLASH_SECTOR_4+record_count*64;
	uint32_t addr_end  =addr_start+64;

	uint32_t i=0;
	while (addr_start < addr_end)
	{
		
		//每次写入数据是4个字节
		if (FLASH_ProgramWord(addr_start, *(uint32_t *)&pbuf[i]) == FLASH_COMPLETE)
		{
			//地址每次偏移4个字节
			addr_start +=4;
			
			i+=4;
			
		}

		else
		{ 
			dgb_printf_safe("flash write record fail,now goto erase sector!\r\n");
			
			//重新擦除扇区
			flash_erase_record();

			return 1;
		}
		
	}
	return 0;
}

void flash_read_record(char *pbuf,uint32_t record_count)
{
	uint32_t addr_start=ADDR_FLASH_SECTOR_4+record_count*64;
	uint32_t addr_end  =addr_start+64;

	uint32_t i=0;
	
	while (addr_start < addr_end)
	{
		*(uint32_t *)&pbuf[i] = *(__IO uint32_t*)addr_start;

		addr_start+=4;
		
		i = i + 4;
	}

}

void flash_erase_record(void)
{

	/* 如果不擦除，写入会失败，读取的数据都为0 */
	
	if (FLASH_EraseSector(FLASH_Sector_4, VoltageRange_3) != FLASH_COMPLETE)
	{ 
			dgb_printf_safe("Erase error\r\n");
			return;
	}
}



