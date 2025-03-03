/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Canbus.h"
#include "Modbus.h"
#include "Auto_Search_Device.h"
#include "Chroma_meter_51101_8.h"

/* Parameter -----------------------------------------------*/
uint8_t space_pressed = 0; //1:Coarse 0:Fine
volatile char machine_type[13]; //Receive the machine type returned by the command 0x0082 and 0x0083
int SVR_value = 0;
uint16_t Cali_Result;
uint16_t PSU_Reset_Cali_Flag = 0;
uint16_t UI_Set_Cali_Point_Flag = 0;
uint16_t UI_Calibration_Point = 0;
char UI_Scaling_Factor[MAX_STRING_LENGTH] = {0};
char UI_USB_Port[MAX_STRING_LENGTH] = {0};
char UI_Target[MAX_STRING_LENGTH] = {0};
int16_t PSU_High_Limit_Comm = 0;
int16_t PSU_Low_Limit_Comm = 0;
float PSU_High_Limit = 0.0f;
float PSU_Low_Limit = 0.0f;
uint8_t PSU_ACV_Factor_Comm = 0;
uint8_t PSU_ACI_Factor_Comm = 0;
uint8_t PSU_DCV_Factor_Comm = 0;
uint8_t PSU_DCI_Factor_Comm = 0;
float PSU_ACV_Factor = 0.0f;
float PSU_ACI_Factor = 0.0f;
float PSU_DCV_Factor = 0.0f;
float PSU_DCI_Factor = 0.0f;
int Chroma_51101_Channel;
char Chroma_51101_Sensor[MAX_STRING_LENGTH];
float Chroma_51101_Factor;
uint8_t Valid_measured_value = 0;

volatile uint8_t communication_found = 0; // 0: No COMM, 1: CAN, 2: MODBUS
pthread_mutex_t lock; // Protect Shared variables
pthread_t Cali_thread; // Main Cali thread
pthread_t Device_Comm_thread;

struct input_event ev;
int Keyboard_fd;
uint8_t Cali_Point_Cali_Complete;
uint8_t Cali_Point_Cali_Complete_To_UI;

uint8_t Canbus_ask_name = 0; // 0 : machine type is not asked yet, 2 : receive answer by asking 0x0082 and 0x0083
uint8_t Manual_Cali_step = SEND_KEY_READ_MODE;
uint16_t Cali_status = 0;
uint8_t Cali_type_polling = 0;
uint8_t Cali_read_step = 0;
//Modbus
uint8_t response_data[256] = {0};
int Mod_bytes_read;

/* function -----------------------------------------------*/
void Manual_Calibration(void);
void Parameter_Init(void);
void Keyboard_Control(void);
const char* Find_Keyboard_Device(void);
float Scaling_Factor_Convert(uint8_t HexValue);
void* Cali_routine(void* arg);
void* Canbus_Polling(void* arg);
void* Modbus_Polling(void* arg);
void* Device_Get_Data(void* arg);

void Start_Cali_thread(void);
void Stop_Cali_thread(void);
//UI Get
char* Get_Machine_Name(void);
uint8_t Get_Communication_Type(void);
uint8_t Get_Keyboard_Adjustment(void);
uint16_t Get_Calibration_Status(void);
float Get_PSU_Cali_Point_High_Limit(void);
float Get_PSU_Cali_Point_Low_Limit(void);
float Get_Device_Measured_Value(void);
uint8_t Get_Calibration_Point_Complete(void);
uint8_t Get_Valid_Measured_Value(void);
//UI Set
void Check_UI_Reset_Cali(const uint16_t UI_flag);
void Check_UI_Set_Cali_Point_Flag(const uint16_t UI_flag);
void Check_UI_Set_Cali_Point(const uint16_t UI_Cali_Point);
void Check_UI_Set_Cali_Point_Scaling_Factor(const char *Scaling_Factor);
void Check_UI_Set_Cali_Point_USB_Port(const char *USB_Port);
void Check_UI_Set_Cali_Point_Target(const char *Target);
void Check_Calibration_Point_Complete(const uint8_t Complete_flag);

float Scaling_Factor_Convert(uint8_t HexValue) {
    switch (HexValue)
    {
    case 0x4:
        return 0.001f;
    case 0x5:
        return 0.01f;
    case 0x6:
        return 0.1f;
    case 0x7:
        return 1.0f;
    case 0x8:
        return 10.0f;
    case 0x9:
        return 100.0f;
    default:
        return -1.0f;
    }
}

