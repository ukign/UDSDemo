#define _NETWORKLAYER_C
#include "DiagnosticTimer.h"
#include "NetworkLayer.h"

NetworkNotification IndicationList[MAX_INDICATION_NUMBER];
uint8_t IndicationInIndex;
uint8_t IndicationOutIndex;
NetworkFrame RxFrameBuff[MAX_BUFF_NUMBER];// = {0};
uint8_t RxInIndex;
uint8_t RxOutIndex;
NetworkFrame TxFrameBuff[MAX_BUFF_NUMBER];// = {0};
uint8_t TxInIndex;
uint8_t TxOutIndex;

static N_Result m_N_Result;
static NWL_Status m_NetworkStatus = NWL_IDLE;
static DuplexMode m_DuplexMode;
static TimePeriodParam m_TimePeriod;
static TransmissionStep m_TxStep;
static RecivingStep m_RxStep;
static AddressFormat m_AddressFormat;
static uint8_t* CFDataPionter;
static CommuParam TxParam;
static CommuParam RxParam;
static uint8_t NetworkDataBufRx[MAX_DTCDATA_BUF];			//接收数据的缓存
uint8_t FrameFillData;
static uint32_t m_PyhReqID;
static uint32_t m_FunReqID;
static uint32_t m_ResponseID;
static uint32_t m_PyhReqID1;
static uint32_t m_FunReqID1;
static uint32_t m_ResponseID1;
static uint32_t m_CurrResponseID;


static uint8_t RxDataBuff[7];

/************private function prototype*********/
void NetworkLayer_SendSF(uint8_t length, uint8_t *data);
void NetworkLayer_SendFF(uint16_t length, uint8_t *data);
void NetworkLayer_SendCF(void);
void NetworkLayer_SendFC(void);
void NetworkLayer_TxEnd(void);
void NetworkLayer_RxEnd(void);
#if 0
void NetworkLayer_AddToTxBuff(NetworkFrame* txFrame);
#endif
//void NetworkLayer_TxFrame(NetworkFrame txFrame);
void NetworkLayer_RxProc(void);
void NetworkLayer_TxProc(void);
void  NetworkLayer_MainProc(void);
void NetworkLayer_RxSF(NetworkFrame RxFrame);
void NetworkLayer_RxFF(NetworkFrame RxFrame);
void NetworkLayer_RxCF(NetworkFrame RxFrame);
void NetworkLayer_RxFC(NetworkFrame RxFrame);
void N_USData_indication(N_PCIType PciType,MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, uint8_t* MessageData, uint16_t Length, N_Result N_Result);
void N_USData_FF_indication(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, uint16_t Length);
void N_USData_confirm(N_PCIType PciType,MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, N_Result N_Result);
bool IsRxBuffEmpty(void);
bool IsRxBuffFull(void);
bool IsTxBuffEmpty(void);
bool IsTxBuffFull(void);
bool IsIndicationListFull(void);
/************private function prototype*********/

/***********time peroid timer define************/
static DiagTimer SendTimer;
static DiagTimer ReciveTimer;
static DiagTimer CFRequestTimer;
static SendCANFun NetworkSend;

/***********time peroid timer define************/

void NetworkLayer_InitParam(uint32_t PyhReqID,uint32_t FunReqID, uint32_t ResponseID,SendCANFun sendFun)
{
	//int n = malloc(sizeof(CommuParam));
	FrameFillData = 0;//set the default value to zero
	m_PyhReqID = PyhReqID;
	m_FunReqID = FunReqID;
	m_ResponseID = ResponseID;
	RxInIndex = 0;
	RxOutIndex = 0;
	TxInIndex = 0;
	TxOutIndex = 0;
	IndicationInIndex = 0;
	IndicationOutIndex = 0;
	
	m_TimePeriod.N_As = 25;
	m_TimePeriod.N_Ar = 25;
	m_TimePeriod.N_Br = 50;
	m_TimePeriod.N_Bs = 75;
	m_TimePeriod.N_Cs = 25;
	m_TimePeriod.N_Cr = 150;

	TxParam.FS_Type = CTS;
	TxParam.BlockSize = 20;
	TxParam.CompletedNumberInBlock = 0;
	TxParam.STmin = 100;
	TxParam.SN = 0;
	TxParam.TotalDataNumber = 100;
	TxParam.CompletedDataNumber = 0;

	RxParam.FS_Type = CTS;
	RxParam.BlockSize = 0;
	RxParam.CompletedNumberInBlock = 0;
	#ifdef BOOTLOADER
	RxParam.STmin = 0x01;
	#else
	RxParam.STmin = 0x0A;
	#endif
	RxParam.SN = 0;
	RxParam.TotalDataNumber = 0;
	RxParam.CompletedDataNumber = 0;	
	RxParam.BuffSize = MAX_DTCDATA_BUF;

	m_DuplexMode = HALF_DUPLEX;
	NetworkSend= sendFun;
}

void NetworkLayer_SetSecondID(uint32_t PyhReqID,uint32_t FunReqID, uint32_t ResponseID)
{
	m_PyhReqID1 = PyhReqID;
	m_FunReqID1 = FunReqID;
	m_ResponseID1 = ResponseID;
}

void NetworkLayer_SetParam(uint8_t TimeAs, uint8_t TimeBs, uint8_t TimeCr, uint8_t TimeAr, uint8_t TimeBr, uint8_t TimeCs, 
	uint8_t Bs, uint8_t m_STmin, DuplexMode nDuplex ,  MType Mtype , uint8_t N_SA , uint8_t N_TA , N_TAtype N_TAtype , uint8_t N_AE , uint8_t FillData)
{
	m_TimePeriod.N_As = TimeAs;
	m_TimePeriod.N_Ar = TimeAr;
	m_TimePeriod.N_Br = TimeBr;
	m_TimePeriod.N_Bs = TimeBs;
	m_TimePeriod.N_Cs = TimeCs;
	m_TimePeriod.N_Cr = TimeCr;
	m_DuplexMode = nDuplex;
	RxParam.STmin = m_STmin;
	RxParam.BlockSize = Bs;
	m_AddressFormat.Mtype = Mtype;
	m_AddressFormat.N_SA = N_SA;
	m_AddressFormat.N_TA = N_TA;
	m_AddressFormat.N_TAtype = N_TAtype;
	m_AddressFormat.N_AE = N_AE;
	FrameFillData = FillData;
}

