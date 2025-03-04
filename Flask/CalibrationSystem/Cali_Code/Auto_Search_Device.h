#ifndef __AUTO_SEARCH_DEVICE_H
#define __AUTO_SEARCH_DEVICE_H

#define MAX_DEVICES 10
#define MAX_NAME_LENGTH 256

#define SCPI_IDN            "*IDN?\n"
#define SCPI_VOLT           "MEASure:VOLTage?\n"
#define SCPI_VOLT_RMS       "MEASure:VOLTage:RMS?\n"
#define SCPI_VOLT_DC        "MEASure:VOLTage:DC?\n"
#define SCPI_CURR           "MEASure:CURRent?\n"
#define SCPI_CURR_RMS       "MEASure:CURRent:RMS?\n"
#define SCPI_CURR_DC        "MEASure:CURRent:DC?\n"
#define SCPI_LOCAL_STATE    "SYSTem:LOCal\n"
#define SCPI_LOAD_ON        "LOAD ON\n"
#define SCPI_LOAD_OFF       "LOAD OFF\n"
#define SCPI_CURR_STAT_L1   "CURR:STAT:L1\n"
#define SCPI_SET_MODE_CCH   "MODE CCH\n"
#define SCPI_READ           "READ?\n"

typedef struct {
    char device_path[MAX_NAME_LENGTH];
    int fd;
} Device;

/* Parameter ----------------------------------------------*/
extern float Device_measured_value;
/* function -----------------------------------------------*/
extern void scan_usb_devices(void);
extern void Store_Device_information(const char *device_path, const char *device_name);
extern int SCPI_Read_Process(const char *device_path, const char *command);
extern int SCPI_Write_Process(const char *device_path, const char *command);
extern int Chroma_51101_Read_Process(const char *device_path, unsigned char command, uint8_t param1, uint8_t response_length);
extern int Chroma_51101_Write_Process(const char *device_path, unsigned char command, uint8_t param1, uint8_t param2, uint8_t response_length);
extern void Device_Close(void);
extern void SCPI_Device_Comm_Control_Close(void);

#endif /* __AUTO_SEARCH_DEVICE_H */