void *Canbus_Polling(void *arg) {
    memset(&frame, 0, sizeof(struct can_frame));
    Canbus_Init();

    while (!communication_found) {
        switch(Canbus_ask_name)
        {
            case 0: //no model name
            {
                // Send 0x0082
                Canbus_TxProcess_Read(CAN_MODEL_NAME_B0B5);
                // Receive
                Canbus_RxProcess(CAN_MODEL_NAME_B0B5);
                break;
            }

            case 1: //half model name
            {
                // Send 0x0083
                Canbus_TxProcess_Read(CAN_MODEL_NAME_B6B11);
                // Receive
                Canbus_RxProcess(CAN_MODEL_NAME_B6B11);
                break;
            }

            case 2: //full model name
            {
                pthread_mutex_lock(&lock);
                if (!communication_found) {
                    communication_found = CANBUS; // Set COMM -> CANBUS
                }
                pthread_mutex_unlock(&lock);
                break;
            }

            default:
                break;
        }
        usleep(20000); // Delay 20ms
    }
    close(can_socket);
    system("sudo ifconfig can0 down");
    pthread_exit(NULL);
}

void Canbus_Calibration(void){

    memset(&frame, 0, sizeof(struct can_frame));
    Canbus_Init();

    while(1){
        switch (Manual_Cali_step)
        {
        case SEND_KEY_READ_MODE:    //Send KEY repeatedly、Ask calibration mode?
            // Send Key
            Canbus_TxProcess_Write(CAN_CALIBRATION_KEY);
            usleep(20000); //Delay 20ms

            // Read PSU mode 0：Normal Mode  1：Calibrate Mode
            Canbus_TxProcess_Read(CAN_READ_PSU_MODE);

            // Receive
            Canbus_RxProcess(CAN_READ_PSU_MODE);
            break;

        case READ_PSU_SCALING_FACTOR:
            Canbus_TxProcess_Read(CAN_SCALING_FACTOR);
            // Receive
            Canbus_RxProcess(CAN_SCALING_FACTOR);
            break;
        
        case UI_RESET_CALI:
            if(PSU_Reset_Cali_Flag == YES)
            {
                Canbus_TxProcess_Write(CAN_CALI_RESULT);
                usleep(20000); // Delay 20ms
                Manual_Cali_step = READ_UI_SET_POINT;
            }
            else if(PSU_Reset_Cali_Flag == NO)
            {
                Manual_Cali_step = READ_UI_SET_POINT;
            }
            break;
        
        case READ_UI_SET_POINT:
            if (UI_Set_Cali_Point_Flag == YES)
            {
                pthread_create(&Device_Comm_thread, NULL, Device_Get_Data, NULL);
                Manual_Cali_step = READ_CALI_STATUS;
                break;
            }
            break;

        case READ_CALI_STATUS:
            // Read Cali Status
            Canbus_TxProcess_Read(CAN_CALI_STATUS);

            // Receive
            Canbus_RxProcess(CAN_CALI_STATUS);
            break;
        
        case READ_CALI_RESULT:
            // Read Cali Status
            Canbus_TxProcess_Read(CAN_CALI_RESULT);

            // Receive
            Canbus_RxProcess(CAN_CALI_RESULT);
            break;

        default:
            break;
        }
        usleep(20000); // Delay 20ms
    }
    close(can_socket);
}

void *Modbus_Polling(void *arg) {
    int Modbus_ask_name = 0; // 0 : machine type is not asked yet, 2 : receive answer by asking 0x0082 and 0x0083

    Modbus_Init();

    while (!communication_found) {
        switch (Modbus_ask_name)
        {
            case 0:
            {
                Modbus_TxProcess_Read(MOD_MODEL_NAME_B0B5);

                Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                 // check receive data complete
                if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) {
                    Modbus_ask_name = 1;
                    memcpy(machine_type, &response_data[3], 6);
                    machine_type[6] = '\0';
                }
                else {
                    Modbus_ask_name = 0;
                }
                break;
            }
            
            case 1:
            {
                Modbus_TxProcess_Read(MOD_MODEL_NAME_B6B11);

                Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                 // check receive data complete
                if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) { // CRC 2 bytes
                    Modbus_ask_name = 2;
                    memcpy(&machine_type[6], &response_data[3], 6);
                    machine_type[12] = '\0';
                }else {
                    Modbus_ask_name = 1;
                }
                break;
            }

            case 2:
            {
                pthread_mutex_lock(&lock);
                if (!communication_found) {
                    communication_found = MODBUS; // Set COMM -> MODBUS
                }
                pthread_mutex_unlock(&lock);
                break;
            }

            default:
                break;
        }
        usleep(20000);
    }

    serialClose(serial_fd);
    pthread_exit(NULL);
}