bool IsRxBuffEmpty(void)
{
	return (RxInIndex == RxOutIndex);
}

bool IsRxBuffFull(void)
{
	return ((RxInIndex ==  (MAX_BUFF_NUMBER - 1) && RxOutIndex == 0) || ((RxInIndex + 1) == RxOutIndex));
}

bool IsTxBuffEmpty(void)
{
	return (TxInIndex == TxOutIndex);
}

bool IsTxBuffFull(void)
{
	return ((TxInIndex ==  (MAX_BUFF_NUMBER - 1) && TxOutIndex == 0) || ((TxInIndex + 1) == TxOutIndex));
}

#if 0
void NetworkLayer_AddToTxBuff(NetworkFrame* txFrame)
{
	#if 1
	TxFrameBuff[TxInIndex] = *txFrame;
	if(TxInIndex >= MAX_BUFF_NUMBER - 1)
	{
		(TxInIndex = 0);
	}
	else
	{
		(TxInIndex++);
	}
	#else
	uint8_t i;
	for(i = 0; i < MAX_BUFF_NUMBER; i++)
	{
		if(TxFrameBuff[i].N_PDU.valid == FALSE)
		{
			TxFrameBuff[i] = txFrame;
			return;
		}
	}
	#endif
}
#endif

void NetworkLayer_SendSF(uint8_t length, uint8_t *data)
{
	#if 0
	NetworkFrame tempFrame;
	uint8_t i;
	
	if(length  <= 7)//SF length must <= 7
	{
		for(i = 0; i < 7; i++)
		{
			if(i >= length)
			{
				*(&tempFrame.CanData.data7 + (6-i)) = FrameFillData;
			}
			else
			{
				*(&tempFrame.CanData.data7 + (6-i)) = *(data + i);
			}
		}
		tempFrame.N_PDU.N_PciType = SF;
		tempFrame.N_PDU.SF_DL = length;

		NetworkLayer_AddToTxBuff(&tempFrame);	
	}
	#else
	uint8_t i;
	
	if(length  <= 7)//SF length must <= 7
	{
		if(!IsTxBuffFull())
		{
			for(i = 0; i < 7; i++)
			{
				if(i >= length)
				{
					*(&TxFrameBuff[TxInIndex].CanData.data7 + (6-i)) = FrameFillData;
				}
				else
				{
					*(&TxFrameBuff[TxInIndex].CanData.data7 + (6-i)) = *(data + i);
				}
			}
			TxFrameBuff[TxInIndex].N_PDU.N_PciType = SF;
			TxFrameBuff[TxInIndex].N_PDU.SF_DL = length;
			(TxInIndex >= MAX_BUFF_NUMBER - 1) ? (TxInIndex = 0) : (TxInIndex++);
		}
		else
		{
			//printf("send SF but tx buf full\r\n");
		}
	}
	#endif
}

void NetworkLayer_SendFF(uint16_t length, uint8_t *data)
{
	#if 0
	NetworkFrame tempFrame;
	if(length > 7 && length < 0xFFF)
	{
		tempFrame.N_PDU.N_PciType = FF;
		tempFrame.N_PDU.SF_DL = length >> 8;			//length high 4 bits
		tempFrame.N_PDU.FF_DL_LOW = length & 0xFF;	//length low nibble
		
		tempFrame.CanData.data2 = *data;
		tempFrame.CanData.data3 = *(data + 1);
		tempFrame.CanData.data4 = *(data + 2);
		tempFrame.CanData.data5 = *(data + 3);
		tempFrame.CanData.data6 = *(data + 4);
		tempFrame.CanData.data7 = *(data + 5);
		
		TxParam.SN = 0;
		TxParam.TotalDataNumber = length;
		TxParam.CompletedDataNumber = 6;
		CFDataPionter = data + 6;
		NetworkLayer_AddToTxBuff(&tempFrame);
	}
	#else
	if(length > 7 && length < 0xFFF)
	{
		TxFrameBuff[TxInIndex].N_PDU.N_PciType = FF;
		TxFrameBuff[TxInIndex].N_PDU.SF_DL = length >> 8;			//length high 4 bits
		TxFrameBuff[TxInIndex].N_PDU.FF_DL_LOW = length & 0xFF;	//length low nibble
		
		TxFrameBuff[TxInIndex].CanData.data2 = *data;
		TxFrameBuff[TxInIndex].CanData.data3 = *(data + 1);
		TxFrameBuff[TxInIndex].CanData.data4 = *(data + 2);
		TxFrameBuff[TxInIndex].CanData.data5 = *(data + 3);
		TxFrameBuff[TxInIndex].CanData.data6 = *(data + 4);
		TxFrameBuff[TxInIndex].CanData.data7 = *(data + 5);
		
		TxParam.SN = 0;
		TxParam.TotalDataNumber = length;
		TxParam.CompletedDataNumber = 6;
		CFDataPionter = data + 6;
		(TxInIndex >= MAX_BUFF_NUMBER - 1) ? (TxInIndex = 0) : (TxInIndex++);
	}
	#endif
}

void NetworkLayer_SendCF(void)
{
	#if 0
	uint8_t i;
	uint8_t length;
	NetworkFrame tempFrame;
	
	if(TxParam.CompletedDataNumber + 7 <= TxParam.TotalDataNumber)
	{
		length = 7;
	}
	else
	{
		length = TxParam.TotalDataNumber - TxParam.CompletedDataNumber;						
	}
	
	tempFrame.N_PDU.N_PciType = CF;
	tempFrame.N_PDU.SF_DL = 	++TxParam.SN;			//length high 4 bits	
	
	for(i = 0; i < 7; i++)
	{
		if(i < length)
		{
			*(&(tempFrame.CanData.data7) + (6-i)) = *(CFDataPionter + i);
		}
		else
		{
			*(&(tempFrame.CanData.data7) + (6-i)) = FrameFillData;
		}
	}

	TxParam.CompletedDataNumber += length;
	CFDataPionter += length;
	
	NetworkLayer_AddToTxBuff(&tempFrame);
	#else
	uint8_t i;
	uint8_t length;
	
	if(TxParam.CompletedDataNumber + 7 <= TxParam.TotalDataNumber)
	{
		length = 7;
	}
	else
	{
		length = TxParam.TotalDataNumber - TxParam.CompletedDataNumber;						
	}
	
	TxFrameBuff[TxInIndex].N_PDU.N_PciType = CF;
	TxFrameBuff[TxInIndex].N_PDU.SF_DL = 	++TxParam.SN;			//length high 4 bits	
	
	for(i = 0; i < 7; i++)
	{
		if(i < length)
		{
			*(&(TxFrameBuff[TxInIndex].CanData.data7) + (6-i)) = *(CFDataPionter + i);
		}
		else
		{
			*(&(TxFrameBuff[TxInIndex].CanData.data7) + (6-i)) = FrameFillData;
		}
	}

	TxParam.CompletedDataNumber += length;
	CFDataPionter += length;
	(TxInIndex >= MAX_BUFF_NUMBER - 1) ? (TxInIndex = 0) : (TxInIndex++);
	#endif
}

