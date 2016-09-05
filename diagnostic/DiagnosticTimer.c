/**
  ******************************************************************************
  * @file    timer.c
  * @author  Soundtech Application Team
  * @version V1.0.0
  * @date    14-Jan-2015
  * @brief   提供定时器功能。
  * <h2><center>&copy; COPYRIGHT 2015 Soundtech</center></h2>
  ******************************************************************************  
  */ 
  
/* Includes ------------------------------------------------------------------*/
#include "DiagnosticTimer.h"

/** @defgroup Private_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup Private_Defines
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup Private_Macros
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup Private_Variables
  * @{
  */ 
static uint32_t SystemTickCount=0;
/**
  * @}
  */ 


/** @defgroup Private_FunctionPrototypes
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup Private_Functions
  * @{
  */ 

/**
  * @brief  Init Timer.
  * @param  None.
  * @retval None.
  */
void DiagTimer_Init(void)
{
	/* Setup SysTick Timer for 1 msec interrupts.
	 ------------------------------------------
		1. The SysTick_Config() function is a CMSIS function which configure:
		   - The SysTick Reload register with value passed as function parameter.
		   - Configure the SysTick IRQ priority to the lowest value (0x0F).
		   - Reset the SysTick Counter register.
		   - Configure the SysTick Counter clock source to be Core Clock Source (HCLK).
		   - Enable the SysTick Interrupt.
		   - Start the SysTick Counter.

		2. You can change the SysTick Clock source to be HCLK_Div8 by calling the
		   SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8) just after the
		   SysTick_Config() function call. The SysTick_CLKSourceConfig() is defined
		   inside the misc.c file.

		3. You can change the SysTick IRQ priority by calling the
		   NVIC_SetPriority(SysTick_IRQn,...) just after the SysTick_Config() function 
		   call. The NVIC_SetPriority() is defined inside the core_cm3.h file.

		4. To adjust the SysTick time base, use the following formula:
		                        
		     Reload Value = SysTick Counter Clock (Hz) x  Desired Time base (s)

		   - Reload Value is the parameter to be passed for SysTick_Config() function
		   - Reload Value should not exceed 0xFFFFFF
	*/
	#if 0
	if (SysTick_Config(SystemCoreClock / 1000))
	{ 
		/* Capture error */ 
		while (1);
	}
	#endif
}

/**
  * @brief  Sets a timer. timer must not equ 0 because 0 means timer is stop.
  * @param  STimer pointer to timer value.
  * @param  TimeLength - timer period.
  * @retval None.
  */
void DiagTimer_Set(DiagTimer *STimer, uint32_t TimeLength)
{
	STimer->TimerCounter = SystemTickCount + TimeLength;
	STimer->valid = TRUE;
	//if(STimer->TimerCounter == 0)	STimer->TimerCounter = 1; //not set timer to 0 for timer is running
}

/**
  * @brief  Stop timer.
  * @param  STimer pointer to timer value.
  * @retval None.
  */
void DiagTimer_Stop(DiagTimer *STimer)
{
	STimer->valid = FALSE;
}

/**
  * @brief  Checks whether given timer has stopped.
  * @param  STimer is timer value.
  * @retval TRUE if timer is stopped.
  */
bool DiagTimer_HasStopped(DiagTimer *STimer)
{
	return (STimer->valid == FALSE); 
}

/**
  * @brief  Checks whether given timer has expired
  *        With timer tick at 1ms maximum timer period is 10000000 ticks
  *        When *STimer is set (SoftTimer-*STimer) has a min value of 0xFFF0BDBF
  *            and will be more than this while the timer hasn't expired
  *        When the timer expires
  *                (SoftTimer-*STimer)==0
  *            and (SoftTimer-*STimer)<=7FFF FFFF for the next 60 hours
  * @param  STimer pointer to timer value.
  * @retval TRUE if timer has expired or timer is stopped, otherwise FALSE.
  */
bool DiagTimer_HasExpired(DiagTimer *STimer)
{
	if(STimer->valid == TRUE)
	{
		if(STimer->TimerCounter == 0)
		{
			STimer->valid = FALSE;
			return TRUE;
		}
		else if((SystemTickCount - STimer->TimerCounter) <= 0x7fffffff)
		{
			STimer->TimerCounter = 0;	//set timer to stop
			STimer->valid = FALSE;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}
}

/**
  * @brief  don't wait if  given timer has expired.
  * @param  STimer pointer to timer value.
  * @retval None.
  */
void DiagTimer_WaitExpired(DiagTimer *STimer)
{
	while(1)
	{
		if(Timer_HasExpired(STimer))
			break;
		//WatchDog_Feed();
	}
}

/**
  * @brief  wait untill  given timer has expired.
  * @param  wait ms.
  * @retval None.
  */
void DiagTimer_DelayMs(uint32_t ms)
{
	uint32_t timer;
	
	Timer_Set (&timer, ms);
	while (!Timer_HasExpired (&timer))
	{
		//__ASM volatile ("nop");
		//WatchDog_Feed();
	}
}

void DiagTimer_DelayUs(uint32_t us)
{
	uint32_t i=0;

	for(i=0;i<us;i++)
	{
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
	}
}
/**
  * @brief  get the system work time.
  * @param  None.
  * @retval None.
  */
uint32_t DiagTimer_GetTickCount(void)
{
	return (SystemTickCount);
}

/**
  * @brief  1ms interval.
  * @param  None.
  * @retval None.
  */
void DiagTimer_ISR_Proc(void)
{
	SystemTickCount++;
}

/**
  * @}
  */ 
  
/******************* (C) COPYRIGHT 2015 Soundtech *****END OF FILE****/