void Modbus_Calibration(void){

    Modbus_Init();

    while (1)
    {
        switch (Manual_Cali_step)
        {
        case SEND_KEY_READ_MODE:    //Send KEY repeatedly、Ask calibration mode?
            // Send Key
            Modbus_TxProcess_Write(MOD_CALIBRATION_KEY);
            usleep(20000); //Delay 20ms

            // Read PSU mode 0：Normal Mode  1：Calibrate Mode
            Modbus_TxProcess_Read(MOD_READ_PSU_MODE);
            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
            if ((Mod_bytes_read > 0) && (response_data[1] == 0x03) && (response_data[4] == 0x01)) {
                Manual_Cali_step = READ_PSU_SCALING_FACTOR;
            }
            else {
               Manual_Cali_step = SEND_KEY_READ_MODE;
            }
            break;

        case READ_PSU_SCALING_FACTOR:
            Modbus_TxProcess_Read(MOD_SCALING_FACTOR);
            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
            if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) {
                PSU_ACV_Factor_Comm = (response_data[4] & 0xF);
                PSU_ACI_Factor_Comm = (response_data[6] & 0xF);
                PSU_DCV_Factor_Comm = (response_data[3] & 0xF);
                PSU_DCI_Factor_Comm = ((response_data[3]>>4) & 0xF);
                PSU_ACV_Factor = Scaling_Factor_Convert(PSU_ACV_Factor_Comm);
                PSU_ACI_Factor = Scaling_Factor_Convert(PSU_ACI_Factor_Comm);
                PSU_DCV_Factor = Scaling_Factor_Convert(PSU_DCV_Factor_Comm);
                PSU_DCI_Factor = Scaling_Factor_Convert(PSU_DCI_Factor_Comm);
                Manual_Cali_step = UI_RESET_CALI;
            }
            else {
               Manual_Cali_step = READ_PSU_SCALING_FACTOR;
            }
            break;
        
        case UI_RESET_CALI:
            if(PSU_Reset_Cali_Flag == YES)
            {
                Modbus_TxProcess_Write(MOD_CALI_RESULT);
                usleep(20000); // Delay 20ms
                Manual_Cali_step = READ_UI_SET_POINT;
            }
            else if(PSU_Reset_Cali_Flag == NO)
            {
                Manual_Cali_step = READ_UI_SET_POINT;
            }
            break;

        case READ_UI_SET_POINT:
            if (UI_Set_Cali_Point_Flag == YES)
            {
                pthread_create(&Device_Comm_thread, NULL, Device_Get_Data, NULL);
                Manual_Cali_step = READ_CALI_STATUS;
                break;
            }
            break;
        
        case READ_CALI_STATUS:
            Modbus_TxProcess_Read(MOD_CALI_STATUS);
            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
            // check receive data complete
            if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) {
                Cali_status = (response_data[3]<<8) | response_data[4];
                switch (Cali_status)
                {
                case CALI_DEVICE_SETTING:   //0x0000
                    Manual_Calibration();
                    break;
                
                case CALI_DATA_LOG:         //0x0001
                    Manual_Cali_step = READ_CALI_STATUS;
                    break;
                
                case CALI_POINT_NOMATCH:    //0x0002
                    Cali_Result = CALI_POINT_FAIL;
                    break;
                
                case CALI_POINT_SETTING:    //0x0003
                    Modbus_TxProcess_Write(MOD_CALIBRATION_POINT);
                    usleep(20000); // Delay 20ms
                    break;
                
                case CALI_FINISH:           //0xFFFF
                    Manual_Cali_step = READ_CALI_RESULT;
                    break;
                
                default:
                    break;
                }
            }
            else{
                Manual_Cali_step = READ_CALI_STATUS;
            }
            break;

        case READ_CALI_RESULT:
            // Read Cali Status
            Modbus_TxProcess_Read(MOD_CALI_RESULT);
            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
            // check receive data complete
            if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) {
                if(response_data[4] == 0x01)
                {
                    Cali_Result = FINISH;
                }
                else if(response_data[4] == 0x02)
                {
                    Cali_Result = NOT_COMPLETE;
                }
            }else{
                Manual_Cali_step = READ_CALI_RESULT;
            }
            break;

        default:
            break;
        }
        usleep(20000); // Delay 20ms
    }
    serialClose(serial_fd);
}