void NetworkLayer_SendFC(void)
{
	#if 0
	NetworkFrame tempFrame;
	uint8_t i;
	
	tempFrame.N_PDU.N_PciType = FC;
	tempFrame.N_PDU.SF_DL = RxParam.FS_Type;
	tempFrame.N_PDU.FF_DL_LOW = RxParam.BlockSize;
	tempFrame.N_PDU.STmin = RxParam.STmin;
	tempFrame.CanData.data7= FrameFillData;
	tempFrame.CanData.data6= FrameFillData;
	tempFrame.CanData.data5= FrameFillData;
	tempFrame.CanData.data4= FrameFillData;
	tempFrame.CanData.data3= FrameFillData;
	
	NetworkLayer_AddToTxBuff(&tempFrame);
	#else	
	TxFrameBuff[TxInIndex].N_PDU.N_PciType = FC;
	TxFrameBuff[TxInIndex].N_PDU.SF_DL = RxParam.FS_Type;
	TxFrameBuff[TxInIndex].N_PDU.FF_DL_LOW = RxParam.BlockSize;
	TxFrameBuff[TxInIndex].N_PDU.STmin = RxParam.STmin;
	TxFrameBuff[TxInIndex].CanData.data7= FrameFillData;
	TxFrameBuff[TxInIndex].CanData.data6= FrameFillData;
	TxFrameBuff[TxInIndex].CanData.data5= FrameFillData;
	TxFrameBuff[TxInIndex].CanData.data4= FrameFillData;
	TxFrameBuff[TxInIndex].CanData.data3= FrameFillData;
	(TxInIndex >= MAX_BUFF_NUMBER - 1) ? (TxInIndex = 0) : (TxInIndex++);
	#endif
}

void NetworkLayer_RxSF(NetworkFrame RxFrame)
{
	if((RxFrame.N_PDU.SF_DL >= 1) && (RxFrame.N_PDU.SF_DL <= 7))//data length filter
	{
		if(RxFrame.N_PDU.DLC == 8)//DLC filter
		{
			if(m_DuplexMode == FULL_DUPLEX)
			{
				if(m_NetworkStatus == NWL_RECIVING)
				{
					m_N_Result = N_UNEXP_PDU;
					m_NetworkStatus = NWL_IDLE;//new start of reception
				}
				else if(m_NetworkStatus == NWL_TRANSMITTING || m_NetworkStatus == NWL_IDLE)
				{
					m_N_Result = N_OK;
					m_NetworkStatus = NWL_IDLE;//only when Full-duplex
				}
				
				RxDataBuff[0] = RxFrame.CanData.data1;
				RxDataBuff[1] = RxFrame.CanData.data2;
				RxDataBuff[2] = RxFrame.CanData.data3;
				RxDataBuff[3] = RxFrame.CanData.data4;
				RxDataBuff[4] = RxFrame.CanData.data5;
				RxDataBuff[5] = RxFrame.CanData.data6;
				RxDataBuff[6] = RxFrame.CanData.data7;
				N_USData_indication(SF ,  RxFrame.CanData.Mtype , RxFrame.CanData.N_SA , RxFrame.CanData.N_TA , RxFrame.CanData.N_TAtype , RxFrame.CanData.N_AE , RxDataBuff, RxFrame.N_PDU.SF_DL ,  m_N_Result);
			}
			else if(m_DuplexMode == HALF_DUPLEX)//p504 use half duplex
			{
				if(m_NetworkStatus == NWL_RECIVING)
				{
					m_N_Result = N_UNEXP_PDU;
					m_NetworkStatus = NWL_IDLE;//new start of reception
					RxDataBuff[0] = RxFrame.CanData.data1;
					RxDataBuff[1] = RxFrame.CanData.data2;
					RxDataBuff[2] = RxFrame.CanData.data3;
					RxDataBuff[3] = RxFrame.CanData.data4;
					RxDataBuff[4] = RxFrame.CanData.data5;
					RxDataBuff[5] = RxFrame.CanData.data6;
					RxDataBuff[6] = RxFrame.CanData.data7;
					N_USData_indication(SF ,  RxFrame.CanData.Mtype , RxFrame.CanData.N_SA , RxFrame.CanData.N_TA , RxFrame.CanData.N_TAtype , RxFrame.CanData.N_AE , RxDataBuff, RxFrame.N_PDU.SF_DL ,  m_N_Result);
				}
				else if(m_NetworkStatus == NWL_IDLE)
				{
					m_N_Result = N_OK;
					RxDataBuff[0] = RxFrame.CanData.data1;
					RxDataBuff[1] = RxFrame.CanData.data2;
					RxDataBuff[2] = RxFrame.CanData.data3;
					RxDataBuff[3] = RxFrame.CanData.data4;
					RxDataBuff[4] = RxFrame.CanData.data5;
					RxDataBuff[5] = RxFrame.CanData.data6;
					RxDataBuff[6] = RxFrame.CanData.data7;
					N_USData_indication(SF ,  RxFrame.CanData.Mtype , RxFrame.CanData.N_SA , RxFrame.CanData.N_TA , RxFrame.CanData.N_TAtype , RxFrame.CanData.N_AE , RxDataBuff, RxFrame.N_PDU.SF_DL ,  m_N_Result);
				}
				else if(m_NetworkStatus == NWL_TRANSMITTING)
				{
					//printf("half duplex mode,ignore SF when transmiting\r\n");
				}
			}
		}
		else
		{
			//printk("SF invalid DLC:%d\r\n", RxFrame.N_PDU.DLC);
		}
	}
	else
	{
		//printk("SF invalid len %d\r\n",RxFrame.N_PDU.SF_DL);
	}
}

