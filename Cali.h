#ifndef __CALI_H
#define __CALI_H

/* Includes ------------------------------------------------------------------*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <termios.h>

#define EN_485 4  // 根據實際 RS485 Enable 腳位設置
#define CANBUS 1
#define MODBUS 2
#define PMBUS 3

#define FINISH 1
#define FAIL 2

#define LED_PIN 17

#define TX_ID 0x000C0300
#define RX_ID 0x000C0200

//----------------CAN Command-----------------//
#define CALIBRATION             0xD000
#define CALI_STEP               0xD001
#define WRITE_SVR               0xD002
#define CALIBRATION_TYPE        0xD010
#define READ_PSU_MODE           0xD011
#define READ_HIGH_LIMIT         0xD012
#define READ_LOW_LIMIT          0xD013
#define CALIBRATION_POINT       0xD014
#define AC_SOURCE_SET           0xD020
#define DC_SOURCE_SET           0xD021
#define DC_LOAD_SET             0xD022
#define POWERMETER_SET          0xD023
#define METER_SET               0xD024

#define MODEL_NAME_B0B5         0x0082
#define MODEL_NAME_B6B10        0x0083

//----------------CALI TYPE-----------------//
#define ACV_DC_OFFSET_3phi 	0x80
#define ACV_DC_OFFSET_1phi 	0x40
#define ACI_DC_OFFSET_3phi 	0x20
#define ACI_DC_OFFSET_1phi 	0x10
#define ACI_3phi			0x08
#define ACI_1phi			0x04
#define DC					0x01

extern char* Get_Machine_Name(void);
extern uint8_t Get_Communication_Type(void);
extern uint8_t Get_Keyboard_Adjustment(void);
extern uint8_t Get_PSU_Calibration_Type(void);
extern uint8_t Get_Calibration_Status(void);
extern uint8_t Get_Calibration_Type_Point(void);
extern void Start_Cali_thread(void);
extern void Stop_Cali_thread(void);

#endif /* __CALI_H */
