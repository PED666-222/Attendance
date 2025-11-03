#include "allhead.h"

/*
指纹模块:可以进行添加指纹、验证指纹、获取总数、清空指纹，同时OLED显示操作，指纹验证成功蜂鸣器短鸣两声，LED亮，失败长鸣，LED长亮
OLED、LED、蜂鸣器独立任务控制
薄膜按键:按下按键蜂鸣器短鸣，OLED显示按下内容
按下KEY1考勤（独立任务运行）
输入密码后，按下KEY2.3.4分别实现添加 获取总数 清空指纹的功能，通过串口1/蓝牙修改时间和日期，并且OLED实时显示操作
异或加解密法

*/

#define	XOR_KEY	0x17

volatile uint32_t	rec_cnt=0;
volatile uint32_t	i=0;
char 	 save_buf[128]="0";
char 	 att_buf[30]="0";

static RTC_TimeTypeDef  RTC_TimeStructure;
static RTC_DateTypeDef  RTC_DateStructure;

TaskHandle_t app_task_init_handle     = NULL;
TaskHandle_t app_task_oled_handle     = NULL;
TaskHandle_t app_task_keyboard_handle = NULL;
TaskHandle_t app_task_led_handle 	  = NULL;
TaskHandle_t app_task_rtc_handle 	  = NULL;
TaskHandle_t app_task_root_handle 	  = NULL;
TaskHandle_t app_task_beep_handle 	  = NULL;
TaskHandle_t app_task_atte_handle 	  = NULL;
TaskHandle_t app_task_mod_handle 	  = NULL;
TaskHandle_t app_task_show_handle 	  = NULL;

QueueHandle_t g_queue_oled;         //oled消息队列
QueueHandle_t g_queue_rtc; 			//rtc消息队列
QueueHandle_t g_queue_flash; 		//flash消息队列

SemaphoreHandle_t g_mutex_oled;		//oled互斥锁
SemaphoreHandle_t g_mutex_printf;	//printf互斥锁

EventGroupHandle_t g_event_group_fpm; //指纹事件标志组
EventGroupHandle_t g_event_group_led; //LED事件标志组
EventGroupHandle_t g_event_group_root;//root事件标志组
EventGroupHandle_t g_event_group_beep;//beep事件标志组
EventGroupHandle_t g_event_group_rtc; //rtc事件标志组


/*   任务menu 	*/ 
static void app_task_init(void* pvParameters);  
/*   任务oled 	*/ 
static void app_task_oled(void* pvParameters);  
/* 任务keyboard */ 
static void app_task_keyboard(void* pvParameters);  
/*   任务led 	*/
static void app_task_led(void* pvParameters);
/*   任务root 	*/
static void app_task_root(void* pvParameters);
/*   任务atte 	*/
static void app_task_atte(void* pvParameters);
/*   任务beep 	*/
static void app_task_beep(void* pvParameters);
/*   任务rtc 	*/
static void app_task_rtc(void* pvParameters);  
/*   任务mod 	*/
static void app_task_mod(void* pvParameters);
/* 任务show */ 
static void app_task_show(void* pvParameters); 

void exceptions_catch(void)
{
	uint32_t icsr=SCB->ICSR;
	uint32_t ipsr=__get_IPSR();

	dgb_printf_safe("exceptions catch\r\n"); 
	dgb_printf_safe("IPSR=0x%08X\r\n",ipsr); 
	dgb_printf_safe("ICSR=0x%08X\r\n",icsr);
 }

 /* OLED互斥锁高度封装 */
#define OLED_SAFE(__CODE)                                \
	{                                                    \
                                                         \
		do                                               \
		{                                                \
			xSemaphoreTake(g_mutex_oled, portMAX_DELAY); \
			if (g_oled_display_flag)                     \
				__CODE;                                  \
			xSemaphoreGive(g_mutex_oled);                \
		} while (0)                                      \
	}

#define DEBUG_PRINTF_EN 1
 
 void dgb_printf_safe(const char *format, ...)
{
#if DEBUG_PRINTF_EN

	va_list args;
	va_start(args, format);

	/* 获取互斥信号量 */
	xSemaphoreTake(g_mutex_printf, portMAX_DELAY);

	vprintf(format, args);

	/* 释放互斥信号量 */
	xSemaphoreGive(g_mutex_printf);

	va_end(args);
#else
	(void)0;
#endif
}

