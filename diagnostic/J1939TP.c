#ifndef J1939TP_C
#define J1939TP_C

#include "J1939TP.h"
#include "DiagnosticTimer.h"

#define TP_CM_BAM		0x1CECFF19
#define TP_CM_DT		0x1CEBFF19
#define TP_GW_BAM		0x0CECFF25
#define TP_GW_DT		0x0CEBFF25
#define BAM_CONTROL	0x20
#define BAM_RX_BUF_SIZE	80

static SendJ1939CANFun sendInterface = (void*)0;
static unsigned char sendBuf[8];
static unsigned char DTIndex;
static unsigned char TotalFrameLength;
static unsigned char* dataBuf;
static DiagTimer DTTimer;
static DiagTimer BAMTimer;
/***********************j1939 parse***************************/
unsigned char BAMRxState;//  1: requested, 0:idle 2:wait for uplayer handing
unsigned short BAMRxLength;
unsigned char BAMRxFrameLength;
unsigned char BAMRxReceived;
unsigned char BAMRxBuf[BAM_RX_BUF_SIZE];
/***********************j1939 parse***************************/

void J1939TPGetReceiveData(unsigned short* length, unsigned char** dataPointer)
{
	if(BAMRxState == 2)
	{
		BAMRxState = 0;
		*length = BAMRxLength;
		*dataPointer = BAMRxBuf;
	}
	else
	{
		*length = 0;
		*dataPointer = (void*)(0);
	}
}

void J1939TPReceiveData(unsigned long id,unsigned char* data, unsigned char length)
{
	if(length == 8)
	{
		if(id == TP_GW_BAM && BAM_CONTROL == *data)
		{
			BAMRxLength = *(data+1) + (*(data+2) << 8);
			BAMRxFrameLength = *(data+3);
			if(BAMRxLength <= BAMRxFrameLength * 7 && BAMRxLength > (BAMRxFrameLength - 1) * 7 && BAMRxLength <= BAM_RX_BUF_SIZE)
			{
				BAMRxState = 1;
				BAMRxReceived = 0;
			}
		}
		else if(id == TP_GW_DT)
		{
			if(((BAMRxReceived / 7 + 1) == *data) && (BAMRxState == 1))
			{
				uint8_t ValidDataLengthInFrame = (BAMRxLength - BAMRxReceived >= 7) ? 7 : (BAMRxLength - BAMRxReceived) ;
				memcpy(BAMRxBuf + BAMRxReceived,data + 1, ValidDataLengthInFrame);
				BAMRxReceived += ValidDataLengthInFrame;
				if(BAMRxReceived >= BAMRxLength)
				{
					BAMRxState = 2;
				}
			}
		}
	}
}
	
void TPCMSetParamBAM(SendJ1939CANFun sendinterface)
{
	sendInterface = sendinterface;
}

void TPCMRequestBAM(short messageLength,  long PGN,unsigned char* buf)
{
	if(messageLength % 7 == 0)
	{
		TotalFrameLength = messageLength / 7;
	}
	else
	{
		TotalFrameLength = (messageLength / 7) + 1;
	}
		
	sendBuf[0] = BAM_CONTROL;
	sendBuf[1] = messageLength;
	sendBuf[2] = messageLength >> 8;
	sendBuf[3] = TotalFrameLength;
	sendBuf[4] = 0xFF;
	sendBuf[5] = (char)(PGN >> 16);
	sendBuf[6] = (char)(PGN >> 8);
	sendBuf[7] = (char)(PGN);
	DTIndex = 1;
	DiagTimer_Set(&DTTimer , 52);
	
	if(sendInterface != ((void*)0))
	{
		sendInterface(TP_CM_BAM,sendBuf,8,0);
	}
	dataBuf = buf;
}

void TPCMDTProc(void)
{	
	
	if(DTIndex <= TotalFrameLength)
	{
		if(DiagTimer_HasExpired(&DTTimer))
		{
			DiagTimer_Set(&DTTimer , 52);
			sendBuf[0] = DTIndex;
			memcpy(sendBuf + 1,dataBuf + 7*(DTIndex - 1),7);
			sendInterface(TP_CM_DT,sendBuf,8,0);
			DTIndex++;
		}
	}
}
#endif
