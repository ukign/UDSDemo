#ifndef _DIAGNOSTIC_H
#define _DIAGNOSTIC_H
#include "NetworkLayerTypeDefines.h"

typedef enum{
	LEVEL_ZERO = 7,
	LEVEL_ONE = 1,
	LEVEL_TWO = 2,
	LEVEL_THREE = 4,
	LEVEL_FOUR = 8,
	LEVEL_UNSUPPORT = 0,
}SecurityLevel;

typedef enum{
	HARD_RESET = 1,
	KEY_OFF_ON_RESET = 2,
	SOFT_RESET = 3,
	ENABLE_RAPID_POWER_SHUTDOWN = 4,
	DISABLE_RAPID_POWER_SHUTDOWN = 5,
}EcuResetType;

typedef enum{
	ISO15031_6DTCFORMAT= 1,
	ISO14229_1DTCFORMAT = 2,
	SAEJ1939_73DTCFORMAT = 3,
}DTCFormatIdentifier;

typedef enum{
	PASSED,
	IN_TESTING,
	FAILED,
}DTCTestResult;

typedef enum{
	EEPROM_DID,
	REALTIME_DID,
	IO_DID,
}DIDType;

typedef enum{
	READONLY = 1,
	WRITEONLY = 2,
	READWRITE = 3,
}ReadWriteAttr;

typedef enum{
	ERXTX,//enableRxAndTx
	ERXDTX,//enableRxAndDisableTx
	DRXETX,//disableRxAndEnableTx
	DRXTX,//disableRxAndTx
	//ERXDTXWEAI,//enableRxAndDisableTxWithEnhancedAddressInformation
	//ERXTXWEAI,//enableRxAndTxWithEnhancedAddressInformation
}CommulicationType;

typedef enum{
	NCM = 1,//application message
	NWMCM,//network manage message
	NWMCM_NCM,//application and netwrok manage message
}communicationParam;

typedef enum{
	SUB_DEFAULT = 1,//sub function supported in default session
	SUB_PROGRAM = 2,//sub function supported in program session
	SUB_EXTENDED = 4,////sub function supported in extedned session
	SUB_FACTORY = 8,//sub funcion supported in factory session,
	SUB_ALL = 7,//sub function supported in both of three session
}SubFunSuppInSession;

typedef enum{
	LEVEL_A,
	LEVEL_B,
	LEVEL_C,
}DTCLevel;

typedef uint8_t (*IoControl)(uint8_t ctrl, uint8_t param);
typedef uint32_t (*SecurityFun)(uint32_t);
typedef DTCTestResult (*DetectFun)(void);
typedef void (*ResetCallBack)(EcuResetType);
typedef void (*CommCallBack)(CommulicationType , communicationParam);
typedef uint8_t (*SendCANFun)(uint32_t ID, uint8_t *array, uint8_t length, uint8_t priority, uint8_t rtr, uint8_t ide);

#define   USE_MALLOC			0
#define	USE_J1939_DTC		0

/*======================== buf size config ================================*/	
#define MAX_DTC_NUMBER 				35//最大DTC个数
#define MAX_DID_NUMBER 				70//最大DID个数
#define MAX_SNAPSHOT_NUMBER 			10//最大快照信息个数
#define MAX_GROUP_NUMBER				5//最大DTC组个数
/*======================== buf size config ================================*/	

