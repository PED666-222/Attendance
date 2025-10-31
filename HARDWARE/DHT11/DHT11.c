#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "DHT11.h"


static GPIO_InitTypeDef GPIO_InitStruct;

void DHT_Init(uint32_t dht_flag)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG,ENABLE);
	
	
	if(dht_flag == 0)
	{
		GPIO_InitStruct.GPIO_Mode=GPIO_Mode_OUT;
	}
	else
	{
		GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IN;
	}
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_9;
	GPIO_InitStruct.GPIO_Speed=GPIO_High_Speed;
	GPIO_InitStruct.GPIO_OType=GPIO_OType_OD;//开漏
	GPIO_InitStruct.GPIO_PuPd=GPIO_PuPd_NOPULL;//配置上拉和下拉电阻，目前为无
	GPIO_Init(GPIOG,&GPIO_InitStruct);
	
	PGout(9)=1;
}


int Get_DHT_Data(uint32_t *arr)
{
	uint32_t time=0;
	int32_t i=0;
	int32_t j=0;
	uint8_t d=0;
	uint8_t check_num=0;
	DHT_Init(0);
	PGout(9)=0;
	Delay_ms(18);
	PGout(9)=1;
	Delay_us(30);
	DHT_Init(1);//设置PG9的引脚为输入模式
	time=0;
	while(PGin(9))//等待低电平
	{
		time++;
		Delay_us(1);
		if(time >= 4000)
			return -1;
	}
	time=0;
	while(PGin(9)==0)//低电平80us
	{
		time++;
		Delay_us(1);
		if(time>=90)
			return -2;
	}
	time=0;
	while(PGin(9)) //高电平80us
	{
		time++;
		Delay_us(1);
		if(time>=90)
			return -2;
	}
	for(i=0;i<5;i++)
	{
		d=0;
		//读取一个字节
		for(j=7;j>=0;j--)
		{
			//读取一位数据
			time=0;
			while(PGin(9)==0)//低电平80us
			{
				time++;
				Delay_us(1);
				if(time>=100)
					return -4;
			}
			Delay_us(40);//延时范围要在28us~70us，防止太短检测到的是数据0的高电平，太长检测到的是数据1的低电平
			if(PGin(9))//PGin(9)过了40us后还是高点平数据为就是1
			{
				d|=1<<j;
				time=0;
				while(PGin(9))//等待高电平结束
				{
					time++;
					Delay_us(1);
					
					if(time >= 100)
						return -5;
				}
			}
		}
		arr[i]=d;
	}
	Delay_us(50);//忽略低电平
	check_num=arr[0]+arr[1]+arr[2]+arr[3];
	if(check_num != arr[4])//计算校验和，检测是否相等
	{
		return -6;
	}
	return 0;
}
