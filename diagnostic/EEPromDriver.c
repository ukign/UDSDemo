#include "cpu.h"
#include "EEpromDriver.h"

/*
** ===================================================================
**eeprom driver start
** ===================================================================
*/
typedef far word * far EEPROM_TAddress; /* Type of address to the EEPROM */
#define EEGlobalToLocal(Address)		((((dword)Address) & 0x3FFFU) | ((((dword)Address) & 0x003FC000UL) << 2) | 0x8000U)
#define EEPROM_AREA_START                 ((EEPROM_TAddress)0x0400UL)
#define EEPROM_AREA_END                   ((EEPROM_TAddress)0x13FFUL)
static bool EEPromEnMode;                    /* Enable/Disable device in a given speed CPU mode */
static word EEBackupArray[0x02];

static void BackupSector(EEPROM_TAddress Addr, word From, word To)
{
	word i;

	for (i = From; i < To; i = i + 2U) {
		EEBackupArray[i/2U] = *(word *far) (((word *far)EEGlobalToLocal(Addr)) + (i/2U)); /* save one sector to RAM */
	}
}

static byte WriteBlock(EEPROM_TAddress Addr, word From, word To, const word* Data)
{
	byte err = 0U;
	word i;
	byte j;
	dword PhraseAddr;

	if(From == To) {
		return ERR_OK;
	}
	i = From;
	PhraseAddr = (dword)Addr;
	while(i < To) {
		/* FSTAT: ACCERR=1,FPVIOL=1 */
		FSTAT = 0x30U;                     /* Clear error flags */
		FCCOBIX = 0U;                      /* Clear index register */
		FCCOBHI = 0x11U;                   /* Program D-Flash command */
		FCCOBLO = (byte)(PhraseAddr >> 16U); /* High address word */
		FCCOBIX++;                         /* Shift index register */
		FCCOB = (word)PhraseAddr;          /* Low address word */
		for(j = 0U;j < 8U;j+=2U) {
			FCCOBIX++;                       /* Shift index register */
			FCCOB = *(const word*)(Data + (i/2U)); /* Load new data */
			i += 2U;
			if(i >= To) {break;}
		}
		FSTAT = 0x80U;                     /* Clear flag command buffer empty */
		while (FSTAT_CCIF == 0U) {}        /* Wait to command complete */
		if ((FSTAT & 0x30U) != 0U) {       /* Is protection violation or acces error detected ? */
			return ERR_NOTAVAIL;             /* If yes then error */
		}
		if (FSTAT_MGSTAT) {                /* Was attempt to write data to the given address errorneous? */
			err = 1U;                        /* If yes then mark an error */
		}
		PhraseAddr += 8U;
	}
	if(err != 0U) {
		return ERR_VALUE;                  /* If yes then error */
	}
	return ERR_OK;                       /* OK */
}


static byte EraseSectorInternal(EEPROM_TAddress Addr)
{
	if (FSTAT_CCIF == 0U) {              /* Is command complete ? */
		return ERR_BUSY;                   /* If yes then error */
	}
	/* FSTAT: ACCERR=1,FPVIOL=1 */
	FSTAT = 0x30U;                       /* Clear error flags */
	FCCOBIX = 0U;                        /* Clear index register */
	FCCOBHI = 0x12U;                     /* Erase D-Flash sector command */
	FCCOBLO = (byte)(((dword)Addr) >> 16); /* High address word */
	FCCOBIX++;                           /* Shift index register */
	FCCOB = (word)(((dword)Addr) & 0xFFFFFFFEUL); /* Low address word aligned to word*/
	FSTAT = 0x80U;                       /* Clear flag command buffer empty */
	while (FSTAT_CCIF == 0U) {}          /* Wait to command complete */
	if ((FSTAT & 0x23U) != 0U) {         /* Is access error or other error detected ? */
		return ERR_NOTAVAIL;               /* If yes then error */
	}
	return ERR_OK;                       /* OK */
}

