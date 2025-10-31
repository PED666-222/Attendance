#include "allhead.h"
/****************************************************************
	1.电容指纹模块手册的阅读、数据交换
	2.串口编程
	3.按键检测
*说  明:	
	1.按键功能
		1)按键1按下，则执行添加指纹工作
		2)按键2按下，则执行刷指纹工作
		3)按键3按下，则执行获取所有用户的总数
		4)按键4按下，则执行删除所有用户
		
	2.电容指纹模块光圈
		1)操作状态时，蓝色
		2)操作确认
			.成功，绿色
			.失败，红色
*/

//USART串口接收长度以及标志位
volatile uint8_t USART2_STA  = 0;

//电容指纹模块触摸中断事件
volatile uint32_t g_fpm_touch_event=0;



static volatile uint8_t  g_usart2_buf[64];
static volatile uint32_t g_usart2_cnt=0;
static volatile uint32_t g_usart2_event=0;


static GPIO_InitTypeDef   GPIO_InitStructure;
static EXTI_InitTypeDef   EXTI_InitStructure;
static NVIC_InitTypeDef   NVIC_InitStructure;
static TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;


//模块LED灯控制协议
static const uint8_t fpm_led_blue[16]  = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x03,0x01,0x01,0x00,0x00,0x49};
static const uint8_t fpm_led_red[16]   = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x02,0x04,0x04,0x02,0x00,0x50};
static const uint8_t fpm_led_green[16] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x02,0x02,0x02,0x02,0x00,0x4C};


static void Tim3_Init(void)
{
	//使能tim3的硬件时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	
	//配置tim3的分频值、计数值
	//tim3硬件时钟=84MHz/8400=10000Hz，就是进行10000次计数，就是1秒时间的到达

	TIM_TimeBaseStructure.TIM_Period = 10000/1000-1; //计数值0 -> 999就是1毫秒时间的到达
	TIM_TimeBaseStructure.TIM_Prescaler = 8400-1;	//预分频值8400
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;	//时钟分频，当前是没有的，不需要进行配置
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	
	//配置tim3的中断
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE);
	
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIM3,ENABLE);
}


static uint16_t check_sum(uint8_t *buf,uint32_t len)
{
	uint16_t sum=0;
	
	uint8_t *p=buf;
	
	while(len--)
		sum+=*p++;
	
	return sum;
}	


static void usart2_printf_recv_buf(void)
{
	uint32_t i;
	
	for(i=0;i<USART2_STA;i++)
	{
		dgb_printf_safe("%02X ",g_usart2_buf[i]);
	
	}
	
	dgb_printf_safe("\r\n");
}


/**
	* @brief	USART2串口发送函数
	* @param 	length: 发送数组长度
	* @param	FPM383C_Databuffer[]: 需要发送的功能协议数组，在上面已有定义
	* @return None
	*/
void fpm_send_data(int32_t length,uint8_t FPM383C_Databuffer[])
{
	int32_t i;
	
	for(i = 0;i<length;i++)
	{
		USART_SendData(USART2,FPM383C_Databuffer[i]);
		
		while(!USART_GetFlagStatus(USART2,USART_FLAG_TXE));
	}
	
	g_usart2_event = 0;
	g_usart2_cnt=0;
}


/**
	* @brief	发送睡眠指令，只有发送睡眠指令后，TOUCHOUT才会变成低电平
	* @param 	None
	* @return None
	*/
void fpm_sleep(void)
{
	//模块睡眠协议
	uint8_t buf[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x33,0x00,0x37};
	
	fpm_send_data(12,buf);
}



uint8_t fpm_ctrl_led(uint8_t color)
{

	if(color == FPM_LED_RED)
		fpm_send_data(16,(uint8_t *)fpm_led_red);
	
	if(color == FPM_LED_GREEN)
		fpm_send_data(16,(uint8_t *)fpm_led_green);	
	
	if(color == FPM_LED_BLUE)
		fpm_send_data(16,(uint8_t *)fpm_led_blue);	
	
	return 0;
}


/* 清空指纹 */
int32_t fpm_empty(void)
{
    uint32_t timeout=4000;	
	

	uint8_t buf[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x0D,0x00,0x11};	
	
	fpm_send_data(12,buf);
	
	while(!g_usart2_event && (--timeout))
	{
		Delay_ms(1);
	}
	
	usart2_printf_recv_buf();	
	
	
	if(g_usart2_buf[0]!=0xEF && g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("fpm_empty检查硬件连接\r\n");

		return -1;
	}	
	
	if(!timeout)
	{
		dgb_printf_safe("清空指纹:超时\r\n");
		
		return -1;
	}
	
	/*  打印出指纹模块工作出错原因 */
	if(g_usart2_buf[9] != 0x00)
	{
	
		dgb_printf_safe("清空指纹：失败 %02X\r\n",g_usart2_buf[9] );
		dgb_printf_safe("请不要按压指纹模块\r\n");
		return -1;
		
	}	

	return 0;
	
}


