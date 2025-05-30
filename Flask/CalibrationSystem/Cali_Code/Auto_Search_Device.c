/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Auto_Search_Device.h"
#include "Chroma_meter_51101_8.h"
#include "Canbus.h"
#include "Modbus.h"

/* Parameter -----------------------------------------------*/
char Device_information[MAX_DEVICES][MAX_NAME_LENGTH];
int device_count = 0;
Device Device_USB_Port[MAX_DEVICES];
int device_usb_port_count = 0;

int Chroma_51101_Data = 0;
float Device_measured_value = 0.0f;

/* function -----------------------------------------------*/
void Store_Device_information(const char *device_path, const char *device_name);
int SCPI_Read_Process(const char *device_path, const char *command);
int SCPI_Write_Process(const char *device_path, const char *command);
int Chroma_51101_Read_Process(const char *device_path, unsigned char command, uint8_t param1, uint8_t response_length);
int Chroma_51101_Write_Process(const char *device_path, unsigned char command, uint8_t param1, uint8_t param2, uint8_t response_length);
const char* Set_DCLoad_Current_Static(float load_value);

void scan_usb_devices(void);
const char* Get_Device_information(int index);
int Device_Open_Init(const char *device_path);
void Device_Close(void);
void SCPI_Device_Comm_Control_Close(void);
void log_to_csv(float measured_value);

int Device_Open_Init(const char *device_path) {
    int fd;

    if (device_usb_port_count >= MAX_DEVICES) {
        fprintf(stderr, "Error: Too many USB devices detected!\n");
        return -1;
    }

    if (strstr(device_path, "usbtmc") != NULL) 
    {
        fd = open(device_path, O_RDWR);
    } 
    else if (strstr(device_path, "ttyUSB") != NULL) 
    {
        fd = init_serial(device_path);
    } 
    else 
    {
        return -1;
    }

    if (fd < 0) 
    {
        perror("Failed to open device");
        return -1;
    }

    Device_USB_Port[device_usb_port_count].fd = fd;
    strncpy(Device_USB_Port[device_usb_port_count].device_path, device_path, MAX_NAME_LENGTH - 1);
    Device_USB_Port[device_usb_port_count].device_path[MAX_NAME_LENGTH - 1] = '\0';
    device_usb_port_count++;

    return 0;
}

void Device_Close(void) {
    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        close(Device_USB_Port[i].fd);
        Device_USB_Port[i].fd = 0;
        memset(Device_USB_Port[i].device_path, 0, MAX_NAME_LENGTH);
    }
    device_usb_port_count = 0;
}

void Store_Device_information(const char *device_path, const char *device_name) {
    char temp[MAX_NAME_LENGTH];
    if (device_count < MAX_DEVICES) {
        snprintf(temp, sizeof(temp), "%s,%s", device_path, device_name);
        strncpy(Device_information[device_count], temp, MAX_NAME_LENGTH - 1);
        Device_information[device_count][MAX_NAME_LENGTH - 1] = '\0';

        device_count++;
    } else {
        printf("Device storage full! Cannot store more devices.\n");
    }
}

// SCPI read data process
int SCPI_Read_Process(const char *device_path, const char *command) {
    char write_buffer[MAX_NAME_LENGTH];
    char read_buffer[MAX_NAME_LENGTH];
    char temp[MAX_NAME_LENGTH];
    int receive_len = 0;
    float Meter_measured_value = 0.0f;
    int fd = -1;

    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        if (strcmp(device_path, Device_USB_Port[i].device_path) == 0)
        {
            fd = Device_USB_Port[i].fd;
            break;
        }
    }

    if (fd < 0) 
    {
        printf("Error: Device %s not found!\n", device_path);
        Cali_Result = DEVICE_FAIL;         //No connect device
        return -1;
    }

    snprintf(write_buffer, sizeof(write_buffer), "%s", command);
    // send
    if (write(fd, write_buffer, strlen(write_buffer)) < 0) {
        Cali_Result = DEVICE_FAIL;         //Write fail
        return -1;
    }
    if (strcmp(command, SCPI_READ) == 0)
    {
        if (Wait_for_response_poll(fd, 500) > 0) {
            // read respone
            memset(read_buffer, 0, sizeof(read_buffer));
            receive_len = read(fd, read_buffer, MAX_NAME_LENGTH - 1);
        }
    }
    else
    {
        usleep(20000); // wait respone

        // read respone
        memset(read_buffer, 0, sizeof(read_buffer));
        receive_len = read(fd, read_buffer, MAX_NAME_LENGTH - 1);
    }

    if (receive_len > 0) {
        read_buffer[receive_len] = '\0';

        if(strcmp(command, SCPI_IDN) == 0) //Determine USB port is usbtmc*
        {
            snprintf(temp, sizeof(temp), "%s,%s", device_path, read_buffer);
            strncpy(Device_information[device_count], temp, MAX_NAME_LENGTH - 1);
            Device_information[device_count][MAX_NAME_LENGTH - 1] = '\0';
            device_count++;
        }
        else if((strcmp(command, SCPI_VOLT_RMS) == 0) || (strcmp(command, SCPI_VOLT_DC) == 0) 
            || (strcmp(command, SCPI_CURR_RMS) == 0) || (strcmp(command, SCPI_CURR_DC) == 0)
            || (strcmp(command, SCPI_FREQ) == 0))
        {
            Device_measured_value = atof(read_buffer);
            //printf("Device_measured_value: %.3f\n", Device_measured_value);
            //log_to_csv(Device_measured_value);
        }
        else if(strcmp(command, SCPI_READ) == 0)
        {
            sscanf(read_buffer, "%f,", &Meter_measured_value);
            Device_measured_value = (Meter_measured_value / Current_Shunt_Factor);
        }
        return 0;
    }
    else if (receive_len < 0) {
        perror("Error reading from SCPI device");
        Cali_Result = DEVICE_FAIL;         //Read fail
        return -1;
    }
    else
    {
        return -1;
    }
    
}

