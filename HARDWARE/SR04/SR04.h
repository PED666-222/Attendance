#ifndef _SR04_H_
#define _SR04_H_

extern void Sr04_Init(void);
extern uint32_t Get_Sr04_Distance(void);
extern void TIM13_Init(void);
extern	void Tim13_Set_Pwm(uint32_t freq,uint32_t duty);
#endif