static byte WriteWord(EEPROM_TAddress AddrRow,word Data16)
{
	if (FSTAT_CCIF == 0U) {              /* Is previous command in process ? */
		return ERR_BUSY;                   /* If yes then error */
	}
	/* FSTAT: ACCERR=1,FPVIOL=1 */
	FSTAT = 0x30U;                       /* Clear error flags */
	FCCOBIX = 0U;                        /* Clear index register */
	FCCOBHI = 0x11U;                     /* Program D-Flash command */
	FCCOBLO = (byte)(((dword)AddrRow) >> 16); /* High address word */
	FCCOBIX++;                           /* Shift index register */
	FCCOB = (word)((dword)AddrRow);      /* Low address word */
	FCCOBIX++;                           /* Shift index register */
	FCCOB = Data16;                      /* Load new data */
	FSTAT = 0x80U;                       /* Clear flag command complete */
	if ((FSTAT & 0x30U) != 0U) {         /* Is protection violation or acces error detected ? */
		return ERR_NOTAVAIL;               /* If yes then error */
	}
	while (!FSTAT_CCIF) {}               /* Wait for command completition */
	if (FSTAT_MGSTAT) {                  /* Was attempt to write data to the given address errorneous? */
		return ERR_VALUE;                  /* If yes then error */
	}
	return ERR_OK;
}

byte EEProm_SetByte(EEPROM_TAddress Addr,byte Data)
{
	byte err;
	word Data16;
	EEPROM_TAddress SecAddr;               /* EEPROM Sector address */

	if(!EEPromEnMode) {                        /* Is the device disabled in the actual speed CPU mode? */
		return ERR_SPEED;                  /* If yes then error */
	}
	if(((dword)Addr < (dword)EEPROM_AREA_START) || ((dword)Addr > (dword)EEPROM_AREA_END)) { /* Is given address out of EEPROM area array ? */
		return ERR_RANGE;                  /* If yes then error */
	}
	if(!FSTAT_CCIF) {                    /* Is reading from EEPROM possible? */
		return ERR_BUSY;                   /* If no then error */
	}
	SecAddr = (EEPROM_TAddress)((dword)Addr & 0x00FFFFFEUL); /* Aligned word address */
	if (*((word *far)EEGlobalToLocal(SecAddr)) == 0xFFFFU) { /* Is the word erased? */
		if (((dword)Addr) & 1U) {          /* Not Aligned word ? */
			return(WriteWord(SecAddr, ((*((word *far)EEGlobalToLocal(SecAddr)) & 0xFF00U) | Data)));
		} else {
			return(WriteWord(Addr, (((word)Data << 8) | (*(((byte *far)EEGlobalToLocal(Addr)) + 1U))))); /* Aligned word ? */
		}
	} else {                             /* Is given address non-erased ? */
		SecAddr = (EEPROM_TAddress)((dword)Addr & 0x00FFFFFCUL); /* Sector Aligned address */
		BackupSector(SecAddr, 0U, 0x04U);  /* Backup sector */
		Data16 = EEBackupArray[(((dword)Addr) % 0x04U) / 2U]; /* Store new data to backup array*/
		if (((dword)Addr) & 1U) {          /* Not Aligned word ? */
			Data16 = (Data16 & 0xFF00U) | Data;
		} else {
			Data16 = ((word)Data << 8) | (Data16 & 0xFFU);
		}
		EEBackupArray[(((dword)Addr) % 0x04U) / 2U] = Data16; /* Write new data to saved sector */
		err = EraseSectorInternal(Addr);   /* Erase sector */
		if(err != 0U) {
			return(err);                     /* Return error code if previous operation finished not correctly */
		}
		err = WriteBlock(SecAddr, 0U, 0x04U,EEBackupArray); /* Restore sector */
		return(err);
	}
}

