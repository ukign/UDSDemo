
#ifndef __TIMER_H
#define __TIMER_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "Cpu.h"

/** @defgroup Exported_Macros
  * @{
  */ 
#ifndef bool
#define bool unsigned char
#endif

typedef struct{
	uint32_t TimerCounter;
	bool valid;
}DiagTimer;

/**
  * @}
  */ 

/** @defgroup Exported_Functions
  * @{
  */ 
void DiagTimer_Init(void);
void DiagTimer_Set(DiagTimer *STimer, uint32_t TimeLength);
bool DiagTimer_HasStopped(DiagTimer *STimer);
bool DiagTimer_HasExpired (DiagTimer *STimer);
void DiagTimer_Stop(DiagTimer *STimer);
void DiagTimer_DelayMs (uint32_t ms);
void DiagTimer_DelayUs(uint32_t us);
void DiagTimer_WaitExpired (DiagTimer *STimer);
uint32_t DiagTimer_GetTickCount(void);
void DiagTimer_ISR_Proc(void);
/**
  * @}
  */ 

#ifdef __cplusplus
}
#endif


#endif /* __TIMER_H */