void Diagnostic_Init(uint32_t requestId, uint32_t responseId, uint32_t funRequestId, uint16_t EEPromStartAddr, uint16_t EEpromSize,SendCANFun sendFun,uint16_t p2CanServerMax, uint16_t p2ECanServerMax);
void Diagnostic_Set2ndReqAndResID(uint32_t requestId1, uint32_t responseId1,uint32_t funRequestId1);
void Diagnostic_DelInit(void);
void Diagnostic_RxFrame(uint32_t ID,uint8_t* data,uint8_t IDE,uint8_t DLC,uint8_t RTR);
void Diagnostic_1msTimer(void);
bool InitAddSecurityAlgorithm(SecurityLevel level, SecurityFun AlgoritthmFun,byte SeedSubFunctionNum,byte KeySubFunctionNum , uint8_t* FaultCounter,uint8_t FaultLimitCounter , uint32_t UnlockFailedDelayTimeMS,SubFunSuppInSession SubFuntioncSupportedInSession,uint8_t KeySize);
void InitFactorySecuriyAlgorithm(void);
bool InitSetSessionSupportAndSecurityAccess(bool support ,uint8_t service,uint8_t PHYDefaultSession_Security,	uint8_t PHYProgramSeesion_Security,	uint8_t PHYExtendedSession_Security,	uint8_t FUNDefaultSession_Security,	uint8_t FUNProgramSeesion_Security,	uint8_t FUNExtendedSession_Security);
void InitAddDID(uint16_t DID , uint8_t DataLength , uint8_t* DataPointer , DIDType DidType , IoControl ControlFun , ReadWriteAttr RWAttr,uint16_t EEaddr, bool SupportWriteInFactoryMode);
#if USE_J1939_DTC
void Diagnostic_DM1MsgEnable(bool dm1en);
bool InitAddDTC(uint32_t DTCCode,DetectFun MonitorFun,byte DectecPeroid, byte ValidTimes,DTCLevel dtcLevel,uint32_t spn, uint8_t fmi);
#else
bool InitAddDTC(uint32_t DTCCode,DetectFun MonitorFun,byte DectecPeroid, byte ValidTimes,DTCLevel dtcLevel);
#endif
void InitAddDTCSnapShot(uint8_t recordNumber , uint16_t ID , uint8_t* datap , uint8_t size);
void InitSetAgingCounterRecordNumber(uint8_t RecordNumer);
void InitSetAgedCounterRecordNumber(uint8_t RecordNumer);
void InitSetOccurrenceCounterRecordNumber(uint8_t RecordNumer);
void InitSetPendingCounterRecordNumber(uint8_t RecordNumer);
//void InitAddDTCExtendedData(uint16_t ID , uint8_t* datap , uint8_t size);
void InitSetDTCAvailiableMask(uint8_t AvailiableMask);
void InitAddDTCGroup(uint32_t Group);
void InitSetSysResetParam(bool support01 , bool support02 , bool support03 , bool support04 , bool support05 , ResetCallBack callback, bool supressPosResponse);
void InitSetCommControlParam(bool supportSubFun00, bool supportSubFun01 , bool supportSubFun02 , bool supportSubFun03 , bool supportType01, bool supportType02, bool supportType03, CommCallBack callback, bool supressPosResponse);
void InitSetSessionControlParam(bool supportSub01, bool supportSub02,bool supportSub03, bool sub02SupportedInDefaultSession, bool sub03SupportedInProgramSession, bool supressPosResponse);
void InitSetTesterPresentSupress(bool supressPosResponse);
void InitSetDTCControlSupress(bool supressPosResponse);
void InitSetCurrentSessionDID(uint16_t m_DID);
void InitSetCanDataBaseVersionDID(uint16_t m_DID);
void InitSetCanDiagnosticVersionDID(uint16_t m_DID);
void InitSetCanNMVersionDID(uint16_t m_DID);
void InitSetCanDriverVersionDID(uint16_t m_DID);
void Diagnostic_LoadAllData(void);
void Diagnostic_ConfigVIN(uint8_t length, uint8_t* data);
/************set netwrok layer parameters********/
void Diagnostic_SetNLParam(uint8_t TimeAs, uint8_t TimeBs, uint8_t TimeCr, uint8_t TimeAr, uint8_t TimeBr, uint8_t TimeCs, uint8_t BlockSize, uint8_t m_STmin, uint8_t FillData);
void Diagnostic_Proc(void);


#endif
