#ifndef _NETWORKLAYER_H
#define _NETWORKLAYER_H

#include "Diagnostic.h"

#define MAX_BUFF_NUMBER			3
#define MAX_INDICATION_NUMBER		3
#define MAX_DTCDATA_BUF			100
#define MAX_DOWNLOADING_BUF		MAX_DTCDATA_BUF

#define CAN_ID_DIAGNOSIS_FUNCTION 		0x7DF

void NetworkLayer_InitParam(uint32_t PyhReqID,uint32_t FunReqID, uint32_t ResponseID,SendCANFun sendFun);
void NetworkLayer_SetSecondID(uint32_t PyhReqID,uint32_t FunReqID, uint32_t ResponseID);
void NetworkLayer_Proc(void);
void N_USData_request(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, uint8_t* MessageData, uint16_t Length);
//void N_USData_confirm(N_PCIType PciType,MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, N_Result N_Result);
//void N_USData_FF_indication(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, uint16_t Length);
//void N_USData_indication(N_PCIType PciType,MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, uint8_t* MessageData, uint16_t Length, N_Result N_Result);

void N_ChangeParameter_request(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, Parameter Parameter, uint8_t Parameter_Value);
void N_ChangeParameter_confirm(MType Mtype, uint8_t N_SA, uint8_t N_TA, N_TAtype N_TAtype, uint8_t N_AE, Parameter Parameter, Result_ChangeParameter Result_ChangeParameter);
void NetworkLayer_RxFrame(uint32_t ID,uint8_t* data,uint8_t IDE,uint8_t DLC,uint8_t RTR);
void NetworkLayer_SetParam(uint8_t TimeAs, uint8_t TimeBs, uint8_t TimeCr, uint8_t TimeAr, uint8_t TimeBr, uint8_t TimeCs, 	uint8_t Bs, uint8_t m_STmin, DuplexMode nDuplex ,  MType Mtype , uint8_t N_SA , uint8_t N_TA , N_TAtype N_TAtype , uint8_t N_AE , uint8_t FillData);
bool IsIndicationListEmpty(void);
NetworkNotification PullIndication(void);
#endif
