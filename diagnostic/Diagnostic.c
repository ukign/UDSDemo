#define _DIAGNOSTIC_C
#include "Cpu.h"
#include "DiagnosticTimer.h"
#include "NetworkLayer.h"
#include "diagnostic.h"
#if USE_J1939_DTC
#include "J1939TP.h"
#endif
#include "EEpromDriver.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define SERVICE_NUMBER 				26
typedef void (*ServiceHandler)(uint8_t N_TAType,uint16_t length, uint8_t *MessageData);
typedef far word * far EEProm_TAddress;

typedef enum{
	SESSION_CONTROL = 0x10,
	RESET_ECU = 0x11,
	SECURITY_ACCESS = 0x27,
	COMMUNICATION_CONTROL = 0x28,
	TESTER_PRESENT = 0x3E,
	GET_TIME_PARAM = 0x83,
	SECURITY_DATA_TRANSMISSION = 0x84,
	CONTROL_DTC_SETTING = 0x85,
	RESPONSE_ON_EVENT = 0x86,
	LINK_CONTROL = 0x87,
	READ_DATA_BY_ID = 0x22,
	READ_MEMORY_BY_ADDRESS = 0x23,
	READ_SCALING_DATA_BY_ID = 0x24,
	READ_DATA_PERIOD_ID = 0x2A,
	DYNAMICALLY_DEFINE_DATA_ID = 0x2C,
	WRITE_DATA_BY_ID = 0x2E,
	WRITE_MEMORY_BY_ADDRESS = 0x3D,
	CLEAR_DTC_INFO = 0x14,
	READ_DTC_INFO = 0X19,
	IO_CONTROL_BY_ID = 0x2F,
	ROUTINE_CONTROL = 0x31,
	REQUEST_DOWNLOAD = 0x34,
	REQUEST_UPLOAD = 0x35,
	TRANSMIT_DATA = 0x36,
	REQUEST_TRANSFER_EXIT = 0x37,
}ServiceName;

typedef enum{
	PR = 0x00,//positive response
	GR = 0x10,//general reject
	SNS = 0x11,//service not supported
	SFNS = 0x12,//sub-function not supported
	IMLOIF = 0x13,//incorrect message length or invalid format
	RTL = 0x14,//response too long
	BRR = 0x21,//busy repeat request
	CNC = 0x22,//condifitons not correct
	RSE = 0x24,//request sequence error
	NRFSC = 0x25,
	FPEORA = 0x26,
	ROOR = 0x31,//reqeust out of range
	SAD = 0x33,//security access denied
	IK = 0x35,//invalid key
	ENOA = 0x36,//exceed number of attempts
	RTDNE = 0x37,//required time delay not expired
	UDNA = 0x70,//upload download not accepted
	TDS = 0x71,//transfer data suspended
	GPF = 0x72,//general programming failure
	WBSC = 0x73,//wrong block sequence coutner
	RCRRP = 0x78,//request correctly received-respone pending
	SFNSIAS = 0x7e,//sub-function not supported in active session
	SNSIAS  = 0x7F,//service not supported in active session
	VTH = 0x92,//voltage too high
	VTL = 0x93,//voltage too low
}NegativeResposeCode;

typedef enum{
	ECU_DEFAULT_SESSION = 1,
	ECU_PAOGRAM_SESSION = 2,
	ECU_EXTENED_SESSION = 3,
}SessionType;

typedef enum{
	WAIT_SEED_REQ,
	WAIT_KEY,
	WAIT_DELAY,
	UNLOCKED,
}SecurityUnlockStep;

typedef enum{
	REPORT_DTCNUMBER_BY_MASK = 1,
	REPORT_DTCCODE_BY_MASK = 2,
	REPORT_DTCSNAPSHOT_BY_ID = 3,
	REPORT_DTCSNAPSHOT_BY_DTCNUMBER = 4,
	REPORT_DTCSNAPSHOT_BY_RECORDNUMBER = 5,
	REPORT_DTCEXTEND_DATA_BY_DTCNUMBER = 6,
	REPORT_DTCNUMBER_BY_SEVERITYMASK,
	REPORT_DTC_BY_SEVERITYMASK,
	REPORT_SEVERITYID_OF_DTC,
	REPORT_SUPPORTED_DTC = 0x0A,
	REPORT_FIRST_FAILED_DTC,
	REPORT_FIRST_CONFIRMED_DTC,
	REPORT_MOST_FAILED_DTC,
	REPORT_MOST_CONFIRMED_DTC,
	REPORT_MIRRORDTC_BY_MASK,
	REPORT_MIRRORDTC_EXTENDED_BY_DTC_NUMBER,
	REPORT_MIRRORDTC_NUMBER_BY_MASK,
	REPORT_OBDDTC_NUMBER_BY_MASK,
	REPORT_OBDDTC_BY_MASK,
}DTCSubFunction;


typedef enum{
	POWERTRAIN,
	CHASSIS,
	BODY,
	NETWORK,
}DTCAlphanumeric ;


typedef union{
	struct{
		uint8_t TestFailed:1;
		uint8_t TestFailedThisMonitoringCycle:1;
		uint8_t PendingDTC:1;
		uint8_t ConfirmedDTC:1;
		uint8_t TestNotCompleteSinceLastClear:1;
		uint8_t TestFailedSinceLastClear:1;
		uint8_t TestNotCompleteThisMonitoringCycle:1;
		uint8_t WarningIndicatorRequested:1;
	}DTCbit;
	uint8_t DTCStatusByte;
}DTCStatusType;

typedef struct _J1939DTC{
	uint32_t SPN;
	uint8_t FMI;
	uint8_t CM;
	uint8_t OC;
}J1939DTC;

typedef struct _DtcNode{
	uint32_t DTCCode;
	DetectFun DetectFunction;//故障检测函数
	uint16_t EEpromAddr;
	uint8_t TripLimitTimes;
	DTCStatusType DTCStatus;
	uint8_t OldCounter;
	uint8_t GoneCounter;
	uint8_t TripCounter;//故障待定计数器
	uint8_t FaultOccurrences;//故障出现次数
	uint16_t SnapShotEEpromAddr;//快照信息地址
	//uint16_t ExtenedDataEEpromAddr;//扩展数据地址
	#if USE_J1939_DTC
	DTCLevel dtcLevel;
	J1939DTC DM1Code;
	#endif
	struct _DtcNode* next;
}DTCNode;

typedef struct _DidNode{
	uint16_t ID;
	uint8_t dataLength;
	uint8_t* dataPointer;
	IoControl Callback;
	DIDType didType;
	ReadWriteAttr RWAttr;
	uint16_t EEpromAddr;
	struct _DidNode* next;
}DIDNode;

typedef struct _GroupNode{
	uint32_t GroupID;
	struct _GroupNode* next;
}DTCGroupNode;

typedef struct _Snapshot{
	uint8_t snapshotRecord;
	uint16_t snapshotID;
	uint8_t* dataPointer;
	uint8_t dataLength;
}Snapshot;

typedef struct{
	bool support;
	ServiceName serviceName;
	uint8_t PHYDefaultSession_Security:4;//security suppport in default session physical address
	uint8_t PHYProgramSeesion_Security:4;//security suppport in program session physical address
	uint8_t PHYExtendedSession_Security:4;//security suppport in extened session physical address
	uint8_t FUNDefaultSession_Security:4;//security suppport in default session function address
	uint8_t FUNProgramSeesion_Security:4;//security suppport in program session function address
	uint8_t FUNExtendedSession_Security:4;//security suppport in extened session function address
	ServiceHandler serviceHandle;
}SessionService;

typedef struct{
	bool valid;
	SecurityLevel level;
	SecurityFun UnlockFunction;
	uint8_t seedID;
	uint8_t keyID;
	uint8_t FaultCounter;
	uint16_t FaultCounterAddr;
	uint8_t FaultLimitCounter;
	uint32_t UnlockFailedDelayTime;
	uint8_t subFunctionSupported;
	DiagTimer SecurityLockTimer;						//解锁失败3次后再次解锁延时定时器
	uint8_t KeySize;
}SecurityUnlock;

/* Private macro -------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

void ServiceNegReponse(uint8_t serviceName,uint8_t RejectCode);
void Diagnostic_ReadDTCPositiveResponse(uint8_t DTCSubFunction,uint8_t DTCStatausMask);
void Service10Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service11Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service27Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service28Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service3EHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service83Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service84Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service85Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service86Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service87Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service22Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service23Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service24Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service2AHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service2CHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service2EHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service3DHandle(uint8_t N_TAType,uint16_t length, uint8_t *MessageData);
void Service14Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service19Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service2FHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service31Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service34Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service35Handle(uint8_t N_TAType,uint16_t length, uint8_t *MessageData);
void Service36Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
void Service37Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData);
DIDNode* SearchDidNode(uint16_t DID);
void Diagnostic_ClearDTC(uint32_t Group);
void Diagnostic_SaveAllDTC(void);
void Diagnostic_LoadAllDTC(void);
DTCNode* GetDTCNodeByCode(uint32_t dtcCode);

/* Private variables ---------------------------------------------------------*/

/*========================about security acces================================*/
const char DriverVersion[] = {1,0,0};
const char NMVersion[] = {0,0,0};
const char DiagnosticVersion[] = {1,0,0};
const char DatabaseVersion[] = {0,0};
/*========================about security acces================================*/


/*========================about security acces================================*/
static uint8_t Seed[5];										//保存解锁种子
static uint8_t key[4];										//保存解锁密匙
static uint32_t retLen;
static SecurityLevel m_SecurityLevel = LEVEL_ZERO;			//当前解锁等级
static SecurityUnlockStep m_UnlockStep = WAIT_SEED_REQ;	//当前解锁步骤
SecurityUnlock UnlockList[3];
/*========================about security acces================================*/

/*========================sesion , id , buf and so on================================*/
static NegativeResposeCode m_NRC;					//否定响应码
uint8_t m_CurrSessionType;							//当前回话类型
bool ResponsePending;									//是否发送过78负响应
bool suppressResponse;
uint16_t ResponseLength;
uint8_t CurrentService;
static uint8_t DiagnosticBuffTX[200];							//发送数据的缓存
static uint8_t J1939BufTX[90];
static DiagTimer S3serverTimer;							 //S3定时器
static uint16_t P2CanServerMax = 0x32;						//回话参数
static uint16_t P2ECanServerMax = 0x1F4;					//回话参数
static uint32_t TesterPhyID;
static uint32_t TesterFunID;
static uint32_t TesterPhyID1;
static uint32_t TesterFunID1;
static uint32_t EcuID1;
static uint32_t EcuID;
static uint8_t N_Ta;
static uint8_t N_Sa;
static uint8_t SessionSupport;//bit0: default session 01 support
						   //bit1: program session 02 support
						   //bit2: extended session 03 support
						   //bit3: sub02 supported in defaultsession
						   //bit4: sub03 supported in program
						   //bit5: supressPosRespnse support
#define Service10Sub01Supported() ((SessionSupport & 0x01) != 0)
#define Service10Sub02Supported() ((SessionSupport & 0x02) != 0)
#define Service10Sub03Supported() ((SessionSupport & 0x04) != 0)
#define Service10Sub01To02OK() ((SessionSupport & 0x08) != 0)
#define Service10Sub02To03OK() ((SessionSupport & 0x10) != 0)
#define Service10SupressSupproted() ((SessionSupport & 0x20) != 0)
/*========================sesion , id , buf and so on================================*/

/*========================about program================================*/
static uint8_t m_BlockIndex = 0;
static uint32_t ProgramAddress = 0;
static uint32_t ProgramLength = 0;
static uint32_t ProgramLengthComplete = 0;
static bool IsUpdating = FALSE;
static bool WaitConfirmBeforeJump = FALSE;				//
static bool WaitConfirmBeforeErase = FALSE;				//擦除前等待78负反馈的确认信息
/*========================about program================================*/

/*========================about DTC and DIDs================================*/	
#if USE_MALLOC
static DTCNode* DTCHeader;
static DIDNode* DIDHeader;
static DTCGroupNode *GroupHeader;
#else
static DTCNode DTCS[MAX_DTC_NUMBER];
static uint8_t DTCAdded;
static DIDNode DIDS[MAX_DID_NUMBER];
static uint8_t DIDAdded;
static Snapshot SnapShots[MAX_SNAPSHOT_NUMBER];
static uint8_t SnapShotAdded = 0;
//static Snapshot ExtendedData[MAX_EXTENDED_DATA_NUMBER];
//static uint8_t ExtendedDataAdded = 0;
static uint8_t AgedCounterRecord = 0;
static uint8_t AgingCounterRecord = 0;
static uint8_t OccurenceCounterRecord = 0;
static uint8_t PendingCounterRecord = 0;

static DTCGroupNode DTCGROUPS[MAX_GROUP_NUMBER];
static uint8_t DTCGroupAdded;
#endif
#if USE_J1939_DTC
static bool DiagDM1Enable = TRUE;
#endif
static bool DtcAvailibaleMask;
static DiagTimer DtcTimer;
static uint16_t ModuleEEpromStartAddr;
static uint16_t EEpromSizeForUse;
static uint16_t EEpromUsed;
#define DTC_BYTE_NUMBER_TO_SAVE	5
bool EnableDTCDetect;						//使能故障码检测
bool HighVoltage;								//过电压标志
bool LowVoltage;								//欠电压标志
bool SaveDTCInBlock;
static uint8_t DTCSaved;
static uint8_t DM1DTCNumber;					//J1939 DTC number
static DiagTimer J1939Timer;					//j1939 timer
static DiagTimer DTCSavedTimer;
/*========================about DTC and DIDs================================*/

/*========================about reset================================*/
static uint8_t ResetTypeSupport;//bit0: 01 sub function supported
						//bit1: 02 sub function supported
						//bit2:03 sub function supported
						//bit3:04 sub function supported
						//bit4:05 sub function supported
						//bit5:posresonpse supress supported
