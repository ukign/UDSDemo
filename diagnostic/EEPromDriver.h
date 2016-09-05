#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H
void Diagnostic_EEProm_Init(void);
byte Diagnostic_EEProm_Write(word add, byte size, byte *data);
byte Diagnostic_EEProm_Read(word add, byte size, byte *data);
#endif