/* 自动注册指纹 */
int32_t fpm_enroll_auto(uint16_t id)
{
    uint8_t buf[17];
	uint16_t cs=0;
    uint32_t timeout=4000;
	
    /* 包头：2字节 */
    buf[0]=0xEF;buf[1]=0x01;
    
    /* 设备地址：4字节 */    
    buf[2]=0xFF;buf[3]=0xFF;buf[4]=0xFF;buf[5]=0xFF;
	
    /* 包标识：1字节 */ 
    buf[6]=0x01;
    
    /* 包长度：2字节 */
    buf[7]=0x00;buf[8]=0x08;
    
    /* 指令码：1字节 */
    buf[9]=0x31;
    
    /* ID两字节：2字节 */
    buf[10]=(id>>8)&0xFF;buf[11]=id&0xFF;
    
    /* 录入次数：1字节 */
    buf[12]=1;
    
    /* 参数：2字节 */
    buf[13]=0x00;
    
    buf[14]=0x3F;
    
    cs=check_sum(&buf[6],9);
    
    buf[15]=(cs>>8)&0xFF;
    buf[16]=(cs)&0xFF;   
	
	
	fpm_send_data(17,buf);
	
	
	while(!g_usart2_event && (--timeout))
	{
		Delay_ms(1);
	}
	
	usart2_printf_recv_buf();
	
	if(g_usart2_buf[0]!=0xEF && g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("自动注册指纹:数据包异常！\r\n");
		dgb_printf_safe("1.检查硬件连接\r\n");
		dgb_printf_safe("2.检查是否有按压指纹模块\r\n");		
		return -1;
	}	
	
	if(!timeout)
	{
		dgb_printf_safe("自动注册指纹:超时\r\n");
		
		return -1;
	}
	
	/*  打印出指纹模块工作出错原因 */
	if(g_usart2_buf[9] != 0x00)
	{
	
		dgb_printf_safe("自动注册指纹：失败 %02X\r\n",g_usart2_buf[9] );
		
		return -1;
		
	}	
	
	
	/* 正确返回 */ 	
	return 0;
}


/* 自动认证指纹 */
int32_t fpm_idenify_auto(uint16_t *id)
{
    uint8_t buf[17];
	uint16_t cs=0;
    uint32_t timeout=4000;
    
    /* 包头：2字节 */
    buf[0]=0xEF;buf[1]=0x01;
    
    /* 设备地址：4字节 */    
    buf[2]=0xFF;buf[3]=0xFF;buf[4]=0xFF;buf[5]=0xFF;
	
    /* 包标识：1字节 */ 
    buf[6]=0x01;
    
    /* 包长度：2字节 */
    buf[7]=0x00;buf[8]=0x08;
    
    /* 指令码：1字节 */
    buf[9]=0x32;
    
    /* 分数等级：1字节 */
    buf[10]=80;
    
    /* ID两字节：2字节 */
    buf[11]=(*id>>8)&0xFF;buf[12]=*id&0xFF;
    
    /* 参数：2字节 */
    buf[13]=0x00;
    
    buf[14]=0x07;
    
    cs=check_sum(&buf[6],9);
    
    buf[15]=(cs>>8)&0xFF;
    buf[16]=(cs)&0xFF;    
    

	fpm_send_data(17,buf);
	
	
	while(!g_usart2_event && (--timeout))
	{
		Delay_ms(1);
	}
	
	usart2_printf_recv_buf();
	
	if(g_usart2_buf[0]!=0xEF && g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("自动验证指纹:数据包异常！\r\n");
		dgb_printf_safe("1.检查硬件连接\r\n");
		dgb_printf_safe("2.检查是否有按压指纹模块\r\n");		
		return -1;
	}	
	
	if(!timeout)
	{
		dgb_printf_safe("自动验证指纹:超时\r\n");
		return -1;
	}
	
	/*  打印出指纹模块工作出错原因 */
	if(g_usart2_buf[9] != 0x00)
	{
		dgb_printf_safe("指纹验证失败--%02X\r\n",g_usart2_buf[9] );
		
		return -1;
	}
	
	/* 分数值异常 */
	if(((g_usart2_buf[13]<<8)|g_usart2_buf[14])==0xFFFF)
	{
		dgb_printf_safe("自动验证指纹：分数值异常，可能没有存在该指纹\r\n");
		
		return -1;		
	}		
	
	/* 成功匹配到指纹的ID值 */
	*id = (g_usart2_buf[11]<<8)|g_usart2_buf[12];
	
	/* 正确返回 */ 	
	return 0;
}