int main(void)
{
	/* 设置系统中断优先级分组4 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	
	/* 系统定时器中断频率为configTICK_RATE_HZ */
	SysTick_Config(SystemCoreClock/configTICK_RATE_HZ);									
	
	/* 初始化串口1 */
	Usart1_Init(115200);  

	/* 创建app_task_menu */
	xTaskCreate((TaskFunction_t )app_task_init,  			/* 任务入口函数 */
			  (const char*    )"app_task_init",				/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小，512字 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_init_handle);		/* 任务控制块指针 */ 

	/* 开启任务调度 */
	vTaskStartScheduler(); 
	
}

static void app_task_init(void* pvParameters)//任务初始化
{
	//LED初始化
	LED_Init();
	//矩阵键盘初始化
	key_board_init();
	//oled初始化
	OLED_Init();
	//oled清屏
	OLED_Clear();
	//按键初始化
	key_init();	
	//蜂鸣器初始化
	beep_init();
	//指纹模块初始化
	fpm_init();
	
	/*解锁FLASH，允许操作FLASH*/
	FLASH_Unlock();

	/* 清空相应的标志位*/  
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
				   FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR); 	

	//尝试获取100条记录
	for(i=0;i<100;i++)
	{
		//获取存储的记录
		flash_read_record(save_buf,i);
		
		//检查记录是否存在换行符号，不存在则不打印输出
		if(strstr((const char *)save_buf,"\n")==0)
			break;		
		
	}	
	rec_cnt=i;
	
	//rtc初始化
	rtc_init();
	//蓝牙串口初始化
	//Usart3_Init(9600);	
	
	//创建互斥锁
	g_mutex_oled   = xSemaphoreCreateMutex();
	g_mutex_printf = xSemaphoreCreateMutex();
	
	//创建消息队列
	g_queue_oled = xQueueCreate(16,sizeof(oled_t));	
	g_queue_rtc  = xQueueCreate(5,64);
	
	//创建事件标志组
	g_event_group_rtc=xEventGroupCreate();
	g_event_group_led=xEventGroupCreate();
	g_event_group_fpm=xEventGroupCreate();
	g_event_group_root=xEventGroupCreate();
	g_event_group_beep=xEventGroupCreate();
	
	printf("-----------------欢迎进入考勤机-----------------\n");

	/* 创建app_task_oled任务 */
	xTaskCreate((TaskFunction_t )app_task_oled,  			/* 任务入口函数 */
			  (const char*    )"app_task_oled",				/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小，512字 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_oled_handle);		/* 任务控制块指针 */ 
			  
	/* 创建app_task_keyboard任务 */
	xTaskCreate((TaskFunction_t )app_task_keyboard,  		/* 任务入口函数 */
			  (const char*    )"app_task_keyboard",			/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小，512字 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_keyboard_handle);	/* 任务控制块指针 */ 
			  
	/* 创建app_task_led任务 */		  
	xTaskCreate((TaskFunction_t )app_task_led,  			/* 任务入口函数 */
			  (const char*    )"app_task_led",				/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_led_handle);		/* 任务控制块指针 */ 

	/* 创建app_task_root任务 */		  
	xTaskCreate((TaskFunction_t )app_task_root,  			/* 任务入口函数 */
			  (const char*    )"app_task_root",				/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_root_handle);		/* 任务控制块指针 */ 
			  
	/* 创建app_task_atte任务 */		  
	xTaskCreate((TaskFunction_t )app_task_atte,  			/* 任务入口函数 */
			  (const char*    )"app_task_atte",				/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_atte_handle);		/* 任务控制块指针 */
			  
	/* 创建app_task_beep任务 */		  
	xTaskCreate((TaskFunction_t )app_task_beep,  			/* 任务入口函数 */
			  (const char*    )"app_task_beep",				/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_beep_handle);		/* 任务控制块指针 */ 

	/* 创建app_task_rtc任务 */
	xTaskCreate((TaskFunction_t )app_task_rtc,  			/* 任务入口函数 */
			  (const char*    )"app_task_rtc",				/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小，512字 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_rtc_handle);		/* 任务控制块指针 */ 

	/* 创建app_task_mod任务 */
	xTaskCreate((TaskFunction_t )app_task_mod,  			/* 任务入口函数 */
			  (const char*    )"app_task_mod",				/* 任务名字 */
			  (uint16_t       )512,  						/* 任务栈大小，512字 */
			  (void*          )NULL,						/* 任务入口函数参数 */
			  (UBaseType_t    )5, 							/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_mod_handle);		/* 任务控制块指针 */
 
	/* 创建app_task_show任务 */
	xTaskCreate((TaskFunction_t )app_task_show,  		/* 任务入口函数 */
			  (const char*    )"app_task_show",			/* 任务名字 */
			  (uint16_t       )512,  				/* 任务栈大小，512字 */
			  (void*          )NULL,				/* 任务入口函数参数 */
			  (UBaseType_t    )5, 					/* 任务的优先级 */
			  (TaskHandle_t*  )&app_task_show_handle);	/* 任务控制块指针 */ 			  

			  
	OLED_ShowString(38,2,(u8 *)"K1",16);
	OLED_ShowCHinese(54,2,0);//考
	OLED_ShowCHinese(70,2,1);//勤
	OLED_ShowString(0,4,(u8 *)"Code Enter ROOT",16);
	OLED_ShowString(64,6,(u8 *)"by",16);
	OLED_ShowCHinese(80,6,26); //
	OLED_ShowCHinese(96,6,27); //
	OLED_ShowCHinese(112,6,28);//			  
	

	/* 删除任务自身 */
	vTaskDelete(NULL);			  
}  