// SCPI write data process
int SCPI_Write_Process(const char *device_path, const char *command) {
    char write_buffer[MAX_NAME_LENGTH];
    int fd = -1;

    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        if (strcmp(device_path, Device_USB_Port[i].device_path) == 0)
        {
            fd = Device_USB_Port[i].fd;
            break;
        }
    }

    if (fd < 0) 
    {
        printf("Error: Device %s not found!\n", device_path);
        Cali_Result = DEVICE_FAIL;         //No connect device
        return -1;
    }

    snprintf(write_buffer, sizeof(write_buffer), "%s", command);

    // send
    if (write(fd, write_buffer, strlen(write_buffer)) < 0) {
        Cali_Result = DEVICE_FAIL;         //Write fail
        return -1;
    }

    return 0;
}

// Determine USB port is ttyUSB*
int Chroma_51101_Read_Process(const char *device_path, unsigned char command, uint8_t param1, uint8_t response_length) {
    char Receive_respone[MAX_NAME_LENGTH];
    int receive_datalen;
    char Device_name[MAX_NAME_LENGTH];
    int fd = -1;

    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        if (strcmp(device_path, Device_USB_Port[i].device_path) == 0)
        {
            fd = Device_USB_Port[i].fd;
            break;
        }
    }

    if (fd < 0) 
    {
        printf("Error: Device %s not found!\n", device_path);
        Cali_Result = DEVICE_FAIL;         //No connect device
        return -1;
    }

    receive_datalen = Read_Packet(fd, command, param1, response_length ,Receive_respone);
    if (receive_datalen > 0) {
        if (command == GET_DEVICE_ADDRESS)
        {
            strncpy(Device_name, "Chroma,51101-8", MAX_NAME_LENGTH - 1);
            Device_name[MAX_NAME_LENGTH - 1] = '\0';
            Store_Device_information(device_path, Device_name);
        }
        else if (command == GET_SENSOR_DATA)
        {
            for (int i = 2; i< 6; i++)
            {
                Chroma_51101_Data |= (Receive_respone[i] << ((5-i) * 8));
            }
            Device_measured_value = (float) ((Chroma_51101_Data / 10000.0f) / Current_Shunt_Factor);
            Chroma_51101_Data = 0;
        }
        return 0;
    }
    else if (receive_datalen < 0)
    {
        Cali_Result = DEVICE_FAIL;         //Read fail
        return -1;
    }
    else
    {
        return -1;
    }
    
}

int Chroma_51101_Write_Process(const char *device_path, unsigned char command, uint8_t param1, uint8_t param2, uint8_t response_length) {
    char Receive_respone[MAX_NAME_LENGTH];
    int receive_datalen;
    char Device_name[MAX_NAME_LENGTH];
    int fd = -1;

    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        if (strcmp(device_path, Device_USB_Port[i].device_path) == 0)
        {
            fd = Device_USB_Port[i].fd;
            break;
        }
    }

    if (fd < 0) 
    {
        printf("Error: Device %s not found!\n", device_path);
        Cali_Result = DEVICE_FAIL;         //No connect device
        return -1;
    }

    receive_datalen = Write_Packet(fd, command, param1, param2, response_length);
    if (receive_datalen < 0)
    {
        Cali_Result = DEVICE_FAIL;         //Write fail
        return -1;
    }

    return 0;
}