#define Service11SupressSupported()  ((ResetTypeSupport & 0x20) != 0)
#define Service11Sub01Supported()  ((ResetTypeSupport & 0x01) != 0)
#define Service11Sub02Supported()  ((ResetTypeSupport & 0x02) != 0)
#define Service11Sub03Supported()  ((ResetTypeSupport & 0x04) != 0)
#define Service11Sub04Supported()  ((ResetTypeSupport & 0x08) != 0)
#define Service11Sub05Supported()  ((ResetTypeSupport & 0x10) != 0)
static ResetCallBack ResetCallBackFun;
static EcuResetType m_EcuResetType;						//ECU复位类型
static bool WaitConfimBeforeReset = FALSE;
/*========================about reset===============================*/

/*========================about tester present===============================*/
static uint8_t TesterPresentSuppport;//posresonpse supress supported
#define Service3ESupressSupported()   ((TesterPresentSuppport & 0x01) != 0)
#define Service85SupressSupported()   ((TesterPresentSuppport & 0x02) != 0)
/*========================about tester present===============================*/

/*========================about commulication control===============================*/	

static uint8_t CommTypeSupport;//bit0:00 sub function supported
							//bit1:01 sub function supported
							//bit2:02 sub function supported
							//bit3:03 sub function supported
							//bit4:01 type supported
							//bit5:02 type supported
							//bit6:03 type supported
							//bit6:posresponse supress supported
#define Service28Sub00Suppoted() ((CommTypeSupport & 0x01) != 0)
#define Service28Sub01Suppoted() ((CommTypeSupport & 0x02) != 0)
#define Service28Sub02Suppoted() ((CommTypeSupport & 0x04) != 0)
#define Service28Sub03Suppoted() ((CommTypeSupport & 0x08) != 0)
#define Service28Type01Suppoted() ((CommTypeSupport & 0x10) != 0)
#define Service28Type02Suppoted() ((CommTypeSupport & 0x20) != 0)
#define Service28Type03Suppoted() ((CommTypeSupport & 0x40) != 0)
#define Service28SupressSupported() ((CommTypeSupport & 0x80) != 0)
static CommCallBack commCallBack;
/*========================about commulication control===============================*/	

/*========================about factory mode use ===============================*/
void Diagnostic_DTCDefaultValue(void);
/*========================about factory mode use===============================*/

SessionService ServiceList[SERVICE_NUMBER] = {
	{FALSE, SESSION_CONTROL,			LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_ZERO,			Service10Handle},//0X10
	{FALSE, RESET_ECU,					LEVEL_UNSUPPORT,	LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_ZERO,			LEVEL_ZERO,			Service11Handle},//0X11
	{FALSE, SECURITY_ACCESS,			LEVEL_UNSUPPORT,	LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service27Handle},//0X27
	{FALSE, COMMUNICATION_CONTROL,		LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_ZERO,			Service28Handle},//0X28
	{FALSE, TESTER_PRESENT,				LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_ZERO,			Service3EHandle},//0X3E
	{FALSE, GET_TIME_PARAM,				LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service83Handle},//0X83
	{FALSE, SECURITY_DATA_TRANSMISSION,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service84Handle},//0X84
	{FALSE, CONTROL_DTC_SETTING,		LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_ZERO,			Service85Handle},//0X85
	{FALSE, RESPONSE_ON_EVENT,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service86Handle},//0X86
	{FALSE, LINK_CONTROL,				LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service87Handle},//0X87
	{FALSE, READ_DATA_BY_ID,			LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service22Handle},//0X22
	{FALSE, READ_MEMORY_BY_ADDRESS,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service23Handle},//0X23
	{FALSE, READ_SCALING_DATA_BY_ID,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service24Handle},//0X24
	{FALSE, READ_DATA_PERIOD_ID,		LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service2AHandle},//0X2A
	{FALSE, DYNAMICALLY_DEFINE_DATA_ID,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service2CHandle},//0X2C
	{FALSE, WRITE_MEMORY_BY_ADDRESS,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service3DHandle},//0X3D
	{FALSE, WRITE_DATA_BY_ID,			LEVEL_UNSUPPORT,	LEVEL_ONE,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service2EHandle},//0X2E
	//{FALSE, WRITE_DATA_BY_ID,				LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_ONE,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service2EHandle},//0X2E
	{FALSE, CLEAR_DTC_INFO,				LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_ZERO,			LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_ZERO,			Service14Handle},//0X14
	{FALSE, READ_DTC_INFO,				LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_ZERO,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service19Handle},//0X19
	{FALSE, IO_CONTROL_BY_ID,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_ONE,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service2FHandle},//0X2F
	{FALSE, ROUTINE_CONTROL,			LEVEL_UNSUPPORT,	LEVEL_ONE,			LEVEL_ONE,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service31Handle},//0X31
	{FALSE, REQUEST_DOWNLOAD,			LEVEL_UNSUPPORT,	LEVEL_ONE,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service34Handle},//0X34
	{FALSE, REQUEST_UPLOAD,				LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service35Handle},//0X35
	{FALSE, TRANSMIT_DATA,				LEVEL_UNSUPPORT,	LEVEL_ONE,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service36Handle},//0X36
	{FALSE, REQUEST_TRANSFER_EXIT,		LEVEL_UNSUPPORT,	LEVEL_ONE,			LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	LEVEL_UNSUPPORT,	Service37Handle},//0X37
};

/* Private functions ---------------------------------------------------------*/
/* Public functions ----------------------------------------------------------*/

/*========interface for application layer setting diagnostic parameters==============*/
bool InitAddSecurityAlgorithm(SecurityLevel level, SecurityFun AlgoritthmFun,byte SeedSubFunctionNum,byte KeySubFunctionNum , uint8_t* FaultCounter,uint8_t FaultLimitCounter , uint32_t UnlockFailedDelayTimeMS, SubFunSuppInSession SubFuntioncSupportedInSession,uint8_t KeySize)
{
	uint8_t i;
	for(i = 0 ; i < 3 ; i++)
	{
		if(UnlockList[i].valid == FALSE)
		{
			UnlockList[i].UnlockFunction = AlgoritthmFun;
			UnlockList[i].seedID = SeedSubFunctionNum;
			UnlockList[i].keyID = KeySubFunctionNum;
			UnlockList[i].level = level;
			UnlockList[i].valid = TRUE;
			UnlockList[i].FaultCounterAddr = (ModuleEEpromStartAddr + EEpromUsed);
			EEpromUsed += 1;
			UnlockList[i].FaultLimitCounter = FaultLimitCounter;
			UnlockList[i].UnlockFailedDelayTime = UnlockFailedDelayTimeMS;
			UnlockList[i].subFunctionSupported = SubFuntioncSupportedInSession;
			UnlockList[i].KeySize = KeySize;
			return TRUE;
		}
	}
	return FALSE;
}

bool InitSetSessionSupportAndSecurityAccess(bool support ,uint8_t service,uint8_t PHYDefaultSession_Security,	uint8_t PHYProgramSeesion_Security,	uint8_t PHYExtendedSession_Security,	uint8_t FUNDefaultSession_Security,	uint8_t FUNProgramSeesion_Security,	uint8_t FUNExtendedSession_Security)
{
	uint8_t i;
	if(support == FALSE)
	{
		//warning ,we suggest set service supported when init,or we can not access this service
	}
	for(i = 0 ; i < SERVICE_NUMBER ; i++)
	{
		if(ServiceList[i].serviceName ==  service)
		{
			ServiceList[i].FUNDefaultSession_Security = FUNDefaultSession_Security;
			ServiceList[i].FUNExtendedSession_Security = FUNExtendedSession_Security;
			ServiceList[i].FUNProgramSeesion_Security = FUNProgramSeesion_Security;
			ServiceList[i].PHYDefaultSession_Security = PHYDefaultSession_Security;
			ServiceList[i].PHYExtendedSession_Security = PHYExtendedSession_Security;
			ServiceList[i].PHYProgramSeesion_Security = PHYProgramSeesion_Security;
			ServiceList[i].support = support;
			return TRUE;	
		}
	}
	return FALSE;
}

void InitAddDID(uint16_t DID , uint8_t DataLength , uint8_t* DataPointer , DIDType DidType , IoControl ControlFun , ReadWriteAttr RWAttr)
{
	#if USE_MALLOC
	DIDNode* didNode = (DIDNode*)malloc(sizeof(DIDNode));
	DIDNode* temp = DIDHeader;
	didNode->ID = DID;
	didNode->dataLength = DataLength;
	didNode->Callback = ControlFun;
	didNode->didType = DidType;
	didNode->RWAttr = RWAttr;
	didNode->next = NULL;
	if(didNode->RWAttr == READWRITE && DidType == EEPROM_DID)
	{
		didNode->dataPointer = (byte*)(ModuleEEpromStartAddr + EEpromUsed);
		EEpromUsed += DataLength;
	}
	else
	{
		didNode->dataPointer = DataPointer;
	}

	if(temp == NULL)
	{
		DIDHeader = didNode;
	}
	else
	{
		while(temp->next != NULL)
		{
			temp = temp->next;
		}
		temp->next = didNode;
	}
	#else
	if(DIDAdded < MAX_DID_NUMBER)
	{
		DIDS[DIDAdded].ID = DID;
		DIDS[DIDAdded].dataLength = DataLength;
		DIDS[DIDAdded].Callback = ControlFun;
		DIDS[DIDAdded].didType = DidType;
		DIDS[DIDAdded].RWAttr = RWAttr;
		if(RWAttr == READWRITE && DidType == EEPROM_DID)
		{
			DIDS[DIDAdded].dataPointer = (byte*)(ModuleEEpromStartAddr + EEpromUsed);
			EEpromUsed += DataLength;
		}
		else
		{
			DIDS[DIDAdded].dataPointer = DataPointer;
		}

		DIDAdded++;
	}
	#endif
}