byte EEProm_GetByte(EEPROM_TAddress Addr,byte *Data)
{
	if(((dword)Addr < (dword)EEPROM_AREA_START) || ((dword)Addr > (dword)EEPROM_AREA_END)) { /* Is given address out of EEPROM area array ? */
		return ERR_RANGE;                  /* If yes then error */
	}
	if(!FSTAT_CCIF) {                    /* Is reading from EEPROM possible? */
		return ERR_BUSY;                   /* If no then error */
	}
	*Data = *((byte *far)EEGlobalToLocal(Addr));
	return ERR_OK;                       /* OK */
}

/*
** ===================================================================
**     Method      :  IEE1_SetWord (component IntEEPROM)
**
**     Description :
**         This method writes a given word (2 bytes) to the specified
**         address in EEPROM. The method also sets address pointer for
**         <SetActByte> and <GetActByte> methods (applicable only if
**         these methods are enabled). The pointer is set to address
**         passed as the parameter + 1.
**     Parameters  :
**         NAME            - DESCRIPTION
**         Addr            - Address to EEPROM
**         Data            - Data to write
**     Returns     :
**         ---             - Error code, possible codes: 
**                           - ERR_OK - OK 
**                           - ERR_SPEED - the component does not work
**                           in the active speed mode 
**                           - ERR_BUSY - device is busy 
**                           - ERR_VALUE - verification of written data
**                           failed (read value does not match with
**                           written value) 
**                           - ERR_NOTAVAIL - other device-specific
**                           error 
**                           - ERR_RANGE - selected address out of the
**                           selected address range
** ===================================================================
*/
byte EEProm_SetWord(EEPROM_TAddress Addr,word Data)
{
	byte err;
	EEPROM_TAddress SecAddr;               /* EEPROM Sector address */

	if(!EEPromEnMode) 
	{                        /* Is the device disabled in the actual speed CPU mode? */
		return ERR_SPEED;                  /* If yes then error */
	}
	if(((dword)Addr < (dword)EEPROM_AREA_START) || ((dword)Addr > (dword)EEPROM_AREA_END)) 
	{ /* Is given address out of EEPROM area array ? */
		return ERR_RANGE;                  /* If yes then error */
	}
	if ((dword)Addr & 0x01U)
	{           /* Aligned address ? */
		return ERR_NOTAVAIL;
	}
	if(!FSTAT_CCIF) 
	{                    /* Is reading from EEPROM possible? */
		return ERR_BUSY;                   /* If no then error */
	}
	if (*((word *far)EEGlobalToLocal(Addr)) == 0xFFFFU) 
	{ /* Is the word erased? */
		return(WriteWord(Addr, Data));
	}
	else
	{                             /* Is given address non-erased ? */
		SecAddr = (EEPROM_TAddress)((dword)Addr & 0x00FFFFFCUL); /* Sector Aligned address */
		BackupSector(SecAddr, 0U, 0x04U);  /* Backup sector */
		EEBackupArray[(((dword)Addr) % 0x04U) / 2U] = Data; /* Write new data to saved sector */
		err = EraseSectorInternal(Addr);   /* Erase sector */
		if(err) {
			return(err);                     /* Return error code if previous operation finished not correctly */
		}
		err = WriteBlock(SecAddr, 0U, 0x04U,EEBackupArray); /* Restore sector */
		return(err);
	}
}

