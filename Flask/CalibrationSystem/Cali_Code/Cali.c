/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Canbus.h"
#include "Modbus.h"
#include "Auto_Search_Device.h"
#include "Chroma_meter_51101_8.h"

/* Parameter -----------------------------------------------*/
uint8_t space_pressed = 0; //1:Coarse 0:Fine
char machine_type[13]; //Receive the machine type returned by the command 0x0082 and 0x0083
int SVR_value = 0;
uint16_t Cali_Result;
uint16_t PSU_Reset_Cali_Flag = 0;
uint16_t UI_Set_Cali_Point_Flag = 0;
uint16_t UI_Calibration_Point = 0;
char UI_Scaling_Factor[MAX_STRING_LENGTH] = {0};
char UI_USB_Port[MAX_STRING_LENGTH] = {0};
char UI_Target[MAX_STRING_LENGTH] = {0};
int16_t PSU_High_Limit_Comm = 0;
uint16_t PSU_High_Limit_uComm = 0;
int16_t PSU_Low_Limit_Comm = 0;
uint16_t PSU_Low_Limit_uComm = 0;
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
int Meter_Channel;
char Chroma_51101_Sensor[MAX_STRING_LENGTH];
float Current_Shunt_Factor;
uint8_t Valid_measured_value = 0;
uint8_t wCali_Status = 0;

volatile uint8_t communication_found = 0; // 0: No COMM, 1: CAN, 2: MODBUS
pthread_mutex_t lock; // Protect Shared variables
pthread_t Cali_thread; // Main Cali thread
pthread_t Device_Comm_thread;

struct input_event ev;
int Keyboard_fd;
uint8_t Cali_Point_Cali_Complete;
uint8_t Cali_Point_Cali_Complete_To_UI;

uint8_t Canbus_ask_name = 0; // 0 : machine type is not asked yet, 2 : receive answer by asking 0x0082 and 0x0083
uint8_t Modbus_ask_name = 0; // 0 : machine type is not asked yet, 2 : receive answer by asking 0x0082 and 0x0083
uint8_t Manual_Cali_step = SEND_KEY_READ_MODE;
uint16_t Cali_status = 0;
uint8_t Cali_type_polling = 0;
uint8_t Cali_read_step = 0;
//Modbus
uint8_t response_data[256] = {0};
int Mod_bytes_read;

//Auto Control Calibration
uint16_t DCS_Volt_Set_Value = 0;
uint16_t DCS_Curr_Set_Value = 0;
uint8_t  DCS_Remote_Switch = 0;
cJSON *direction_logic = NULL;
int16_t Cali_Default_Value = 0;

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
int Wait_for_response_poll(int fd, int timeout_ms);

//Auto Control Calibration
void Calibration_Device_Auto_Control(void);
cJSON *find_step_setting(cJSON *device_json, const char *step_cmd);

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
    //memset(&frame, 0, sizeof(struct can_frame));
    //Canbus_Init();

    while (!communication_found) {
        switch(Canbus_ask_name)
        {
            case 0: //no model name
            {
                // Send 0x0082
                Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_MODEL_NAME_B0B5);
                // Receive
                Canbus_RxProcess(CAN_MODEL_NAME_B0B5);
                break;
            }

            case 1: //half model name
            {
                // Send 0x0083
                Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_MODEL_NAME_B6B11);
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
    //close(can_socket);
    //system("sudo ifconfig can0 down");
    pthread_exit(NULL);
}

void Canbus_Calibration(void){

    //memset(&frame, 0, sizeof(struct can_frame));
    //Canbus_Init();

    while(1){
        switch (Manual_Cali_step)
        {
        case SEND_KEY_READ_MODE:    //Send KEY repeatedly、Ask calibration mode?
            // Send Key
            Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_CALIBRATION_KEY);
            usleep(20000); //Delay 20ms

            // Read PSU mode 0：Normal Mode  1：Calibrate Mode
            Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_READ_PSU_MODE);

            // Receive
            Canbus_RxProcess(CAN_READ_PSU_MODE);
            break;

        case READ_PSU_SCALING_FACTOR:
            Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_SCALING_FACTOR);
            // Receive
            Canbus_RxProcess(CAN_SCALING_FACTOR);
            break;
        
        case UI_RESET_CALI:
            if(PSU_Reset_Cali_Flag == YES)
            {
                Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_CALI_RESULT);
                Manual_Cali_step = READ_UI_SET_POINT;
            }
            else if(PSU_Reset_Cali_Flag == NO)
            {
                Manual_Cali_step = READ_UI_SET_POINT;
            }
            wCali_Status = 3;
            Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_CALI_STATUS);
            break;
        
        case READ_UI_SET_POINT:
            if (UI_Set_Cali_Point_Flag == YES)
            {
                pthread_create(&Device_Comm_thread, NULL, Device_Get_Data, NULL);
                UI_Set_Cali_Point_Flag = 0;
                Manual_Cali_step = READ_CALI_STATUS;
                break;
            }
            break;

        case READ_CALI_STATUS:
            // Read Cali Status
            Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_CALI_STATUS);

            // Receive
            Canbus_RxProcess(CAN_CALI_STATUS);
            break;
        
        case READ_CALI_RESULT:
            // Read Cali Status
            Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_CALI_RESULT);

            // Receive
            Canbus_RxProcess(CAN_CALI_RESULT);
            break;

        default:
            break;
        }
        usleep(20000); // Delay 20ms
    }
    //close(can_socket);
}