#if USE_J1939_DTC
bool InitAddDTC(uint32_t DTCCode,DetectFun MonitorFun,byte DectecPeroid, byte ValidTimes,DTCLevel dtcLevel,uint32_t spn, uint8_t fmi)
#else
bool InitAddDTC(uint32_t DTCCode,DetectFun MonitorFun,byte DectecPeroid, byte ValidTimes,DTCLevel dtcLevel)
#endif
{
	#if USE_MALLOC
	DTCNode* dtcNode = (DTCNode*)malloc(sizeof(DTCNode));
	DTCNode* temp = DTCHeader;
	dtcNode->DTCCode = DTCCode;
	dtcNode->DetectFunction = MonitorFun;
	dtcNode->dtcLevel = dtcLevel;
	dtcNode->EEpromAddr = ModuleEEpromStartAddr + EEpromUsed;
	dtcNode->next = NULL;
	EEpromUsed += DTC_BYTE_NUMBER_TO_SAVE;

	if(temp == NULL)
	{
		DTCHeader = dtcNode;
	}
	else
	{
		while(temp->next != NULL)
		{
			temp = temp->next;
		}
		temp->next = dtcNode;
	}
	return TRUE;
	#else
	if(DTCAdded < MAX_DTC_NUMBER)
	{
		DTCS[DTCAdded].DTCCode = DTCCode;
		DTCS[DTCAdded].DetectFunction = MonitorFun;
		DTCS[DTCAdded].TripLimitTimes = ValidTimes;
		#if USE_J1939_DTC
		DTCS[DTCAdded].dtcLevel = dtcLevel;
		DTCS[DTCAdded].DM1Code.SPN = spn;
		DTCS[DTCAdded].DM1Code.FMI = fmi;
		//DTCS[DTCAdded].DM1Code.CM = 0;
		DTCS[DTCAdded].DM1Code.OC = 0;
		#endif
		DTCS[DTCAdded].EEpromAddr = ModuleEEpromStartAddr + EEpromUsed;
		EEpromUsed += DTC_BYTE_NUMBER_TO_SAVE;

		DTCAdded++;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
	#endif
}

void InitAddDTCSnapShot(uint8_t recordNumber , uint16_t ID , uint8_t* datap , uint8_t size)
{
	if(SnapShotAdded < MAX_SNAPSHOT_NUMBER)
	{
		SnapShots[SnapShotAdded].snapshotRecord = recordNumber;
		SnapShots[SnapShotAdded].snapshotID = ID;
		SnapShots[SnapShotAdded].dataPointer = datap;
		SnapShots[SnapShotAdded].dataLength = size;
		SnapShotAdded++;
	}
}

void InitSetAgingCounterRecordNumber(uint8_t RecordNumer)
{
	AgingCounterRecord = RecordNumer;
}

void InitSetAgedCounterRecordNumber(uint8_t RecordNumer)
{
	AgedCounterRecord = RecordNumer;
}

void InitSetOccurrenceCounterRecordNumber(uint8_t RecordNumer)
{
	OccurenceCounterRecord = RecordNumer;
}

void InitSetPendingCounterRecordNumber(uint8_t RecordNumer)
{
	PendingCounterRecord = RecordNumer;
}

#if 0
void InitAddDTCExtendedData(uint16_t ID , uint8_t* datap , uint8_t size)
{
	if(ExtendedDataAdded < MAX_EXTENDED_DATA_NUMBER)
	{
		ExtendedData[ExtendedDataAdded].snapshotID = ID;
		ExtendedData[ExtendedDataAdded].dataPointer = datap;
		ExtendedData[ExtendedDataAdded].dataLength = size;
		ExtendedDataAdded++;
	}
}
#endif

void InitSetDTCAvailiableMask(uint8_t AvailiableMask)
{
	DtcAvailibaleMask = AvailiableMask;
}

void InitAddDTCGroup(uint32_t Group)
{
	#if USE_MALLOC
	DTCGroupNode* temp = GroupHeader;
	DTCGroupNode* groupNode = (DTCGroupNode*)malloc(sizeof(DTCGroupNode));
	groupNode->GroupID = Group;
	groupNode->next = NULL;
	if(temp == NULL)
	{
		GroupHeader = groupNode;
	}
	else
	{
		while(temp->next != NULL)
		{
			temp = temp->next;
		}
		temp->next = groupNode;
	}
	#else
	if(DTCGroupAdded < MAX_GROUP_NUMBER)
	{
		DTCGROUPS[DTCGroupAdded].GroupID = Group;
		DTCGroupAdded++;
	}
	#endif
}

void InitSetSysResetParam(bool support01 , bool support02 , bool support03 , bool support04 , bool support05 , ResetCallBack callback,bool supressPosResponse)
{
	ResetTypeSupport = support01 | (support02<<1) | (support03 << 2) | (support04 << 3) | (support05 << 4) | (supressPosResponse << 5);
	ResetCallBackFun = callback;
}

void InitSetCommControlParam(bool supportSubFun00, bool supportSubFun01 , bool supportSubFun02 , bool supportSubFun03 , bool supportType01, bool supportType02, bool supportType03, CommCallBack callback, bool supressPosResponse)
{
	CommTypeSupport = supportSubFun00 | (supportSubFun01 << 1) | (supportSubFun02 << 2)  | (supportSubFun03 << 3) | (supportType01 << 4) | (supportType02 << 5) | (supportType03 << 6) | (supressPosResponse << 7);
	commCallBack = callback;
}

void InitSetSessionControlParam(bool supportSub01, bool supportSub02,bool supportSub03, bool sub02SupportedInDefaultSession, bool sub03SupportedInProgramSession, bool supressPosResponse)
{
	SessionSupport = supportSub01 | (supportSub02 << 1 ) | (supportSub03 << 2) | (sub02SupportedInDefaultSession << 3) | (sub03SupportedInProgramSession << 4) | (supressPosResponse << 5);
}

void InitSetTesterPresentSupress(bool supressPosResponse)
{
	TesterPresentSuppport = (supressPosResponse & 0x01);
}

void InitSetDTCControlSupress(bool supressPosResponse)
{
	TesterPresentSuppport |= (supressPosResponse << 1);
}

void InitSetCanDriverVersionDID(uint16_t m_DID)
{
	InitAddDID(m_DID,3 , DriverVersion ,   	REALTIME_DID , NULL , READONLY);
}

void InitSetCanNMVersionDID(uint16_t m_DID)
{
	InitAddDID(m_DID,3 , NMVersion ,   		REALTIME_DID , NULL , READONLY);
}

void InitSetCanDiagnosticVersionDID(uint16_t m_DID)
{
	InitAddDID(m_DID,3 , DiagnosticVersion ,   	REALTIME_DID , NULL , READONLY);
}

void InitSetCanDataBaseVersionDID(uint16_t m_DID)
{
	InitAddDID(m_DID,2 , DatabaseVersion ,   	REALTIME_DID , NULL , READONLY);
}

void InitSetCurrentSessionDID(uint16_t m_DID)
{
	InitAddDID(m_DID,1 , &m_CurrSessionType ,      REALTIME_DID , NULL , READONLY);
}



/************set netwrok layer parameters********/
void Diagnostic_SetNLParam(uint8_t TimeAs, uint8_t TimeBs, uint8_t TimeCr, uint8_t TimeAr, uint8_t TimeBr, uint8_t TimeCs, 
	uint8_t BlockSize, uint8_t m_STmin, uint8_t FillData)
{
	NetworkLayer_SetParam(TimeAs , TimeBs , TimeCr , TimeAr , TimeBr , TimeCs , BlockSize , m_STmin , HALF_DUPLEX , DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , FillData);
}

void Diagnostic_Set2ndReqAndResID(uint32_t requestId1, uint32_t responseId1,uint32_t funRequestId1)
{
	TesterPhyID1 = requestId1;
	EcuID1 = responseId1;
	TesterFunID1 = funRequestId1;
	NetworkLayer_SetSecondID(requestId1 , funRequestId1 , responseId1);
}

void Diagnostic_Init(uint32_t requestId, uint32_t responseId, uint32_t funRequestId,uint16_t EEPromStartAddr, uint16_t EEpromSize, SendCANFun sendFun,uint16_t p2CanServerMax, uint16_t p2ECanServerMax)
{
	TesterPhyID = requestId;
	EcuID = responseId;
	TesterFunID = funRequestId;
	
	N_Ta = (uint8_t)EcuID;
	N_Sa = (uint8_t)(EcuID >> 8);
	NetworkLayer_InitParam(TesterPhyID, TesterFunID , EcuID , sendFun);
	TPCMSetParamBAM(sendFun);

	P2CanServerMax = p2CanServerMax;
	P2ECanServerMax = p2ECanServerMax;
	
	EnableDTCDetect = TRUE;
	SaveDTCInBlock = FALSE;
	DM1DTCNumber = 0;
	HighVoltage = FALSE;
	ResponsePending = FALSE;
	m_CurrSessionType = ECU_DEFAULT_SESSION;
	
	WaitConfirmBeforeJump = FALSE;
	WaitConfimBeforeReset = FALSE;
	SessionSupport = 0;

	#if USE_MALLOC
	DTCHeader = NULL;//DIDList.next = NULL;
	DIDHeader = NULL;//DTCList.next = NULL;
	GroupHeader = NULL;//DTCGroupList.next = NULL;
	#else
	DTCAdded = 0;
	DIDAdded = 0;
	DTCGroupAdded = 0;
	#endif
	ResetTypeSupport = 0;
	ResetCallBackFun = NULL;
	CommTypeSupport = 0;
	commCallBack = NULL;
	ModuleEEpromStartAddr = EEPromStartAddr;
	EEpromSizeForUse = EEpromSize;
	EEpromUsed = 0;
	memset(UnlockList, 0 ,sizeof(UnlockList));
	Diagnostic_EEProm_Init();

}
/*========interface for application layer setting diagnostic parameters==============*/


void GenerateSeed(uint8_t *seed, uint32_t length)
{
	uint32_t SystemTick = DiagTimer_GetTickCount();
	#if 0
	seed[0] = 0x49;
	seed[1] = 0xB4;
	seed[2] = 0xE0;
	seed[3] = 0x6C;
	#else
	seed[0] = (uint8_t)SystemTick ^ (uint8_t)(SystemTick >> 3);
	seed[1] = (uint8_t)SystemTick ^ (uint8_t)(SystemTick >> 7);
	seed[2] = (uint8_t)SystemTick ^ (uint8_t)(SystemTick >> 11);
	seed[3] = (uint8_t)(SystemTick>>3) ^ (uint8_t)(SystemTick >> 11);
	#endif
}

void GotoSession(SessionType session)
{
	if(session != ECU_DEFAULT_SESSION)
	{
		DiagTimer_Set(&S3serverTimer, 5000);
	}
	else
	{
		/***********when s3 timeout,session change, comunication recover********/
		if(commCallBack != NULL)
		{
			commCallBack(0x00, 0x03);
		}
		
		/***********when s3 timeout,session change, DTC enable***********/
		EnableDTCDetect = TRUE;
	}
	m_CurrSessionType = session;
	m_SecurityLevel = LEVEL_ZERO;//session change ECU lock even if from extended session to extended session
	if(m_UnlockStep != WAIT_DELAY)
	{
		m_UnlockStep = WAIT_SEED_REQ;//by ukign 2016.04.01
	}

	if(m_CurrSessionType == ECU_PAOGRAM_SESSION)
	{
		WaitConfirmBeforeJump = TRUE;
	}
}

void Service10PosResponse(SessionType session)
{
	DiagnosticBuffTX[0] = 0x50;
	DiagnosticBuffTX[1] = session;
	DiagnosticBuffTX[2] = (uint8_t)(P2CanServerMax >> 8);
	DiagnosticBuffTX[3] = (uint8_t)P2CanServerMax;
	DiagnosticBuffTX[4] = (uint8_t)(P2ECanServerMax >> 8);
	DiagnosticBuffTX[5] = (uint8_t)P2ECanServerMax;
	ResponseLength = 6;
}

void Service10Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	uint8_t SubFunction;
	uint8_t suppressPosRspMsgIndicationBit;
	m_NRC = PR;
	//printf("service 10 handler\r\n");
	if(length == 2)
	{
		if(Service10SupressSupproted())
		{
			SubFunction = *(MessageData + 1) & 0x7F;
			suppressPosRspMsgIndicationBit = *(MessageData + 1) & 0x80;
		}
		else
		{
			SubFunction = *(MessageData + 1);
			suppressPosRspMsgIndicationBit = 0;
		}
		
		switch(SubFunction)/* get sub-function parameter value without bit 7 */
		{
			case ECU_DEFAULT_SESSION: /* test if sub-function parameter value is supported */
				if(!Service10Sub01Supported())
				{
					m_NRC = SFNS;
				}
				break;
			case ECU_EXTENED_SESSION: /* test if sub-function parameter value is supported */
				if(!Service10Sub03Supported())
				{
					m_NRC = SFNS;
				}
				else
				{
					if(m_CurrSessionType == ECU_PAOGRAM_SESSION && !Service10Sub02To03OK())
					{
						m_NRC = SFNSIAS;
					}
				}
				break;
			case ECU_PAOGRAM_SESSION: /* test if sub-function parameter value is supported */
				if(!Service10Sub02Supported())
				{
					m_NRC = SFNS;
				}
				else
				{
					if(m_CurrSessionType == ECU_DEFAULT_SESSION && !Service10Sub01To02OK())
					{
						m_NRC = SFNSIAS;
					}
				}
				break;
			default:
				m_NRC = SFNS; /* NRC 0x12: sub-functionNotSupported *///
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}
	
	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
		Service10PosResponse(SubFunction);
	}

	if(m_NRC == PR)
	{
		GotoSession(SubFunction);
	}
}

void Service11Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	uint8_t SubFunction;
	uint8_t suppressPosRspMsgIndicationBit;
	m_NRC = PR;
	if(length == 2)
	{
		if(Service11SupressSupported())
		{
			SubFunction = *(MessageData + 1) & 0x7F;
			m_EcuResetType = SubFunction;
			suppressPosRspMsgIndicationBit = *(MessageData + 1) & 0x80;
		}
		else
		{
			SubFunction = *(MessageData + 1);
			m_EcuResetType = SubFunction;
			suppressPosRspMsgIndicationBit = 0;
		}
		
		switch(SubFunction)/* get sub-function parameter value without bit 7 */
		{
			case HARD_RESET:
				{
					if(!Service11Sub01Supported())
					{
						m_NRC = SFNS;
					}
				}
				break;
			case KEY_OFF_ON_RESET: /* test if sub-function parameter value is supported */
				{
					if(!Service11Sub02Supported())
					{
						m_NRC = SFNS;
					}
				}
				break;
			case SOFT_RESET: /* test if sub-function parameter value is supported */
				{
					if(!Service11Sub03Supported())
					{
						m_NRC = SFNS;
					}
				}
				break;
			case ENABLE_RAPID_POWER_SHUTDOWN:
				{
					if(!Service11Sub04Supported())
					{
						m_NRC = SFNS;
					}
				}
				break;
			case DISABLE_RAPID_POWER_SHUTDOWN:
				{
					if(!Service11Sub05Supported())
					{
						m_NRC = SFNS;
					}
				}
				break;
			default:
				m_NRC = SFNS; /* NRC 0x12: sub-functionNotSupported */
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}
	
	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */		
		DiagnosticBuffTX[0] = CurrentService + 0x40;
		DiagnosticBuffTX[1] = SubFunction;
		ResponseLength = 2;
	}

	if(m_NRC == PR)
	{
		if(suppressResponse == FALSE)//需要正响应时，等级响应结束
		{
			WaitConfimBeforeReset = TRUE;
		}
		else//不需要正响应时直接复位
		{
			if(ResetCallBackFun != NULL)
			{
				ResetCallBackFun(m_EcuResetType);
			}
		}
	}
}

