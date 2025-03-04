#ifndef __CANBUS_H
#define __CANBUS_H

/* Includes ------------------------------------------------------------------*/
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>


#define CAN_TX_ID 0x000C0300
#define CAN_RX_ID 0x000C0200

//----------------CAN Command-----------------//
#define CAN_CALIBRATION_KEY         0xD000
#define CAN_CALI_STATUS             0xD001
#define CAN_WRITE_SVR               0xD002
#define CAN_CALIBRATION_POINT       0xD003
#define CAN_CALI_RESULT             0xD004
#define CAN_READ_PSU_MODE           0xD010
#define CAN_READ_HIGH_LIMIT         0xD011
#define CAN_READ_LOW_LIMIT          0xD012
#define CAN_SVR_POLLING             0xD013
#define CAN_AC_SOURCE_SET           0xD020
#define CAN_DC_SOURCE_SET           0xD021
#define CAN_DC_LOAD_SET             0xD022
#define CAN_POWERMETER_SET          0xD023
#define CAN_METER_SET               0xD024

#define CAN_MODEL_NAME_B0B5         0x0082
#define CAN_MODEL_NAME_B6B11        0x0083

#define CAN_SCALING_FACTOR          0x00C0

extern int can_socket;
extern struct can_frame frame;

extern void Canbus_Init(void);
extern void Canbus_TxProcess_Read(uint32_t CAN_Address);
extern void Canbus_TxProcess_Write(uint32_t CAN_Address);
extern void Canbus_RxProcess(uint16_t Commmand);

#endif /* __CANBUS_H */