void Manual_Calibration(void){
    int polling_cnt = 0;
    uint8_t receive_cnt = 0;
    SVR_value = 2048;
    Cali_Point_Cali_Complete = 0;
    // Set keyboard device path
    const char *device = Find_Keyboard_Device();

    if (device == NULL) {
        Cali_Result = PERIPHERAL_FAIL; //device setting fail
        return;
    }

    Keyboard_fd = open(device, O_RDONLY | O_NONBLOCK); //O_RDONLY:only read 
    
    if (Keyboard_fd == -1) {
        Cali_Result = PERIPHERAL_FAIL; //Unable to open input device
    }

    // Set terminal properties to disable input echoing
    // Monitor keyboard events
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);           // Get current terminal settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);         // Disable line buffering and echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // Set new terminal settings

    while (1) {
        polling_cnt++;
        Keyboard_Control();

        if (polling_cnt >= 200) {
            polling_cnt = 0;
            switch (Cali_read_step)
            {
            case 0:     //read SVR polling
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_SVR_POLLING);
                    // Receive
                    Canbus_RxProcess(CAN_SVR_POLLING);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_SVR_POLLING);
                    // Reveice
                    Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                    // check receive data complete
                    if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) { // CRC 2 bytes
                        Cali_type_polling = response_data[4];
                        Cali_read_step++;
                    }
                }
                break;

            case 1:     //read High limit
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_READ_HIGH_LIMIT);
                    // Receive
                    Canbus_RxProcess(CAN_READ_HIGH_LIMIT);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_READ_HIGH_LIMIT);
                    // Reveice
                    Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                    // check receive data complete
                    if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) { // CRC 2 bytes
                        PSU_High_Limit_Comm = ((response_data[3] << 8) | response_data[4]);
                        if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                        {
                            PSU_High_Limit = PSU_High_Limit_Comm * PSU_ACV_Factor;
                        } 
                        else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                        {
                            PSU_High_Limit = PSU_High_Limit_Comm * PSU_ACI_Factor;
                        } 
                        else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                        {
                            PSU_High_Limit = PSU_High_Limit_Comm * PSU_DCV_Factor;
                        } 
                        else if (strcmp(UI_Scaling_Factor, "DCI") == 0) 
                        {
                            PSU_High_Limit = PSU_High_Limit_Comm * PSU_DCI_Factor;
                        } 
                        else if ((strcmp(UI_Scaling_Factor, "mA") == 0) || (strcmp(UI_Scaling_Factor, "mV") == 0)) 
                        {
                            PSU_High_Limit = PSU_High_Limit_Comm * 0.001f;
                        } 
                        else 
                        {
                            PSU_High_Limit = PSU_High_Limit_Comm;
                        }
                        Cali_read_step++;
                    }
                }
                break;

            case 2:     //read Low limit
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_READ_LOW_LIMIT);
                    // Receive
                    Canbus_RxProcess(CAN_READ_LOW_LIMIT);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_READ_LOW_LIMIT);
                    // Reveice
                    Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                    // check receive data complete
                    if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) { // CRC 2 bytes
                        PSU_Low_Limit_Comm = ((response_data[3] << 8) | response_data[4]);
                        if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                        {
                            PSU_Low_Limit = PSU_Low_Limit_Comm * PSU_ACV_Factor;
                        } 
                        else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                        {
                            PSU_Low_Limit = PSU_Low_Limit_Comm * PSU_ACI_Factor;
                        } 
                        else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                        {
                            PSU_Low_Limit = PSU_Low_Limit_Comm * PSU_DCV_Factor;
                        } 
                        else if (strcmp(UI_Scaling_Factor, "DCI") == 0) 
                        {
                            PSU_Low_Limit = PSU_Low_Limit_Comm * PSU_DCI_Factor;
                        }
                        else if ((strcmp(UI_Scaling_Factor, "mA") == 0) || (strcmp(UI_Scaling_Factor, "mV") == 0)) 
                        {
                            PSU_Low_Limit = PSU_Low_Limit_Comm * 0.001f;
                        } 
                        else 
                        {
                            PSU_Low_Limit = PSU_Low_Limit_Comm;
                        }
                        Cali_read_step++;
                    }
                }
                break;

            case 3:     //read AC Source set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_AC_SOURCE_SET);
                    // Receive
                    Canbus_RxProcess(CAN_AC_SOURCE_SET);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_AC_SOURCE_SET_B0B5);
                    // Reveice
                    Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                    // check receive data complete
                    if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) { // CRC 2 bytes
                        Cali_read_step++;
                    }
                }
                break;

            case 4:     //read DC Source set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_DC_SOURCE_SET);
                    // Receive
                    Canbus_RxProcess(CAN_DC_SOURCE_SET);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_DC_SOURCE_SET_B0B5);
                    // Reveice
                    Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                    // check receive data complete
                    if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) { // CRC 2 bytes
                        Cali_read_step++;
                    }
                }
                break;

            case 5:     //read DC Load set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_DC_LOAD_SET);
                    // Receive
                    Canbus_RxProcess(CAN_DC_LOAD_SET);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_DC_LOAD_SET_B0B5);
                    // Reveice
                    Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                    // check receive data complete
                    if ((Mod_bytes_read > 0) && (response_data[1] == 0x03)) { // CRC 2 bytes
                        Cali_read_step++;
                    }
                }
                break;

            case 6:     //polling SVR
                if (Cali_type_polling == 1) {    //Need polling SVR value
                    if (communication_found == CANBUS) {
                        //Polling SVR value
                        Canbus_TxProcess_Write(CAN_WRITE_SVR);
                    } else if (communication_found == MODBUS) {
                        //Polling SVR value
                        Modbus_TxProcess_Write(MOD_WRITE_SVR);
                    }
                }
                break;
            default:
                break;
            }

            if(Cali_Point_Cali_Complete == 1)
            {
                if (communication_found == CANBUS) {
                    Canbus_TxProcess_Write(CAN_CALI_STATUS);
                } else if (communication_found == MODBUS) {
                    Modbus_TxProcess_Write(MOD_CALI_STATUS);
                }
                Cali_Point_Cali_Complete_To_UI = 1;
                Manual_Cali_step = READ_UI_SET_POINT;
                PSU_High_Limit = 0.0f;
                PSU_Low_Limit = 0.0f;
                Cali_read_step = 0;
                pthread_cancel(Device_Comm_thread);
                delay(1000);
                break;
            }

            if((Device_measured_value >= PSU_Low_Limit) && (Device_measured_value <= PSU_High_Limit))
            {
                Valid_measured_value = 1;       //UI value->Green
            }
            else
            {
                Valid_measured_value = 0;       //UI value->Red
            }
        }
         
        usleep(100); 
    }
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    close(Keyboard_fd);
}