void *Modbus_Polling(void *arg) {

    //Modbus_Init();

    while (!communication_found) {
        switch (Modbus_ask_name)
        {
            case 0:
            {
                Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_MODEL_NAME_B0B5);

                Modbus_RxProcess(MOD_MODEL_NAME_B0B5);
                break;
            }
            
            case 1:
            {
                Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_MODEL_NAME_B6B11);

                Modbus_RxProcess(MOD_MODEL_NAME_B6B11);
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

    //serialClose(serial_fd);
    pthread_exit(NULL);
}

void Modbus_Calibration(void){

    //Modbus_Init();

    while (1)
    {
        switch (Manual_Cali_step)
        {
        case SEND_KEY_READ_MODE:    //Send KEY repeatedly?sk calibration mode?
            // Send Key
            Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_CALIBRATION_KEY);
            usleep(20000); //Delay 20ms

            // Read PSU mode 0：Normal Mode  1：Calibrate Mode
            Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_READ_PSU_MODE);

            Modbus_RxProcess(MOD_READ_PSU_MODE);
            break;

        case READ_PSU_SCALING_FACTOR:
            Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_SCALING_FACTOR);

            Modbus_RxProcess(MOD_SCALING_FACTOR);
            break;
        
        case UI_RESET_CALI:
            if(PSU_Reset_Cali_Flag == YES)
            {
                Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_CALI_RESULT);
                usleep(20000); // Delay 20ms
                Manual_Cali_step = READ_UI_SET_POINT;
            }
            else if(PSU_Reset_Cali_Flag == NO)
            {
                Manual_Cali_step = READ_UI_SET_POINT;
            }
            wCali_Status = 3;
            Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_CALI_STATUS);
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
            Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_CALI_STATUS);

            Modbus_RxProcess(MOD_CALI_STATUS);
            break;

        case READ_CALI_RESULT:
            // Read Cali Status
            Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_CALI_RESULT);

            Modbus_RxProcess(MOD_CALI_RESULT);
            break;

        default:
            break;
        }
        usleep(20000); // Delay 20ms
    }
    //serialClose(serial_fd);
}

