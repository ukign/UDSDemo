/**
  ******************************************************************************
  * @file    NMLayerTypeDefines.h
  * @author  Soundtech Application Team
  * @version V1.0.0
  * @date    28-Sep-2015
  * @brief   CAN network layer refrence type defines 
  * <h2><center>&copy; COPYRIGHT 2015 Soundtech</center></h2>
  ******************************************************************************  
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __NMLAYER_TYPE_DEFINES_H
#define __NMLAYER_TYPE_DEFINES_H

#ifdef __cplusplus
 extern "C" {
#endif 

/** @defgroup Exported_TypesDefinitions
  * @{
  */ 


typedef enum{
	N_OK,//sender and receiver
	N_TIMEOUT_A,//sender and receiver
	N_TIMEOUT_Bs,//sender only
	N_TIMEOUT_Cr,//receiver only
	N_WRONG_SN,//receiver only
	N_INVALID_FS,//sneder only
	N_UNEXP_PDU,//receiver only
	N_WFT_OVRN,//
	N_BUFFER_OVFLW,//sender only
	N_ERROR,//sender and receiver
}N_Result;

typedef enum{
	DIAGNOSTIC,
	REMOTE_DIAGNOSTICS,
}MType;

typedef enum{
	PHYSICAL,
	FUNCTIONAL,
}N_TAtype;

typedef enum{
	STmin,
	BS,
}Parameter;

typedef enum{
	N_CHANGE_OK,			//sender and receiver
	N_RX_ON,				//receiver only
	N_WRONG_PARAMETER,	//sender and receiver
	N_WRONG_VALUE,		//sender and receiver
}Result_ChangeParameter;

typedef enum{
	CTS,	 //continue to send
	WT,		//wait
	OVFLW,	//overflow
}FS_Type;

typedef enum
{
	NWL_IDLE,
	NWL_TRANSMITTING,
	NWL_RECIVING,
	NWL_WAIT,
}NWL_Status;

typedef enum{
	SF = 0,	//single frame
	FF = 1,	//first frame
	CF = 2,	//consecutive frame
	FC = 3,	//flow control
}N_PCIType;

typedef enum{
	TX_IDLE,
	TX_WAIT_FF_CONF,
	TX_WAIT_FC,
	TX_WAIT_CF_REQ,
	TX_WAIT_CF_CONF,
}TransmissionStep;

typedef enum{
	RX_IDLE,
	RX_WAIT_FC_REQ,
	RX_WAIT_FC_CONF,
	RX_WAIT_CF,
}RecivingStep;

typedef enum{
	HALF_DUPLEX,
	FULL_DUPLEX,
}DuplexMode;

typedef struct{
	uint16_t N_As;
	uint16_t N_Ar;
	uint16_t N_Bs;
	uint16_t N_Br;
	uint16_t N_Cs;
	uint16_t N_Cr;
	bool FF_ConfirmedOnSender;		//when transmission
	bool FC_RecivedOnSender;		//when transmission
	bool CF_RequestedOnSender;	//when transmission
	bool CF_ConfirmedOnSender;		//when transmission
	bool FC_RxBeforeFFOnSender;	//when transmission
	bool FC_RequestedOnReciver;	//when reciving
	bool FC_ConfirmedOnReciver;	//when reciving
	bool CF_RecivedOnReciver;		//when reciving
}TimePeriodParam;

typedef struct{
	MType Mtype;
	uint8_t N_SA;
	uint8_t N_TA;
	N_TAtype N_TAtype;
	uint8_t N_AE;
}AddressFormat;

typedef union{
	struct{
		MType Mtype;
		N_TAtype N_TAtype;
		uint8_t N_SA;
		uint8_t N_TA;
		uint8_t N_AE;
		uint32_t ID;
		uint8_t DLC;
		uint8_t RTR;
		uint8_t IDE;
		bool valid;
		uint8_t data7;
		uint8_t data6;
		uint8_t data5;
		uint8_t data4;
		uint8_t data3;
		uint8_t STmin;		//STmin ,data2
		uint8_t FF_DL_LOW;	//BS ,FF_DL_LOW ,data1
		uint8_t SF_DL:4;		//SF_DL ,FF_DL_HIGH ,TxParam.SN ,FS
		N_PCIType N_PciType:4;
	}N_PDU;
	
	struct{
		MType Mtype;
		N_TAtype N_TAtype;
		uint8_t N_SA;/* 接收ID号高位 */
		uint8_t N_TA;/* 接收ID号低位 */
		uint8_t N_AE;
		uint32_t ID;
		uint8_t DLC;
		uint8_t RTR;
		uint8_t IDE;
		bool valid;
		uint8_t data7;
		uint8_t data6;
		uint8_t data5;
		uint8_t data4;
		uint8_t data3;
		uint8_t data2;
		uint8_t data1;
		uint8_t data0;
	}CanData;
}NetworkFrame;

typedef struct{
	FS_Type FS_Type;
	uint8_t BlockSize;
	uint8_t CompletedNumberInBlock;
	uint8_t STmin;
	uint8_t SN;
	uint16_t TotalDataNumber;
	uint16_t CompletedDataNumber;
	uint16_t BuffSize;
}CommuParam;

typedef enum{
	CONFIRM,
	FF_INDICATION,
	INDICATION,
}NetWorkNotificationType;

typedef struct{
	NetWorkNotificationType NotificationType;
	MType Mtype;
	uint8_t N_SA;
	uint8_t N_TA;
	N_TAtype N_TAtype;
	uint8_t N_AE;
	uint8_t *MessageData;
	uint16_t length;
	N_Result N_Resut;
	bool valid;
}NetworkNotification;

#ifndef NULL
	#define NULL (void*)(0)
#endif

/**
  * @}
  */
#ifdef __cplusplus
}
#endif


#endif /* __NMLAYER_TYPE_DEFINES_H */


/******************* (C) COPYRIGHT 2015 Soundtech *****END OF FILE****/
