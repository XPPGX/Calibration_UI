#ifndef __AUTO_SEARCH_DEVICE_H
#define __AUTO_SEARCH_DEVICE_H

#define MAX_DEVICES 10
#define MAX_NAME_LENGTH 256

//Chroma series common
#define SCPI_IDN            "*IDN?\n"
#define SCPI_LOCAL_STATE    "SYSTem:LOCal\n"
//Chroma series power meter
#define SCPI_VOLT           "MEASure:VOLTage?\n"
#define SCPI_VOLT_RMS       "MEASure:VOLTage:RMS?\n"
#define SCPI_VOLT_DC        "MEASure:VOLTage:DC?\n"
#define SCPI_VOLT_PEAK_PLUS        "MEASure:VOLTage:PEAK+?\n"
#define SCPI_VOLT_PEAK_MINUS       "MEASure:VOLTage:PEAK-?\n"
#define SCPI_VOLT_THD       "MEASure:VOLTage:THD?\n"

#define SCPI_CURR           "MEASure:CURRent?\n"
#define SCPI_CURR_RMS       "MEASure:CURRent:RMS?\n"
#define SCPI_CURR_DC        "MEASure:CURRent:DC?\n"
#define SCPI_CURR_PEAK_PLUS        "MEASure:CURRent:PEAK+?\n"
#define SCPI_CURR_PEAK_MINUS       "MEASure:CURRent:PEAK-?\n"
#define SCPI_CURR_THD       "MEASure:CURRent:THD?\n"
#define SCPI_CURR_INRUSH    "MEASure:CURRent:INRush?\n"

#define SCPI_POWER_REAL     "MEASure:POWer:REAL?\n"
#define SCPI_POWER_PFACTOR  "MEASure:POWer:PFACtor?\n"
#define SCPI_POWER_DC       "MEASure:POWer:DC?\n"

#define SCPI_FREQ           "MEASure:FREQuency?\n"

#define SCPI_SET_CT_ON      "INPut:CT ON\n"
#define SCPI_SET_CT_OFF     "INPut:CT OFF\n"
#define SCPI_SET_CT_RATIO   "INPut:CT:RATio"

//3Phi(Set Channel)
#define SCPI_VOLT_RMS_CH       "MEASure:VOLTage:RMS?"
#define SCPI_VOLT_DC_CH        "MEASure:VOLTage:DC?"
#define SCPI_VOLT_PEAK_PLUS_CH        "MEASure:VOLTage:PEAK+?"
#define SCPI_VOLT_PEAK_MINUS_CH       "MEASure:VOLTage:PEAK-?"
#define SCPI_VOLT_THD_CH       "MEASure:VOLTage:THD?"

#define SCPI_CURR_RMS_CH       "MEASure:CURRent:RMS?"
#define SCPI_CURR_DC_CH        "MEASure:CURRent:DC?"
#define SCPI_CURR_PEAK_PLUS_CH        "MEASure:CURRent:PEAK+?"
#define SCPI_CURR_PEAK_MINUS_CH       "MEASure:CURRent:PEAK-?"
#define SCPI_CURR_THD_CH       "MEASure:CURRent:THD?"
#define SCPI_CURR_INRUSH_CH    "MEASure:CURRent:INRush?"

#define SCPI_POWER_REAL_CH     "MEASure:POWer:REAL?"
#define SCPI_POWER_PFACTOR_CH  "MEASure:POWer:PFACtor?"
#define SCPI_POWER_DC_CH       "MEASure:POWer:DC?"

#define SCPI_FREQ_CH           "MEASure:FREQuency?"

//Chroma series DC Load
#define SCPI_LOAD_ON        "LOAD ON\n"
#define SCPI_LOAD_OFF       "LOAD OFF\n"
#define SCPI_CURR_STAT_L1   "CURR:STAT:L1"
#define SCPI_CURR_STAT_L2   "CURR:STAT:L2"
#define SCPI_VOLT_STAT_L1   "VOLT:STAT:L1"
#define SCPI_VOLT_STAT_L2   "VOLT:STAT:L2"
#define SCPI_VOLT_STAT_ILIMIT "VOLT:STAT:ILIM"

//Chroma series AC Source
#define SCPI_SOURCE_ON      "OUTPut ON\n"
#define SCPI_SOURCE_OFF     "OUTPut OFF\n"
#define SCPI_ACS_SET_VOLT   "VOLT:AC"
#define SCPI_ACS_SET_FREQ   "FREQ"

//GDM8342(Meter)
#define SCPI_READ           "READ?\n"

//ATA1045(AC source)
#define ATA_MODEL           "?MODEL\n"
#define ATA_SET_VOLT        "SVOL\n"
#define ATA_ON              "PON\n"
#define ATA_OFF             "POFF\n"

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