/*
** ===================================================================
**     Method      :  IEE1_SetLong (component IntEEPROM)
**
**     Description :
**         This method writes a given long word (4 bytes) to the
**         specified address in EEPROM. The method also sets address
**         pointer for <SetActByte> and <GetActByte> methods
**         (applicable only if these methods are enabled). The pointer
**         is set to address passed as the parameter + 3.
**     Parameters  :
**         NAME            - DESCRIPTION
**         Addr            - Address to EEPROM
**         Data            - Data to write
**     Returns     :
**         ---             - Error code, possible codes: 
**                           - ERR_OK - OK 
**                           - ERR_SPEED - the component does not work
**                           in the active speed mode 
**                           - ERR_BUSY - device is busy 
**                           - ERR_VALUE - verification of written data
**                           failed (read value does not match with
**                           written value) 
**                           - ERR_NOTAVAIL - other device-specific
**                           error 
**                           - ERR_RANGE - selected address out of the
**                           selected address range
** ===================================================================
*/
byte EEProm_SetLong(EEPROM_TAddress Addr,dword Data)
{
	byte err;
	EEPROM_TAddress SecAddr;               /* EEPROM Sector address */

	if(!EEPromEnMode) {                        /* Is the device disabled in the actual speed CPU mode? */
		return ERR_SPEED;                  /* If yes then error */
	}
	if(((dword)Addr < (dword)EEPROM_AREA_START) || ((dword)Addr > (dword)EEPROM_AREA_END)) { /* Is given address out of EEPROM area array ? */
		return ERR_RANGE;                  /* If yes then error */
	}
	if ((dword)Addr & 0x03U) {           /* Aligned address ? */
		return ERR_NOTAVAIL;
	}
	if(!FSTAT_CCIF) {                    /* Is reading from EEPROM possible? */
		return ERR_BUSY;                   /* If no then error */
	}
	if (*((dword *far)EEGlobalToLocal(Addr)) == 0xFFFFFFFFUL) 
	{ /* Is the dword erased? */
		err = WriteBlock(Addr, 0U, 0x04U,((word*)&Data)); /* Write dword */
	}
	else
	{                             /* Is given address non-erased ? */
		SecAddr = (EEPROM_TAddress)((dword)Addr & 0x00FFFFFCUL); /* Sector Aligned address */
		BackupSector(SecAddr, 0U, 0x04U);  /* Backup sector */
		EEBackupArray[(((dword)Addr) % 0x04U) / 2U] = (word)(Data >> 16); /* Write new data to saved sector */
		EEBackupArray[((((dword)Addr) % 0x04U) / 2U) + 1U] = (word)(Data); /* Write new data to saved sector */
		err = EraseSectorInternal(Addr);   /* Erase sector */
		if(err) {
			return(err);                     /* Return error code if previous operation finished not correctly */
		}
		err = WriteBlock(SecAddr, 0U, 0x04U,EEBackupArray); /* Restore sector */
	}
	return(err);
}

void EEProm_SetHigh(void)
{
	EEPromEnMode = TRUE;                       /* Set the flag "device enabled" in the actual speed CPU mode */
}

void EEProm_SetSlow(void)
{
	EEPromEnMode = FALSE;                      /* Set the flag "device disabled" in the actual speed CPU mode */
}


/*
** ===================================================================
**eeprom driver end
** ===================================================================
*/

void Diagnostic_EEProm_Init(void)
{
	FCLKDIV = 0x0FU;                     /* Set up Clock Divider Register */
	EEPromEnMode = TRUE;                       /* Set the flag "device enabled" in the actual speed CPU mode */
}

/*----------------------FLASH write-------------------*/  
byte Diagnostic_EEProm_Write(word add, byte size, byte *data)
{ 
	uint8_t i;
	for(i = 0; i < size; )
	{
		if(((add + i) %4) == 0 && (size - i) >= 4)
		{
			EEProm_SetLong((EEPROM_TAddress)(add + i) , *((uint32_t*)(data + i)));
			i += 4;
		}
		else if(((add + i) %2) == 0 && (size - i) >= 2)
		{
			EEProm_SetWord((EEPROM_TAddress)(add + i) , *((uint16_t*)(data + i)));
			i += 2;
		}
		else
		{
			EEProm_SetByte((EEPROM_TAddress)(add + i) , *(data + i));
			i++;
		}
	}
}	

/*----------------------FLASH write-------------------*/  
byte Diagnostic_EEProm_Read(word add, byte size, byte *data)
{ 
	int i;
	for(i = 0; i < size ; i++)
	{
		EEProm_GetByte((EEPROM_TAddress)(add + i) , data + i);
	}
}