static void app_task_oled(void* pvParameters)//OLED显示
{
	oled_t oled;
	BaseType_t xReturn=pdFALSE;	
	for(;;)
	{
		xReturn = xQueueReceive( g_queue_oled,	/* 消息队列的句柄 */
								&oled, 			/* 得到的消息内容 */
								portMAX_DELAY);	/* 等待时间一直等 */
		if(xReturn != pdPASS)
			continue;
		
		switch(oled.ctrl)
		{
			case OLED_CTRL_DISPLAY_ON:
			{
				/* 亮屏 */
				OLED_Display_On();
			}break;

			case OLED_CTRL_DISPLAY_OFF:
			{
				/* 灭屏 */
				OLED_Display_Off();	
			}break;

			case OLED_CTRL_CLEAR:
			{
				/* 清屏 */
				OLED_Clear();
			}break;

			case OLED_CTRL_SHOW_STRING:
			{
				/* 显示字符串 */
				OLED_ShowString(oled.x,
								oled.y,
								oled.str,
								oled.font_size);
				
			}break;

			case OLED_CTRL_SHOW_CHINESE:
			{
				/* 显示汉字 */
				OLED_ShowCHinese(oled.x,
								oled.y,
								oled.chinese);
			}break;			

			case OLED_CTRL_SHOW_PICTURE:
			{
				/* 显示图片 */
				OLED_DrawBMP(	oled.x,
								oled.y,
								oled.x+oled.pic_width,
								oled.y+oled.pic_height,
								oled.pic);
			}break;

			default:dgb_printf_safe("[app_task_oled] oled ctrl code is invalid\r\n");	
				break;
		}
	}
}   

