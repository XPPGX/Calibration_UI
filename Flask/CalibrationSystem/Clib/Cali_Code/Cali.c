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

volatile uint8_t communication_found = 0; // 0: No COMM, 1: CAN, 2: MODBUS
pthread_mutex_t lock; // Protect Shared variables
pthread_t Cali_thread; // Main Cali thread
pthread_t Device_Comm_thread;

struct input_event ev;
int Keyboard_fd;
uint8_t End_polling;
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
//UI Set
void Check_UI_Reset_Cali(const uint16_t UI_flag);
void Check_UI_Set_Cali_Point_Flag(const uint16_t UI_flag);
void Check_UI_Set_Cali_Point(const uint16_t UI_Cali_Point);
void Check_UI_Set_Cali_Point_Scaling_Factor(const char *Scaling_Factor);
void Check_UI_Set_Cali_Point_USB_Port(const char *USB_Port);
void Check_UI_Set_Cali_Point_Target(const char *Target);

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
    int Canbus_ask_name = 0; // 0 : machine type is not asked yet, 2 : receive answer by asking 0x0082 and 0x0083
    int Can_read_bytes = 0;
    memset(&frame, 0, sizeof(struct can_frame));
    Canbus_Init();

    while (!communication_found) {
        switch(Canbus_ask_name)
        {
            case 0: //no model name
            {
                // Send 0x0082
                Canbus_TxProcess_Read(CAN_MODEL_NAME_B0B5);
                // Try to read
                Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                if (Can_read_bytes > 0) {
                    if((frame.data[0] == (CAN_MODEL_NAME_B0B5 & 0xFF)) && (frame.data[1] == ((CAN_MODEL_NAME_B0B5>>8) & 0xFF)))
                    {
                        Canbus_ask_name = 1;
                        memcpy(machine_type, &frame.data[2], 6);
                        machine_type[6] = '\0';
                    }
                    else
                    {
                        Canbus_ask_name = 0;
                    }
                }
                else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    Cali_Result = FAIL; //CAN Read error
                }
                break;
            }

            case 1: //half model name
            {
                // Send 0x0083
                Canbus_TxProcess_Read(CAN_MODEL_NAME_B6B11);
                Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                if(Can_read_bytes > 0){
                    if((frame.data[0] == (CAN_MODEL_NAME_B6B11 & 0xFF)) && (frame.data[1] == ((CAN_MODEL_NAME_B6B11>>8) & 0xFF)))
                    {
                        Canbus_ask_name = 2;
                        memcpy(&machine_type[6], &frame.data[2], 6);
                        machine_type[12] = '\0';
                    }
                    else
                    {
                        Canbus_ask_name = 1;
                    }
                }
                else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    Cali_Result = FAIL; //CAN Read error
                }
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
    uint8_t Manual_Cali_step = SEND_KEY_READ_MODE;
    int Can_read_bytes = 0;
    uint16_t Cali_status = 0;

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

            // Receive(Cali_mode)
            Can_read_bytes = read(can_socket, &frame, sizeof(frame));
            if (Can_read_bytes > 0) {
                if((frame.data[0] == (CAN_READ_PSU_MODE & 0xFF)) && (frame.data[1] == ((CAN_READ_PSU_MODE>>8) & 0xFF)))
                {
                    if(frame.data[2] == 0x01){
                        Canbus_TxProcess_Read(CAN_SCALING_FACTOR);
                        // Receive(Cali_mode)
                        Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                        if (Can_read_bytes > 0) 
                        {
                            if((frame.data[0] == (CAN_SCALING_FACTOR & 0xFF)) && (frame.data[1] == ((CAN_SCALING_FACTOR>>8) & 0xFF)))
                            {
                                PSU_ACV_Factor_Comm = (frame.data[3] & 0xF);
                                PSU_ACI_Factor_Comm = (frame.data[5] & 0xF);
                                PSU_DCV_Factor_Comm = (frame.data[2] & 0xF);
                                PSU_DCI_Factor_Comm = ((frame.data[2]>>4) & 0xF);
                                PSU_ACV_Factor = Scaling_Factor_Convert(PSU_ACV_Factor_Comm);
                                PSU_ACI_Factor = Scaling_Factor_Convert(PSU_ACI_Factor_Comm);
                                PSU_DCV_Factor = Scaling_Factor_Convert(PSU_DCV_Factor_Comm);
                                PSU_DCI_Factor = Scaling_Factor_Convert(PSU_DCI_Factor_Comm);
                                Manual_Cali_step = UI_RESET_CALI;
                            }
                        }
                    }
                }
            } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                Cali_Result = FAIL; //CAN Read error
            }
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

            // receive data(Cali_Status)
            Can_read_bytes = read(can_socket, &frame, sizeof(frame));
            if (Can_read_bytes > 0) {
                if((frame.data[0] == 0x01) && (frame.data[1] == 0xD0))
                {
                    Cali_status = (frame.data[3]<<8) | frame.data[2];
                    switch (Cali_status)
                    {
                    case CALI_DEVICE_SETTING:   //0x0000
                        Manual_Calibration();
                        break;
                    
                    case CALI_DATA_LOG:         //0x0001
                        Manual_Cali_step = READ_CALI_STATUS;
                        break;
                    
                    case CALI_POINT_NOMATCH:    //0x0002
                        Cali_Result = FAIL;
                        break;
                    
                    case CALI_POINT_SETTING:    //0x0003
                        Canbus_TxProcess_Write(CAN_CALIBRATION_POINT);
                        usleep(20000); // Delay 20ms
                        break;
                    
                    case CALI_FINISH:           //0xFFFF
                        Cali_Result = FINISH;
                        break;
                    
                    default:
                        break;
                    }
                }
            } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                Cali_Result = FAIL; //CAN read fail
            }
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
                if (Mod_bytes_read > 0) {
                    Modbus_ask_name = 1;
                    memcpy(machine_type, &response_data[3], 6);
                    machine_type[6] = '\0';
                } else if (Mod_bytes_read == -2) {
                    //Cali_Result = FAIL;
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
                if (Mod_bytes_read > 0) { // CRC 2 bytes
                    Modbus_ask_name = 2;
                    memcpy(&machine_type[6], &response_data[3], 6);
                    machine_type[12] = '\0';
                } else if (Mod_bytes_read == -2) {
                    //Cali_Result = FAIL;
                } else {
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
    uint8_t Manual_Cali_step = 1;

    Modbus_Init();

    while (1)
    {
        if (Manual_Cali_step == 1) //Send KEY repeatedly、Ask calibration mode?
        {
            // Send Key
            Modbus_TxProcess_Write(MOD_CALIBRATION_KEY);
            usleep(20000); //Delay 20ms

            // Read PSU mode 0：Normal Mode  1：Calibrate Mode
            Modbus_TxProcess_Read(MOD_READ_PSU_MODE);
            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
            // check receive data complete
            if ((Mod_bytes_read > 0) && (response_data[4] == 0x01)) {
                Manual_Cali_step = 2;
            } else if (Mod_bytes_read == -2) {
                //Cali_Result = FAIL;
            }
            else {
               Manual_Cali_step = 1;
            }
        }
        else if (Manual_Cali_step == 2)
        {
            Modbus_TxProcess_Read(MOD_CALI_STATUS);
            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
            // check receive data complete
            if (Mod_bytes_read > 0) {
                if(response_data[4] == 0x00){
                    Manual_Calibration();
                }
                else if(response_data[4] == 0x01){
                    Manual_Cali_step = 2;
                }
                else if(response_data[4] == 0xFF){
                    Cali_Result = FINISH;
                    break;
                }
            } else if (Mod_bytes_read == -2) {
                //Cali_Result = FAIL;
            }
        }
        usleep(20000); // Delay 20ms
    }
    
}

void Manual_Calibration(void){
    int polling_cnt = 0;
    uint8_t Cali_type_polling = 0;
    uint8_t receive_cnt = 0;
    int Can_read_bytes = 0;
    SVR_value = 2048;
    uint8_t read_step = 0;
    // Set keyboard device path
    const char *device = Find_Keyboard_Device();

    if (device == NULL) {
        Cali_Result = FAIL; //device setting fail
        return;
    }

    Keyboard_fd = open(device, O_RDONLY | O_NONBLOCK); //O_RDONLY:only read 
    
    if (Keyboard_fd == -1) {
        Cali_Result = FAIL; //Unable to open input device
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
            switch (read_step)
            {
            case 0:     //read SVR polling
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_SVR_POLLING);
                    // Reveice
                    Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                    if (Can_read_bytes > 0) {
                        if((frame.data[0] == (CAN_SVR_POLLING & 0xFF)) && (frame.data[1] == ((CAN_SVR_POLLING>>8) & 0xFF))){
                            Cali_type_polling = frame.data[2];
                            read_step++;
                        }
                    } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        Cali_Result = FAIL; //CAN Read error
                    }
                }
                /*
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_CALIBRATION_TYPE);
                    // Reveice(Cali_type)
                    Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                    // check receive data complete
                    if (Mod_bytes_read > 0) { // CRC 2 bytes
                        PSU_Calibration_Type = response_data[4];
                        receive_cnt = 1;
                    } else if (Mod_bytes_read == -2) {
                        //Cali_Result = FAIL;
                    }
                }
                */
                break;
            case 1:     //read High limit
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_READ_HIGH_LIMIT);
                    // Reveice
                    Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                    if (Can_read_bytes > 0) {
                        if((frame.data[0] == (CAN_READ_HIGH_LIMIT & 0xFF)) && (frame.data[1] == ((CAN_READ_HIGH_LIMIT>>8) & 0xFF))){
                            if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                            {
                                PSU_High_Limit = ((frame.data[3] << 8) | frame.data[2]) * PSU_ACV_Factor;
                            } 
                            else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                            {
                                PSU_High_Limit = ((frame.data[3] << 8) | frame.data[2]) * PSU_ACI_Factor;
                            } 
                            else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                            {
                                PSU_High_Limit = ((frame.data[3] << 8) | frame.data[2]) * PSU_DCV_Factor;
                            } 
                            else if (strcmp(UI_Scaling_Factor, "DCI") == 0) 
                            {
                                PSU_High_Limit = ((frame.data[3] << 8) | frame.data[2]) * PSU_DCI_Factor;
                            } 
                            else 
                            {
                                PSU_High_Limit = ((frame.data[3] << 8) | frame.data[2]);
                            }
                            read_step++;
                        }
                    } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        Cali_Result = FAIL; //CAN Read error
                    }
                }
                break;
            case 2:     //read Low limit
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_READ_LOW_LIMIT);
                    // Reveice
                    Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                    if (Can_read_bytes > 0) {
                        if((frame.data[0] == (CAN_READ_LOW_LIMIT & 0xFF)) && (frame.data[1] == ((CAN_READ_LOW_LIMIT>>8) & 0xFF))){
                            if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                            {
                                PSU_Low_Limit = ((frame.data[3] << 8) | frame.data[2]) * PSU_ACV_Factor;
                            } 
                            else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                            {
                                PSU_Low_Limit = ((frame.data[3] << 8) | frame.data[2]) * PSU_ACI_Factor;
                            } 
                            else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                            {
                                PSU_Low_Limit = ((frame.data[3] << 8) | frame.data[2]) * PSU_DCV_Factor;
                            } 
                            else if (strcmp(UI_Scaling_Factor, "DCI") == 0) 
                            {
                                PSU_Low_Limit = ((frame.data[3] << 8) | frame.data[2]) * PSU_DCI_Factor;
                            } 
                            else 
                            {
                                PSU_Low_Limit = ((frame.data[3] << 8) | frame.data[2]);
                            }
                            read_step++;
                        }
                    } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        Cali_Result = FAIL; //CAN Read error
                    }
                }
                break;
            case 3:     //read AC Source set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_AC_SOURCE_SET);
                    // Reveice
                    Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                    if (Can_read_bytes > 0) {
                        if((frame.data[0] == (CAN_AC_SOURCE_SET & 0xFF)) && (frame.data[1] == ((CAN_AC_SOURCE_SET>>8) & 0xFF))){
                            
                            read_step++;
                        }
                    } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        Cali_Result = FAIL; //CAN Read error
                    }
                }
                break;
            case 4:     //read DC Source set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_DC_SOURCE_SET);
                    // Reveice
                    Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                    if (Can_read_bytes > 0) {
                        if((frame.data[0] == (CAN_DC_SOURCE_SET & 0xFF)) && (frame.data[1] == ((CAN_DC_SOURCE_SET>>8) & 0xFF))){
                            
                            read_step++;
                        }
                    } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        Cali_Result = FAIL; //CAN Read error
                    }
                }
                break;
            case 5:     //read DC Load set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_DC_LOAD_SET);
                    // Reveice
                    Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                    if (Can_read_bytes > 0) {
                        if((frame.data[0] == (CAN_DC_LOAD_SET & 0xFF)) && (frame.data[1] == ((CAN_DC_LOAD_SET>>8) & 0xFF))){
                            
                            read_step++;
                        }
                    } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        Cali_Result = FAIL; //CAN Read error
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

            if(End_polling == 1)
            {
                if (communication_found == CANBUS) {
                    Canbus_TxProcess_Write(CAN_CALI_STATUS);
                } else if (communication_found == MODBUS) {
                    Modbus_TxProcess_Write(MOD_CALI_STATUS);
                }
                delay(1000);
                break;
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
    return device_path;
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
                    End_polling = 1;
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
    if (strncmp(UI_USB_Port, "usbtmc", 6) == 0)
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
    else if (strncmp(UI_USB_Port, "ttyUSB", 6) == 0)
    {
        Chroma_51101_Read_Process(UI_USB_Port, GET_SENSOR_DATA);
    }
}

void Start_Cali_thread(void) {
    // Start Communication Thread
    pthread_create(&Cali_thread, NULL, Cali_routine, NULL);
}

void Stop_Cali_thread(void) {
    // Stop Communication Thread
    pthread_cancel(Cali_thread);
    pthread_join(Cali_thread, NULL);
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

void Parameter_Init(void) {
    memset(machine_type, ' ', sizeof(char) * 12);
    machine_type[12] = '\0';
    communication_found = 0;
    space_pressed = 0;
    Cali_Result = 0;
    PSU_Reset_Cali_Flag = 0;
}