void Service27Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	uint8_t SubFunction ;
	uint8_t suppressPosRspMsgIndicationBit;
	m_NRC = PR;
	if(length >= 2)//min length check
	{
		uint8_t index = 0;
		bool subFunctionExist = FALSE;
		bool subFunctionSupInSession = FALSE;
		SubFunction = *(MessageData + 1) & 0x7F;
		suppressPosRspMsgIndicationBit = *(MessageData + 1) & 0x80;
		while(index < 3 && (!subFunctionExist))
		{
			if(UnlockList[index].valid == TRUE && (UnlockList[index].seedID == SubFunction || UnlockList[index].keyID == SubFunction))
			{
				subFunctionExist = TRUE;
				if(m_CurrSessionType == ECU_DEFAULT_SESSION)
				{
					if(UnlockList[index].subFunctionSupported & SUB_DEFAULT)
					{
						subFunctionSupInSession = TRUE;
					}
					else
					{
						subFunctionSupInSession = FALSE;
					}
				}
				else if(m_CurrSessionType == ECU_PAOGRAM_SESSION)
				{
					if(UnlockList[index].subFunctionSupported & SUB_PROGRAM)
					{
						subFunctionSupInSession = TRUE;
					}
					else
					{
						subFunctionSupInSession = FALSE;
					}
				}
				else if(m_CurrSessionType == ECU_EXTENED_SESSION)
				{
					if(UnlockList[index].subFunctionSupported & SUB_EXTENDED)
					{
						subFunctionSupInSession = TRUE;
					}
					else
					{
						subFunctionSupInSession = FALSE;
					}
				}
			}
			else
			{
				index++;
			}
		}
		
		if(subFunctionExist && subFunctionSupInSession)//sub function check ok
		{
			if(UnlockList[index].seedID == SubFunction)//request seed
			{
				if(length == 2)//length check again
				{
					if(m_UnlockStep == WAIT_DELAY)
					{
						m_NRC = RTDNE;
					}
					else if(m_UnlockStep == UNLOCKED && m_SecurityLevel == UnlockList[index].level)//by ukign 20160401,when ECU unlocked,retrun seed is all zero
					{
						DiagnosticBuffTX[0] = 0x67;
						DiagnosticBuffTX[1] = SubFunction;
						DiagnosticBuffTX[2] = 0;
						DiagnosticBuffTX[3] = 0;
						DiagnosticBuffTX[4] = 0;
						DiagnosticBuffTX[5] = 0;
						ResponseLength = UnlockList[index].KeySize + 2;
					}
					else
					{
						GenerateSeed(Seed,4);
						DiagnosticBuffTX[0] = 0x67;
						DiagnosticBuffTX[1] = SubFunction;
						DiagnosticBuffTX[2] = Seed[0];
						DiagnosticBuffTX[3] = Seed[1];
						if(UnlockList[index].KeySize == 2)
						{
							
						}
						else if(UnlockList[index].KeySize == 4)
						{
							DiagnosticBuffTX[4] = Seed[2];
							DiagnosticBuffTX[5] = Seed[3];
						}
						m_UnlockStep = WAIT_KEY;
						ResponseLength = UnlockList[index].KeySize + 2;
					}
				}
				else
				{
					m_NRC = IMLOIF;
				}
			}
			else if(SubFunction ==  UnlockList[index].keyID)//send key
			{
				if(length == UnlockList[index].KeySize + 2)
				{
					if(m_UnlockStep == WAIT_KEY)
					{
						//uint32_t key1 = UnlockList[index].UnlockFunction(*(uint32_t*)Seed); 
						*((uint32_t*)key) = UnlockList[index].UnlockFunction(*(uint32_t*)Seed);
						Diagnostic_EEProm_Read(UnlockList[index].FaultCounterAddr, 1 ,&UnlockList[index].FaultCounter);
						if(((key[0] == *(MessageData + 2) && key[1] == *(MessageData + 3)) && UnlockList[index].KeySize == 2) ||
							((key[0] == *(MessageData + 2) && key[1] == *(MessageData + 3) && key[2] == *(MessageData + 4) && key[3] == *(MessageData + 5)) && UnlockList[index].KeySize == 4))
						{
							m_UnlockStep = UNLOCKED;
							UnlockList[index].FaultCounter = 0;
							m_SecurityLevel = UnlockList[index].level;
							DiagnosticBuffTX[0] = 0x67;
							DiagnosticBuffTX[1] = SubFunction;
							ResponseLength = 2;
						}
						else 
						{
							UnlockList[index].FaultCounter++;
							if(UnlockList[index].FaultCounter >= 3)
							{
								m_NRC = ENOA;
								m_UnlockStep = WAIT_DELAY;
								DiagTimer_Set(&UnlockList[index].SecurityLockTimer , UnlockList[index].UnlockFailedDelayTime);
							}
							else
							{
								m_NRC = IK;
								m_UnlockStep = WAIT_SEED_REQ;
							}
						}
						Diagnostic_EEProm_Write(UnlockList[index].FaultCounterAddr, 1 ,&UnlockList[index].FaultCounter);					
					}
					else if(m_UnlockStep == WAIT_DELAY)
					{
						m_NRC = RTDNE;
					}
					else if(m_UnlockStep == WAIT_SEED_REQ)//send key before request seed order error
					{
						m_NRC = RSE;
					}
					else//unlocked condition error
					{
						m_NRC = RSE;
					}
				}
				else
				{
					m_NRC = IMLOIF;
				}
			}
		}
		else
		{
			if(!subFunctionExist)
			{
				m_NRC = SFNS;
			}
			else if(!subFunctionSupInSession)
			{
				m_NRC = SFNSIAS;
			}
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}
	
	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
	}

}

void Service28Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	uint8_t SubFunction;
	uint8_t ControlParam;
	uint8_t suppressPosRspMsgIndicationBit;
	m_NRC = PR;
	if(length == 3)
	{
		if(Service28SupressSupported())
		{
			SubFunction = *(MessageData + 1) & 0x7F;
			suppressPosRspMsgIndicationBit = *(MessageData + 1) & 0x80;
		}
		else
		{
			SubFunction = *(MessageData + 1);
			suppressPosRspMsgIndicationBit = 0;
		}
		ControlParam = *(MessageData + 2);
		
		switch(SubFunction)/* get sub-function parameter value without bit 7 */
		{
			case ERXTX:
				{
					if(!Service28Sub00Suppoted())
					{
						m_NRC = SFNS;
					}
				}
				break;
			case ERXDTX: /* test if sub-function parameter value is supported */
				{
					if(!Service28Sub01Suppoted())
					{
						m_NRC = SFNS;
					}
				}
				break;
			case DRXETX: /* test if sub-function parameter value is supported */
				{
					if(!Service28Sub02Suppoted())
					{
						m_NRC = SFNS;
					}
				}
				break;
			case DRXTX: /* test if sub-function parameter value is supported */
				{
					if(!Service28Sub03Suppoted())
					{
						m_NRC = SFNS;
					}
				}
				break;
			default:
				m_NRC = SFNS; /* NRC 0x12: sub-functionNotSupported */
		}
		
		if(m_NRC == 0)
		{
			#if 1
			switch(ControlParam)
			{
				case NCM:
					{
						if(!Service28Type01Suppoted())
						{
							m_NRC = ROOR;
						}
					}
					break;
				case NWMCM: /* test if sub-function parameter value is supported */
					{
						if(!Service28Type02Suppoted())
						{
							m_NRC = ROOR;
						}
					}
					break;
				case NWMCM_NCM: /* test if sub-function parameter value is supported */
					{
						if(!Service28Type03Suppoted())
						{
							m_NRC = ROOR;
						}
					}
					break;
				default:
					m_NRC = ROOR; /* NRC 0x12: sub-functionNotSupported */
			}
			#endif
		}
		
	}
	else
	{
		m_NRC = IMLOIF;
	}
	
	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
	}

	if(m_NRC == 0x00)
	{
		if(commCallBack != NULL)
		{
			commCallBack(SubFunction, ControlParam);
		}
		DiagnosticBuffTX[0] = CurrentService + 0x40;
		DiagnosticBuffTX[1] = SubFunction;
		ResponseLength = 2;
	}
}

void Service3EHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	uint8_t SubFunction;
	uint8_t suppressPosRspMsgIndicationBit;
	m_NRC = PR;
	if(length == 2)
	{
		if(Service3ESupressSupported())
		{
			SubFunction = *(MessageData + 1) & 0x7F;
			suppressPosRspMsgIndicationBit = *(MessageData + 1) & 0x80;
		}
		else
		{
			SubFunction = *(MessageData + 1);
			suppressPosRspMsgIndicationBit = 0;
		}
		
		if(SubFunction != 0)
		{
			m_NRC = SFNS;
		}
		else
		{
			DiagnosticBuffTX[0] = CurrentService+ 0x40;
			DiagnosticBuffTX[1] = SubFunction;
			ResponseLength = 2;
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}
	
	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
	}
}

void Service83Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}

void Service84Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}

void Service85Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	uint8_t SubFunction;
	uint8_t suppressPosRspMsgIndicationBit;
	m_NRC = PR;
	if(length == 2)
	{
		if(Service85SupressSupported())
		{
			SubFunction = *(MessageData + 1) & 0x7F;
			suppressPosRspMsgIndicationBit = *(MessageData + 1) & 0x80;
		}
		else
		{
			SubFunction = *(MessageData + 1);
			suppressPosRspMsgIndicationBit = 0;
		}
		
		switch(SubFunction)/* get sub-function parameter value without bit 7 */
		{
			case 0x01:
				{
					EnableDTCDetect = TRUE;
					DiagnosticBuffTX[0] = CurrentService + 0x40;
					DiagnosticBuffTX[1] = SubFunction;
					ResponseLength = 2;
				}
				break;
			case 0x02: /* test if sub-function parameter value is supported */
				{
					EnableDTCDetect = FALSE;
					DiagnosticBuffTX[0] = CurrentService + 0x40;
					DiagnosticBuffTX[1] = SubFunction;
					ResponseLength = 2;
				}
				break;
			default:
				m_NRC = SFNS; /* NRC 0x12: sub-functionNotSupported */
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}
	
	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
	}
}

void Service86Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}

void Service87Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}

DIDNode* SearchDidNode(uint16_t DID)
{
	#if USE_MALLOC
	DIDNode *tmpNode = DIDHeader;
	while(tmpNode != NULL)
	{
		if(tmpNode->ID == DID)
		{
			return tmpNode;
		}
		else
		{
			tmpNode = tmpNode->next;
		}
	}
	return NULL;
	#else
	uint8_t i;
	for(i = 0; i < DIDAdded ; i++)
	{
		if(DIDS[i].ID == DID)
		{
			return DIDS + i;
		}
	}
	return NULL;
	#endif
}

void Service22Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	bool suppressPosRspMsgIndicationBit = FALSE;
	m_NRC = PR;
	if(length == 3)
	{
		uint16_t DID = (*(MessageData + 1) << 8) + *(MessageData + 2);
		DIDNode* didNode = SearchDidNode(DID);
		if(didNode == NULL)
		{
			m_NRC = ROOR;//PID not in our PID list
		}
		else if(didNode->didType == IO_DID)
		{
			if(didNode->RWAttr == WRITEONLY)
			{
				m_NRC = ROOR;//mabe a IO Control DID
			}
			else
			{
				DiagnosticBuffTX[0] = 0x62;
				DiagnosticBuffTX[1] = *(MessageData + 1);
				DiagnosticBuffTX[2] = *(MessageData + 2);
				memcpy(DiagnosticBuffTX + 3 , didNode->dataPointer ,didNode->dataLength);
				
				ResponseLength = didNode->dataLength + 3;
			}
		}
		else
		{
			if(didNode->RWAttr == WRITEONLY)
			{
				m_NRC = ROOR;//this DID maybe supported by 22 service but not supported by 2E service
			}
			else
			{
				DiagnosticBuffTX[0] = 0x62;
				DiagnosticBuffTX[1] = *(MessageData + 1);
				DiagnosticBuffTX[2] = *(MessageData + 2);
				if(didNode->RWAttr == READWRITE && didNode->didType == EEPROM_DID)
				{
					Diagnostic_EEProm_Read((uint32_t)didNode->dataPointer , didNode->dataLength , DiagnosticBuffTX+3);
				}
				else
				{
					memcpy(DiagnosticBuffTX + 3 , didNode->dataPointer ,didNode->dataLength);
				}
				ResponseLength = didNode->dataLength + 3;
			}
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}

	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
	}
}

void Service23Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}

void Service24Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}

void Service2AHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}

void Service2CHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}


void Service2EHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	bool suppressPosRspMsgIndicationBit = FALSE;
	m_NRC = PR;
	if(length >= 3)
	{
		uint16_t DID = (*(MessageData + 1) << 8) + *(MessageData + 2);
		DIDNode* didNode = SearchDidNode(DID);
		if(didNode == NULL)
		{
			m_NRC = ROOR;//PID not in our PID list
		}
		else if(didNode->didType == IO_DID)
		{
			m_NRC = ROOR;//mabe a IO Control DID
		}
		else if(didNode->RWAttr == READONLY)
		{
			m_NRC = ROOR;//this DID maybe supported by 22 service but not supported by 2E service
		}
		else
		{
			if(didNode->dataLength + 3 == length)
			{
				if(didNode->RWAttr == READWRITE && didNode->didType == EEPROM_DID)
				{
					Diagnostic_EEProm_Write((uint32_t)(didNode->dataPointer) , didNode->dataLength , MessageData + 3);
				}
				else
				{
					memcpy(didNode->dataPointer , MessageData + 3, didNode->dataLength);
				}
				DiagnosticBuffTX[0] = 0x6E;
				DiagnosticBuffTX[1] = *(MessageData + 1);
				DiagnosticBuffTX[2] = *(MessageData + 2);
				ResponseLength = 3;
			}
			else
			{
				m_NRC = IMLOIF;
			}
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}
	
	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
	}
}


void Service3DHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	
}

bool SearchDTCGroup(uint32_t group)
{
	#if USE_MALLOC
	DTCGroupNode *temp = GroupHeader;
	bool GroupFound = FALSE;
	while(temp !=NULL && GroupFound == FALSE)
	{
		if(temp->GroupID == group)
		{
			return TRUE;
		}
		else
		{
			temp = temp->next;
		}
	}
	return FALSE;
	
	#else
	
	uint8_t i;
	for(i = 0; i < DTCGroupAdded; i++)
	{
		if(DTCGROUPS[i].GroupID == group)
		{
			return TRUE;
		}
	}
	return FALSE;
	#endif
}

void Service14Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	m_NRC = 0;
	suppressResponse = FALSE;
	if(length == 4)
	{
		uint32_t Group = ((*(MessageData + 1) << 16) + (*(MessageData + 2) << 8) + *(MessageData + 3)) & 0xFFFFFF;
		
		if(SearchDTCGroup(Group))//14229-1 page 183,send pos even if no DTCS are stored
		{
			DiagnosticBuffTX[0] = CurrentService + 0x40;
			ResponseLength = 1;
			Diagnostic_ClearDTC(Group);
		}
		else
		{
			m_NRC = ROOR;
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}
}

void SearchDTCByMaskAndFillResponse(uint8_t mask)
{
	#if USE_MALLOC
	DTCNode* tmpNode = DTCHeader;
	while(tmpNode != NULL)
	{
		if((tmpNode->DTCStatus.DTCStatusByte & mask & DtcAvailibaleMask) != 0)
		{
			DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(tmpNode->DTCCode >> 16);
			DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(tmpNode->DTCCode >> 8);
			DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(tmpNode->DTCCode);
			DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(tmpNode->DTCStatus.DTCStatusByte & DtcAvailibaleMask);
		}
		tmpNode = tmpNode->next;		
	}
	#else
	uint8_t i;
	for(i = 0 ; i < DTCAdded;i++)
	{
		if((DTCS[i].DTCStatus.DTCStatusByte & mask & DtcAvailibaleMask) != 0)
		{
			DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(DTCS[i].DTCCode >> 16);
			DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(DTCS[i].DTCCode >> 8);
			DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(DTCS[i].DTCCode);
			DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(DTCS[i].DTCStatus.DTCStatusByte & DtcAvailibaleMask);
		}
	}
	#endif
}

