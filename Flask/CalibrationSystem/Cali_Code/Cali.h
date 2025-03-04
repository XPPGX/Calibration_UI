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
#include <poll.h>

#define CANBUS  1
#define MODBUS  2
#define PMBUS   3

#define FINISH              1
#define NOT_COMPLETE        2
#define DEVICE_FAIL         3
#define DUT_FAIL            4
#define CALI_POINT_FAIL     5
#define PERIPHERAL_FAIL     6

#define YES     1
#define NO      2

#define INT_MAX 1000
#define MAX_STRING_LENGTH 256

//CALI step
#define SEND_KEY_READ_MODE      1
#define READ_PSU_SCALING_FACTOR 2
#define UI_RESET_CALI           3
#define READ_UI_SET_POINT       4
#define READ_CALI_STATUS        5
#define READ_CALI_RESULT        6

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
extern volatile char machine_type[13];
extern uint16_t Cali_Result;
extern uint16_t UI_Calibration_Point;
extern uint8_t Canbus_ask_name;
extern uint8_t Manual_Cali_step;
extern uint16_t Cali_status;
extern uint8_t Cali_type_polling;
extern uint8_t Cali_read_step;
extern int16_t PSU_High_Limit_Comm;
extern int16_t PSU_Low_Limit_Comm;
extern float PSU_High_Limit;
extern float PSU_Low_Limit;
extern uint8_t PSU_ACV_Factor_Comm;
extern uint8_t PSU_ACI_Factor_Comm;
extern uint8_t PSU_DCV_Factor_Comm;
extern uint8_t PSU_DCI_Factor_Comm;
extern float PSU_ACV_Factor;
extern float PSU_ACI_Factor;
extern float PSU_DCV_Factor;
extern float PSU_DCI_Factor;
extern char UI_Scaling_Factor[MAX_STRING_LENGTH];
extern char UI_USB_Port[MAX_STRING_LENGTH];
extern char UI_Target[MAX_STRING_LENGTH];
extern float Current_Shunt_Factor;

/* function -----------------------------------------------*/
extern void Start_Cali_thread(void);
extern void Stop_Cali_thread(void);
extern void Manual_Calibration(void);
extern float Scaling_Factor_Convert(uint8_t HexValue);
extern int Wait_for_response_poll(int fd, int timeout_ms);
//UI Get
extern char* Get_Machine_Name(void);
extern uint8_t Get_Communication_Type(void);
extern uint8_t Get_Keyboard_Adjustment(void);
extern uint16_t Get_Calibration_Status(void);
extern float Get_PSU_Cali_Point_High_Limit(void);
extern float Get_PSU_Cali_Point_Low_Limit(void);
extern float Get_Device_Measured_Value(void);
extern uint8_t Get_Calibration_Point_Complete(void);
extern uint8_t Get_Valid_Measured_Value(void);
//UI Set
extern void Check_UI_Reset_Cali(const uint16_t UI_flag);
extern void Check_UI_Set_Cali_Point_Flag(const uint16_t UI_flag);
extern void Check_UI_Set_Cali_Point(const uint16_t UI_Cali_Point);
extern void Check_UI_Set_Cali_Point_Scaling_Factor(const char *Scaling_Factor);
extern void Check_UI_Set_Cali_Point_USB_Port(const char *USB_Port);
extern void Check_UI_Set_Cali_Point_Target(const char *Target);
extern void Check_Calibration_Point_Complete(const uint8_t Complete_flag);

#endif /* __CALI_H */