const char* Find_Keyboard_Device(void)
{
    static char device_path[256]= {0};
    char temp_path[256] = {0};
    struct dirent *entry;
    DIR *dp = opendir("/dev/input");
    int fd;
    struct input_id device_info;
    char name[256] = {0};
    int event_num = 0;
    int min_event_num = INT_MAX;

    if (dp == NULL) {
        return NULL;    //DIR fail
    }

    while ((entry = readdir(dp)) != NULL)
    {
        if (strncmp(entry->d_name, "event", 5) == 0)
        {
            event_num = atoi(entry->d_name + 5); //Get eventX

            snprintf(temp_path, sizeof(temp_path), "/dev/input/%s", entry->d_name);
            fd = open(temp_path, O_RDONLY | O_NONBLOCK);
            if (fd != -1) {
                if (ioctl(fd, EVIOCGID, &device_info) == 0) { //EVIOCGID：Device ID、Product ID、Version
                    memset(name, 0, sizeof(name));
                    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
                    if (strstr(name, "HID 1189:8890")) {    //check name
                        if (event_num < min_event_num)
                        {
                            min_event_num = event_num;
                            snprintf(device_path, sizeof(device_path), "%s", temp_path);
                        }
                    }
                }
                close(fd);
            }
        }
    }
    closedir(dp);
    if (min_event_num == INT_MAX) {
        return NULL;    //No match device
    }
    return strdup(device_path);
}

