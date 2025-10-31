#ifndef _ALLHEAD_H_
#define _ALLHEAD_H_

/* 标准C库*/
#include <stdio.h>	
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "stack_macros.h"
#include "semphr.h"
#include "event_groups.h"

/* 外设相关 */
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "dht11.h"
#include "key.h"
#include "rtc.h"
#include "flash.h"
#include "beep.h"
#include "oled.h"
#include "oledfont.h"  	
#include "bmp.h"
#include "keyboard.h"
#include "FPM383F.h"
#include "global.h"

#define rtosMQSendFromISR(                              \
    handle,                                             \
    buffer,                                             \
    size,                                               \
    ret                                                 \
    )                                                   \
    {                                                   \
        ret = xQueueSendFromISR(handle,                 \
            (void *)&buffer, NULL);                     \
    }

/*版权声明：本文为CSDN博主「pq113_6」的原创文章，遵循CC 4.0 BY-SA版权协议，转载请附上原文出处链接及本声明。
原文链接：https://blog.csdn.net/pq113_6/article/details/124625958*/

extern TaskHandle_t app_task_init_handle		;
extern TaskHandle_t app_task_oled_handle     	;
extern TaskHandle_t app_task_keyboard_handle 	;
extern TaskHandle_t app_task_led_handle 	  	;
extern TaskHandle_t app_task_rtc_handle 	  	;
extern TaskHandle_t app_task_root_handle 	  	;
extern TaskHandle_t app_task_beep_handle 	  	;
extern TaskHandle_t app_task_atte_handle 	  	;
extern TaskHandle_t app_task_mod_handle 	  	;
extern TaskHandle_t app_task_show_handle		;

extern QueueHandle_t g_queue_oled;         //oled消息队列
extern QueueHandle_t g_queue_flash;

extern SemaphoreHandle_t g_mutex_oled;		//oled互斥锁
extern SemaphoreHandle_t g_mutex_printf;	//printf互斥锁

extern EventGroupHandle_t g_event_group_fpm;//指纹事件标志组
extern EventGroupHandle_t g_event_group_led;//指纹事件标志组
extern EventGroupHandle_t g_event_group_beep;//beep事件标志组

/* 类型 */
typedef struct __oled_t
{

#define OLED_CTRL_DISPLAY_ON        0x01
#define OLED_CTRL_DISPLAY_OFF       0x02
#define OLED_CTRL_INIT              0x03
#define OLED_CTRL_CLEAR             0x04
#define OLED_CTRL_CLEAR_REGION      0x05
#define OLED_CTRL_SHOW_STRING       0x06
#define OLED_CTRL_SHOW_CHINESE      0x07
#define OLED_CTRL_SHOW_PICTURE      0x08
	
#define OLED_CTRL_DISPLAY_DIRECTION     0x09

	uint8_t ctrl;
	uint8_t x;
	uint8_t y;

	uint8_t *str;
	uint8_t font_size;
    uint8_t chinese;

	uint8_t clr_region_width;
	uint8_t clr_region_height;	
	
	const uint8_t *pic;
	uint8_t pic_width;
	uint8_t pic_height;
	
	uint8_t direction;
}oled_t;


extern void dgb_printf_safe(const char *format, ...);


#endif