void NetworkLayer_RxFF(NetworkFrame RxFrame)
{
	if(RxFrame.CanData.N_TAtype == PHYSICAL)
	{
		uint16_t FF_DL = (RxFrame.N_PDU.SF_DL << 8) | (RxFrame.N_PDU.FF_DL_LOW);
		
		if(FF_DL >= 8)
		{
			if(RxFrame.N_PDU.DLC == 8)
			{
				RxDataBuff[0] = RxFrame.CanData.data2;
				RxDataBuff[1] = RxFrame.CanData.data3;
				RxDataBuff[2] = RxFrame.CanData.data4;
				RxDataBuff[3] = RxFrame.CanData.data5;
				RxDataBuff[4] = RxFrame.CanData.data6;
				RxDataBuff[5] = RxFrame.CanData.data7;
				if(m_DuplexMode == FULL_DUPLEX)
				{
					if(m_NetworkStatus == NWL_IDLE)
					{
						m_N_Result = N_OK;
						m_NetworkStatus = NWL_RECIVING;
					}
					else if(m_NetworkStatus == NWL_RECIVING)
					{
						m_N_Result = N_UNEXP_PDU;
						m_NetworkStatus = NWL_RECIVING;//new start of reception
					}
					else if(m_NetworkStatus == NWL_TRANSMITTING)
					{
						m_N_Result = N_OK;
						m_NetworkStatus = NWL_RECIVING;//only when Full-duplex
					}

					if(RxParam.BuffSize < FF_DL)
					{
						//printk("Rx FF error size overflow FF_DL:%d,buffsize:%d\r\n",FF_DL,RxParam.BuffSize);
						RxParam.FS_Type = OVFLW;
						NetworkLayer_SendFC();
					}
					else
					{
						RxParam.SN = 0x0;
						RxParam.FS_Type = CTS;
						N_USData_FF_indication(RxFrame.N_PDU.Mtype , RxFrame.N_PDU.N_SA , RxFrame.N_PDU.N_TA , RxFrame.N_PDU.N_TAtype , RxFrame.N_PDU.N_AE , FF_DL);
						N_USData_indication(FF , RxFrame.N_PDU.Mtype , RxFrame.N_PDU.N_SA , RxFrame.N_PDU.N_TA , RxFrame.N_PDU.N_TAtype , RxFrame.N_PDU.N_AE , RxDataBuff , FF_DL ,  m_N_Result);
					}
				}
				else if(m_DuplexMode == HALF_DUPLEX)//p504 use half duplex
				{
					if(m_NetworkStatus == NWL_IDLE)
					{
						m_N_Result = N_OK;
						m_NetworkStatus = NWL_RECIVING;
						if(RxParam.BuffSize < FF_DL)
						{
							//printk("Rx FF error size overflow FF_DL:%d,buffsize:%d\r\n",FF_DL,RxParam.BuffSize);
							RxParam.FS_Type = OVFLW;
							NetworkLayer_SendFC();
						}
						else
						{
							RxParam.SN = 0x0;
							RxParam.FS_Type = CTS;
							N_USData_FF_indication(RxFrame.N_PDU.Mtype , RxFrame.N_PDU.N_SA , RxFrame.N_PDU.N_TA , RxFrame.N_PDU.N_TAtype , RxFrame.N_PDU.N_AE , FF_DL);
							N_USData_indication(FF , RxFrame.N_PDU.Mtype , RxFrame.N_PDU.N_SA , RxFrame.N_PDU.N_TA , RxFrame.N_PDU.N_TAtype , RxFrame.N_PDU.N_AE , RxDataBuff , FF_DL ,  m_N_Result);
						}
					}
					else if(m_NetworkStatus == NWL_RECIVING)
					{
						m_N_Result = N_UNEXP_PDU;
						m_NetworkStatus = NWL_RECIVING;//new start of reception
						if(RxParam.BuffSize < FF_DL)
						{
							//printf("FF size overflow :%d expect:%d\r\n",FF_DL,RxParam.BuffSize);
							RxParam.FS_Type = OVFLW;
							NetworkLayer_SendFC();
						}
						else
						{
							RxParam.SN = 0x0;
							RxParam.FS_Type = CTS;
							N_USData_FF_indication(RxFrame.N_PDU.Mtype , RxFrame.N_PDU.N_SA , RxFrame.N_PDU.N_TA , RxFrame.N_PDU.N_TAtype , RxFrame.N_PDU.N_AE , FF_DL);
							N_USData_indication(FF , RxFrame.N_PDU.Mtype , RxFrame.N_PDU.N_SA , RxFrame.N_PDU.N_TA , RxFrame.N_PDU.N_TAtype , RxFrame.N_PDU.N_AE , RxDataBuff , FF_DL ,  m_N_Result);
						}
					}
					else if(m_NetworkStatus == NWL_TRANSMITTING)
					{
						//printf("half duplex ,ignore FF when TX\r\n");
					}
				}
			}
			else
			{
				//printf("FF invalid DLC : %d\r\n",RxFrame.N_PDU.DLC);
			}
		}
		else
		{
			//printf("FF invalid len %d\r\n",FF_DL);
		}
	}
	else if(RxFrame.CanData.N_TAtype == FUNCTIONAL)
	{
		//printf("ignore functional  FF\r\n");
	}
}

