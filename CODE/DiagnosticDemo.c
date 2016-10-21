/** ###################################################################
**     Filename  : DiagnosticDemo.c
**     Project   : DiagnosticDemo
**     Processor : MC9S12G128VLH
**     Version   : Driver 01.14
**     Compiler  : CodeWarrior HC12 C Compiler
**     Date/Time : 2016/9/1, 9:16
**     Abstract  :
**         Main module.
**         This module contains user's application code.
**     Settings  :
**     Contents  :
**         No public methods
**
** ###################################################################*/
/* MODULE DiagnosticDemo */

/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
#include "CAN1.h"
#include "TJA1043_Can_Sel.h"
#include "TJA1043_EN.h"
#include "RTI1.h"
/* Include shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
#include "diagnostic.h"

void ForceReset(){
	union{
		void (*vector)(void);
		byte c[2];
	}softReset;

	softReset.c[0]=*(byte *)0xFFFE;
	softReset.c[1]=*(byte *)0xFFFF;
	softReset.vector(); 
}

void SystemReset(EcuResetType resetType)
{
	if(resetType == 1)//硬件复位
	{
		ForceReset();
	}
	else if(resetType == 2)//key-off-on复位
	{
		//ForceReset();不支持复位类型时，不作处理即可
	}
	else if(resetType == 3)//软件复位
	{
		ForceReset();
	}
}

void CommulicatonControl(CommulicationType type, communicationParam param)
{
	bool rxenable = ((type & 0x02) == 0x00);//允许接收
	bool txenable = ((type & 0x01) == 0x00);//允许发送
	bool CtrlNmMessage = ((param & 0x02) == 0x02);//控制网络管理消息
	bool CtrlAppMessage = ((param & 0x01) == 0x01);//控制应用报文
	
	if(CtrlNmMessage)
	{
		//NMSetCommulicationEnable(txenable, rxenable);

	}

	if(CtrlAppMessage)
	{
		//AppSetMsgEnable(txenable, rxenable);
	}
}

uint32_t HD10SeedToKeyLevel1(uint32_t seed)
{
	return 0x12345678;//针对某一个请求的安全算法
}

byte SendFrame(VUINT32 ID, byte *array, byte length, byte priority , uint8_t rtr ,uint8_t ide) 
{
	byte i;

	CANTBSEL=CANTFLG;
	if (CANTBSEL==0) 
	{
		return 0;  
	}

	if(ide == 1)//extend frame
	{
		*((VUINT8 *) (&CANTXIDR0)) = (ID >> 21); 
		*((VUINT8 *) (&CANTXIDR1)) = 0x18 | (((ID >>13) & 0xE0) |  ((ID >> 15) & 0x07)); 
		*((VUINT8 *) (&CANTXIDR2)) = (ID >> 7); 
		*((VUINT8 *) (&CANTXIDR3)) = ((ID << 1) & 0xFE) | (rtr & 0x01); 
	}
	else//standard frame
	{
		*((VUINT8 *) (&CANTXIDR0)) = (ID >> 3); 
		*((VUINT8 *) (&CANTXIDR1)) = ((ID << 5) & 0xE0) | ((rtr & 0x01) << 4) | ((ide & 0x01) << 3); 
	}

	for (i=0;i<length;i++)
	{
		*(&CANTXDSR0+i)=array[i];
	}   
	CANTXDLR=length;  
	CANTXTBPR=priority; 

	CANTFLG=CANTBSEL;    //发送缓冲区相应TXE位写1清除该位来通知MSCAN发送数据  
	return CANTBSEL;
}

uint16_t TestF180Data = 0xF180;
uint8_t TestCurrentTemp = 0x40;
uint16_t CurrentSpeed = 0x0895;
uint8_t CurrentVoltage = 135;

uint8_t IO9826_MixMtrCtrl;
uint8_t IoControl_9826(uint8_t ctrl, uint8_t param) {
	if(ctrl == 0) {
		IO9826_MixMtrCtrl = 2;
	} 
	else if(ctrl == 1) 
	{
		IO9826_MixMtrCtrl = 2;
	} else if(ctrl == 2) 
	{

	}else if(ctrl == 3) 
	{
		if(param == 0) 
		{
			IO9826_MixMtrCtrl = 0;
		} 
		else if(param == 1)
		{
			IO9826_MixMtrCtrl = 1;
		}
	}
	return IO9826_MixMtrCtrl;
}

uint8_t NMGetLimpHome(void)
{
	 if (1)
	 {
		return 2;//failed
	 }
	 else
	 {
		return 0;//passed
	 }
}

#pragma CODE_SEG __NEAR_SEG NON_BANKED
ISR(Can_Rx_Interrupt)
{
	// NOTE: The routine should include the following actions to obtain
	//       correct functionality of the hardware.
	//
	//      The ISR is invoked by RXF flag. The RXF flag is cleared
	//      if a "1" is written to the flag in CANRFLG register.
	//      Example: CANRFLG = CANRFLG_RXF_MASK;

	uint8_t tmp;
	uint8_t canData[8];
	uint8_t canDlc;
	uint8_t canRtr;
	uint8_t canIde;
	uint32_t canId;

	if (CANRFLG_RXF)
	{
		tmp = CANRXIDR1;
		if (tmp & (1<<3))
		{ /*1 Extended format (29 bit)*/
			canId = CANRXIDR0;
			canId <<= 21;
			tmp = ((tmp>>5)<<3) | (tmp&7);
			canId |= ((uint32_t)tmp)<<15;
			canId |= ((uint32_t)CANRXIDR2)<<7;
			tmp = CANRXIDR3;
			canRtr = tmp & 1;
			canId |= (tmp>>1);
		}
		else
		{ /*0 Standard format (11 bit)*/
			canRtr = tmp & 1;
			canId = CANRXIDR0;
			canId <<= 3;
			canId |= ((tmp >> 5) & 0x07);
		}
		canDlc = CANRXDLR_DLC;
		if (canDlc == 0) 
		{
			CANRFLG_RXF = 1;
			return ;
		}
		
		canIde = (CANRXIDR0 >> 3) & 0x01;
		for (tmp = 0; tmp < CANRXDLR_DLC; ++tmp)
		{
			canData[tmp] = *(&CANRXDSR0 + tmp);

		}
		Diagnostic_RxFrame(canId , canData , canIde , canDlc , canRtr);
		CANRFLG_RXF = 1;
	}
}