void FillAllDTCResponse()
{
	#if USE_MALLOC
	DTCNode* tmpNode = DTCHeader;
	while(tmpNode != NULL)
	{
		DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(tmpNode->DTCCode >> 16);
		DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(tmpNode->DTCCode >> 8);
		DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(tmpNode->DTCCode);
		DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(tmpNode->DTCStatus.DTCStatusByte & DtcAvailibaleMask);
		tmpNode = tmpNode->next;		
	}
	#else
	uint8_t i;
	for(i = 0 ; i < DTCAdded;i++)
	{
		DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(DTCS[i].DTCCode >> 16);
		DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(DTCS[i].DTCCode >> 8);
		DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(DTCS[i].DTCCode);
		DiagnosticBuffTX[ResponseLength++] =  (uint8_t)(DTCS[i].DTCStatus.DTCStatusByte & DtcAvailibaleMask);
	}
	#endif
}

uint16_t GetDTCNumberByMask(uint8_t mask)
{
	#if USE_MALLOC
	DTCNode* tmpNode = DTCHeader;
	uint16_t number = 0;
	while(tmpNode != NULL)
	{
		if((tmpNode->DTCStatus.DTCStatusByte & mask & DtcAvailibaleMask) != 0)
		{
			number++;
		}
		tmpNode = tmpNode->next;		
	}
	return number;
	#else
	uint8_t i;
	uint16_t number = 0;
	for(i = 0; i < DTCAdded ; i++)
	{

		if((DTCS[i].DTCStatus.DTCStatusByte & mask & DtcAvailibaleMask) != 0)
		{
			number++;
		}
	}
	return number;
	#endif
}

DTCNode* GetDTCNodeByCode(uint32_t dtcCode)
{
	#if USE_MALLOC
	return NULL;
	#else
	uint8_t i;
	for(i = 0 ; i < DTCAdded;i++)
	{	
		if((DTCS[i].DTCCode & 0xFFFFFF) == (dtcCode & 0xFFFFFF))
		{
			return DTCS+i;
		}
	}
	return NULL;
	#endif
}

void FillDTCSnapshotReponse(uint16_t EEPromAddr, uint8_t ReordMask)
{
	uint8_t i,j;
	uint8_t snapshotRecordNumber = 0;
	ResponseLength = 8;
	if(ReordMask != 0xFF)
	{
		for(i = 0 ; i < SnapShotAdded ; i++)
		{
			if(SnapShots[i].snapshotRecord == ReordMask)
			{
				snapshotRecordNumber++;
				DiagnosticBuffTX[ResponseLength] = (uint8_t)(SnapShots[i].snapshotID >> 8);
				DiagnosticBuffTX[ResponseLength + 1] = (uint8_t)(SnapShots[i].snapshotID);
				ResponseLength += 2;
				Diagnostic_EEProm_Read(EEPromAddr, SnapShots[i].dataLength , DiagnosticBuffTX + ResponseLength);
				ResponseLength += SnapShots[i].dataLength;
			}
		}
		DiagnosticBuffTX[7]  = snapshotRecordNumber;
	}
	else
	{
		uint8_t currentRecord = SnapShots[0].snapshotRecord;
		uint8_t currentRecordNumber = 0;
		uint8_t currentRecordSizeIndex = 7;
		for(i = 0 ; i < SnapShotAdded ; i++)
		{
			if(SnapShots[i].snapshotRecord == currentRecord)
			{
				currentRecordNumber++;
				DiagnosticBuffTX[ResponseLength] = (uint8_t)(SnapShots[i].snapshotID >> 8);
				DiagnosticBuffTX[ResponseLength + 1] = (uint8_t)(SnapShots[i].snapshotID);
				ResponseLength += 2;
				Diagnostic_EEProm_Read(EEPromAddr, SnapShots[i].dataLength , DiagnosticBuffTX + ResponseLength);
				ResponseLength += SnapShots[i].dataLength;
			}
			else
			{
				currentRecord = SnapShots[i].snapshotRecord;
				DiagnosticBuffTX[currentRecordSizeIndex] = currentRecordNumber;
				currentRecordSizeIndex = ResponseLength;
				ResponseLength++;
				currentRecordNumber = 1;
				DiagnosticBuffTX[ResponseLength] = (uint8_t)(SnapShots[i].snapshotID >> 8);
				DiagnosticBuffTX[ResponseLength + 1] = (uint8_t)(SnapShots[i].snapshotID);
				ResponseLength += 2;
				Diagnostic_EEProm_Read(EEPromAddr, SnapShots[i].dataLength , DiagnosticBuffTX + ResponseLength);
				ResponseLength += SnapShots[i].dataLength;
			}
		}
		DiagnosticBuffTX[currentRecordSizeIndex]  = currentRecordNumber;
	}
}

void Service19Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	uint8_t SubFunction;
	uint8_t suppressPosRspMsgIndicationBit;
	uint8_t mask = *(MessageData + 2);
	m_NRC = 0;
	if(length >= 2)
	{
		SubFunction = *(MessageData + 1);
		suppressPosRspMsgIndicationBit = FALSE;
		switch(SubFunction)
		{
			case REPORT_DTCNUMBER_BY_MASK:
				{
					if(length == 3)
					{
						uint16_t number= GetDTCNumberByMask(mask);
						DiagnosticBuffTX[0]  = 0x59;
						DiagnosticBuffTX[1]  = 0x01;
						DiagnosticBuffTX[2]  = DtcAvailibaleMask;
						DiagnosticBuffTX[3]  = 0x00;//ISO15031 DTCFormat
						DiagnosticBuffTX[4]  = (uint8_t)(number >> 8);
						DiagnosticBuffTX[5]  = (uint8_t)(number);
						ResponseLength = 6;
					}
					else
					{
						m_NRC = IMLOIF;
					}
				}
				break;
			case REPORT_DTCCODE_BY_MASK: /* test if sub-function parameter value is supported */
				{
					if(length == 3)
					{
						DiagnosticBuffTX[0]  = CurrentService  + 0x40;
						DiagnosticBuffTX[1]  = SubFunction;
						DiagnosticBuffTX[2]  = DtcAvailibaleMask;
						ResponseLength = 3;
						SearchDTCByMaskAndFillResponse(mask);
					}
					else
					{
						m_NRC = IMLOIF;
					}
				}
				break;
			case REPORT_DTCSNAPSHOT_BY_ID: /* test if sub-function parameter value is supported */
				{
					m_NRC = SFNS;
				}
				break;
			case REPORT_DTCSNAPSHOT_BY_DTCNUMBER:
				{
					if(length == 6)
					{
						uint32_t dtcCode = (*(uint32_t*)(MessageData + 1));
						DTCNode* tempNode = GetDTCNodeByCode(dtcCode);
						if(tempNode != NULL)
						{
							Snapshot matchedSnaptshot;
							DiagnosticBuffTX[0]  = CurrentService  + 0x40;
							DiagnosticBuffTX[1]  = SubFunction;
							DiagnosticBuffTX[2]  = (uint8_t)(tempNode->DTCCode >> 16);
							DiagnosticBuffTX[3]  = (uint8_t)(tempNode->DTCCode >> 8);
							DiagnosticBuffTX[4]  = (uint8_t)(tempNode->DTCCode);
							DiagnosticBuffTX[5]  = (tempNode->DTCStatus.DTCStatusByte) & DtcAvailibaleMask;
							DiagnosticBuffTX[6]  = *(MessageData + 5);
							ResponseLength = 7;
							FillDTCSnapshotReponse(tempNode->SnapShotEEpromAddr , *(MessageData + 5));
							if(DiagnosticBuffTX[7] == 0)
							{
								m_NRC = ROOR;
							}
						}
						else
						{
							m_NRC = ROOR;
						}
					}
					else
					{
						m_NRC = IMLOIF;
					}
				}
				break;
			case REPORT_DTCEXTEND_DATA_BY_DTCNUMBER:
				{
					if(length == 6)
					{
						uint32_t dtcCode = (*(uint32_t*)(MessageData + 1));
						DTCNode* tempNode = GetDTCNodeByCode(dtcCode);
						if(tempNode != NULL)
						{
							Snapshot matchedSnaptshot;
							uint8_t DataRecordNumber = *(MessageData + 5);
							DiagnosticBuffTX[0]  = CurrentService  + 0x40;
							DiagnosticBuffTX[1]  = SubFunction;
							DiagnosticBuffTX[2]  = (uint8_t)(tempNode->DTCCode >> 16);
							DiagnosticBuffTX[3]  = (uint8_t)(tempNode->DTCCode >> 8);
							DiagnosticBuffTX[4]  = (uint8_t)(tempNode->DTCCode);
							DiagnosticBuffTX[5]  = (tempNode->DTCStatus.DTCStatusByte) & DtcAvailibaleMask;
							ResponseLength = 6;
							
							if(DataRecordNumber == 0xFF)
							{
								if(AgingCounterRecord != 0)
								{
									DiagnosticBuffTX[ResponseLength++] = AgingCounterRecord;
									DiagnosticBuffTX[ResponseLength++] = tempNode->OldCounter;
								}
								
								if(AgedCounterRecord != 0)
								{
									DiagnosticBuffTX[ResponseLength++] = AgedCounterRecord;
									DiagnosticBuffTX[ResponseLength++] = tempNode->GoneCounter;
								}

								if(OccurenceCounterRecord != 0)
								{
									DiagnosticBuffTX[ResponseLength++] = OccurenceCounterRecord;
									DiagnosticBuffTX[ResponseLength++] = tempNode->FaultOccurrences;
								}

								if(PendingCounterRecord != 0)
								{
									DiagnosticBuffTX[ResponseLength++] = PendingCounterRecord;
									DiagnosticBuffTX[ResponseLength++] = tempNode->TripCounter;
								}
							}
							else if(DataRecordNumber == 0)
							{
								m_NRC = ROOR;
							}
							else if(DataRecordNumber == AgingCounterRecord)
							{
								DiagnosticBuffTX[ResponseLength++] = AgingCounterRecord;
								DiagnosticBuffTX[ResponseLength++] = tempNode->OldCounter;
							}
							else if(DataRecordNumber == AgedCounterRecord)
							{
								DiagnosticBuffTX[ResponseLength++] = AgedCounterRecord;
								DiagnosticBuffTX[ResponseLength++] = tempNode->GoneCounter;
							}
							else if(DataRecordNumber == OccurenceCounterRecord)
							{
								DiagnosticBuffTX[ResponseLength++] = OccurenceCounterRecord;
								DiagnosticBuffTX[ResponseLength++] = tempNode->FaultOccurrences;
							}
							else if(DataRecordNumber == PendingCounterRecord)
							{
								DiagnosticBuffTX[ResponseLength++] = PendingCounterRecord;
								DiagnosticBuffTX[ResponseLength++] = tempNode->TripCounter;
							}
							else
							{
								m_NRC = ROOR;
							}
						}
						else
						{
							m_NRC = ROOR;
						}
					}
					else
					{
						m_NRC = IMLOIF;
					}
				}
				break;
			case REPORT_SUPPORTED_DTC:
				{
					if(length == 2)
					{
						DiagnosticBuffTX[0]  = CurrentService + 0x40;
						DiagnosticBuffTX[1]  = SubFunction;
						DiagnosticBuffTX[2]  = DtcAvailibaleMask;
						ResponseLength = 3;
						FillAllDTCResponse();
					}
					else
					{
						m_NRC = IMLOIF;
					}
				}
				break;
			default:
				m_NRC = SFNS; /* NRC 0x12: sub-functionNotSupported */
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}
	
	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
	}
}


void Service2FHandle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	uint8_t suppressPosRspMsgIndicationBit = FALSE;
	m_NRC = 0;
	if(length >= 4)
	{
		uint16_t PID = (*(MessageData + 1) << 8) | *(MessageData + 2);
		uint8_t control = *(MessageData + 3);
		DIDNode* didNode = SearchDidNode(PID);
		if(didNode == NULL)
		{
			m_NRC = ROOR;//PID not in our PID list
		}
		else if(didNode->didType != IO_DID)
		{
			m_NRC = ROOR;//mabe a IO Control DID
		}
		else
		{
			if(length == 4)
			{
				if(control == 0 || control == 1 || control == 2)
				{
					DiagnosticBuffTX[0] = 0x6F;
					DiagnosticBuffTX[1] = *(MessageData + 1);
					DiagnosticBuffTX[2] = *(MessageData + 2);
					DiagnosticBuffTX[3] = control;
					DiagnosticBuffTX[4] = didNode->Callback(control , 0);
					ResponseLength = 5;
				}
				else
				{
					m_NRC = IMLOIF;
				}
			}
			else if(length == 5)
			{
				if(control == 3)
				{
					uint8_t param = *(MessageData + 4);
					DiagnosticBuffTX[0] = 0x6F;
					DiagnosticBuffTX[1] = *(MessageData + 1);
					DiagnosticBuffTX[2] = *(MessageData + 2);
					DiagnosticBuffTX[3] = control;
					DiagnosticBuffTX[4] = didNode->Callback(control , param);
					ResponseLength = 5;
				}
				else
				{
					m_NRC = IMLOIF;
				}
			}
			else
			{
				m_NRC = IMLOIF;
			}
		}
	}
	else
	{
		m_NRC = IMLOIF;
	}

	if ( (suppressPosRspMsgIndicationBit) && (m_NRC == 0x00) && (ResponsePending == FALSE))
	{
		suppressResponse = TRUE; /* flag to NOT send a positive response message */
	}
	else
	{
		suppressResponse = FALSE; /* flag to send the response message */
	}
}