void Keyboard_Control(void)
{
    // Read events
    if (read(Keyboard_fd, &ev, sizeof(struct input_event)) > 0){
        // Only handle key press events
        if (ev.type == EV_KEY) {
            switch (ev.code) {
                case KEY_SPACE:
                // update adjustment(1:Coarse 0:Fine)
                if (ev.value == 1) {
                    space_pressed = !space_pressed; //1:Coarse 0:Fine
                }
                break;
                case KEY_RIGHT:
                if (ev.value == 1) { // press right
                    if (space_pressed == 1) {
                        // Coarse
                        SVR_value += 50;
                    } else {
                        // Fine
                        SVR_value += 1;
                    }
                }
                break;

                case KEY_LEFT:
                if (ev.value == 1) { // press left
                    if (space_pressed == 1) {
                        // Coarse
                        SVR_value -= 50;
                    } else {
                        // Fine
                        SVR_value -= 1;
                    }
                }
                break;

                case KEY_ENTER:
                if (ev.value == 1) {
                    Cali_Point_Cali_Complete = 1;
                    /*
                    if((Device_measured_value >= PSU_Low_Limit) && (Device_measured_value <= PSU_High_Limit))
                    {
                        Cali_Point_Cali_Complete = 1;
                    }
                    */
                    break;
                }
            }
            if(SVR_value < 0)
            {
                SVR_value = 0;
            }
            else if(SVR_value > 4095)
            {
                SVR_value = 4095;
            }
        }
    }
}

void* Cali_routine(void* arg) {
    Parameter_Init();
    pthread_t canbus_thread, modbus_thread;

    // Init Mutex
    pthread_mutex_init(&lock, NULL);

    // Create CANBUS 、 RS485 polling thread
    pthread_create(&canbus_thread, NULL, Canbus_Polling, NULL);
    pthread_create(&modbus_thread, NULL, Modbus_Polling, NULL);

    // Wait thread end
    pthread_join(canbus_thread, NULL);
    pthread_join(modbus_thread, NULL);

    if (communication_found == CANBUS) {
        printf("Using CANBUS for communication\n");
    } else if (communication_found == MODBUS) {
        printf("Using MODBUS for communication\n");
    } else {
        printf("No communication detected\n");
    }

    pthread_mutex_destroy(&lock);


    if (communication_found == CANBUS) {
        Canbus_Calibration();
    } else if (communication_found == MODBUS) {
        Modbus_Calibration();
    }

    return NULL;
}

void* Device_Get_Data(void* arg) {
    char *token;
    Chroma_51101_Channel = -1;

    while(1){
        if (strstr(UI_USB_Port, "usbtmc") != NULL)
        {
            if (strncmp(UI_Target, "V,RMS", 5) == 0)
            {
                SCPI_Read_Process(UI_USB_Port, SCPI_VOLT_RMS);
            }
            else if (strncmp(UI_Target, "I,RMS", 5) == 0)
            {
                SCPI_Read_Process(UI_USB_Port, SCPI_CURR_RMS);
            }
            else if (strncmp(UI_Target, "V,DC", 4) == 0)
            {
                SCPI_Read_Process(UI_USB_Port, SCPI_VOLT_DC);
            }
            else if (strncmp(UI_Target, "I,DC", 4) == 0)
            {
                SCPI_Read_Process(UI_USB_Port, SCPI_CURR_DC);
            }
        }
        else if (strstr(UI_USB_Port, "ttyUSB") != NULL)
        {
            if(Chroma_51101_Channel < 0)
            {
                token = strtok(UI_Target, ",");
                if (token != NULL)
                {
                    Chroma_51101_Channel = atoi(token);
                }
                token = strtok(NULL, ",");
                if (token != NULL) {
                    strncpy(Chroma_51101_Sensor, token, sizeof(Chroma_51101_Sensor) - 1);
                    Chroma_51101_Sensor[sizeof(Chroma_51101_Sensor) - 1] = '\0';
                
                    if (strncmp(Chroma_51101_Sensor, "VA480", 5) == 0)
                    {
                        Chroma_51101_Write_Process(UI_USB_Port, SENSOR_TYPE_SETTING, Chroma_51101_Channel, VA_480, 2);
                    }
                    else if (strncmp(Chroma_51101_Sensor, "VA10", 4) == 0)
                    {
                        Chroma_51101_Write_Process(UI_USB_Port, SENSOR_TYPE_SETTING, Chroma_51101_Channel, VA_10, 2);
                    }
                }
                token = strtok(NULL, ",");
                if (token != NULL) {
                    Chroma_51101_Factor = atof(token);
                }
            }
            else{
                Chroma_51101_Read_Process(UI_USB_Port, GET_SENSOR_DATA, Chroma_51101_Channel, 1);
            }
        }
        usleep(20000);
    }
}