void Manual_Calibration(void){
    int polling_cnt = 0;
    uint8_t receive_cnt = 0;
    SVR_value = 2048;
    Cali_Point_Cali_Complete = 0;
    // Set keyboard device path
    // const char *device = Find_Keyboard_Device();

    // if (device == NULL) {
    //     Cali_Result = PERIPHERAL_FAIL; //device setting fail
    //     return;
    // }

    // Keyboard_fd = open(device, O_RDONLY | O_NONBLOCK); //O_RDONLY:only read 
    
    // if (Keyboard_fd == -1) {
    //     Cali_Result = PERIPHERAL_FAIL; //Unable to open input device
    // }

    // // Set terminal properties to disable input echoing
    // // Monitor keyboard events
    // struct termios oldt, newt;
    // tcgetattr(STDIN_FILENO, &oldt);           // Get current terminal settings
    // newt = oldt;
    // newt.c_lflag &= ~(ICANON | ECHO);         // Disable line buffering and echoing
    // tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // Set new terminal settings

    while (1) {
        polling_cnt++;
        //Keyboard_Control();

        if (polling_cnt >= 200) {
            polling_cnt = 0;
            switch (Cali_read_step)
            {
            case 0:     //read SVR polling
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_SVR_POLLING);
                    // Receive
                    Canbus_RxProcess(CAN_SVR_POLLING);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_SVR_POLLING);
                    // Reveice
                    Modbus_RxProcess(MOD_SVR_POLLING);
                }
                break;

            case 1:     //read High limit
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_READ_HIGH_LIMIT);
                    // Receive
                    Canbus_RxProcess(CAN_READ_HIGH_LIMIT);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_READ_HIGH_LIMIT);
                    // Reveice
                    Modbus_RxProcess(MOD_READ_HIGH_LIMIT);
                }
                break;

            case 2:     //read Low limit
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_READ_LOW_LIMIT);
                    // Receive
                    Canbus_RxProcess(CAN_READ_LOW_LIMIT);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_READ_LOW_LIMIT);
                    // Reveice
                    Modbus_RxProcess(MOD_READ_LOW_LIMIT);
                }
                break;

            case 3:     //read AC Source set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_AC_SOURCE_SET);
                    // Receive
                    Canbus_RxProcess(CAN_AC_SOURCE_SET);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_AC_SOURCE_SET_B0B5);
                    // Reveice
                    Modbus_RxProcess(MOD_AC_SOURCE_SET_B0B5);
                }
                break;

            case 4:     //read DC Source set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_DC_SOURCE_SET);
                    // Receive
                    Canbus_RxProcess(CAN_DC_SOURCE_SET);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_DC_SOURCE_SET_B0B5);
                    // Reveice
                    Modbus_RxProcess(MOD_DC_SOURCE_SET_B0B5);
                }
                break;

            case 5:     //read DC Load set
                if (communication_found == CANBUS) 
                {
                    Canbus_TxProcess_Read(CAN_TX_ID_DUT, CAN_DC_LOAD_SET);
                    // Receive
                    Canbus_RxProcess(CAN_DC_LOAD_SET);
                }
                else if (communication_found == MODBUS)
                {
                    Modbus_TxProcess_Read(MOD_TX_ID_DUT, MOD_DC_LOAD_SET_B0B5);
                    // Reveice
                    Modbus_RxProcess(MOD_DC_LOAD_SET_B0B5);
                }
                break;

            case 6:     // Auto control device
                Calibration_Device_Auto_Control();
                Cali_read_step++;
                break;
            default:
                break;
            }

            if(Cali_Point_Cali_Complete == 1)
            {
                pthread_cancel(Device_Comm_thread);
                if (strstr(UI_USB_Port, "usbtmc") != NULL)
                {
                    SCPI_Write_Process(UI_USB_Port, SCPI_LOCAL_STATE);
                }

                Manual_Cali_step = READ_UI_SET_POINT;
                PSU_High_Limit = 0.0f;
                PSU_Low_Limit = 0.0f;
                Cali_read_step = 0;
                Cali_Point_Cali_Complete_To_UI = 1;

                delay(1000);
                break;
            }

            // if((Device_measured_value >= PSU_Low_Limit) && (Device_measured_value <= PSU_High_Limit))
            // {
            //     Valid_measured_value = 1;       //UI value->Green
            // }
            // else
            // {
            //     Valid_measured_value = 0;       //UI value->Red
            // }
        }
         
        usleep(100); 
    }
    // Restore terminal settings
    //tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    //close(Keyboard_fd);
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
    Meter_Channel = -1;
    uint8_t Chroma_51101_Flag = 0;
    uint8_t GDM_8342_Flag = 0;

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
            if(Meter_Channel < 0)
            {
                token = strtok(UI_Target, ",");
                if (token != NULL)
                {
                    Meter_Channel = atoi(token);
                }
                token = strtok(NULL, ",");
                if (token != NULL) {
                    strncpy(Chroma_51101_Sensor, token, sizeof(Chroma_51101_Sensor) - 1);
                    Chroma_51101_Sensor[sizeof(Chroma_51101_Sensor) - 1] = '\0';
                    if (strcmp(Chroma_51101_Sensor, "VA480") == 0)
                    {
                        Chroma_51101_Write_Process(UI_USB_Port, SENSOR_TYPE_SETTING, Meter_Channel, VA_480, 2);
                        Chroma_51101_Flag = 1;
                    }
                    else if (strcmp(Chroma_51101_Sensor, "VA10") == 0)
                    {
                        Chroma_51101_Write_Process(UI_USB_Port, SENSOR_TYPE_SETTING, Meter_Channel, VA_10, 2);
                        Chroma_51101_Flag = 1;
                    }
                    else if (strcmp(Chroma_51101_Sensor, "V") == 0)
                    {
                        GDM_8342_Flag = 1;
                    }
                }
                token = strtok(NULL, ",");
                if (token != NULL) {
                    Current_Shunt_Factor = atof(token);
                }
            }
            else{
                if(Chroma_51101_Flag == 1)
                {
                    Chroma_51101_Read_Process(UI_USB_Port, GET_SENSOR_DATA, Meter_Channel, 1);
                }
                else if(GDM_8342_Flag == 1)
                {
                    SCPI_Read_Process(UI_USB_Port, SCPI_READ);
                }
            }
        }
        usleep(20000);
    }
}

int Wait_for_response_poll(int fd, int timeout_ms) {
    struct pollfd fds;
    fds.fd = fd;
    fds.events = POLLIN;

    return poll(&fds, 1, timeout_ms);
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
    Modbus_ask_name = 0;
    Cali_Result = 0;
    Cali_status = 0;
    Cali_type_polling = 0;
    Cali_read_step = 0;

    UI_Calibration_Point = 0;
    Valid_measured_value = 0;
}