void scan_usb_devices(void) {
    DIR *dir;
    struct dirent *entry;
    char device_path[MAX_NAME_LENGTH];
    uint8_t scan_can_device_count = 0;
    uint8_t scan_mod_device_count = 0;

    memset(Device_information, 0, sizeof(Device_information));
    device_count = 0;
    Device_Close();

    memset(&frame, 0, sizeof(struct can_frame));
    Canbus_Init();
    Canbus_ask_name = 0;

    while (scan_can_device_count < 5) {
        switch(Canbus_ask_name)
        {
            case 0: //no model name
            {
                // Send 0x0082
                Canbus_TxProcess_Read(CAN_TX_ID_DEVICE, CAN_MODEL_NAME_B0B5);
                // Receive
                Canbus_RxProcess(CAN_MODEL_NAME_B0B5);
                break;
            }

            case 1: //half model name
            {
                // Send 0x0083
                Canbus_TxProcess_Read(CAN_TX_ID_DEVICE, CAN_MODEL_NAME_B6B11);
                // Receive
                Canbus_RxProcess(CAN_MODEL_NAME_B6B11);
                break;
            }

            case 2: //full model name
            {
                Canbus_TxProcess_Write(CAN_TX_ID_DEVICE, CAN_SYSTEM_CONFIG);
                usleep(20000);
                DCS_Remote_Switch = 0;
                Canbus_TxProcess_Write(CAN_TX_ID_DEVICE, CAN_OPERATION);
                usleep(20000);
                Store_Device_information("CANBUS", machine_type);
                scan_can_device_count = 5;
            }

            default:
                break;
        }
        usleep(20000); // Delay 20ms
        scan_can_device_count++;
    }
    
    Modbus_Init();
    Modbus_ask_name = 0;

    while (scan_mod_device_count < 5) {
        switch(Modbus_ask_name)
        {
            case 0: //no model name
            {
                Modbus_TxProcess_Read(MOD_TX_ID_DEVICE, MOD_MODEL_NAME_B0B5);

                Modbus_RxProcess(MOD_MODEL_NAME_B0B5);
                break;
            }

            case 1: //half model name
            {
                Modbus_TxProcess_Read(MOD_TX_ID_DEVICE, MOD_MODEL_NAME_B6B11);

                Modbus_RxProcess(MOD_MODEL_NAME_B6B11);
                break;
            }

            case 2: //full model name
            {
                Modbus_TxProcess_Write(MOD_TX_ID_DEVICE, MOD_SYSTEM_CONFIG);
                usleep(20000);
                DCS_Remote_Switch = 0;
                Modbus_TxProcess_Write(MOD_TX_ID_DEVICE, MOD_OPERATION);
                usleep(20000);
                Store_Device_information("MODBUS", machine_type);
                scan_mod_device_count = 5;
            }

            default:
                break;
        }
        usleep(20000); // Delay 20ms
        scan_mod_device_count++;
    }

    // Scan /dev Table usbtmc and ttyUSB devices
    dir = opendir("/dev");
    if (!dir) {
        perror("Failed to open /dev directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "usbtmc", 6) == 0 || strncmp(entry->d_name, "ttyUSB", 6) == 0) {
            snprintf(device_path, sizeof(device_path), "/dev/%s", entry->d_name);
            printf("Device: %s\n", device_path);

            if (Device_Open_Init(device_path) == 0) {
                if ((strstr(device_path, "usbtmc") != NULL && SCPI_Read_Process(device_path, SCPI_IDN) == 0) ||
                    (strstr(device_path, "ttyUSB") != NULL && SCPI_Read_Process(device_path, SCPI_IDN) == 0) ||
                    (strstr(device_path, "ttyUSB") != NULL && Chroma_51101_Read_Process(device_path, GET_DEVICE_ADDRESS, 0x00, 0) == 0)) 
                {
                    printf("Device: %s identified\n", device_path);
                    if (strstr(device_path, "usbtmc") != NULL)
                    {
                        SCPI_Write_Process(device_path, SCPI_LOCAL_STATE);
                    }
                } else {
                    printf("Failed to communicate with: %s\n", device_path);
                }
            } else {
                printf("Failed to open: %s\n", device_path);
                Cali_Result = DEVICE_FAIL;     //Open init port fail
            }
        }
    }
    closedir(dir);
}

void SCPI_Device_Comm_Control_Close(void) {
    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        if (strstr(Device_USB_Port[i].device_path, "usbtmc") != NULL)
        {
            SCPI_Write_Process(Device_USB_Port[i].device_path, SCPI_LOCAL_STATE);
        }
    }
}

const char* Set_DCLoad_Current_Static(float load_value) {
    static char load_command[MAX_NAME_LENGTH];

    snprintf(load_command, sizeof(load_command), "%s %.2fA\n", SCPI_CURR_STAT_L1, load_value);

    return load_command;
}

const char* Get_Device_information(int index) {
    if (index >= 0 && index < device_count) {
        return Device_information[index];
    } else {
        return "Invalid Index";
    }
}

void log_to_csv(float measured_value) {
    FILE *fp = fopen("DWAM_TEST.csv", "a"); // 以 "a" 模式開啟，追加數據
    if (fp == NULL) {
        printf("無法開啟 CSV 檔案\n");
        return;
    }

    // 取得當前時間
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 寫入 CSV 格式：時間, 測量值
    fprintf(fp, "%s,%.3f\n", time_str, measured_value);

    fclose(fp);
}

/*
int main() {
    printf("Starting USB device scan...\n");
    scan_usb_devices();
    printf("Scan complete.\n");
    Get_Device_information(0);
    Get_Device_information(1);
    return 0;
}
*/