void Service31Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	#if 0
	uint8_t ServiceName = *MessageData;
	uint8_t SubFunction =  *(MessageData + 1);
	uint16_t Routine = (*(MessageData + 2) << 8) | *(MessageData + 3);
	if(length == 4)
	{
		if(SubFunction == 1)
		{
			//Printf("routine = %x\r\n",Routine);
			if(Routine == 0xFF00)
			{
				if(0)//if(GlobalDTCControlParam.DataBits.SystemState == SYSTEM_BOOT)
				{
					WaitConfirmBeforeErase = TRUE;
					IsUpdating = TRUE;
					ServiceNegReponse(ServiceName,RCRRP);
				}
				else
				{
					ServiceNegReponse(ServiceName,CNC);//not in bootloader 
				}
			}
			else
			{
				ServiceNegReponse(ServiceName,ROOR);//no more routine except 0xFF00
			}
		}
		else if(SubFunction == 3)
		{
			ServiceNegReponse(ServiceName,CNC);
		}
		else
		{
			ServiceNegReponse(ServiceName,SFNS);
		}
	}
	else if( length == 8)
	{
		if(SubFunction == 1)
		{
			if(Routine == 0xFF01)
			{
				uint32_t RxCRC = 0;
				uint32_t AppCRC;
				DiagnosticBuffTX[0] = 0x71;
				DiagnosticBuffTX[1] = 0x01;
				DiagnosticBuffTX[2] = 0xFF;
				DiagnosticBuffTX[3] = 0x01;
				
				#ifdef BOOTLOADER
				AppCRC = CalcAppCanCRC();//CalcAppCRC();
				RxCRC = (*(MessageData + 4) << 24) + (*(MessageData + 5) << 16) + (*(MessageData + 6) << 8) + *(MessageData + 7);
				if(RxCRC == AppCRC)
				{
					WriteCanCRC(RxCRC);//WriteCRC(RxCRC);
					//Printf("app CRC  %.8x = RXCRC %.8x\r\n",AppCRC,RxCRC );
					DiagnosticBuffTX[4] = 0x00;
					Diagnostic_UpdatePromgramTimes();
				}
				else
				{
					//Printf("app CRC  %.8x != RXCRC %.8x\r\n",AppCRC,RxCRC );
					DiagnosticBuffTX[4] = 0x01;
				}
				N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , 5);
				#else
					ServiceNegReponse(ServiceName,CNC);
				#endif
				//TODO:verify app
			}
			else
			{
				ServiceNegReponse(ServiceName,ROOR);//no more routine except 0xFF00
			}
		}
		else if(SubFunction == 3)
		{
			ServiceNegReponse(ServiceName,CNC);
		}
		else
		{
			ServiceNegReponse(ServiceName,SFNS);
		}
	}
	else
	{
		ServiceNegReponse(ServiceName,IMLOIF);
	}
	#endif
}

void Service34Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	#if 0
	uint8_t ServiceName = *MessageData;
	uint8_t SubFunction =  *(MessageData + 1);
	uint8_t DataLengthID = *(MessageData + 2);
	if(length == 11)
	{
		if(SubFunction == 0x00)
		{
			if(DataLengthID == 0x44)
			{
				if(0)//if(GlobalDTCControlParam.DataBits.SystemState == SYSTEM_BOOT)
				{
					ProgramAddress = (*(MessageData + 3) << 24 ) + (*(MessageData + 4) << 16)  + (*(MessageData + 5) << 8) + *(MessageData + 6);
					ProgramLength = (*(MessageData + 7) << 24) + (*(MessageData + 8) << 16)  + (*(MessageData + 9) << 8) + *(MessageData + 10);
					//Printf("request program addr:%.8x,len:%.8x\r\n",ProgramAddress,ProgramLength);
					#ifdef BOOTLOADER
					if(ProgramAddress == APP_START_ADDR && ProgramLength < APP_MAX_SIZE)
					{
						WriteAppLength(ProgramLength);
						GlobalDTCControlParam.DataBits.Downloading = TRUE;
						m_BlockIndex = 0;
						
						DiagnosticBuffTX[0] = 0x74;
						DiagnosticBuffTX[1] = 0x20;
						DiagnosticBuffTX[2] =  (MAX_DOWNLOADING_BUF >> 8) & 0xFF;
						DiagnosticBuffTX[3] = MAX_DOWNLOADING_BUF & 0xFF;
						N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , 4);
					}
					else
					{
						ServiceNegReponse(ServiceName,UDNA);
					}
					#endif
				}
				else
				{
					ServiceNegReponse(ServiceName,UDNA);//not in bootloader 
				}
			}
			else
			{
				ServiceNegReponse(ServiceName,ROOR);
			}
		}
		else
		{
			ServiceNegReponse(ServiceName,SFNS);
		}
	}
	else
	{
		ServiceNegReponse(ServiceName,IMLOIF);
	}
	#endif
}

void Service35Handle(uint16_t length, uint8_t *MessageData)
{
	
}

void Service36Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	#if 0
	uint8_t ServiceName = *MessageData;
	uint8_t BLockIndex = *(MessageData + 1);
	if(length <= MAX_DTCDATA_BUF)
	{
		if(GlobalDTCControlParam.DataBits.Downloading == TRUE || ProgramAddress < 0x800A000)
		{
			//printf("rx block %2x != %2x\r\n",BLockIndex,m_BlockIndex);
			if(BLockIndex == ((m_BlockIndex + 1) & 0xFF))
			{
				m_BlockIndex = BLockIndex;
				if(HighVoltage == TRUE)
				{
					ServiceNegReponse(ServiceName,VTH);
				}
				else if(LowVoltage == TRUE)
				{
					ServiceNegReponse(ServiceName,VTL);
				}
				else
				{
					#ifdef BOOTLOADER
					//if(ProgramLength > ProgramLengthComplete)
					//{
						//printf("36 service will program %d bytes , address:%.8x \r\n",length-2,ProgramAddress + ProgramLengthComplete);
						if(Update_WriteData(ProgramAddress + ProgramLengthComplete, MessageData + 2 , length - 2) == 0)
						{
							ProgramLengthComplete +=  length - 2;
							DiagnosticBuffTX[0] = 0x76;
							DiagnosticBuffTX[1] = BLockIndex;
							N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , 2);
						}
						else
						{
							m_BlockIndex = 0;
							ServiceNegReponse(ServiceName,GPF);//program error
						}
					//}
					//else
					//{
						//m_BlockIndex = 0;
						//ServiceNegReponse(ServiceName,TDS);//program complete
					//}
					#endif
				}
			}
			else
			{
				ServiceNegReponse(ServiceName,WBSC);
			}
		}
		else
		{
			ServiceNegReponse(ServiceName,RSE);//not downloading not accept or address incorrect
		}
	}
	else
	{
		ServiceNegReponse(ServiceName,IMLOIF);//length error buff will overflow
	}
	#endif
}

void Service37Handle(uint8_t N_TAType, uint16_t length, uint8_t *MessageData)
{
	#if 0
	uint8_t ServiceName = *MessageData;
	if(length == 1)
	{
		if(GlobalDTCControlParam.DataBits.Downloading == TRUE)
		{
			GlobalDTCControlParam.DataBits.Downloading = FALSE;
			if(0)//if(GlobalDTCControlParam.DataBits.SystemState == SYSTEM_BOOT)
			{
				ProgramAddress = 0;
				ProgramLength = 0;
				ProgramLengthComplete = 0;
				
				m_BlockIndex = 0;
				DiagnosticBuffTX[0] = 0x77;
				N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , 1);
			}
			else
			{
				ServiceNegReponse(ServiceName,CNC);//not in bootloader 
			}
		}
		else
		{
			ServiceNegReponse(ServiceName,RSE);
		}
	}
	else
	{
		ServiceNegReponse(ServiceName,IMLOIF);
	}
	#endif
}

#if 0
void DTC_Handler(void* DTCCodePtr)
{
	DTCCode* DTC = (DTCCode*)DTCCodePtr;
	
	if(DTC->DetectFunction() == TRUE)
	{
		if(DTC->DetectCounter < 0)//关键点b
		{
			DTC->DetectCounter = 2;
		}
		else
		{
			DTC->DetectCounter += 2;
		}
		
		if(DTC->DetectCounter >= DTC->DetectValidTimes)//关键点c
		{
			if(DTC->DTCStatus.DTCbit.TestFailed != 1 || DTC->DTCStatus.DTCbit.TestFailedThisMonitoringCycle == 0)
			{
				if(DTC->FaultOccurrences < 0xFF)
				{
					DTC->FaultOccurrences++;
				}
				DTC->OldCounter = 0;
				
				DTC->DTCStatus.DTCbit.TestFailed = 1;//测试失败
				DTC->DTCStatus.DTCbit.ConfirmedDTC = 1;//已确认的诊断故障，关键点e
				DTC->DTCStatus.DTCbit.TestFailedThisMonitoringCycle = 1;//本次操作循环测试失败
				//DTC->DTCStatus.DTCbit.PendingDTC = 1;//未确认的诊断故障码，关键点d
				//DTC->DTCStatus.DTCbit.TestFailedSinceLastClear = 1;//自上次清除后测试失败
				//Printf("faild %x %x %x\r\n",DTC->DTCHightByte,DTC->DTCMiddleByte,DTC->DTCLowByte);
			}
		}
	}
	else
	{
		DTC->DetectCounter--;
		if(DTC->DetectCounter <=  (0 - DTC->DetectValidTimes))
		{
			if(DTC->DTCStatus.DTCbit.TestFailed != 0)
			{
				DTC->DTCStatus.DTCbit.TestFailed = 0;
				//Printf("success %x %x %x\r\n",DTC->DTCHightByte,DTC->DTCMiddleByte,DTC->DTCLowByte);
			}
		}
	}
}


void Diagnostic_ReadDTCPositiveResponse(uint8_t DTCSubFunction,uint8_t DTCStatausMask)
{
	uint8_t i;
	uint16_t DTCCounter = 0;
	//printf("DTC MASK %x\r\n",DTCStatausMask);
	if(DTCSubFunction == REPORT_DTCNUMBER_BY_MASK)
	{
		for(i = 0 ; i < DTC_LIST_COUNT ; i++)
		{
			if((DTCCodeList[i].DTCStatus.DTCStatusByte & DTCStatausMask & SUPPORTED_DTC_CODE) != 0 )
			{
				DTCCounter++;
			}
		}
		
		DiagnosticBuffTX[0] = 0x59;
		DiagnosticBuffTX[1] = DTCSubFunction;
		DiagnosticBuffTX[2] = DTCStatausMask;
		DiagnosticBuffTX[3] = ISO15031_6DTCFORMAT;
		DiagnosticBuffTX[4]  = (uint8_t)(DTCCounter>>8);
		DiagnosticBuffTX[5]  = (uint8_t)DTCCounter;
		N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , 6);
	}
	else if(DTCSubFunction == REPORT_DTCCODE_BY_MASK)
	{
		uint8_t length = 3;
		DiagnosticBuffTX[0] = 0x59;
		DiagnosticBuffTX[1] = DTCSubFunction;
		DiagnosticBuffTX[2] = DTCStatausMask;
		for(i = 0 ; i < DTC_LIST_COUNT ; i++)
		{
			if((DTCCodeList[i].DTCStatus.DTCStatusByte & DTCStatausMask & SUPPORTED_DTC_CODE) != 0)
			{
				length += 4;
				DiagnosticBuffTX[length - 4] = (DTCCodeList[i].Alpha << 6) + DTCCodeList[i].DTCHightByte;
				DiagnosticBuffTX[length - 3] = DTCCodeList[i].DTCMiddleByte;
				DiagnosticBuffTX[length - 2] = DTCCodeList[i].DTCLowByte;
				DiagnosticBuffTX[length - 1] = DTCCodeList[i].DTCStatus.DTCStatusByte & SUPPORTED_DTC_CODE;
			}
		}
		N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , length);
	}
	else if(DTCSubFunction == REPORT_SUPPORTED_DTC)
	{
		uint8_t length = 3;
		DiagnosticBuffTX[0] = 0x59;
		DiagnosticBuffTX[1] = DTCSubFunction;
		DiagnosticBuffTX[2] = SUPPORTED_DTC_CODE;
		for(i = 0 ; i < DTC_LIST_COUNT ; i++)
		{
			length += 4;
			DiagnosticBuffTX[length - 4] = (DTCCodeList[i].Alpha << 6) + DTCCodeList[i].DTCHightByte;
			DiagnosticBuffTX[length - 3] = DTCCodeList[i].DTCMiddleByte;
			DiagnosticBuffTX[length - 2] = DTCCodeList[i].DTCLowByte;
			DiagnosticBuffTX[length - 1] = DTCCodeList[i].DTCStatus.DTCStatusByte & SUPPORTED_DTC_CODE;
		}
		N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , length);
	}
}
#endif

void ServiceNegReponse(uint8_t serviceName,uint8_t RejectCode)
{
	DiagnosticBuffTX[0] = 0x7F;
	DiagnosticBuffTX[1] = serviceName;
	DiagnosticBuffTX[2] = RejectCode;
	N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , 3);
}