// Load JSON File
cJSON *load_json_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("無法開啟 JSON 檔案");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    char *json_data = (char *)malloc(filesize + 1);
    fread(json_data, 1, filesize, file);
    json_data[filesize] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(json_data);
    free(json_data);
    return json;
}

cJSON *find_step_setting(cJSON *device_json, const char *step_cmd) {
    cJSON *cali_points = cJSON_GetObjectItem(device_json, "CaliPoints_Info");
    cJSON *point;
    cJSON_ArrayForEach(point, cali_points) {
        cJSON *cmd = cJSON_GetObjectItem(point, "step_cmd");
        if (cmd && cJSON_IsString(cmd) && strcmp(cmd->valuestring, step_cmd) == 0) {
            cJSON *temp = cJSON_GetObjectItem(point, "adjust_direction");
            if (temp && cJSON_IsString(temp)) {
                direction_logic = temp;
            } else {
                direction_logic = cJSON_CreateString("Positive");
            }
            return cJSON_GetObjectItem(point, "Step_Setting");
        }
    }
    return NULL;
}

// 根據 Identifier 來比對 Control_Device，取得 USB_Port
int find_device_info(cJSON *device_config_json, const char *control_device, char *usb_port) {
    if (strcmp(control_device, "Keyboard") == 0) {
        printf("Keyboard detected, special handling required!\n");
        strcpy(usb_port, "N/A");  // 設定為 "N/A" 表示無需 USB 端口
        return 1;
    }

    cJSON *device;
    cJSON_ArrayForEach(device, device_config_json) {
        cJSON *identifier_array = cJSON_GetObjectItem(device, "Identifier");
        cJSON *usb_port_array = cJSON_GetObjectItem(device, "USB_Port");

        if (cJSON_IsArray(identifier_array) && cJSON_IsArray(usb_port_array)) {
            int array_size = cJSON_GetArraySize(identifier_array);
            for (int i = 0; i < array_size; i++) {
                cJSON *identifier = cJSON_GetArrayItem(identifier_array, i);
                if (identifier && cJSON_IsString(identifier) && strcmp(identifier->valuestring, control_device) == 0) {
                    cJSON *usb_port_value = cJSON_GetArrayItem(usb_port_array, i);
                    if (usb_port_value && cJSON_IsString(usb_port_value)) {
                        strcpy(usb_port, usb_port_value->valuestring);
                        return 1;
                    }
                }
            }
        }
    }

    return 0; // 找不到對應設備
}