ISR(RTI_Interrupt)
{
	// NOTE: The routine should include the following actions to obtain
	//       correct functionality of the hardware.
	//
	//      The ISR is invoked by RTIF flag. The RTIF flag is cleared
	//      if a "1" is written to the flag in CPMUFLG register.
	//      Example: CPMUFLG = 128;
	
	CPMUFLG_RTIF = 1; /*clear real time interrupt flag*/
	Diagnostic_1msTimer();
}

#pragma CODE_SEG DEFAULT

void Diagnostic_Init_Config(void)
{
	Diagnostic_Init(0x721, 0x728 , 0x7DF, 0xA00 , 1024 , SendFrame,0x0032,0x00C8);
   	//********************************** service 10*****************************************//
   	//Diagnostic_Set2ndReqAndResID(0x18DA19F9, 0x18DAF919 , 0x18DBFFF9);
   
	InitSetSessionSupportAndSecurityAccess(TRUE,0x10,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO);
	InitSetSessionControlParam(TRUE , TRUE , TRUE , FALSE , FALSE , TRUE);   
   
	//********************************** service 27*****************************************//
    
	InitAddSecurityAlgorithm(LEVEL_ONE,HD10SeedToKeyLevel1,0x01,0x02, NULL ,3 , 10000, SUB_PROGRAM | SUB_EXTENDED,4);
	InitFactorySecuriyAlgorithm();
	//InitAddSecurityAlgorithm(LEVEL_THREE,HD10SeedToKey,0x07,0x08, NULL ,3 , 10000, SUB_PROGRAM);
	InitSetSessionSupportAndSecurityAccess(TRUE,0x27,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO,LEVEL_UNSUPPORT,LEVEL_UNSUPPORT,LEVEL_UNSUPPORT);
	 
	//********************************** service 3E*****************************************//
	  
	InitSetSessionSupportAndSecurityAccess(TRUE,0x3E,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO);
	InitSetTesterPresentSupress(TRUE);
	  //********************************** service 11*****************************************//
	 
	 
	InitSetSessionSupportAndSecurityAccess(TRUE,0x11,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO);
	InitSetSysResetParam(TRUE , FALSE , FALSE , FALSE , FALSE , SystemReset , TRUE);	 
	 
	   //********************************** service 28*****************************************//
	   
	InitSetSessionSupportAndSecurityAccess(TRUE,0x28,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO);
	InitSetCommControlParam(TRUE , TRUE , TRUE , TRUE , TRUE , TRUE , TRUE , CommulicatonControl , TRUE);  
	 //********************************** service 85*****************************************//
	 
	InitSetSessionSupportAndSecurityAccess(TRUE,0x85,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO);
	InitSetDTCControlSupress(TRUE);
	  //********************************** service 22  2E  2F*****************************************//

	InitAddDID(0xF180, 2 , NULL ,  EEPROM_DID , NULL , READONLY , 0 ,TRUE);//只能读的DID,存储在EEPROM中，可在下线配置时写
	InitAddDID(0xF190, 17, NULL , EEPROM_DID , NULL , READWRITE , 0 , FALSE);//可读写的DID，存储在EEPROM中
	InitAddDID(0x9816,1 , &TestCurrentTemp , REALTIME_DID , NULL , READONLY , 0 ,FALSE);//只读DID，实时数据(如报文数据等)。
	InitAddDID(0x9823,1 , &IO9826_MixMtrCtrl , IO_DID , IoControl_9826 , READWRITE , 0 ,FALSE);//可读写的IO DID，既可以通过22服务读取，也可以通过2F服务控制
	InitAddDID(0x9826,1 , NULL , IO_DID , IoControl_9826 , WRITEONLY , 0 , FALSE);//只能通过2F控制的IO DID。
	#if 1
	InitSetCanDriverVersionDID(0x0A01);
	InitSetCanNMVersionDID(0x0A02);
	InitSetCanDiagnosticVersionDID(0x0A03);
	InitSetCanDataBaseVersionDID(0x0A04);
	InitSetCurrentSessionDID(0xF186);
	#endif
	 
	InitSetSessionSupportAndSecurityAccess(TRUE,0x22,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO,LEVEL_ZERO);

	InitSetSessionSupportAndSecurityAccess(TRUE,0x2F,LEVEL_UNSUPPORT,LEVEL_UNSUPPORT,LEVEL_ONE,LEVEL_UNSUPPORT,LEVEL_UNSUPPORT,LEVEL_UNSUPPORT);

	InitSetSessionSupportAndSecurityAccess(TRUE,0x2E,LEVEL_UNSUPPORT,LEVEL_ONE,LEVEL_ONE,LEVEL_UNSUPPORT,LEVEL_UNSUPPORT,LEVEL_UNSUPPORT);

	//********************************** service 19*****************************************//
 	InitSetSessionSupportAndSecurityAccess(TRUE,0x19,LEVEL_ZERO,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO,LEVEL_UNSUPPORT,LEVEL_ZERO);

	InitSetDTCAvailiableMask(0x09);             //  支持的故障状态掩码
	InitAddDTC(0x910223,NMGetLimpHome,10, 1 ,LEVEL_C);			//limphome
	//********************************** service 14*****************************************//

	InitSetSessionSupportAndSecurityAccess(TRUE,0x14,LEVEL_ZERO,LEVEL_UNSUPPORT,LEVEL_ZERO,LEVEL_ZERO,LEVEL_UNSUPPORT,LEVEL_ZERO);	 
	InitAddDTCGroup(0x00FFFFFF); 

	//**********************************snaptshot*****************************************//
	InitAddDTCSnapShot(0x01 , 0x9102 , &CurrentSpeed , 2);
	InitAddDTCSnapShot(0x01 , 0x9103 , &CurrentVoltage , 1);
	InitAddDTCSnapShot(0x02 , 0x9105 , &CurrentVoltage , 3);
	InitAddDTCSnapShot(0x04 , 0x9106 , &CurrentVoltage , 1);
	InitAddDTCSnapShot(0x04 , 0x9108 , &CurrentVoltage , 1);

	InitSetAgingCounterRecordNumber(3);
	InitSetAgedCounterRecordNumber(4);
	InitSetOccurrenceCounterRecordNumber(1);
	InitSetPendingCounterRecordNumber(2);
	
	Diagnostic_LoadAllData();
	
	Diagnostic_SetNLParam(70, 150, 150, 70, 70, 70, 8, 20, 0xFF);
}

void main(void)
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/
	TJA1043_Can_Sel_PutVal(1);
  	TJA1043_EN_PutVal(1);
	Diagnostic_Init_Config();
  /* Write your code here */
	while(1)
	{
		Diagnostic_Proc();
	}
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END DiagnosticDemo */
/*
** ###################################################################
**
**     This file was created by Processor Expert 3.02 [04.44]
**     for the Freescale HCS12 series of microcontrollers.
**
** ###################################################################
*/