void Diagnostic_ServiceHandle(uint8_t N_SA , uint8_t N_TA , uint8_t N_TAtype , uint16_t length , uint8_t *MessageData)
{
	uint8_t  SIDIndex;
	bool ValidSid;
	uint16_t ServiceIndex;
	uint8_t DataIndex;
	ValidSid = FALSE;
	ServiceIndex = 0;
	CurrentService = MessageData[0];	
	#if 0
	printf("rx[");
	for(DataIndex = 0; DataIndex < length; DataIndex++)
	{
		printf(" %x ",*(MessageData + DataIndex));
	}
	printf("]\r\n");
	#endif
	while((ServiceIndex < SERVICE_NUMBER) && (!ValidSid))
	{
		if(ServiceList[ServiceIndex].serviceName == CurrentService)
		{
			if(ServiceList[ServiceIndex].support == TRUE)
			{
				ValidSid = TRUE;
			}
			else//found service but service not enable by application layer
			{
				ValidSid = FALSE;
				break;
			}
		}
		else
		{
			ServiceIndex++;
		}
	}
	
	if(ValidSid == TRUE)
	{
		if(N_TAtype == PHYSICAL)
		{
			suppressResponse = FALSE;
			if(ECU_DEFAULT_SESSION == m_CurrSessionType)
			{
				if(ServiceList[ServiceIndex].PHYDefaultSession_Security == LEVEL_UNSUPPORT)
				{
					m_NRC = SNSIAS;//ServiceNegReponse(ServiceName,SNSIAS);
				}
				else
				{
					if((ServiceList[ServiceIndex].PHYDefaultSession_Security & m_SecurityLevel) == m_SecurityLevel)
					{
						ServiceList[ServiceIndex].serviceHandle(N_TAtype,length,MessageData);
						//LastService = ServiceList[ServiceIndex];
					}
					else
					{
						m_NRC = SAD;//ServiceNegReponse(ServiceName,SAD);
					}
				}
			}
			else if(ECU_EXTENED_SESSION == m_CurrSessionType)
			{
				if(ServiceList[ServiceIndex].PHYExtendedSession_Security == LEVEL_UNSUPPORT)
				{
					m_NRC = SNSIAS;//ServiceNegReponse(ServiceName,SNSIAS);
				}
				else
				{
					if((ServiceList[ServiceIndex].PHYExtendedSession_Security & m_SecurityLevel) == m_SecurityLevel)
					{
						ServiceList[ServiceIndex].serviceHandle(N_TAtype,length,MessageData);
						//LastService = ServiceList[ServiceIndex];
					}
					else
					{
						m_NRC = SAD;//ServiceNegReponse(ServiceName,SAD);
					}
				}
			}
			else if(ECU_PAOGRAM_SESSION == m_CurrSessionType)
			{
				if(ServiceList[ServiceIndex].PHYProgramSeesion_Security == LEVEL_UNSUPPORT)
				{
					m_NRC = SNSIAS;//ServiceNegReponse(ServiceName,SNSIAS);
				}
				else
				{
					if((ServiceList[ServiceIndex].PHYProgramSeesion_Security & m_SecurityLevel) == m_SecurityLevel)
					{
						ServiceList[ServiceIndex].serviceHandle(N_TAtype,length,MessageData);
						//LastService = ServiceList[ServiceIndex];
					}
					else
					{
						m_NRC = SAD;//ServiceNegReponse(ServiceName,SAD);
					}
				}
			}
		}
		else if(N_TAtype == FUNCTIONAL)
		{
			if(ECU_DEFAULT_SESSION == m_CurrSessionType)
			{
				if(ServiceList[ServiceIndex].FUNDefaultSession_Security == LEVEL_UNSUPPORT)
				{
					m_NRC = SNSIAS;//ServiceNegReponse(ServiceName,SNSIAS);
				}
				else
				{
					if((ServiceList[ServiceIndex].FUNDefaultSession_Security & m_SecurityLevel) == m_SecurityLevel)
					{
						ServiceList[ServiceIndex].serviceHandle(N_TAtype,length,MessageData);
						//LastService = ServiceList[ServiceIndex];
					}
					else
					{
						m_NRC = SAD;//ServiceNegReponse(ServiceName,SAD);
					}
				}
			}
			else if(ECU_EXTENED_SESSION == m_CurrSessionType)
			{
				if(ServiceList[ServiceIndex].FUNExtendedSession_Security == LEVEL_UNSUPPORT)
				{
					m_NRC = SNSIAS;//ServiceNegReponse(ServiceName,SNSIAS);
				}
				else
				{
					if((ServiceList[ServiceIndex].FUNExtendedSession_Security & m_SecurityLevel) == m_SecurityLevel)
					{
						ServiceList[ServiceIndex].serviceHandle(N_TAtype,length,MessageData);
						//LastService = ServiceList[ServiceIndex];
					}
					else
					{
						m_NRC = SAD;//ServiceNegReponse(ServiceName,SAD);
					}
				}
			}
			else if(ECU_PAOGRAM_SESSION == m_CurrSessionType)
			{
				if(ServiceList[ServiceIndex].FUNProgramSeesion_Security == LEVEL_UNSUPPORT)
				{
					m_NRC = SNSIAS;//ServiceNegReponse(ServiceName,SNSIAS);
				}
				else
				{
					if((ServiceList[ServiceIndex].FUNProgramSeesion_Security & m_SecurityLevel) == m_SecurityLevel)
					{
						ServiceList[ServiceIndex].serviceHandle(N_TAtype,length,MessageData);
						//LastService = ServiceList[ServiceIndex];
					}
					else
					{
						m_NRC = SAD;//ServiceNegReponse(ServiceName,SAD);
					}
				}
			}
		}
	}
	else
	{
		if(N_TAtype == PHYSICAL)//功能寻址无效SED不响应
		{
			m_NRC = SNS;//ServiceNegReponse(ServiceName,SNS);
			suppressResponse = FALSE;
		}
		else
		{
			suppressResponse = TRUE;
		}
	}
}


void Diagnostic_MainProc(void)
{
	uint32_t rxId = 0;
	bool ExistValidNotify = FALSE;
	if(!IsIndicationListEmpty())
	{
		NetworkNotification temp = PullIndication();
		rxId = ((temp.N_SA << 8) + temp.N_TA);
		
		if(temp.NotificationType == INDICATION)
		{
			uint8_t RequestEquipment = 0xFF;
			if((rxId & 0xFFFF) == (TesterPhyID & 0xFFFF) || (rxId & 0xFFFF) == (TesterFunID & 0xFFFF))
			{
				RequestEquipment = 0;
				N_Ta = (uint8_t)EcuID;
				N_Sa = (uint8_t)(EcuID >> 8);
			}
			else if((rxId & 0xFFFF) == (TesterPhyID1 & 0xFFFF) || (rxId & 0xFFFF) == (TesterFunID1 & 0xFFFF))
			{
				RequestEquipment = 1;
				N_Ta = (uint8_t)EcuID1;
				N_Sa = (uint8_t)(EcuID1 >> 8);
			}
		
			if(RequestEquipment == 0 || RequestEquipment == 1)
			{
				if(temp.N_Resut == N_OK ||temp.N_Resut == N_UNEXP_PDU)
				{
					Diagnostic_ServiceHandle(temp.N_SA,temp.N_TA,temp.N_TAtype,temp.length, temp.MessageData);

					#if 0
					if((temp.N_TAtype == FUNCTIONAL) && ((m_NRC == SNS) || (m_NRC == SFNS) || (m_NRC == SNSIAS) ||
					(m_NRC == SFNSIAS) || (m_NRC == ROOR)) && (ResponsePending == FALSE))
					#else
					if((temp.N_TAtype == FUNCTIONAL) && ((m_NRC == SNS) || (m_NRC == SFNS) || (m_NRC == SFNSIAS) || (m_NRC == ROOR)) && (ResponsePending == FALSE))
					#endif
					{
						//printf("res supress,pending =  %d\r\n",ResponsePending);
						/* suppress negative response message */
					}
					else if (suppressResponse == TRUE)
					{
						//printf("res supress bit is TRUE\r\n");
						/* suppress positive response message */
					}
					else
					{
						if(m_NRC == PR)
						{
							N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , ResponseLength);
						}
						else
						{
							ServiceNegReponse(CurrentService,m_NRC);
						}
					}

					
					if(m_CurrSessionType  != ECU_DEFAULT_SESSION)
					{
						DiagTimer_Set(&S3serverTimer, 5000);
					}
				}
			}
		}
		else if(temp.NotificationType == CONFIRM)
		{
			if((rxId & 0xFFFF) == (EcuID & 0xFFFF))
			{
				if(WaitConfirmBeforeJump == TRUE)
				{
					
				}
				else if(WaitConfirmBeforeErase == TRUE)
				{
					WaitConfirmBeforeErase = FALSE;
					#ifdef BOOTLOADER
					//Printf("31 service will erase app\r\n");
					//Update_EraseFlash();
					DiagnosticBuffTX[0] = 0x71;
					DiagnosticBuffTX[1] = 0x01;
					DiagnosticBuffTX[2] = 0xFF;
					DiagnosticBuffTX[3] = 0x00;
					DiagnosticBuffTX[4] = 0x00;
					N_USData_request(DIAGNOSTIC , N_Sa ,  N_Ta , PHYSICAL , 0 , DiagnosticBuffTX , 5);
					#endif
				}
				else if(WaitConfimBeforeReset == TRUE)
				{
					WaitConfimBeforeReset = FALSE;
					if(ResetCallBackFun != NULL)
					{
						ResetCallBackFun(m_EcuResetType);
					}
				}
			}
		}
		else if(temp.NotificationType == FF_INDICATION)
		{
			//Printf("RX FF ind\r\n");
		}
	}
	#if 1
	else
	{
		if(SaveDTCInBlock != FALSE)
		{
			if(DiagTimer_HasExpired(&DTCSavedTimer))
			{
				#if USE_MALLOC

				#else
				if(DTCSaved < DTCAdded)
				{
					Diagnostic_EEProm_Write(DTCS[DTCSaved].EEpromAddr, DTC_BYTE_NUMBER_TO_SAVE , &(DTCS[DTCSaved].DTCStatus));
					DTCSaved++;
					DiagTimer_Set(&DTCSavedTimer, 50);
				}
				else
				{
					SaveDTCInBlock = FALSE;
				}
				#endif
			}
		}
	}
	#endif
}

void Diagnostic_TimeProc(void)
{
	if(m_CurrSessionType != ECU_DEFAULT_SESSION)
	{
		if(DiagTimer_HasExpired(&S3serverTimer))
		{
			//printk("[time out]S3 server timeout\r\n");
			GotoSession(ECU_DEFAULT_SESSION);
		}
	}

	if(m_UnlockStep == WAIT_DELAY)
	{
		uint8_t i;
		for(i = 0 ; i < 3 ; i ++)
		{
			if(UnlockList[i].valid == TRUE)
			{
				if(DiagTimer_HasExpired(&UnlockList[i].SecurityLockTimer))
				{
					UnlockList[i].FaultCounter--;
					Diagnostic_EEProm_Read(UnlockList[i].FaultCounterAddr, 1 ,&UnlockList[i].FaultCounter);
					m_UnlockStep = WAIT_SEED_REQ;
				}
			}
		}
	}

	
	if(ResponsePending == TRUE && WaitConfirmBeforeJump == FALSE)
	{
		ResponsePending = FALSE;
		Service10PosResponse(m_CurrSessionType);
	}

	#if USE_J1939_DTC
	if(DM1DTCNumber != 0 && DiagDM1Enable != FALSE)
	{
		if(DiagTimer_HasExpired(&J1939Timer) || DiagTimer_HasStopped(&J1939Timer))
		{
			uint8_t i = 0;
			uint8_t j = 0;
			DiagTimer_Set(&J1939Timer, 1000);
			for(i = 0; i < DTCAdded;i++)
			{
				if(DTCS[i].DTCStatus.DTCbit.TestFailed == 1 && (DTCS[i].dtcLevel == LEVEL_B || DTCS[i].dtcLevel == LEVEL_A))
				{
					J1939BufTX[j * 8 + 0] = 0x40;
					J1939BufTX[j * 8 + 1] = 0xFF;
					J1939BufTX[j * 8 + 6] = 0xFF;
					J1939BufTX[j * 8 + 7] = 0xFF;
					J1939BufTX[j * 8 + 2] = (uint8_t)(DTCS[i].DM1Code.SPN);
					J1939BufTX[j * 8 + 3] = (uint8_t)(DTCS[i].DM1Code.SPN >> 8);
					J1939BufTX[j * 8 + 4] = (uint8_t)(((DTCS[i].DM1Code.SPN >> 11) & 0x00E0) | DTCS[i].DM1Code.FMI);
					J1939BufTX[j * 8 + 5] = (uint8_t)(((DTCS[i].FaultOccurrences) & 0xEF) | (DTCS[i].DM1Code.OC << 7));
					j++;
				}
			}
			TPCMRequestBAM(DM1DTCNumber * 8, 0xFFCA00 , J1939BufTX);
			DM1DTCNumber = 0;				
		}
	}
	#endif
	
	#if 0
	if(GlobalDTCControlParam.DataBits.EnableDTCDetect == TRUE)
	{
		if(DiagTimer_HasExpired(&DTCDetectTimer))
		{
			DiagTimer_Set(&DTCDetectTimer, 50);
			for(i = 0 ; i < DTC_LIST_COUNT ; i++)
			{
				if(DiagTimer_HasExpired(&DTCCodeList[i].DetectTimer))
				{
					Timer_Set(&DTCCodeList[i].DetectTimer, DTCCodeList[i].DectecPeroid);
					DTCCodeList[i].FaultHandler(&DTCCodeList[i]);
				}
			}
		}
	}
	#endif
}

void SaveSnapShotData(uint16_t EEPromAddr)
{
	uint8_t length = 0;
	uint8_t i;
	for(i = 0 ; i < SnapShotAdded; i++)
	{
		memcpy(DiagnosticBuffTX + length,SnapShots[i].dataPointer,SnapShots[i].dataLength);
		length += SnapShots[i].dataLength;
	}

	Diagnostic_EEProm_Write(EEPromAddr , length , DiagnosticBuffTX);
}