/* 获取有效指纹总数 */
int32_t fpm_id_total(uint16_t *total)
{
    uint32_t timeout=4000;	
	
	uint8_t buf[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x1D,0x00,0x21};	
	
	fpm_send_data(12,buf);
	
	while(!g_usart2_event && (--timeout))
	{
		Delay_ms(1);
	}
	
	usart2_printf_recv_buf();	
	
	if(g_usart2_buf[0]!=0xEF && g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("fpm_id_total请检查硬件连接\r\n");

		return -1;
	}	
	
	if(!timeout)
	{
		dgb_printf_safe("获取有效指纹总数:超时\r\n");
		
		return -1;
	}
	
	/*  打印出指纹模块工作出错原因 */
	if(g_usart2_buf[9] != 0x00)
	{
		dgb_printf_safe("获取有效指纹总数：失败 %02X\r\n",g_usart2_buf[9] );
		dgb_printf_safe("请不要按压指纹模块\r\n");
		return -1;
	}	
	
	*total=(g_usart2_buf[10]<<8)|g_usart2_buf[11];

	return 0;
}


/**
	* @brief  外部中断引脚初始化函数
	* @param  None
	* @return None
	*/
void fpm_touch_init()
{
	/* 打开端口E F的硬件时钟（就是对硬件供电），默认状态下，所有时钟都是关闭 */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);
	
	
	/* 打开系统配置syscfg时钟（就是对硬件供电），默认状态下，所有时钟都是关闭 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG,ENABLE);	


	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_6;//指定0号引脚
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN;//引脚工作在输入模式
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz;//速度越高，响应时间越短，但是功耗就越高，电磁干扰也越高
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_DOWN;//使能下拉电阻
	GPIO_Init(GPIOE,&GPIO_InitStructure);

	/* 将外部中断连接到指定的引脚，特别说明：引脚编号决定了使用哪个外部中断 */
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource6);


	/* 配置外部中断0 */
	EXTI_InitStructure.EXTI_Line = EXTI_Line6; //指定外部中断6
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;//工作在中断模式
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;//上升沿的触发
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;//允许外部中断6工作
	EXTI_Init(&EXTI_InitStructure);	
	
	
	NVIC_InitStructure.NVIC_IRQChannel =EXTI9_5_IRQn ;//中断号
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x03;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x03;//响应优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;//中断打开
	NVIC_Init(&NVIC_InitStructure);
		
}



/**
	* @brief	在main函数里调用此函数以初始化FPM383C指纹模块
	* @param 	None
	* @return None
	*/
void fpm_init(void)
{
	Tim3_Init();
	Usart2_Init(57600);
	fpm_touch_init();
	fpm_sleep();
}


const char *fpm_error_code_auto_enroll(uint8_t error_code)
{
	const char *p;
	
	switch(error_code)
	{
		case 0x00:p="自动注册成功";
		break;
		
		case 0x01:p="自动执行失败";
		break;	

		case 0x07:p="生成特征失败";
		break;		

		case 0x0A:p="合并模板";
		break;		

		case 0x0B:p="ID号超出范围";
		break;	
		
		case 0x1F:p="指纹库已满";
		break;		
	
		case 0x22:p="指纹模板非空";
		break;	
		
		case 0x25:p="录入次数设置错误";
		break;	

		case 0x26:p="超时";
		break;	

		case 0x27:p="指纹已存在";
		break;	
		
		case 0x31:p="功能与加密等级不匹配";
		break;

		default :
			p="模块返回确认码有误";break;
	}

	return p;
}


/**
	* @brief	外部中断服务函数
	* @param 	None
	* @return None
	*/
void EXTI9_5_IRQHandler(void)
{
	
	if (EXTI_GetITStatus(EXTI_Line6) != RESET)
	{
		g_fpm_touch_event=1;
		
		EXTI_ClearITPendingBit(EXTI_Line6);
	}
}


/**
	* @brief	串口2中断服务函数
	* @param 	None
	* @return None
	*/
void USART2_IRQHandler(void)
{
	static uint8_t d=0;
	
	//检测是否接收到数据
	if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
	{
		d=USART_ReceiveData(USART2);
		
		if(g_usart2_cnt< sizeof(g_usart2_buf))
			g_usart2_buf[g_usart2_cnt++]=d;
		
		//清空标志位，可以响应新的中断请求
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
}


void TIM3_IRQHandler(void)
{
	static uint32_t cnt=0;

	//检测标志位
	if(TIM_GetITStatus(TIM3,TIM_IT_Update) == SET)
	{
		if(cnt!=g_usart2_cnt)
		{
			cnt=g_usart2_cnt;
		}
		else if(cnt && (cnt == g_usart2_cnt))
		{
			g_usart2_event=1;
			cnt=0;
		}
		
		//清空标志位
		TIM_ClearITPendingBit(TIM3,TIM_IT_Update);
	}
}

