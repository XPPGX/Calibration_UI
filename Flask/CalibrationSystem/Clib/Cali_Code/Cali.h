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

#define CANBUS  1
#define MODBUS  2
#define PMBUS   3

#define FINISH          1
#define NOT_COMPLETE    2
#define FAIL            3

#define YES     1
#define NO      2

#define INT_MAX 1000
#define MAX_STRING_LENGTH 256

//CALI step
#define SEND_KEY_READ_MODE      1
#define UI_RESET_CALI           2
#define READ_UI_SET_POINT       3
#define READ_CALI_STATUS        4

//----------------CALI STATUS----------------//
#define CALI_DEVICE_SETTING     0x0000
#define CALI_DATA_LOG           0x0001
#define CALI_POINT_NOMATCH      0x0002
#define CALI_POINT_SETTING      0x0003
#define CALI_FINISH             0xFFFF

//----------------CALI POINT----------------//
#define ACI_DC_OFFSET_1P 	0x0001
#define DCV 	            0x0002
#define DCI_POS_FULL_LOAD 	0x0003
#define DCI_NEG_FULL_LOAD 	0x0004
#define ACV 	            0x0005
#define ACI_LIGHT_LOAD 	    0x0006
#define ACI_HEAVY_LOAD 	    0x0007
#define CALI_POINT_END 	    0xFFFF

/* Parameter -----------------------------------------------*/
extern int SVR_value;
extern uint16_t Cali_Result;
extern uint16_t UI_Calibration_Point;

/* function -----------------------------------------------*/
extern void Start_Cali_thread(void);
extern void Stop_Cali_thread(void);
//UI Get
extern char* Get_Machine_Name(void);
extern uint8_t Get_Communication_Type(void);
extern uint8_t Get_Keyboard_Adjustment(void);
extern uint16_t Get_Calibration_Status(void);
extern float Get_PSU_Cali_Point_High_Limit(void);
extern float Get_PSU_Cali_Point_Low_Limit(void);
//UI Set
void Check_UI_Reset_Cali(const uint16_t UI_flag);
void Check_UI_Set_Cali_Point_Flag(const uint16_t UI_flag);
void Check_UI_Set_Cali_Point(const uint16_t UI_Cali_Point);
void Check_UI_Set_Cali_Point_Scaling_Factor(const char *Scaling_Factor);
void Check_UI_Set_Cali_Point_USB_Port(const char *USB_Port);
void Check_UI_Set_Cali_Point_Target(const char *Target);

#endif /* __CALI_H */