// Send Step Command
void Send_Step_Command(const char *usb_port, const char *control_device, const char *setting_type, const char *setting_value) {
    char step_command[256] = "";
    float Avg_value = 0.0f;
    float new_error = 0.0f;
    float last_error = 0.0f;
    float error_ratio = 0.0f;
    float step_size = 0.1f;
    float resolution_value = 0.0f;
    float Filter_Device_measured_value = 0.0;
    float Sum_Device_measured_value = 0.0;
    uint8_t filter_count = 0;
    uint8_t OK_cnt = 0;

    if (strncmp(control_device, "DCL", 3) == 0) 
    {
        if (strcmp(setting_type, "I") == 0)
        {
            snprintf(step_command, sizeof(step_command), "%s %s\n", SCPI_CURR_STAT_L1, setting_value);
            SCPI_Write_Process(usb_port, step_command);
        } 
        else if (strcmp(setting_type, "V") == 0)
        {
            snprintf(step_command, sizeof(step_command), "%s %s\n", SCPI_VOLT_STAT_L1, setting_value);
            SCPI_Write_Process(usb_port, step_command);
        } 
        else if (strcmp(setting_type, "Switch") == 0)
        {
            if (strcmp(setting_value, "ON") == 0) 
            {
                SCPI_Write_Process(usb_port, SCPI_LOAD_ON);
            }
            else if (strcmp(setting_value, "OFF") == 0) 
            {
                SCPI_Write_Process(usb_port, SCPI_LOAD_OFF);
            }
        }
        else if (strcmp(setting_type, "Wait") == 0)
        {
            Valid_measured_value = 0;
            filter_count = 0;
            Sum_Device_measured_value = 0.0;
            Filter_Device_measured_value = 0.0;

            sleep(3); //wait device setting response

            while(1)
            {
                filter_count++;
                Sum_Device_measured_value += Device_measured_value;
                if (filter_count >= 10)
                {
                    Filter_Device_measured_value = (Sum_Device_measured_value / filter_count);
                    if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_ACV_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_ACI_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_DCV_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "DCI") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_DCI_Factor)+0.5);
                    }
                    if (communication_found == CANBUS) {
                        //Polling SVR value
                        Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_CALI_DEFAULT_SET);
                    } else if (communication_found == MODBUS) {
                        //Polling SVR value
                        Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_CALI_DEFAULT_SET);
                    }
                    break;
                }
                usleep(100000); // delay 100ms
                if((Device_measured_value >= PSU_Low_Limit) && (Device_measured_value <= PSU_High_Limit))
                {
                    Valid_measured_value = 1;       //UI value->Green
                }
                else
                {
                    Valid_measured_value = 0;       //UI value->Red
                }
            }
        }
    } 
    else if (strncmp(control_device, "ACS", 3) == 0) 
    {
        if (strcmp(setting_type, "V") == 0)
        {
            snprintf(step_command, sizeof(step_command), "%s %s\n", SCPI_ACS_SET_VOLT, setting_value);
            SCPI_Write_Process(usb_port, step_command);
        } 
        else if (strcmp(setting_type, "Switch") == 0)
        {
            if (strcmp(setting_value, "ON") == 0) 
            {
                SCPI_Write_Process(usb_port, SCPI_SOURCE_ON);
            }
            else if (strcmp(setting_value, "OFF") == 0) 
            {
                SCPI_Write_Process(usb_port, SCPI_SOURCE_OFF);
            }
        }
        else if (strcmp(setting_type, "Wait") == 0)
        {
            Valid_measured_value = 0;
            filter_count = 0;
            Sum_Device_measured_value = 0.0;
            Filter_Device_measured_value = 0.0;

            sleep(3); //wait device setting response

            while(1)
            {
                filter_count++;
                Sum_Device_measured_value += Device_measured_value;
                if (filter_count >= 10)
                {
                    Filter_Device_measured_value = (Sum_Device_measured_value / filter_count);
                    if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_ACV_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_ACI_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_DCV_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "DCI") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_DCI_Factor)+0.5);
                    }
                    if (communication_found == CANBUS) {
                        //Polling SVR value
                        Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_CALI_DEFAULT_SET);
                    } else if (communication_found == MODBUS) {
                        //Polling SVR value
                        Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_CALI_DEFAULT_SET);
                    }
                    break;
                }
                usleep(100000); // delay 100ms
                if((Device_measured_value >= PSU_Low_Limit) && (Device_measured_value <= PSU_High_Limit))
                {
                    Valid_measured_value = 1;       //UI value->Green
                }
                else
                {
                    Valid_measured_value = 0;       //UI value->Red
                }
            }
        }
    } 
    else if (strncmp(control_device, "DCS", 3) == 0) 
    {
        if (strcmp(setting_type, "V") == 0)
        {
            DCS_Volt_Set_Value = (uint16_t) strtoul(setting_value, NULL, 10) * 100;
            if (strcmp(usb_port, "CANBUS") == 0)
            {
                Canbus_TxProcess_Write(CAN_TX_ID_DEVICE, CAN_VOUT_SET);
            }
            else if (strcmp(usb_port, "MODBUS") == 0)
            {
                Modbus_TxProcess_Write(MOD_TX_ID_DEVICE, MOD_VOUT_SET);
            }
        }
        else if (strcmp(setting_type, "I") == 0)
        {
            DCS_Curr_Set_Value = (uint16_t) strtoul(setting_value, NULL, 10) * 100;
            if (strcmp(usb_port, "CANBUS") == 0)
            {
                Canbus_TxProcess_Write(CAN_TX_ID_DEVICE, CAN_IOUT_SET);
            }
            else if (strcmp(usb_port, "MODBUS") == 0)
            {
                Modbus_TxProcess_Write(MOD_TX_ID_DEVICE, MOD_IOUT_SET);
            }
        }
        else if (strcmp(setting_type, "Switch") == 0)
        {
            if (strcmp(setting_value, "ON") == 0) 
            {
                DCS_Remote_Switch = 1;
            }
            else if (strcmp(setting_value, "OFF") == 0) 
            {
                DCS_Remote_Switch = 0;
            }

            if (strcmp(usb_port, "CANBUS") == 0)
            {
                Canbus_TxProcess_Write(CAN_TX_ID_DEVICE, CAN_OPERATION);
            }
            else if (strcmp(usb_port, "MODBUS") == 0)
            {
                Modbus_TxProcess_Write(MOD_TX_ID_DEVICE, MOD_OPERATION);
            }
        }
        else if (strcmp(setting_type, "Wait") == 0)
        {
            Valid_measured_value = 0;
            filter_count = 0;
            Sum_Device_measured_value = 0.0;
            Filter_Device_measured_value = 0.0;

            sleep(3); //wait device setting response

            while(1)
            {
                filter_count++;
                Sum_Device_measured_value += Device_measured_value;
                if (filter_count >= 10)
                {
                    Filter_Device_measured_value = (Sum_Device_measured_value / filter_count);
                    if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_ACV_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_ACI_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_DCV_Factor)+0.5);
                    } 
                    else if (strcmp(UI_Scaling_Factor, "DCI") == 0) 
                    {
                        Cali_Default_Value = (int16_t)((Filter_Device_measured_value / PSU_DCI_Factor)+0.5);
                    }
                    if (communication_found == CANBUS) {
                        //Polling SVR value
                        Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_CALI_DEFAULT_SET);
                    } else if (communication_found == MODBUS) {
                        //Polling SVR value
                        Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_CALI_DEFAULT_SET);
                    }
                    break;
                }
                usleep(100000); // delay 100ms
                if((Device_measured_value >= PSU_Low_Limit) && (Device_measured_value <= PSU_High_Limit))
                {
                    Valid_measured_value = 1;       //UI value->Green
                }
                else
                {
                    Valid_measured_value = 0;       //UI value->Red
                }
            }
        }
    } 
    else if (strcmp(control_device, "Keyboard") == 0)
    {
        if (strcmp(setting_type, "Wait") == 0)
        {
            // **Define PID parameters**
            float Kp = 150.0f;  // Proportional gain
            float Ki = 20.0f;   // Integral gain
            float Kd = 10.0f;   // Derivative gain

            float Integral = 0.0f;
            float Derivative = 0.0f;
            // **Define dynamic variable limit**
            float step_size_max = 200.0;
            float step_size_min = 1.0;
            Valid_measured_value = 0;
            SVR_value = atoi(setting_value);
            Avg_value = (PSU_Low_Limit + PSU_High_Limit)/2;
            step_size = 100;
            resolution_value = (PSU_High_Limit-PSU_Low_Limit)/4;
            new_error = 0.0f;
            last_error = 0.0f;
            error_ratio = 0.0f;
            OK_cnt = 0;
            filter_count = 0;
            Sum_Device_measured_value = 0.0;
            Filter_Device_measured_value = 0.0;

            if (communication_found == CANBUS) {
                //Polling SVR value
                Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_WRITE_SVR);
            } else if (communication_found == MODBUS) {
                //Polling SVR value
                Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_WRITE_SVR);
            }

            sleep(3); //wait device setting response

            while(1)
            {
                filter_count++;
                Sum_Device_measured_value += Device_measured_value;
                if (filter_count >= 12)
                {
                    Filter_Device_measured_value = (Sum_Device_measured_value / filter_count);
                    new_error = Filter_Device_measured_value - Avg_value; // Calculation error
                    //printf("Count數: %d, Error: %.3f\n", filter_count, new_error);

                    filter_count = 0;
                    Sum_Device_measured_value = 0.0f;

                    if (fabs(new_error) < resolution_value) // new error < resolution value ->stop
                    {
                        OK_cnt++;
                        if (OK_cnt >= 3)
                        {
                            break;
                        }
                    }
                    else
                    {
                        //OK_cnt = 0;
                        // **Calculation error ratio**
                        // error_ratio = fabs(new_error) / (fabs(last_error) + 1e-6);
                        Integral += new_error; // Accumulate integral
                        Derivative = new_error - last_error; // Calculate derivative

                        // Adjustment step_size
                        // step_size *= fmax(fmin(error_ratio, 1.1), 0.9);
                        // Compute step size using PID
                        printf("new_error = %.2f, Integral = %.2f, Derivative = %.2f\n", new_error, Integral, Derivative);
                        step_size = fabs((Kp * new_error) + (Ki * Integral) + (Kd * Derivative));

                        // **When error becomes smaller, gradually reduce the maximum step limit**
                        // if (fabs(new_error) < (3 * resolution_value))
                        // {
                        //     step_size_max *= 0.8;
                        //     step_size *= 0.7;
                        // }

                        // if (fabs(new_error) < (2 * resolution_value))
                        // {
                        //     step_size *= 0.5;
                        // }
                        // //**確保 `step_size_max` 不會縮小到低於 `step_size_min`**
                        // step_size_max = fmax(step_size_max, step_size_min * 2);  // 最少保持比 `step_size_min` 大 2 倍


                        if (step_size > step_size_max)
                        {
                            step_size = step_size_max;
                        }
                        else if (step_size < step_size_min)
                        {
                            step_size = step_size_min;
                        }
                        // **判斷正/反邏輯**
                        if (direction_logic && cJSON_IsString(direction_logic) 
                            && strcmp(direction_logic->valuestring, "Positive") == 0)  // positive logic
                        {
                            if (Filter_Device_measured_value < Avg_value)
                            {
                                SVR_value += step_size;
                            }
                            else
                            {
                                SVR_value -= step_size;
                            }
                        }
                        else if (direction_logic && cJSON_IsString(direction_logic) 
                                && strcmp(direction_logic->valuestring, "Negative") == 0) // Negative logic
                        {
                            if (Filter_Device_measured_value < Avg_value)
                            {
                                SVR_value -= step_size;
                            }
                            else
                            {
                                SVR_value += step_size;
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
                        //printf("SVR value = %d\n", SVR_value);
                        printf("SVR value = %d, Step Size = %.2f\n", SVR_value, step_size);
                            
                        if (communication_found == CANBUS) {
                            //Polling SVR value
                            Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_WRITE_SVR);
                        } else if (communication_found == MODBUS) {
                            //Polling SVR value
                            Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_WRITE_SVR);
                        }
                    }
                    last_error = new_error;
                }

                if((Device_measured_value >= PSU_Low_Limit) && (Device_measured_value <= PSU_High_Limit))
                {
                    Valid_measured_value = 1;       //UI value->Green
                }
                else
                {
                    Valid_measured_value = 0;       //UI value->Red
                }

                usleep(100000); // delay 100ms
            }
        }
        else if (strcmp(setting_type, "Next") == 0)
        {
            usleep(200000); // Delay 200ms
            wCali_Status = 1;
            if (communication_found == CANBUS) {
                Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_CALI_STATUS);
            } else if (communication_found == MODBUS) {
                Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_CALI_STATUS);
            }
            Cali_Point_Cali_Complete = 1;
            sleep(1);
        } 
    }
    else {
        printf("未知的設備識別碼: %s\n", control_device);
        return;
    }

    printf("發送指令: [%s] 到設備 [%s]\n", step_command, usb_port);
}

