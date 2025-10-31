#ifndef __BEEP_H__
#define __BEEP_H__

extern void beep_init(void);//≥ı ºªØ
extern void beep_on(void);
extern void beep_off(void);

#define BEEP(x)		PFout(8)=(x)


#endif
