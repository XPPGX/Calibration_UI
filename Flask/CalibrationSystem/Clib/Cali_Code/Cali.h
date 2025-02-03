#ifndef __CALI_H
#define __CALI_H

/* Includes ------------------------------------------------------------------*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>             
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <termios.h>
#include <dirent.h>

#define CANBUS 1
#define MODBUS 2
#define PMBUS 3

#define FINISH 1
#define FAIL 2

#define INT_MAX 1000

//----------------CALI TYPE-----------------//
#define ACV_DC_OFFSET_3phi 	0x80
#define ACV_DC_OFFSET_1phi 	0x40
#define ACI_DC_OFFSET_3phi 	0x20
#define ACI_DC_OFFSET_1phi 	0x10
#define ACI_3phi			0x08
#define ACI_1phi			0x04
#define DC					0x01

extern int SVR_value;
extern uint8_t Cali_Status;

extern char* Get_Machine_Name(void);
extern uint8_t Get_Communication_Type(void);
extern uint8_t Get_Keyboard_Adjustment(void);
extern uint8_t Get_PSU_Calibration_Type(void);
extern uint8_t Get_Calibration_Status(void);
extern uint8_t Get_Calibration_Type_Point(void);
extern void Start_Cali_thread(void);
extern void Stop_Cali_thread(void);

#endif /* __CALI_H */