void Calibration_Device_Auto_Control(void) {
    char step_cmd_str[10];  // 預留空間存字串
    char step_index[10];
    char usb_port[50] = {0};

    //Load JSON File
    cJSON *bic5k_json = load_json_file("./scripts/BIC-5K.json");
    cJSON *device_config_json = load_json_file("./DeviceConfig/DeviceConfig.json");

    if (!bic5k_json || !device_config_json) {
        printf("載入 JSON 檔案失敗\n");
    }

    snprintf(step_cmd_str, sizeof(step_cmd_str), "0x%04X", UI_Calibration_Point); // uint16_t to char

    cJSON *step_setting = find_step_setting(bic5k_json, step_cmd_str);
    if (!step_setting) {
        printf("未找到對應的 Step_Setting\n");
    }

    for (int i = 1; ; i++) {
        snprintf(step_index, sizeof(step_index), "%d", i);
        cJSON *step = cJSON_GetObjectItem(step_setting, step_index);
        if (!step) break;

        // Get Control_Device, Setting_Type, Setting_Value
        cJSON *control_device = cJSON_GetObjectItem(step, "Control_Device");
        cJSON *setting_type = cJSON_GetObjectItem(step, "Setting_Type");
        cJSON *setting_value = cJSON_GetObjectItem(step, "Setting_Value");

        if (!control_device || !setting_type || !setting_value ||
            !cJSON_IsString(control_device) || !cJSON_IsString(setting_type) || !cJSON_IsString(setting_value)) {
            continue;
        }

        printf("步驟 %d: 設備 = %s, 類型 = %s, 值 = %s\n", i, control_device->valuestring, setting_type->valuestring, setting_value->valuestring);

        // 取得設備的 USB_Port
        if (!find_device_info(device_config_json, control_device->valuestring, usb_port)) {
            printf("未找到設備資訊\n");
            continue;
        }

        printf("找到設備 %s, 端口 %s\n", control_device->valuestring, usb_port);

        // 發送步驟指令
        Send_Step_Command(usb_port, control_device->valuestring, setting_type->valuestring, setting_value->valuestring);
        usleep(20000);
    }
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

// // **檢查方向是否正確**
// if ((Filter_Device_measured_value > last_measured_value && last_SVR_value < SVR_value) ||
//     (Filter_Device_measured_value < last_measured_value && last_SVR_value > SVR_value))
// {
//     adjust_direction = 1; // 翻轉方向
//     printf("正邏輯\n");
// }
// else if ((Filter_Device_measured_value > last_measured_value && last_SVR_value > SVR_value) ||
//          (Filter_Device_measured_value < last_measured_value && last_SVR_value < SVR_value))
// {
//     adjust_direction = -1; // 翻轉方向
//     printf("反邏輯\n");
// }
// printf("新濾波值: %.3f, 舊濾波值: %.3f", Filter_Device_measured_value, last_measured_value);
// printf("新SVR: %d, 舊SVR: %d", SVR_value, last_SVR_value);

// direction_cnt = 0;
// while(1)
// {
//     direction_cnt++;
//     initial_measured_value += Device_measured_value;
//     if (direction_cnt >= 50)
//     {
//         initial_measured_value = initial_measured_value/direction_cnt;
//         printf("2048初始值: %.3f\n", initial_measured_value);
//         break;
//     }
//     usleep(20000);
// }
// SVR_value = 2148;
// if (communication_found == CANBUS) {
//     Canbus_TxProcess_Write(CAN_TX_ID_DUT, CAN_WRITE_SVR);
// } else if (communication_found == MODBUS) {
//     Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_WRITE_SVR);
// }
// // 等待響應
// sleep(1);
// direction_cnt = 0;

// while(1)
// {
//     direction_cnt++;
//     new_measured_value += Device_measured_value;
//     if (direction_cnt >= 50)
//     {
//         new_measured_value = new_measured_value/direction_cnt;
//         direction_cnt = 0;
//         printf("2148改變值: %.3f\n", new_measured_value);
//         break;
//     }
//     usleep(20000);
// }

// // **判斷邏輯方向**
// if (new_measured_value > initial_measured_value) {
//     adjust_direction = 1;  // SVR 增加 → 測量值增加，為正邏輯
// } else {
//     adjust_direction = 0;  // SVR 增加 → 測量值減少，為反邏輯
// }
// printf("系統邏輯模式: %d\n", adjust_direction);

//last_SVR_value = SVR_value;
// // **根據方向與誤差調整 SVR**
// if (Filter_Device_measured_value < Avg_value)
// {
//     SVR_value += step_size * adjust_direction;
// }
// else if (Filter_Device_measured_value > Avg_value)
// {
//     SVR_value -= step_size * adjust_direction;
// }

// uint8_t adjust_direction;  //1:正邏輯 0:反邏輯
// uint8_t direction_cnt = 0;
// float initial_measured_value = 0.0f;
// float new_measured_value = 0.0f;
// if((fabs(Avg_value) > 100) || (fabs(Avg_value) < 1))
// {
//     resolution_value = 0.1f;
// }
// else
// {
//     resolution_value = 0.01f;
// }
// 動態調整步伐：誤差大則步伐大，誤差小則步伐小
// if (fabs(new_error) > 1.0) 
// {
//     step_size = 50.0;
// } 
// else if (fabs(new_error) > 0.5) 
// {
//     step_size = 10.0;
// }
// else 
// {
//     step_size = 1.0;
// }
//----------------------------------------------------------------------------------------------------//
// DCL_value = atof(setting_value);
// Avg_value = (PSU_Low_Limit + PSU_High_Limit)/2;
// step_size = 0.1f;
// resolution_value = (PSU_High_Limit-PSU_Low_Limit)/4;
// new_error = 0.0f;
// last_error = 0.0f;
// error_ratio = 0.0f;
// new_error = Filter_Device_measured_value - Avg_value; // 計算誤差
// printf("Count數: %d, Error: %.3f\n", filter_count, new_error);

// filter_count = 0;
// Sum_Device_measured_value = 0.0f;

// if (fabs(new_error) < resolution_value) // 若誤差小於解析門檻則停止
// {
//     OK_cnt++;
//     if (OK_cnt >= 10)
//     {
//         break;
//     }
// }
// else
// {
//     OK_cnt = 0;
    
//     // 動態調整步伐：誤差大則步伐大，誤差小則步伐小
//     if (fabs(new_error) > 0.5) 
//     {
//         step_size = 0.2;
//     }
//     else if (fabs(new_error) > 0.1) 
//     {
//         step_size = 0.05;
//     }
//     else 
//     {
//         step_size = 0.01;
//     }

//     // **判斷正/反邏輯**
//     if (direction_logic && cJSON_IsString(direction_logic) 
//         && strcmp(direction_logic->valuestring, "Positive") == 0)  // 正邏輯
//     {
//         if (Device_measured_value < Avg_value)
//         {
//             DCL_value += step_size; // 讓測量值變大
//         }
//         else
//         {
//             DCL_value -= step_size; // 讓測量值變小
//         }
//     }
//     else if (direction_logic && cJSON_IsString(direction_logic) 
//             && strcmp(direction_logic->valuestring, "Negative") == 0) // 反邏輯
//     {
//         if (Device_measured_value < Avg_value)
//         {
//             DCL_value -= step_size; // 讓測量值變大
//         }
//         else
//         {
//             DCL_value += step_size; // 讓測量值變小
//         }
//     }

//     // 更新 setting_value
//     snprintf(setting_value, 10, "%.2f", DCL_value);
//     snprintf(step_command, sizeof(step_command), "%s %s\n", SCPI_CURR_STAT_L1, setting_value);
    
//     SCPI_Write_Process(usb_port, step_command);
// }
// if (fabs(new_error) > (5 * resolution_value))  // 當誤差變小到一定程度
// {
//     //step_size_max *= 0.8;  // 逐步縮小 `step_size_max`
//     step_size = 50;
// }
// else if (fabs(new_error) < (2 * resolution_value))
// {
//     step_size = 1;
// }
// else
// {
//     step_size = 10;
// 
//**當誤差已經很小，逐步減小 step_size，避免過衝**
// if (fabs(new_error) < (2 * resolution_value))
// {
//     //step_size *= 0.5;  // 若誤差已經接近目標，縮小步進
//     step_size = 1;
// }