void DtcHandle(DTCNode* DtcNode)
{
	bool OperateCycleChange = FALSE;
	uint8_t CurrentResult;

	if(DtcNode!= NULL && DtcNode->DetectFunction != NULL)
	{
		CurrentResult = DtcNode->DetectFunction();
	}
	else
	{
		CurrentResult = PASSED;
	}
	
	if(CurrentResult == PASSED)
	{
		if(DtcNode->DTCStatus.DTCbit.TestNotCompleteThisMonitoringCycle == 1)
		{
			DtcNode->DTCStatus.DTCbit.TestNotCompleteThisMonitoringCycle = 0;//14229-1-Figure D.9-2
			OperateCycleChange = TRUE;
		}

		if(DtcNode->DTCStatus.DTCbit.TestNotCompleteSinceLastClear == 1)
		{
			DtcNode->DTCStatus.DTCbit.TestNotCompleteSinceLastClear = 0;//14229-1-Figure D.9-1
		}
		if(DtcNode->DTCStatus.DTCbit.TestFailed == 1)
		{
			DtcNode->DTCStatus.DTCbit.TestFailed = 0;//14229-1-Figure D.9-7
		}
		#if 0
		if(DtcNode->DTCStatus.DTCbit.PendingDTC == 1)
		{
			DtcNode->DTCStatus.DTCbit.PendingDTC = 0;//14229-1-Table D.5
		}
		#endif
	}
	else if(CurrentResult == FAILED)
	{
		if(DtcNode->DTCStatus.DTCbit.TestNotCompleteThisMonitoringCycle == 1)
		{
			DtcNode->DTCStatus.DTCbit.TestNotCompleteThisMonitoringCycle = 0;//14229-1-Figure D.9-2
			OperateCycleChange = TRUE;
		}

		if(DtcNode->DTCStatus.DTCbit.TestNotCompleteSinceLastClear == 1)
		{
			DtcNode->DTCStatus.DTCbit.TestNotCompleteSinceLastClear = 0;//14229-1-Figure D.9-1
		}
		if(DtcNode->DTCStatus.DTCbit.TestFailed == 0)
		{
			DtcNode->DTCStatus.DTCbit.TestFailed = 1;//14229-1-Figure D.9-3,8
		}
		if(DtcNode->DTCStatus.DTCbit.TestFailedThisMonitoringCycle ==0 )
		{
			DtcNode->DTCStatus.DTCbit.TestFailedThisMonitoringCycle = 1;//本次操作循环测试失败14229-1-Figure D.9-4
			DtcNode->TripCounter++;
			//DtcNode->DTCStatus.DTCbit.ConfirmedDTC = 1;
			//SaveSnapShotData(DtcNode->SnapShotEEpromAddr);
			#if 1
			if(DtcNode->TripCounter >= DtcNode->TripLimitTimes && DtcNode->DTCStatus.DTCbit.TestFailed == 1)//iso14229-1 Frigure D.4
			{
				if(DtcNode->DTCStatus.DTCbit.ConfirmedDTC != 1)
				{
					DtcNode->TripCounter = 0;
					DtcNode->DTCStatus.DTCbit.ConfirmedDTC = 1;
					SaveSnapShotData(DtcNode->SnapShotEEpromAddr);
				}
			}
			if(DtcNode->FaultOccurrences < 0xff)
			{
				DtcNode->FaultOccurrences++;//上汽商用cvtc 27033 section10.1.1
			}
			#endif
		}
		DtcNode->DTCStatus.DTCbit.PendingDTC = 1;//未确认的诊断故障码，14229-1-Figure D.9-5
		DtcNode->DTCStatus.DTCbit.TestFailedSinceLastClear = 1;//自上次清除后测试失败14229-1-Figure D.9-6	
		DtcNode->OldCounter == 0;

		#if USE_J1939_DTC
		if(DtcNode->dtcLevel == LEVEL_A || DtcNode->dtcLevel == LEVEL_B)
		{
			if(DM1DTCNumber < 255)
			{
				DM1DTCNumber++;
			}
		}
		#endif
	}
	else
	{
		
	}

	if(DtcNode->DTCStatus.DTCbit.TestFailedThisMonitoringCycle == 0 && DtcNode->DTCStatus.DTCbit.TestNotCompleteThisMonitoringCycle == 0)//iso14229-1 Frigure D.4
	{
		DtcNode->TripCounter == 0;
	}
}

void Diagnostic_DTCProc(void)
{
	if(EnableDTCDetect != FALSE && DiagTimer_GetTickCount() >= 1000)
	{
		if(DiagTimer_HasStopped(&DtcTimer) || DiagTimer_HasExpired(&DtcTimer))
		{
			#if USE_MALLOC
			DTCNode* DtcNode = DTCHeader;
			while(DtcNode != NULL)
			{
				DtcHandle(DtcNode);
				DtcNode = DtcNode->next;
			}
			#else
			uint8_t i;
			DM1DTCNumber = 0;   
			for(i = 0 ; i < DTCAdded ; i++)
			{
				DtcHandle(DTCS + i);
			}
			#endif
			DiagTimer_Set(&DtcTimer , 50);
		}
	}
}

void J1939Proc(void)
{
	#if USE_J1939_DTC
	if(DiagDM1Enable != FALSE)
	{
		TPCMDTProc();
	}
	#endif
}


void Diagnostic_Proc(void)
{
	J1939Proc();
	NetworkLayer_Proc();
	Diagnostic_MainProc();
	Diagnostic_TimeProc();
	Diagnostic_DTCProc();
}

void Diagnostic_RxFrame(uint32_t ID,uint8_t* data,uint8_t IDE,uint8_t DLC,uint8_t RTR)
{
	NetworkLayer_RxFrame(ID,data,IDE,DLC,RTR);
	if(0x0CECFF25 == ID || 0x0CEBFF25 == ID)
	{
		J1939TPReceiveData(ID, data , DLC);
	}
}

void Diagnostic_1msTimer(void)
{
	DiagTimer_ISR_Proc();
}

void Diagnostic_DelInit(void)
{
	//Diagnostic_SaveAllDTC();
	SaveDTCInBlock = TRUE;
	DTCSaved = 0;
	DiagTimer_Set(&DTCSavedTimer, 10);
}


void Diagnostic_ClearDTC(uint32_t Group)
{
	#if USE_MALLOC
	DTCNode* DtcNode = DTCHeader;
	while(DtcNode != NULL)
	{
		if((DtcNode->DTCCode & Group) == DtcNode->DTCCode)//ONLY FFFFFF group supported this method
		{
			DtcNode->DTCStatus.DTCStatusByte = 0x50;//when clear bit4 and bit6 must be setted to 1
			DtcNode->FaultOccurrences = 0;
			DtcNode->OldCounter = 0;
			DtcNode->GoneCounter = 0;
			DtcNode->TripCounter = 0;
			//Diagnostic_EEProm_Write(DtcNode->EEpromAddr, DTC_BYTE_NUMBER_TO_SAVE , &(DtcNode->DTCStatus.DTCStatusByte));
		}
		DtcNode = DtcNode->next;
	}
	#else
	uint8_t i;
	for(i = 0 ; i < DTCAdded ; i++)
	{
		if((DTCS[i].DTCCode & Group) == DTCS[i].DTCCode)
		{
			DTCS[i].DTCStatus.DTCStatusByte = 0x50;//when clear bit4 and bit6 must be setted to 1
			DTCS[i].FaultOccurrences = 0;
			DTCS[i].OldCounter = 0;
			DTCS[i].GoneCounter = 0;
			DTCS[i].TripCounter = 0;
			#if 1
			if(SaveDTCInBlock == FALSE)
			{
				SaveDTCInBlock = TRUE;
				DTCSaved = 0;
			}
			#endif
			//Diagnostic_EEProm_Write(DTCS[i].EEpromAddr, DTC_BYTE_NUMBER_TO_SAVE , &(DTCS[i].DTCStatus));
		}
	}
	#endif
}

void Diagnostic_SaveAllDTC(void)
{
	#if USE_MALLOC
	DTCNode* DtcNode = DTCHeader;
	while(DtcNode  != NULL)
	{
		if(DtcNode->DTCStatus.DTCbit.TestFailedThisMonitoringCycle == 0  && DtcNode->DTCStatus.DTCbit.ConfirmedDTC == 1)
		{
			if(DtcNode->OldCounter >= 100)//故障码老化机制
			{
				DtcNode->OldCounter = 0;
				DtcNode->GoneCounter++;
				DtcNode->DTCStatus.DTCStatusByte = 0x50;//when clear bit4 and bit6 must be setted to 1 and others 0
				DtcNode->FaultOccurrences = 0;
				DtcNode->OldCounter = 0;
				DtcNode->GoneCounter = 0;
				DtcNode->TripCounter = 0;
			}
			else
			{
				DtcNode->OldCounter++;
			}
		}
		else
		{
			DtcNode->OldCounter = 0;
		}
		DtcNode->DTCStatus.DTCStatusByte &= 0xBD;//bit1 and bit6 can not be saved
		Diagnostic_EEProm_Write(DtcNode->EEpromAddr, DTC_BYTE_NUMBER_TO_SAVE , &(DtcNode->DTCStatus.DTCStatusByte));
		DtcNode  = DtcNode ->next;
	}
	#else
	uint8_t i;
	uint16_t DtcStartAddr = DTCS[0].EEpromAddr;
	uint16_t DtcBytesToSave = 0;
	uint8_t  DtcBytes[180];
	for(i = 0 ; i < DTCAdded ; i++)
	{
		if(DTCS[i].DTCStatus.DTCbit.TestNotCompleteSinceLastClear == 0 && DTCS[i].DTCStatus.DTCbit.TestFailedThisMonitoringCycle == 0)
		{
			DTCS[i].DTCStatus.DTCbit.PendingDTC = 0;
		}
	
		if(DTCS[i].DTCStatus.DTCbit.TestFailedThisMonitoringCycle == 0  && DTCS[i].DTCStatus.DTCbit.ConfirmedDTC == 1)
		{
			if(DTCS[i].OldCounter >= 40)//故障码老化机制
			{
				DTCS[i].OldCounter = 0;
				DTCS[i].GoneCounter++;
				DTCS[i].DTCStatus.DTCStatusByte = 0x50;//when clear bit4 and bit6 must be setted to 1 and others 0
				DTCS[i].FaultOccurrences = 0;
				DTCS[i].OldCounter = 0;
				DTCS[i].GoneCounter = 0;
				DTCS[i].TripCounter = 0;
			}
			else
			{
				DTCS[i].OldCounter++;
			}
		}
		else
		{
			DTCS[i].OldCounter = 0;
		}
		DTCS[i].DTCStatus.DTCStatusByte &= 0xBD;//bit1 and bit6 can not be saved
		memcpy(DtcBytes + DtcBytesToSave,&(DTCS[i].DTCStatus),DTC_BYTE_NUMBER_TO_SAVE);
		DtcBytesToSave += DTC_BYTE_NUMBER_TO_SAVE;
		//Diagnostic_EEProm_Write(DTCS[i].EEpromAddr, DTC_BYTE_NUMBER_TO_SAVE , &(DTCS[i].DTCStatus));//&(DTCS[i].DTCStatus.DTCStatusByte));
	}
	Diagnostic_EEProm_Write(DtcStartAddr , DtcBytesToSave , DtcBytes);
	#endif
}

void Diagnostic_LoadAllDTC(void)
{
	#if USE_MALLOC
	DTCNode* DtcNode = DTCHeader;
	while(DtcNode  != NULL)
	{
		Diagnostic_EEProm_Read(DtcNode->EEpromAddr, DTC_BYTE_NUMBER_TO_SAVE , &(DtcNode->DTCStatus.DTCStatusByte));
		DtcNode->DTCStatus.DTCStatusByte |= 0x40;//when IGN ON, bit6 default 1,
		DtcNode->DTCStatus.DTCStatusByte &= 0xFD;//when IGN ON, bit1 default 0
		DtcNode  = DtcNode ->next;
	}
	#else
	uint8_t i;
	for(i = 0 ; i < DTCAdded ; i++)
	{
		Diagnostic_EEProm_Read(DTCS[i].EEpromAddr, DTC_BYTE_NUMBER_TO_SAVE , &(DTCS[i].DTCStatus));
		DTCS[i].DTCStatus.DTCStatusByte |= 0x40;//when IGN ON, bit6 default 1,
		DTCS[i].DTCStatus.DTCStatusByte &= 0xFD;//when IGN ON, bit1 default 0
	}
	#endif
}

void Diagnostic_LoadSecuriyFaultCounter(void)
{
	uint8_t i;
	for(i = 0 ; i < 3 ; i++)
	{
		if(UnlockList[i].valid != FALSE)
		{
			Diagnostic_EEProm_Read(UnlockList[i].FaultCounterAddr, 1 ,&UnlockList[i].FaultCounter);
			if(UnlockList[i].FaultCounter == 0xFF)
			{
				UnlockList[i].FaultCounter = 0;
			}
			else if(UnlockList[i].FaultCounter >= UnlockList[i].FaultLimitCounter)
			{
				UnlockList[i].FaultCounter = 0;
				m_UnlockStep = WAIT_DELAY;
				DiagTimer_Set(&UnlockList[i].SecurityLockTimer , UnlockList[i].UnlockFailedDelayTime);
			}
			Diagnostic_EEProm_Write(UnlockList[i].FaultCounterAddr, 1 ,&UnlockList[i].FaultCounter);
		}
	}
}

void Diagnostic_DTCDefaultValue(void)
{
	#if USE_MALLOC
	DTCNode* DtcNode = DTCHeader;
	while(DtcNode != NULL)
	{
		DtcNode->DTCStatus.DTCStatusByte = 0x50;
		DtcNode->FaultOccurrences = 0;
		DtcNode->OldCounter = 0;
		DtcNode->GoneCounter = 0;
		DtcNode->TripCounter = 0;
		Diagnostic_EEProm_Write(DtcNode->EEpromAddr, DTC_BYTE_NUMBER_TO_SAVE , &(DtcNode->DTCStatus.DTCStatusByte));
		DtcNode= DtcNode->next;
	}
	#else

	#endif
}

void Diagnostic_BindingSnapshot(void)
{
	uint8_t i,j;
	uint16_t SnapshotDataLength = 0;
	for(i = 0 ;  i < SnapShotAdded ; i ++)
	{
		SnapshotDataLength += SnapShots[i].dataLength;
	}
	
	for(i = 0 ;  i < MAX_DTC_NUMBER ; i ++)
	{
		DTCS[i].SnapShotEEpromAddr = ModuleEEpromStartAddr + EEpromUsed + SnapshotDataLength * i;
	}
}

void Diagnostic_LoadAllData(void)
{
	Diagnostic_LoadAllDTC();
	Diagnostic_LoadSecuriyFaultCounter();
	Diagnostic_BindingSnapshot();
}

/*
** ===================================================================
**eeprom driver end
** ===================================================================
*/

void Diagnostic_ConfigVIN(uint8_t length, uint8_t* data)
{
	DIDNode* didNode = SearchDidNode(0xF190);
	if(didNode != NULL && didNode->dataLength == length)
	{
		if(didNode->RWAttr == READWRITE && didNode->didType == EEPROM_DID)
		{
			Diagnostic_EEProm_Write((uint32_t)(didNode->dataPointer) , didNode->dataLength , data);
		}
	}
}

#if USE_J1939_DTC
void Diagnostic_DM1MsgEnable(bool dm1en)
{
	DiagDM1Enable = dm1en;
}
#endif