void NetworkLayer_RxCF(NetworkFrame RxFrame)
{
	if(RxFrame.N_PDU.DLC == 8)
	{
		if(m_NetworkStatus == NWL_RECIVING)
		{
			uint8_t CurrFrameLength = 0;
			if(((RxParam.SN + 1) & 0x0F) == (RxFrame.N_PDU.SF_DL & 0x0F))
			{
				m_N_Result = N_OK;
				if(RxParam.CompletedDataNumber < RxParam.TotalDataNumber)
				{
					RxParam.SN = RxFrame.N_PDU.SF_DL;
					RxDataBuff[0] = RxFrame.CanData.data1;
					RxDataBuff[1] = RxFrame.CanData.data2;
					RxDataBuff[2] = RxFrame.CanData.data3;
					RxDataBuff[3] = RxFrame.CanData.data4;
					RxDataBuff[4] = RxFrame.CanData.data5;
					RxDataBuff[5] = RxFrame.CanData.data6;
					RxDataBuff[6] = RxFrame.CanData.data7;
					
					if(RxParam.CompletedDataNumber + 7 < RxParam.TotalDataNumber)
					{
						CurrFrameLength = 7;
					}
					else
					{
						CurrFrameLength = RxParam.TotalDataNumber - RxParam.CompletedDataNumber;
					}

					N_USData_indication(CF, RxFrame.N_PDU.Mtype , RxFrame.N_PDU.N_SA , RxFrame.N_PDU.N_TA , RxFrame.N_PDU.N_TAtype , RxFrame.N_PDU.N_AE , RxDataBuff , CurrFrameLength ,  m_N_Result);
					
				}
				else 
				{
					//printf("RX end,ignore CF\r\n");
				}
			}
			else
			{
				//printf("CF error SN expect %d but %d\r\n", RxParam.SN + 1,RxFrame.N_PDU.SF_DL);
				m_N_Result = N_WRONG_SN;
				N_USData_indication(CF, RxFrame.N_PDU.Mtype , RxFrame.N_PDU.N_SA , RxFrame.N_PDU.N_TA , RxFrame.N_PDU.N_TAtype , RxFrame.N_PDU.N_AE , RxDataBuff , CurrFrameLength ,  m_N_Result);
			}
			
		}
		else
		{
			//printf("not in RX ,ignore CF : %d\r\n");
			//printf("not in RX ,ignore CF : %d\r\n",RxFrame.N_PDU.SF_DL);
		}
	}
	else
	{
		//printf("CF invalid DLC : %d\r\n",RxFrame.N_PDU.DLC);
	}
}

void NetworkLayer_RxFC(NetworkFrame RxFrame)
{
	if(RxFrame.CanData.N_TAtype == PHYSICAL)
	{
		if(m_NetworkStatus == NWL_TRANSMITTING || m_NetworkStatus == NWL_WAIT)
		{
			if(RxFrame.N_PDU.DLC == 8)//iso 15765 7.4.2
			{
				if(RxFrame.N_PDU.SF_DL <= OVFLW)
				{
					TxParam.FS_Type = (FS_Type)RxFrame.N_PDU.SF_DL;
				}
				else
				{
					m_N_Result = N_INVALID_FS;
					//printf("FC wrong FS : %d\r\n",RxFrame.N_PDU.SF_DL);
				}

				TxParam.BlockSize =RxFrame.N_PDU.FF_DL_LOW;
				
				if((RxFrame.N_PDU.STmin >= 0x80 && RxFrame.N_PDU.STmin <= 0xF0) || (RxFrame.N_PDU.STmin >= 0xFA))
				{
					TxParam.STmin = 0x7F;
				}
				else if(RxFrame.N_PDU.STmin >= 0xF1 && RxFrame.N_PDU.STmin <= 0xF9)
				{
					TxParam.STmin = 0x1;
				}
				else
				{
					TxParam.STmin = RxFrame.N_PDU.STmin;
				}

				N_USData_indication(FC , m_AddressFormat.Mtype , m_AddressFormat.N_SA , m_AddressFormat.N_TA , m_AddressFormat.N_TAtype , m_AddressFormat.N_AE , (void*)0, 0 ,  m_N_Result);
			
			}
			else
			{
				//printf("FC invalid DLC:%d\r\n",RxFrame.N_PDU.DLC);
			}
		}
		else 
		{
			m_TimePeriod.FC_RxBeforeFFOnSender = TRUE;
			m_N_Result = N_TIMEOUT_Bs;
			N_USData_confirm(FF , m_AddressFormat.Mtype , m_AddressFormat.N_SA , m_AddressFormat.N_TA , m_AddressFormat.N_TAtype , m_AddressFormat.N_AE , m_N_Result);
			//printf("not in TX or wait ,ignore FC \r\n");
		}
	}
	else if(RxFrame.CanData.N_TAtype == FUNCTIONAL)
	{
		//printf("ignore functional  FC\r\n");
	}
}

void NetworkLayer_RxProc(void)
{
	if(!IsRxBuffEmpty())
	{
		N_PCIType PciType = RxFrameBuff[RxOutIndex].N_PDU.N_PciType;
		if(PciType == SF)//SF frame
		{
			NetworkLayer_RxSF(RxFrameBuff[RxOutIndex]);			
		}
		else if(PciType == FF)
		{
			NetworkLayer_RxFF(RxFrameBuff[RxOutIndex]);
		}
		else if(PciType == CF)
		{
			NetworkLayer_RxCF(RxFrameBuff[RxOutIndex]);
		}
		else if(PciType == FC)
		{
			NetworkLayer_RxFC(RxFrameBuff[RxOutIndex]);
		}
		else
		{
			m_N_Result = N_UNEXP_PDU;
		}
		
		(RxOutIndex == MAX_BUFF_NUMBER - 1) ? (RxOutIndex = 0) : (RxOutIndex++);
	}
}

void NetworkLayer_RxFrame(uint32_t ID,uint8_t* data,uint8_t IDE,uint8_t DLC,uint8_t RTR)
{
	if(ID == m_PyhReqID || ID == m_FunReqID || ID == m_PyhReqID1 || ID == m_FunReqID1)
	{
		uint8_t i;
		NetworkFrame TempFrame;
		for(i = 0 ; i < DLC; i ++) 
		{
			*(&(TempFrame.CanData.data7) + (7 -i)) = *(data + i);
		}
		
		TempFrame.CanData.ID = ID;
		TempFrame.CanData.DLC = DLC;
		TempFrame.CanData.RTR = RTR;
		TempFrame.CanData.IDE = IDE;
		TempFrame.CanData.N_SA = (uint8_t)(ID >> 8);
		TempFrame.CanData.N_TA = (uint8_t)ID;
		(ID == CAN_ID_DIAGNOSIS_FUNCTION || ((ID & 0x0000FF00) == 0x0000FF00)) ? (TempFrame.CanData.N_TAtype = FUNCTIONAL) : (TempFrame.CanData.N_TAtype = PHYSICAL);

		#if 1
		RxFrameBuff[RxInIndex] = TempFrame;
		(RxInIndex >= MAX_BUFF_NUMBER - 1) ? (RxInIndex = 0) : (RxInIndex++);
		#else
		for(i = 0; i < MAX_BUFF_NUMBER; i++)
		{
			if(RxFrameBuff[i].N_PDU.valid == FALSE)
			{
				RxFrameBuff[i] = TempFrame;
				return;
			}
		}
		#endif
	}

	if(ID == m_PyhReqID || ID == m_FunReqID)
	{
		m_CurrResponseID = m_ResponseID;
	}
	else if(ID == m_PyhReqID1 || ID == m_FunReqID1)
	{
		m_CurrResponseID = m_ResponseID1;
	}
}