static void app_task_keyboard(void* pvParameters)//薄膜按键
{
	u8   i=0;
	char key_old=0;
	char key_cur=0;
	char password[9]="0";
	char master_xor[9]="0";
	char xor_er[9]="0";
	
	oled_t	 oled;
	uint32_t key_sta=0;
	uint8_t  buf_pw[8]={0};
	uint8_t  buf_order[16]={0};
	
	BaseType_t 	xReturn=pdFALSE;
	
	char master_pw[9]={49,50,51,52,53,54,55,56,'\0'};//  1 2 3 4 5 6 7 8 ASCLL码
	
	xor_encryption(XOR_KEY,master_pw,master_xor,sizeof(master_pw));
	for(;;)
	{
		switch(key_sta)
		{
			case 0://获取按下的按键
			{
				key_cur = get_key_board();	

				if(key_cur != 'N')
				{
					key_old = key_cur;
					key_sta=1;
				}
			}break;
			case 1://确认按下的按键
			{
				key_cur = get_key_board();	
				
				if((key_cur != 'N') && (key_cur == key_old))
				{
					dgb_printf_safe("KEY %c Down\r\n",key_cur);
					key_sta=2;
					xEventGroupSetBits(g_event_group_beep,0x01);//蜂鸣器
						
					
					if(key_cur == 'C')//清零
					{
						dgb_printf_safe("密码清0\n");
						for(i=0;i<8;i++)
						{
							password[i]=0;
						}
						i = 0;	
					}
					else if(key_cur == '*')//退一位密码
					{
						dgb_printf_safe("密码退位\n");
						password[i]=0;
						i--;
					}
					else
					{
						password[i]=key_cur;
						i++;
					}
				}
			}break;
			case 2://获取释放的按键
			{
				key_cur = get_key_board();	
					
				if(key_cur == 'N')
				{
					key_sta=0;
					key_old =  'N';
				}
			}break;
			default:break;
		}
		if(i == 8)
		{
			i=0;
			
			oled.ctrl=OLED_CTRL_CLEAR;					//清屏
			xQueueSend(g_queue_oled,&oled,100);	
			
			sprintf((char *)buf_pw,"pw:%s",password);
			dgb_printf_safe("Input %s\r\n",buf_pw);
			/* oled显示 */
			oled.ctrl=OLED_CTRL_SHOW_STRING;
			oled.x=16;
			oled.y=3;
			oled.str=buf_pw;
			oled.font_size=16;
			xReturn = xQueueSend(g_queue_oled,&oled,100);			
			if(xReturn != pdPASS) dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
			
			xor_decryption(XOR_KEY,master_xor,xor_er,sizeof(master_xor));
			
			if(memcmp(xor_er,password,8) == 0) //密码12345678
			{	
				oled.ctrl=OLED_CTRL_CLEAR;					//清屏
				xQueueSend(g_queue_oled,&oled,100);
				sprintf((char *)buf_order,"Success!");
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=32;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend(g_queue_oled,&oled,100);
				if(xReturn != pdPASS) dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				
				xEventGroupSetBits(g_event_group_beep,0x04);//蜂鸣器
				xEventGroupSetBits(g_event_group_led,0x01); //LED
				xEventGroupSetBits(g_event_group_root,0x01); //root
				vTaskDelay(2000);
				
				//管理员界面
				oled.ctrl=OLED_CTRL_CLEAR;					//清屏
				xQueueSend(g_queue_oled,&oled,100);
				
				sprintf((char *)buf_order,"K2  Add Fpm");
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=16;
				oled.y=0;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend(g_queue_oled,&oled,100);
				if(xReturn != pdPASS) dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				vTaskDelay(1000);
				memset(&oled,0,sizeof(oled));
				sprintf((char *)buf_order,"K3  Fpm Num");
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=16;
				oled.y=2;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend(g_queue_oled,&oled,100);
				if(xReturn != pdPASS) dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				vTaskDelay(500);
				sprintf((char *)buf_order,"K4  Del All");
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=16;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend(g_queue_oled,&oled,100);
				if(xReturn != pdPASS) dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				vTaskDelay(500);
				sprintf((char *)buf_order,"U1/3 Mod Time");
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=8;
				oled.y=6;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend(g_queue_oled,&oled,100);
				if(xReturn != pdPASS) dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
			
			}else //密码错误
			{
				xEventGroupSetBits(g_event_group_beep,0x02);//蜂鸣器
				xEventGroupSetBits(g_event_group_led,0x02); //LED
				
				oled.ctrl=OLED_CTRL_CLEAR;					//清屏
				xQueueSend(g_queue_oled,&oled,100);
				xSemaphoreTake(g_mutex_oled,portMAX_DELAY);
				sprintf((char *)buf_order,"Code Error!");
				/* oled显示 */
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=24;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend(g_queue_oled,&oled,100);
				xSemaphoreGive(g_mutex_oled);
				
			}
		}
	}
}   