void Start_Cali_thread(void) {
    // Start Communication Thread
    pthread_create(&Cali_thread, NULL, Cali_routine, NULL);
}

void Stop_Cali_thread(void) {
    // Stop Communication Thread
    pthread_cancel(Device_Comm_thread);
    pthread_join(Device_Comm_thread, NULL);
    pthread_cancel(Cali_thread);
    pthread_join(Cali_thread, NULL);
    SCPI_Device_Comm_Control_Close();
}

char* Get_Machine_Name(void) {
    return machine_type;
}

uint8_t Get_Communication_Type(void) {
    return communication_found;
}

uint8_t Get_Keyboard_Adjustment(void) {
    return space_pressed;
}

uint16_t Get_Calibration_Status(void) {
    return Cali_Result;
}

float Get_PSU_Cali_Point_High_Limit(void) {
    return PSU_High_Limit;
}

float Get_PSU_Cali_Point_Low_Limit(void) {
    return PSU_Low_Limit;
}

float Get_Device_Measured_Value(void) {
    return Device_measured_value;
}

uint8_t Get_Calibration_Point_Complete(void) {
    return Cali_Point_Cali_Complete_To_UI;
}

uint8_t Get_Valid_Measured_Value(void) {
    return Valid_measured_value;
}

void Check_UI_Reset_Cali(const uint16_t UI_flag) {
    PSU_Reset_Cali_Flag = UI_flag;
}

void Check_UI_Set_Cali_Point_Flag(const uint16_t UI_flag) {
    UI_Set_Cali_Point_Flag = UI_flag;
}

void Check_UI_Set_Cali_Point(const uint16_t UI_Cali_Point) {
    UI_Calibration_Point = UI_Cali_Point;
}

void Check_UI_Set_Cali_Point_Scaling_Factor(const char *Scaling_Factor) {
    strncpy(UI_Scaling_Factor, Scaling_Factor, MAX_STRING_LENGTH - 1);
    UI_Scaling_Factor[MAX_STRING_LENGTH - 1] = '\0';
}
void Check_UI_Set_Cali_Point_USB_Port(const char *USB_Port) {
    strncpy(UI_USB_Port, USB_Port, MAX_STRING_LENGTH - 1);
    UI_USB_Port[MAX_STRING_LENGTH - 1] = '\0';
}
void Check_UI_Set_Cali_Point_Target(const char *Target) {
    strncpy(UI_Target, Target, MAX_STRING_LENGTH - 1);
    UI_Target[MAX_STRING_LENGTH - 1] = '\0';
}

void Check_Calibration_Point_Complete(const uint8_t Complete_flag) {
    Cali_Point_Cali_Complete_To_UI = Complete_flag;
}

void Parameter_Init(void) {
    memset(machine_type, ' ', sizeof(char) * 12);
    machine_type[12] = '\0';
    Manual_Cali_step = SEND_KEY_READ_MODE;
    communication_found = 0;
    space_pressed = 0;
    
    Canbus_ask_name = 0;
    Cali_Result = 0;
    Cali_status = 0;
    Cali_type_polling = 0;
    Cali_read_step = 0;

    UI_Calibration_Point = 0;
    Valid_measured_value = 0;
}


//debug

//int main() {
//    printf("掃描連接儀器\n");
//    scan_usb_devices();
//    printf("Start\n");
//    Start_Cali_thread();
//    pthread_join(Cali_thread, NULL);
//    return 0;
//}