static  void NetworkLayer_TxFrame(NetworkFrame txFrame)
{
	uint8_t TxDataBuff[8];
	//uint32_t id = m_AddressFormat.N_TA  | (m_AddressFormat.N_SA << 8) | (m_ResponseID & 0xFFFF0000);
	
	TxDataBuff[0] = txFrame.CanData.data0;
	TxDataBuff[1] = txFrame.CanData.data1;
	TxDataBuff[2] = txFrame.CanData.data2;
	TxDataBuff[3] = txFrame.CanData.data3;
	TxDataBuff[4] = txFrame.CanData.data4;
	TxDataBuff[5] = txFrame.CanData.data5;
	TxDataBuff[6] = txFrame.CanData.data6;
	TxDataBuff[7] = txFrame.CanData.data7;
	if(NetworkSend != NULL)
	{
		NetworkSend(m_CurrResponseID,TxDataBuff,8,0,0,0);
		m_N_Result = N_OK;
	}
	else
	{
		m_N_Result = N_ERROR;
	}
	N_USData_confirm(txFrame.N_PDU.N_PciType , m_AddressFormat.Mtype , m_AddressFormat.N_SA , m_AddressFormat.N_TA , m_AddressFormat.N_TAtype , m_AddressFormat.N_AE , m_N_Result);
}

void NetworkLayer_TxEnd(void)
{
	m_TxStep = TX_IDLE;
	m_NetworkStatus = NWL_IDLE;
}

void NetworkLayer_TxStart(void)
{
	DiagTimer_Set(&SendTimer,m_TimePeriod.N_As);
	m_NetworkStatus = NWL_TRANSMITTING;
	m_TxStep = TX_WAIT_FF_CONF;
}

void NetworkLayer_RxEnd(void)
{
	//printk("RX end\r\n");
	m_RxStep = RX_IDLE;
	m_NetworkStatus = NWL_IDLE;
	RxParam.TotalDataNumber = 0;
	RxParam.CompletedDataNumber = 0;
	RxParam.CompletedNumberInBlock = 0;
}

void NetworkLayer_RxStart(void)
{
	DiagTimer_Set(&ReciveTimer,m_TimePeriod.N_Br);
	m_NetworkStatus = NWL_RECIVING;
	m_RxStep = RX_WAIT_FC_REQ;
	RxParam.CompletedDataNumber = 0;
	RxParam.CompletedNumberInBlock = 0;
}

void NetworkLayer_TxProc(void)
{
	#if 1
	if(!IsTxBuffEmpty())
	{
		NetworkLayer_TxFrame(TxFrameBuff[TxOutIndex]);
		(TxOutIndex >= (MAX_BUFF_NUMBER - 1)) ? (TxOutIndex = 0) : (TxOutIndex++);
	}
	#else
	for(i = 0; i < MAX_BUFF_NUMBER; i++)
	{
		if(TxFrameBuff[i].N_PDU.valid == TRUE)
		{
			NetworkLayer_TxFrame(TxFrameBuff[i]);
			TxFrameBuff[i].N_PDU.valid = FALSE;
			break;
		}
	}
	#endif
}

void NetworkLayer_NotifyToUpperLayer(NetworkNotification notify)
{
	if(!IsIndicationListFull())
	{
		IndicationList[IndicationInIndex] = notify;
		(IndicationInIndex >= MAX_INDICATION_NUMBER -1) ? (IndicationInIndex = 0) : IndicationInIndex++;
	}
}

void N_USData_request(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, uint8_t* MessageData, uint16_t Length)//interface request for upper layer
{
	m_AddressFormat.Mtype = Mtype;
	m_AddressFormat.N_SA = N_SA;
	m_AddressFormat.N_TA = N_TA;
	m_AddressFormat.N_TAtype = N_TAtype;
	m_AddressFormat.N_AE = N_AE;
	//printf("diag req SA = %x,TA = %x,length = %x,TAtype = %d\r\n",N_SA,N_TA,Length,N_TAtype);
	if(Mtype == DIAGNOSTIC)
	{
		if(Length <= 7)
		{
			NetworkLayer_SendSF((uint8_t)Length,MessageData);
		}
		else
		{
			NetworkLayer_SendFF(Length,MessageData);
			NetworkLayer_TxStart();
		}
	}
}

void N_USData_confirm(N_PCIType PciType,MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, N_Result N_Result)//interface confirm to upper layer
{
	if(PciType == FF)
	{
		m_TimePeriod.FF_ConfirmedOnSender = TRUE;
	}
	else if(PciType == CF)
	{
		TxParam.CompletedNumberInBlock++;
		m_TimePeriod.CF_ConfirmedOnSender = TRUE;
		
		if(TxParam.TotalDataNumber == TxParam.CompletedDataNumber)
		{
			NetworkNotification notify;
			notify.NotificationType = CONFIRM;
			notify.Mtype = Mtype;
			notify.N_SA = N_SA;
			notify.N_TA = N_TA;
			notify.N_TAtype = N_TAtype;
			notify.N_AE = N_AE;
			notify.N_Resut = N_Result;
			notify.valid = TRUE;
			NetworkLayer_NotifyToUpperLayer(notify);
		}
	}
	else if(PciType == FC)
	{
		if(m_RxStep == RX_WAIT_FC_CONF)
		{
			m_TimePeriod.FC_ConfirmedOnReciver = TRUE;
		}
		else
		{
			//printk("send FC on wrong time\r\n");
		}
	}
	else if(PciType == SF)
			{
		NetworkNotification notify;
		notify.NotificationType = CONFIRM;
		notify.Mtype = Mtype;
		notify.N_SA = N_SA;
		notify.N_TA = N_TA;
		notify.N_TAtype = N_TAtype;
		notify.N_AE = N_AE;
		notify.N_Resut = N_Result;
		notify.valid = TRUE;
		NetworkLayer_NotifyToUpperLayer(notify);
	}
}