static void app_task_led(void* pvParameters)//LED任务
{
	EventBits_t led_event_bits=0;
	for(;;)
	{
		led_event_bits=xEventGroupWaitBits(g_event_group_led,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
		if(led_event_bits & 0x01)//操作正确
		{
			GPIO_ResetBits(GPIOF,GPIO_Pin_9  | GPIO_Pin_10);
			GPIO_ResetBits(GPIOE,GPIO_Pin_13 | GPIO_Pin_14);			
			vTaskDelay(100);
			GPIO_SetBits(GPIOF,GPIO_Pin_9  | GPIO_Pin_10);
			GPIO_SetBits(GPIOE,GPIO_Pin_13 | GPIO_Pin_14);
		}
		if(led_event_bits & 0x02)//操作失败
		{
			GPIO_ResetBits(GPIOF,GPIO_Pin_9  | GPIO_Pin_10);
			GPIO_ResetBits(GPIOE,GPIO_Pin_13 | GPIO_Pin_14);			
			vTaskDelay(500);
			GPIO_SetBits(GPIOF,GPIO_Pin_9  | GPIO_Pin_10);
			GPIO_SetBits(GPIOE,GPIO_Pin_13 | GPIO_Pin_14);
		}

	}
}


static void app_task_root(void* pvParameters)//管理员
{
	oled_t	 	oled;
	uint8_t 	fmp_error_code;
	uint16_t 	id_total;
	uint8_t  	buf_order[16]={0};
	EventBits_t fpm_event_bits=0;
	EventBits_t root_event_bits=0;
	BaseType_t 	xReturn=pdFALSE;
	
	for(;;)
	{
		/* 按键实现添加指纹、验证指纹、获取指纹总数、清空指纹 */
		root_event_bits=xEventGroupWaitBits(g_event_group_root,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
		fpm_event_bits=xEventGroupWaitBits(g_event_group_fpm,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
		
		/* 添加指纹 */
		if((root_event_bits & 0x01) && (fpm_event_bits & 0x02))
		{
			fpm_ctrl_led(FPM_LED_BLUE);
			dgb_printf_safe("--------请将手指放到指纹模块触摸感应区--------\r\n");
			
			oled.ctrl=OLED_CTRL_CLEAR;					//清屏
			xQueueSend(g_queue_oled,&oled,100);	
			sprintf((char *)buf_order,"Add Fpm");	//显示命令
			oled.ctrl=OLED_CTRL_SHOW_STRING;
			oled.x=32;
			oled.y=2;
			oled.str=buf_order;
			oled.font_size=16;
			xReturn = xQueueSend(g_queue_oled,&oled,100);					
			if(xReturn != pdPASS)
				dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
			
			fmp_error_code =fpm_id_total(&id_total);

            if(fmp_error_code == 0)
            {
                dgb_printf_safe("获取指纹总数：%04d\r\n",id_total);
				
				/* 添加指纹*/
				fmp_error_code=fpm_enroll_auto(id_total+1);

				if(fmp_error_code == 0)
				{
					fpm_ctrl_led(FPM_LED_GREEN);					
					
					dgb_printf_safe("自动注册指纹成功\r\n");
					
					sprintf((char *)buf_order,"Add Success");	//显示命令
					oled.ctrl=OLED_CTRL_SHOW_STRING;
					oled.x=16;
					oled.y=4;
					oled.str=buf_order;
					oled.font_size=16;
					xReturn = xQueueSend(g_queue_oled,&oled,100);				
					if(xReturn != pdPASS)
						dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
					
					xEventGroupSetBits(g_event_group_beep,0x01);//蜂鸣器	
					xEventGroupSetBits(g_event_group_led,0x01); //LED					
				}
				else
				{
					fpm_ctrl_led(FPM_LED_RED);
					
					sprintf((char *)buf_order,"Failed");	//显示命令
					oled.ctrl=OLED_CTRL_SHOW_STRING;
					oled.x=48;
					oled.y=4;
					oled.str=buf_order;
					oled.font_size=16;
					xReturn = xQueueSend(g_queue_oled,&oled,100);		
					if(xReturn != pdPASS)
						dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
					
					xEventGroupSetBits(g_event_group_beep,0x02);//蜂鸣器
					xEventGroupSetBits(g_event_group_led,0x02); //LED
				}
				Delay_ms(100);	
				
				fpm_sleep();
				
				Delay_ms(1000);	
            }
		}
		
		/* 获取用户总数 */
		if((root_event_bits & 0x01) && (fpm_event_bits & 0x04))
		{
			fpm_ctrl_led(FPM_LED_BLUE);
			
			dgb_printf_safe("\n========================================\r\n");
			oled.ctrl=OLED_CTRL_CLEAR;					//清屏
			xQueueSend(g_queue_oled,&oled,100);	
			sprintf((char *)buf_order,"Fpm Num");	//显示命令
			oled.ctrl=OLED_CTRL_SHOW_STRING;
			oled.x=32;
			oled.y=2;
			oled.str=buf_order;
			oled.font_size=16;
			xReturn = xQueueSend(g_queue_oled,&oled,100);		
			if(xReturn != pdPASS)
				dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
			
			fmp_error_code =fpm_id_total(&id_total);

            if(fmp_error_code == 0)
            {
				fpm_ctrl_led(FPM_LED_GREEN);
				
                dgb_printf_safe("获取指纹总数：%04d\r\n",id_total);
                
				sprintf((char *)buf_order,"Num:%04d",id_total);	//显示命令
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=32;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend( 	g_queue_oled,/* 消息队列的句柄 */
										&oled,	/* 发送的消息内容 */
										100);		/* 等待时间 100 Tick */				
				if(xReturn != pdPASS)
					dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				
                xEventGroupSetBits(g_event_group_beep,0x01);//蜂鸣器
				xEventGroupSetBits(g_event_group_led,0x01); //LED
            }
			else
			{
				fpm_ctrl_led(FPM_LED_RED);
				
				sprintf((char *)buf_order,"Failed");	//显示命令
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=32;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend( 	g_queue_oled,/* 消息队列的句柄 */
										&oled,	/* 发送的消息内容 */
										100);		/* 等待时间 100 Tick */				
				if(xReturn != pdPASS)
					dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				
				xEventGroupSetBits(g_event_group_beep,0x02);//蜂鸣器
				xEventGroupSetBits(g_event_group_led,0x02); //LED
			}			
			
			Delay_ms(100);		
			
			fpm_sleep();	

			Delay_ms(1000);				
		}	

		
		/* 清空指纹 */
		if((root_event_bits & 0x01) && (fpm_event_bits & 0x08))
		{
			fpm_ctrl_led(FPM_LED_BLUE);
			
			dgb_printf_safe("\n========================================\r\n");
			oled.ctrl=OLED_CTRL_CLEAR;					//清屏
			xQueueSend(g_queue_oled,&oled,100);	
			sprintf((char *)buf_order,"Del All");	//显示命令
			oled.ctrl=OLED_CTRL_SHOW_STRING;
			oled.x=32;
			oled.y=2;
			oled.str=buf_order;
			oled.font_size=16;
			xReturn = xQueueSend( 	g_queue_oled,/* 消息队列的句柄 */
									&oled,	/* 发送的消息内容 */
									100);		/* 等待时间 100 Tick */				
			if(xReturn != pdPASS)
				dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
			
			fmp_error_code=fpm_empty();
            if(fmp_error_code == 0)
            {
				fpm_ctrl_led(FPM_LED_GREEN);
				
                dgb_printf_safe("清空指纹成功\r\n");
				
				sprintf((char *)buf_order,"Del Success");	//显示命令
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=16;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend( 	g_queue_oled,/* 消息队列的句柄 */
										&oled,	/* 发送的消息内容 */
										100);		/* 等待时间 100 Tick */				
				if(xReturn != pdPASS)
					dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
                
                xEventGroupSetBits(g_event_group_beep,0x01);//蜂鸣器
				xEventGroupSetBits(g_event_group_led,0x01); //LED
            }
			else
			{
				fpm_ctrl_led(FPM_LED_RED);
				
				sprintf((char *)buf_order,"Failed");	//显示命令
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=32;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend( 	g_queue_oled,/* 消息队列的句柄 */
										&oled,	/* 发送的消息内容 */
										100);		/* 等待时间 100 Tick */				
				if(xReturn != pdPASS)
					dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				
				xEventGroupSetBits(g_event_group_beep,0x02);//蜂鸣器
				xEventGroupSetBits(g_event_group_led,0x02); //LED
			}			
			Delay_ms(100);		
			
			fpm_sleep();	

			Delay_ms(1000);								
		}	
	}
}  

static void app_task_atte(void* pvParameters)//考勤
{
	oled_t	 	oled;
	uint8_t 	fmp_error_code;
	uint16_t 	id;
	uint8_t  	buf_order[16]={0};
	EventBits_t fpm_event_bits=0;
	BaseType_t 	xReturn=pdFALSE;
	for(;;)
	{
		fpm_event_bits=xEventGroupWaitBits(g_event_group_fpm,0x01,pdTRUE,pdFALSE,portMAX_DELAY);
		/* 刷指纹 */
		if(fpm_event_bits & 0x01)
		{
			fpm_ctrl_led(FPM_LED_BLUE);
			
			dgb_printf_safe("\n============================================\r\n");
			dgb_printf_safe("请将手指放到指纹模块触摸感应区\r\n");
			
			oled.ctrl=OLED_CTRL_CLEAR;					//清屏
			xQueueSend(g_queue_oled,&oled,100);	
			sprintf((char *)buf_order,"Mod Fpm");	//显示命令
			oled.ctrl=OLED_CTRL_SHOW_STRING;
			oled.x=32;
			oled.y=2;
			oled.str=buf_order;
			oled.font_size=16;
			xReturn = xQueueSend( 	g_queue_oled,
									&oled,	     
									100);		 			
			if(xReturn != pdPASS)
				dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
			
			/* 参数为0xFFFF进行1:N匹配 */
			id = 0xFFFF;
			
			fmp_error_code=fpm_idenify_auto(&id);
            
            if(fmp_error_code == 0)
            {
				fpm_ctrl_led(FPM_LED_GREEN);
				
                dgb_printf_safe("%04d考勤成功!\r\n",id);
					
				sprintf((char *)buf_order,"%04d Attend",id);	//显示命令
				sprintf((char *)att_buf,"%04d Attend",id);	
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=16;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend(g_queue_oled,&oled,100);		 			
				if(xReturn != pdPASS)
					dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				
                xEventGroupSetBits(g_event_group_beep,0x01);//蜂鸣器
				xEventGroupSetBits(g_event_group_led,0x01); //LED
            }
			else
			{
				fpm_ctrl_led(FPM_LED_RED);
				
				sprintf((char *)buf_order,"Failed");	//显示命令
				oled.ctrl=OLED_CTRL_SHOW_STRING;
				oled.x=36;
				oled.y=4;
				oled.str=buf_order;
				oled.font_size=16;
				xReturn = xQueueSend(g_queue_oled,&oled,100);	
				if(xReturn != pdPASS)
					dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
				
				xEventGroupSetBits(g_event_group_beep,0x02);//蜂鸣器
				xEventGroupSetBits(g_event_group_led,0x02); //LED
			}
			
			Delay_ms(100);		
			
			fpm_sleep();	

			Delay_ms(1000);				
			
		}
	}
}	

static void app_task_beep(void* pvParameters)//蜂鸣器任务
{
	EventBits_t beep_event_bits=0;
	for(;;)
	{
		beep_event_bits=xEventGroupWaitBits(g_event_group_beep,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
		if(beep_event_bits & 0x01)//按键声音
		{
			beep_on();Delay_ms(50);beep_off();
		}
		if(beep_event_bits & 0x02)//按键失败
		{
			beep_on();Delay_ms(500);beep_off();
		}
		if(beep_event_bits & 0x04)//按键成功
		{
			beep_on();Delay_ms(50);beep_off();Delay_ms(50);
			beep_on();Delay_ms(50);beep_off();
		}
	}
}

static void app_task_rtc(void* pvParameters)//rtc_flash
{
	EventBits_t event_bits_rtc=0;
	
	dgb_printf_safe("--Attendance record num is %d--\r\n",rec_cnt);
			
	for(;;)
	{
		//等待事件标志组置位
		event_bits_rtc=xEventGroupWaitBits(g_event_group_rtc,0x0f,pdTRUE,pdFALSE,portMAX_DELAY);
		if((event_bits_rtc & 0x01) && (strstr((const char *)att_buf,"Attend")))
		{
			if(rec_cnt<100)
			{
				
//				if(strstr((const char *)att_buf,"Attend"))
//				{
						// 获取日期
					RTC_GetDate(RTC_Format_BCD,&RTC_DateStructure);
					
					// 获取时间
					RTC_GetTime(RTC_Format_BCD, &RTC_TimeStructure);
					
					printf(" 20%02x/%02x/%02x Week:%x %02x:%02x:%02x\r\n", \
										
										RTC_DateStructure.RTC_Year, RTC_DateStructure.RTC_Month, RTC_DateStructure.RTC_Date,RTC_DateStructure.RTC_WeekDay,\
										RTC_TimeStructure.RTC_Hours, RTC_TimeStructure.RTC_Minutes, RTC_TimeStructure.RTC_Seconds);
					//格式化字符串,末尾添加\r\n作为一个结束标记，方便我们读取的时候进行判断
					sprintf((char *)save_buf,"[%s] 20%02x/%02x/%02x Week:%x %02x:%02x:%02x\r\n", \
									att_buf,\
									RTC_DateStructure.RTC_Year, RTC_DateStructure.RTC_Month, RTC_DateStructure.RTC_Date,RTC_DateStructure.RTC_WeekDay,\
									RTC_TimeStructure.RTC_Hours, RTC_TimeStructure.RTC_Minutes, RTC_TimeStructure.RTC_Seconds);
					//写入温湿度记录
					if(0==flash_write_record(save_buf,rec_cnt))
					{
						//显示
						dgb_printf_safe("input ---%s",save_buf);					

						//记录自加1
						rec_cnt++;						
					}
					else
					{
						//数据记录清零，重头开始存储数据
						rec_cnt=0;
					}
				//}
				
				
			}
			else
			{
				//超过100条记录则打印
				dgb_printf_safe("The record has reached 100 and cannot continue writing\r\n");
			}
			memset(att_buf,0,sizeof(att_buf));

		}
		
	}
}   

static void app_task_mod(void* pvParameters)//修改时间
{
	uint8_t 	  rt;
	oled_t		  oled;
	char 		  rtc_buf[30]="0";
	uint8_t  	  buf_order[16]={0};
	uint32_t 	  year,month,date,weekday=0;
	uint32_t 	  hour,minute,second=0;
	BaseType_t 	  xReturn=pdFALSE;
	EventBits_t   root_event_bits=0;
	Usart3_Init(9600);
	for(;;)
	{
		root_event_bits=xEventGroupWaitBits(g_event_group_root,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
		xReturn = xQueueReceive( g_queue_rtc,&rtc_buf,portMAX_DELAY);	
		
		dgb_printf_safe("%s\n",rtc_buf);
		
		if((root_event_bits & 0x01) && (strstr((const char *)rtc_buf,"DATE SET")))
		{
			vTaskSuspend(app_task_rtc_handle);
			rt = sscanf((char *)rtc_buf,"DATE SET-20%d-%d-%d-%d#",&year,&month,&date,&weekday);
			RTC_DateStructure.RTC_Year 		= ((year/10) << 4)+year % 10;
			RTC_DateStructure.RTC_Month 	= ((month/10) << 4)+month % 10;
			RTC_DateStructure.RTC_Date 		= ((date/10) << 4)+date % 10;
			RTC_DateStructure.RTC_WeekDay 	= ((weekday/10) <<4) +weekday % 10;
			RTC_SetDate(RTC_Format_BCD, &RTC_DateStructure);	
			vTaskResume(app_task_rtc_handle);
			memset((void *)rtc_buf,0,sizeof(rtc_buf));
			oled.ctrl=OLED_CTRL_CLEAR;					//清屏
			xQueueSend(g_queue_oled,&oled,100);
			
			sprintf((char *)buf_order,"Mod Date Success");
			oled.ctrl=OLED_CTRL_SHOW_STRING;
			oled.x=0;
			oled.y=3;
			oled.str=buf_order;
			oled.font_size=16;
			xReturn = xQueueSend(g_queue_oled,&oled,100);
			if(xReturn != pdPASS) dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
			vTaskDelay(1000);
			memset(&oled,0,sizeof(oled));
			
			xEventGroupSetBits(g_event_group_beep,0x04);//蜂鸣器
			xEventGroupSetBits(g_event_group_led,0x01); //LED
				
		}
		else if((root_event_bits & 0x01) && (strstr((const char *)rtc_buf,"TIME SET")))
		{			
			rt = sscanf((char *)rtc_buf,"TIME SET-%d-%d-%d#",&hour,&minute,&second); 
			RTC_TimeStructure.RTC_H12     = RTC_H12_PM;
			RTC_TimeStructure.RTC_Hours   = ((hour/10) << 4)+hour % 10;
			RTC_TimeStructure.RTC_Minutes = ((minute/10) << 4)+minute % 10;
			RTC_TimeStructure.RTC_Seconds = ((second/10) << 4)+second % 10; 
			RTC_SetTime(RTC_Format_BCD, &RTC_TimeStructure); 
			memset((void *)rtc_buf,0,sizeof(rtc_buf));
			oled.ctrl=OLED_CTRL_CLEAR;					//清屏
			xQueueSend(g_queue_oled,&oled,100);
			
			sprintf((char *)buf_order,"Mod Time Success");
			oled.ctrl=OLED_CTRL_SHOW_STRING;
			oled.x=0;
			oled.y=3;
			oled.str=buf_order;
			oled.font_size=16;
			xReturn = xQueueSend(g_queue_oled,&oled,100);
			if(xReturn != pdPASS) dgb_printf_safe("[app_task_rtc] xQueueSend oled string error code is %d\r\n",xReturn);
			vTaskDelay(1000);
			memset(&oled,0,sizeof(oled));
			
			xEventGroupSetBits(g_event_group_beep,0x04);//蜂鸣器
			xEventGroupSetBits(g_event_group_led,0x01); //LED
		}
		else
		{
			dgb_printf_safe("Input error\r\n");
		}
	}
	
}  

static void app_task_show(void* pvParameters)
{
	BaseType_t 	xReturn=pdFALSE;
	char 		flash_buf[64];
	for(;;)
	{
		xReturn = xQueueReceive( g_queue_rtc,&flash_buf,portMAX_DELAY);
		if(strstr((const char *)flash_buf,"show all"))
		{
			for(i=0;i<100;i++)
			{
				//获取存储的记录
				flash_read_record(save_buf,i);
				
				//检查记录是否存在换行符号，不存在则不打印输出
				if(strstr(save_buf,"\n")==0)
					break;		
				
				//打印记录
				dgb_printf_safe("record[%d]  %s",i,save_buf);
				
			}
			
			//如果i等于0，代表没有一条记录
			if(i==0)
			{
				dgb_printf_safe("There is no record\r\n");
			}
		}
		else if(strstr((char *)flash_buf,"clear"))
		{
			//扇区擦除
			flash_erase_record();
			
			dgb_printf_safe("Empty all data records successfully\r\n");
			
			//清零记录计数值
			rec_cnt=0;
											
		}
		else 
			continue;
		memset(flash_buf,0,sizeof(flash_buf));
		
	}
}


/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to pvPortMalloc() fails.
	pvPortMalloc() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call xQueueReceive() with a block time
	specified, or call vTaskDelay()).  If the application makes use of the
	vTaskDelete() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	printf("!![vApplicationStackOverflowHook] %s is StackOverflow\r\n", pcTaskName);
	for( ;; );
}


void vApplicationTickHook( void )
{

}
