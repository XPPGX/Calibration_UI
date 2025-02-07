#ifndef __AUTO_SEARCH_DEVICE_H
#define __AUTO_SEARCH_DEVICE_H

#define MAX_DEVICES 10
#define MAX_NAME_LENGTH 256

#define SCPI_IDN        "*IDN?\n"
#define SCPI_VOLT_RMS   "MEASure:VOLTage:RMS?\n"
#define SCPI_VOLT_DC    "MEASure:VOLTage:DC?\n"
#define SCPI_CURR_RMS   "MEASure:CURRent:RMS?\n"
#define SCPI_CURR_DC    "MEASure:CURRent:DC?\n"

typedef struct {
    char device_path[MAX_NAME_LENGTH];
    int fd;
} Device;

/* function -----------------------------------------------*/
extern void Store_Device_information(const char *device_path, const char *device_name);
extern int SCPI_Read_Process(const char *device_path, const char *command);
extern int Chroma_51101_Read_Process(const char *device_path, unsigned char command);

#endif /* __AUTO_SEARCH_DEVICE_H */