void N_USData_FF_indication(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, uint16_t Length)
{
	NetworkNotification notify;
	notify.NotificationType = FF_INDICATION;
	notify.Mtype = Mtype;
	notify.N_SA = N_SA;
	notify.N_TA = N_TA;
	notify.N_TAtype = N_TAtype;
	notify.N_AE = N_AE;
	notify.valid = TRUE;
	notify.length = Length;
	NetworkLayer_NotifyToUpperLayer(notify);
}

void N_USData_indication(N_PCIType PciType,MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, uint8_t* MessageData, uint16_t Length, N_Result N_Result)
{
	uint8_t i;
	NetworkNotification notify;
	
	if(PciType == FC)
	{
		if(N_Result == N_OK)
		{
			if(m_TxStep == TX_WAIT_FC)
			{
				////printk("Rx FC type : %d\r\n",TxParam.FS_Type);
				TxParam.CompletedNumberInBlock = 0;
				
				if(TxParam.FS_Type == CTS)
				{
					m_NetworkStatus = NWL_TRANSMITTING;
					m_TimePeriod.FC_RecivedOnSender = TRUE;
				}
				else if(TxParam.FS_Type == WT)
				{
					m_NetworkStatus = NWL_WAIT;
					DiagTimer_Set(&SendTimer,m_TimePeriod.N_Bs);
					//printk("Rx Fs wait\r\n");
				}
				else if(TxParam.FS_Type == OVFLW)
				{
					//printk("rx fs overflow,terminal transmitr\n");
					NetworkLayer_TxEnd();
				}
			}
			else
			{
				
			}
		}
		else
		{
			NetworkLayer_TxEnd();
		}
	}
	else if(PciType == CF)
	{
		if(N_Result == N_OK)
		{
			RxParam.CompletedNumberInBlock++;
			m_TimePeriod.CF_RecivedOnReciver = TRUE;
			for(i = 0 ; i < Length ; i ++)
			{
				NetworkDataBufRx[RxParam.CompletedDataNumber++] = *(MessageData + i);
			}

			if(RxParam.CompletedDataNumber == RxParam.TotalDataNumber)
			{
				notify.NotificationType = INDICATION;
				notify.Mtype = Mtype;
				notify.N_SA = N_SA;
				notify.N_TA = N_TA;
				notify.N_TAtype = N_TAtype;
				notify.N_AE = N_AE;
				notify.N_Resut = N_Result;
				notify.MessageData = NetworkDataBufRx;
				notify.length = RxParam.TotalDataNumber;
				notify.valid = TRUE;
				NetworkLayer_NotifyToUpperLayer(notify);
				NetworkLayer_RxEnd();
			}
		}
		else
		{
			NetworkLayer_RxEnd();
		}
	}
	else if(PciType == FF)
	{
		RxParam.TotalDataNumber = Length;
		NetworkLayer_RxStart();
		for(i = 0 ; i < 6 ; i ++)
		{
			NetworkDataBufRx[RxParam.CompletedDataNumber++] = *(MessageData + i);
		}
	}
	else if(PciType == SF)
	{
		if(m_N_Result == N_UNEXP_PDU)//Request-interrupt-by-SF
		{
			NetworkLayer_RxEnd();
		}
		notify.NotificationType = INDICATION;
		notify.Mtype = Mtype;
		notify.N_SA = N_SA;
		notify.N_TA = N_TA;
		notify.N_TAtype = N_TAtype;
		notify.N_AE = N_AE;
		notify.N_Resut = N_Result;
		notify.MessageData = MessageData;
		notify.length = Length;
		notify.valid = TRUE;
		NetworkLayer_NotifyToUpperLayer(notify);
	}
}

void N_ChangeParameter_request(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, Parameter Parameter, uint8_t Parameter_Value)
{
	Result_ChangeParameter result;
	if(Parameter == STmin)
	{
		RxParam.STmin = Parameter_Value;
		result = N_CHANGE_OK;
	}
	else if(Parameter == BS)
	{
		RxParam.BlockSize = Parameter_Value;
		result = N_CHANGE_OK; 
	}
	else
	{
		result = N_WRONG_PARAMETER;
	}
	N_ChangeParameter_confirm(Mtype, N_SA, N_TA, N_TAtype, N_AE, Parameter,result);
}

void N_ChangeParameter_confirm(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, Parameter Parameter, Result_ChangeParameter Result_ChangeParameter)
{

}

