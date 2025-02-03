#ifndef __MODBUS_H
#define __MODBUS_H

/* Includes ------------------------------------------------------------------*/

#define EN_485 4  // 根據實際 RS485 Enable 腳位設置
#define MOD_TX_ID 0xC0

//----------------MOD Command-----------------//
#define MOD_CALIBRATION             0xD000
#define MOD_CALI_STEP               0xD001
#define MOD_WRITE_SVR               0xD002
#define MOD_CALIBRATION_TYPE        0xD010
#define MOD_READ_PSU_MODE           0xD011
#define MOD_READ_HIGH_LIMIT         0xD012
#define MOD_READ_LOW_LIMIT          0xD013
#define MOD_CALIBRATION_POINT       0xD014
#define MOD_AC_SOURCE_SET_B0B5      0xD020
#define MOD_DC_SOURCE_SET_B0B5      0xD023
#define MOD_DC_LOAD_SET_B0B5        0xD026
#define MOD_POWERMETER_SET_B0B5     0xD029
#define MOD_METER_SET_B0B5          0xD02C

#define MOD_MODEL_NAME_B0B5         0x0086
#define MOD_MODEL_NAME_B6B11        0x0089

extern int serial_fd;

extern void Modbus_Init(void);
extern void Modbus_TxProcess_Read(uint16_t StartAddress);
extern void Modbus_TxProcess_Write(uint16_t StartAddress);
extern int Modbus_RxProcess(uint8_t *response, size_t max_response_size);

#endif /* __MODBUS_H */