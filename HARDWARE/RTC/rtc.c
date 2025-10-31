#include "allhead.h"

static RTC_InitTypeDef  RTC_InitStructure;
static RTC_TimeTypeDef  RTC_TimeStructure;
static RTC_DateTypeDef  RTC_DateStructure;
static EXTI_InitTypeDef EXTI_InitStructure;
static NVIC_InitTypeDef NVIC_InitStructure;

extern EventGroupHandle_t g_event_group_rtc;

void rtc_init(void)
{
	 /* Enable the PWR clock ,使能电源时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

	/* Allow access to RTC ，允许访问RTC*/
	PWR_BackupAccessCmd(ENABLE);
	

	/* 使能LSI*/
	RCC_LSICmd(ENABLE);
	
	/* 检查该LSI是否有效*/  
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);

	/* 选择LSI作为RTC的硬件时钟源*/
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);


	/* ck_spre(1Hz) = RTCCLK /(uwAsynchPrediv + 1)/(uwSynchPrediv + 1)*/
	/* Enable the RTC Clock ，使能RTC时钟*/
	RCC_RTCCLKCmd(ENABLE);

	/* Wait for RTC APB registers synchronisation ，等待RTC相关寄存器就绪*/
	RTC_WaitForSynchro();
 

	/* Configure the RTC data register and RTC prescaler，配置RTC数据寄存器与RTC的分频值 */
	RTC_InitStructure.RTC_AsynchPrediv = 0x7F;				//异步分频系数
	RTC_InitStructure.RTC_SynchPrediv = 0xF9;				//同步分频系数
	RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;	//24小时格式
	RTC_Init(&RTC_InitStructure);

	if(RTC_ReadBackupRegister(RTC_BKP_DR0)==0x1234)
	{
		/* Set the date: Monday 2023/10/16 */
		RTC_DateStructure.RTC_Year = 0x23;
		RTC_DateStructure.RTC_Month = RTC_Month_October;
		RTC_DateStructure.RTC_Date = 0x16;
		RTC_DateStructure.RTC_WeekDay = RTC_Weekday_Monday;
		RTC_SetDate(RTC_Format_BCD, &RTC_DateStructure);

		/* Set the time to 14h 56mn 00s PM  */
		RTC_TimeStructure.RTC_H12     = RTC_H12_PM;
		RTC_TimeStructure.RTC_Hours   = 0x20;
		RTC_TimeStructure.RTC_Minutes = 0x09;
		RTC_TimeStructure.RTC_Seconds = 0x30; 
		RTC_SetTime(RTC_Format_BCD, &RTC_TimeStructure); 	
	
	}

	
	//关闭唤醒功能
	RTC_WakeUpCmd(DISABLE);
	
	//为唤醒功能选择RTC配置好的时钟源
	RTC_WakeUpClockConfig(RTC_WakeUpClock_CK_SPRE_16bits);
	
	//设置唤醒计数值为自动重载，写入值默认是0
	RTC_SetWakeUpCounter(1-1);
	
	//清除RTC唤醒中断标志
	RTC_ClearITPendingBit(RTC_IT_WUT);
	
	//使能RTC唤醒中断
	RTC_ITConfig(RTC_IT_WUT, ENABLE);

	//使能唤醒功能
	RTC_WakeUpCmd(ENABLE);

	/* Configure EXTI Line22，配置外部中断控制线22 */
	EXTI_InitStructure.EXTI_Line = EXTI_Line22;			//当前使用外部中断控制线22
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;		//中断模式
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;		//上升沿触发中断 
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;			//使能外部中断控制线22
	EXTI_Init(&EXTI_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = RTC_WKUP_IRQn;		//允许RTC唤醒中断触发
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY;	//抢占优先级为0x3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 5;		//响应优先级为0x3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//使能
	NVIC_Init(&NVIC_InitStructure);
	
	//向备份寄存器0写入0x1234，告诉用户当前RTC时间与日期已经设置过，不需要重复设置
	RTC_WriteBackupRegister(RTC_BKP_DR0,0x1234);
}

void RTC_WKUP_IRQHandler(void)
{
	BaseType_t  xHigherPriorityTaskWoken = pdFALSE;	
	//检测是否有唤醒中断出现
	if(RTC_GetITStatus(RTC_IT_WUT)==SET)
	{
		xEventGroupSetBitsFromISR(g_event_group_rtc,0x01,&xHigherPriorityTaskWoken);
		//添加用户代码
     	//清空标志位
		RTC_ClearITPendingBit(RTC_IT_WUT);
		EXTI_ClearITPendingBit(EXTI_Line22);
	}
	if(xHigherPriorityTaskWoken)
	{
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}	
}