void NetworkLayer_MainProc(void)
{
	if(m_NetworkStatus == NWL_TRANSMITTING)
	{
		if(m_TxStep == TX_WAIT_FF_CONF)
		{
			if(m_TimePeriod.FF_ConfirmedOnSender == TRUE)
			{
				m_TxStep  = TX_WAIT_FC;
				DiagTimer_Set(&SendTimer,m_TimePeriod.N_Bs);
				m_TimePeriod.FF_ConfirmedOnSender = FALSE;
			}
			else
			{
				if(DiagTimer_HasExpired(&SendTimer))
				{
					NetworkLayer_TxEnd();//time out end
					m_N_Result = N_TIMEOUT_A;
					//printf("timeout wait FF conf\r\n");
					N_USData_confirm(FF , m_AddressFormat.Mtype , m_AddressFormat.N_SA , m_AddressFormat.N_TA , m_AddressFormat.N_TAtype , m_AddressFormat.N_AE , m_N_Result);
				}
			}
		}
		else if(m_TxStep == TX_WAIT_FC)
		{
			if(m_TimePeriod.FC_RecivedOnSender == TRUE)
			{
				m_TxStep  = TX_WAIT_CF_REQ;
				DiagTimer_Set(&SendTimer,m_TimePeriod.N_Cs);
				DiagTimer_Set(&CFRequestTimer , 0);
				m_TimePeriod.FC_RecivedOnSender = FALSE;
			}
			else
			{
				if(DiagTimer_HasExpired(&SendTimer))
				{
					//printf("timeout wait FC\r\n");
					NetworkLayer_TxEnd();//time out end
					m_N_Result = N_TIMEOUT_Bs;
					N_USData_confirm(FF , m_AddressFormat.Mtype , m_AddressFormat.N_SA , m_AddressFormat.N_TA , m_AddressFormat.N_TAtype , m_AddressFormat.N_AE , m_N_Result);
				}
			}
		}
		else if(m_TxStep == TX_WAIT_CF_REQ)
		{
			if(DiagTimer_HasExpired(&CFRequestTimer))//when first CF or wait STmin end
			{
				NetworkLayer_SendCF();
				m_TxStep = TX_WAIT_CF_CONF;
				DiagTimer_Set(&SendTimer,m_TimePeriod.N_As);
			}
		}
		else if(m_TxStep == TX_WAIT_CF_CONF)
		{
			if(m_TimePeriod.CF_ConfirmedOnSender == TRUE)
			{
				DiagTimer_Set(&CFRequestTimer , TxParam.STmin + 1);
				m_TimePeriod.CF_ConfirmedOnSender  = FALSE;
				if(TxParam.CompletedDataNumber < TxParam.TotalDataNumber)
				{
					if(TxParam.CompletedNumberInBlock < TxParam.BlockSize || TxParam.BlockSize == 0)
					{
						m_TxStep  = TX_WAIT_CF_REQ;
						DiagTimer_Set(&SendTimer,m_TimePeriod.N_Cs);
					}
					else
					{
						m_TxStep = TX_WAIT_FC;
						DiagTimer_Set(&SendTimer,m_TimePeriod.N_Bs);
					}
				}
				else
				{
					NetworkLayer_TxEnd();//normal end
				}		
			}
			else
			{
				if(DiagTimer_HasExpired(&SendTimer))
				{
					//printf("timeout wait CF conf\r\n");
					NetworkLayer_TxEnd();//time out end
				}
			}
		}
	}
	else if(m_NetworkStatus == NWL_RECIVING)
	{
		if(m_RxStep == RX_WAIT_FC_REQ)
		{
			NetworkLayer_SendFC();
			m_RxStep = RX_WAIT_FC_CONF;
			DiagTimer_Set(&ReciveTimer,m_TimePeriod.N_Ar);
		}
		else if(m_RxStep == RX_WAIT_FC_CONF)
		{
			if(m_TimePeriod.FC_ConfirmedOnReciver == TRUE)
			{
				m_RxStep  = RX_WAIT_CF;
				DiagTimer_Set(&ReciveTimer,m_TimePeriod.N_Cr);
				m_TimePeriod.FC_ConfirmedOnReciver = FALSE;
			}
			else
			{
				if(DiagTimer_HasExpired(&ReciveTimer))
				{
					//printf("timeout wait FC conf\r\n");
					NetworkLayer_RxEnd();//time out end 
					m_N_Result = N_TIMEOUT_A;
					N_USData_indication(FC , m_AddressFormat.Mtype , m_AddressFormat.N_SA , m_AddressFormat.N_TA , m_AddressFormat.N_TAtype , m_AddressFormat.N_AE , (void*)0, 0 ,  m_N_Result);
				}
			}
		}
		else if(m_RxStep == RX_WAIT_CF)
		{
			if(m_TimePeriod.CF_RecivedOnReciver == TRUE)
			{
				m_TimePeriod.CF_RecivedOnReciver = FALSE;
				if(RxParam.CompletedDataNumber < RxParam.TotalDataNumber)
				{
					if(RxParam.CompletedNumberInBlock < RxParam.BlockSize || RxParam.BlockSize == 0)
					{
						m_RxStep  = RX_WAIT_CF;
						DiagTimer_Set(&ReciveTimer,m_TimePeriod.N_Cr);
					}
					else 
					{
						//printf("block end %d >= %d\r\n",RxParam.CompletedNumberInBlock,RxParam.BlockSize);
						RxParam.CompletedNumberInBlock = 0;
						//RxParam.SN = 0;
						m_RxStep  = RX_WAIT_FC_REQ;
						DiagTimer_Set(&ReciveTimer,m_TimePeriod.N_Br);
					}
				}
				else
				{
					NetworkLayer_RxEnd();//normal end
				}
			}
			else
			{
				if(DiagTimer_HasExpired(&ReciveTimer))
				{
					//printf("timeout wait CF\r\n");
					NetworkLayer_RxEnd();//time out end
					m_N_Result = N_TIMEOUT_Cr;
					N_USData_indication(CF , m_AddressFormat.Mtype , m_AddressFormat.N_SA , m_AddressFormat.N_TA , m_AddressFormat.N_TAtype , m_AddressFormat.N_AE , (void*)0, 0 ,  m_N_Result);
				}
			}
		}
	}
	else if(m_NetworkStatus == NWL_WAIT)
	{
		if(m_TxStep == TX_WAIT_FC)
		{
			if(DiagTimer_HasExpired(&SendTimer))
			{
				//printf("FC Wait time out : FC with FS = 0 not recive\r\n");
				NetworkLayer_TxEnd();//time out end
				m_N_Result = N_TIMEOUT_Bs;
				N_USData_confirm(FF , m_AddressFormat.Mtype , m_AddressFormat.N_SA , m_AddressFormat.N_TA , m_AddressFormat.N_TAtype , m_AddressFormat.N_AE , m_N_Result);
			}
		}
	}
}

void NetworkLayer_Proc(void)
{
	#if 0
	static uint32_t timer1;
	uint8_t data[8];
	if(Timer_HasExpired(&timer1))
	{
		Timer_Set(&timer1, 2000);
		data[0]++;
		SendCANFrame(0x777, data , 8 , 6); 
	}
	#endif
	NetworkLayer_RxProc();
	NetworkLayer_TxProc();
	NetworkLayer_MainProc();
}


bool IsIndicationListEmpty(void)
{
	return (IndicationInIndex == IndicationOutIndex);
}

bool IsIndicationListFull(void)
{
	return (IndicationInIndex == (MAX_INDICATION_NUMBER - 1) &&  IndicationOutIndex == 0) || (IndicationOutIndex == IndicationInIndex+1);
}
NetworkNotification PullIndication(void)
{
	uint8_t tempOutIndex = IndicationOutIndex;
	(IndicationOutIndex >= MAX_INDICATION_NUMBER - 1) ? (IndicationOutIndex = 0) : (IndicationOutIndex++);
	return IndicationList[tempOutIndex];
}