
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
	
}

void DiagTimer_Set(DiagTimer *STimer, uint32_t TimeLength)
{
	STimer->TimerCounter = SystemTickCount + TimeLength;
	STimer->valid = TRUE;
}


void DiagTimer_Stop(DiagTimer *STimer)
{
	STimer->valid = FALSE;
}


bool DiagTimer_HasStopped(DiagTimer *STimer)
{
	return (STimer->valid == FALSE); 
}


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


void DiagTimer_WaitExpired(DiagTimer *STimer)
{
	while(1)
	{
		if(Timer_HasExpired(STimer))
			break;
		//WatchDog_Feed();
	}
}


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


