#ifndef _FLASH_H_
#define _FLASH_H_

extern uint32_t flash_write_record(char *pbuf,uint32_t record_count);
extern void flash_read_record(char *pbuf,uint32_t record_count);
extern void flash_erase_record(void);

#endif  
