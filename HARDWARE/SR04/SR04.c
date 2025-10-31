#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "SR04.h"

static GPIO_InitTypeDef  GPIO_InitStructure;
static GPIO_InitTypeDef  GPIO_InitStruct;
TIM_OCInitTypeDef  		 TIM_OCInitStructure;
TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

void TIM13_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM13, ENABLE);
	
		/*定时器的基本配置，用于配置定时器的输出脉冲的频率为100Hz */
	TIM_TimeBaseStructure.TIM_Period = (40000/100)-1;					//设置定时脉冲的频率
	TIM_TimeBaseStructure.TIM_Prescaler = 2100-1;						//第一次分频，简称为预分频
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;				//第二次分频,当前实现1分频，也就是不分频
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	
	TIM_TimeBaseInit(TIM13, &TIM_TimeBaseStructure);

	/* 配置PF8 引脚为复用模式 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;					//第8根引脚
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;				//设置复用模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;				//推挽模式，增加驱动电流
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;			//设置IO的速度为100MHz，频率越高性能越好，频率越低，功耗越低
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;			//不需要上拉电阻
	GPIO_Init(GPIOF, &GPIO_InitStructure);	
	
	GPIO_PinAFConfig(GPIOF, GPIO_PinSource8, GPIO_AF_TIM13);
	
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;//让输出通道在PW1模式下工作
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;//允许输出脉冲
	TIM_OCInitStructure.TIM_Pulse = 200;						 //比较值，即占空比50%	
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;	 //输出的有效状态为高电平	
	TIM_OC1Init(TIM13, &TIM_OCInitStructure);
	
	TIM_Cmd(TIM13, ENABLE);
	Tim13_Set_Pwm(15,0);
}

void Tim13_Set_Pwm(uint32_t freq,uint32_t duty)
{
	uint32_t cmp=0;
	/* 关闭TIM13 */
	//TIM_Cmd(TIM13, DISABLE);
	
    /*定时器的基本配置，用于配置定时器的输出脉冲的频率为 freq Hz */
    TIM_TimeBaseStructure.TIM_Period = (400000/freq)-1; //设置定时脉冲的频率
    TIM_TimeBaseStructure.TIM_Prescaler = 2100-1; //第一次分频，简称为预分频
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM13, &TIM_TimeBaseStructure);
	
    cmp = (TIM_TimeBaseStructure.TIM_Period+1) * duty/100;
    TIM_SetCompare1(TIM13,duty);	
	
	/* 使能TIM13 */
	//TIM_Cmd(TIM13, ENABLE);	
}

void Sr04_Init(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);
	
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_6;
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed=GPIO_Speed_2MHz;
	GPIO_InitStruct.GPIO_OType=GPIO_OType_PP;//推挽
	GPIO_InitStruct.GPIO_PuPd=GPIO_PuPd_NOPULL;//配置上拉和下拉电阻，目前为无
	GPIO_Init(GPIOB,&GPIO_InitStruct);
	
	PBout(6)=0;
	
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_6;
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IN;
	GPIO_Init(GPIOE,&GPIO_InitStruct);	
	
	
}

uint32_t Get_Sr04_Distance(void)
{
	uint32_t t=0;
	PBout(6)=1;
	Delay_us(10);
	PBout(6)=0;
	while(PEin(6)==0)
	{
		t++;
		Delay_us(1);
		
		//如果超时，就返回一个错误码
		if(t>=1000000)
			return 0xFFFFFFFF;
	}
	
	t=0;
	while(PEin(6))
	{
		t++;
		Delay_us(9);//声音传播9us，就传播了3mm
	}
	
	return 3*t/2;
}

