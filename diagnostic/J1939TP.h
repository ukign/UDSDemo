#ifndef J1939TP_H
#define J1939TP_H

typedef char (*SendJ1939CANFun)(long ID, char *array, char length, char priority);

void TPCMSetParamBAM(SendJ1939CANFun sendinterface);
void TPCMRequestBAM(short messageLength,  long PGN,unsigned char* buf);
void TPCMDTProc(void);
void J1939TPReceiveData(unsigned long id,unsigned char* data, unsigned char length);
void J1939TPGetReceiveData(unsigned short* length, unsigned char** dataPointer);
#endif

