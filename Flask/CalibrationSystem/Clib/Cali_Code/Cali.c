/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Canbus.h"
#include "Modbus.h"

/* Parameter -----------------------------------------------*/
uint8_t space_pressed = 0; //1:Coarse 0:Fine
volatile char machine_type[13]; //Receive the machine type returned by the command 0x0082 and 0x0083
int SVR_value = 0;
uint8_t PSU_Calibration_Type;
uint8_t Cali_Status;
uint8_t Cali_Type_Point;

volatile uint8_t communication_found = 0; // 0: No COMM, 1: CAN, 2: MODBUS
pthread_mutex_t lock; // Protect Shared variables
pthread_t Cali_thread; // Main Cali thread

struct input_event ev;
int Keyboard_fd;
uint8_t End_polling;
//Modbus
uint8_t response_data[256] = {0};
int Mod_bytes_read;

/* function -----------------------------------------------*/
void Manual_Calibration(void);
char* Get_Machine_Name(void);
uint8_t Get_Communication_Type(void);
uint8_t Get_Keyboard_Adjustment(void);
uint8_t Get_PSU_Calibration_Type(void);
uint8_t Get_Calibration_Status(void);
uint8_t Get_Calibration_Type_Point(void);
void Parameter_Init(void);
void Keyboard_Control(void);
const char* find_keyboard_device(void);

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
                    if((frame.data[0] == 0x82) && (frame.data[1] == 0x00))
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
                    Cali_Status = FAIL; //CAN Read error
                }
                break;
            }

            case 1: //half model name
            {
                // Send 0x0083
                Canbus_TxProcess_Read(CAN_MODEL_NAME_B6B11);
                Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                if(Can_read_bytes > 0){
                    if((frame.data[0] == 0x83) && (frame.data[1] == 0x00))
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
                    Cali_Status = FAIL; //CAN Read error
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
    uint8_t Manual_Cali_step = 1;
    int Can_read_bytes = 0;

    memset(&frame, 0, sizeof(struct can_frame));
    Canbus_Init();

    while(1){
        if (Manual_Cali_step == 1){ //Send KEY repeatedly、Ask calibration mode?
            // Send Key
            Canbus_TxProcess_Write(CAN_CALIBRATION);
            usleep(20000); //Delay 20ms

            // Read PSU mode 0：Normal Mode  1：Calibrate Mode
            Canbus_TxProcess_Read(CAN_READ_PSU_MODE);

            // Receive(Cali_mode)
            Can_read_bytes = read(can_socket, &frame, sizeof(frame));
            if (Can_read_bytes > 0) {
                if((frame.data[0] == 0x11) && (frame.data[1] == 0xD0)){
                    if(frame.data[2] == 0x01){
                        Manual_Cali_step = 2;
                    }
                }
            } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                Cali_Status = FAIL; //CAN Read error
            }
        }
        else if (Manual_Cali_step == 2){
            // Read Cali Step
            Canbus_TxProcess_Read(CAN_CALI_STEP);

            // receive data(Cali_Step)
            Can_read_bytes = read(can_socket, &frame, sizeof(frame));
            if (Can_read_bytes > 0) {
                if((frame.data[0] == 0x01) && (frame.data[1] == 0xD0)){
                    if(frame.data[2] == 0x00){
                        Manual_Calibration();
                    }
                    else if(frame.data[2] == 0x01){
                        Manual_Cali_step = 2;
                    }
                    else if(frame.data[2] == 0xFF){
                        Cali_Status = FINISH;
                        break;
                    }
                }
            } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                Cali_Status = FAIL; //CAN read fail
            }
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
                    //Cali_Status = FAIL;
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
                    //Cali_Status = FAIL;
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
            Modbus_TxProcess_Write(MOD_CALIBRATION);
            usleep(20000); //Delay 20ms

            // Read PSU mode 0：Normal Mode  1：Calibrate Mode
            Modbus_TxProcess_Read(MOD_READ_PSU_MODE);
            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
            // check receive data complete
            if ((Mod_bytes_read > 0) && (response_data[4] == 0x01)) {
                Manual_Cali_step = 2;
            } else if (Mod_bytes_read == -2) {
                //Cali_Status = FAIL;
            }
            else {
               Manual_Cali_step = 1;
            }
        }
        else if (Manual_Cali_step == 2)
        {
            Modbus_TxProcess_Read(MOD_CALI_STEP);
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
                    Cali_Status = FINISH;
                    break;
                }
            } else if (Mod_bytes_read == -2) {
                //Cali_Status = FAIL;
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
    // Set keyboard device path
    const char *device = find_keyboard_device();

    if (device == NULL) {
        Cali_Status = FAIL; //device setting fail
        return;
    }

    Keyboard_fd = open(device, O_RDONLY | O_NONBLOCK); //O_RDONLY:only read 
    
    if (Keyboard_fd == -1) {
        Cali_Status = FAIL; //Unable to open input device
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
            
            if (Cali_type_polling == 1) {    //Need polling SVR value
                if (communication_found == CANBUS) {
                    //Polling SVR value
                    Canbus_TxProcess_Write(CAN_WRITE_SVR);
                } else if (communication_found == MODBUS) {
                    //Polling SVR value
                    Modbus_TxProcess_Write(MOD_WRITE_SVR);
                }
            } else {
                switch (receive_cnt)
                {
                    case 0:
                    {
                        if (communication_found == CANBUS) 
                        {
                            // Read Calibration Type
                            Canbus_TxProcess_Read(CAN_CALIBRATION_TYPE);
                            // Reveice(Cali_type)
                            Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                            if (Can_read_bytes > 0) {
                                if((frame.data[0] == 0x10) && (frame.data[1] == 0xD0)){
                                    PSU_Calibration_Type = frame.data[2];
                                    receive_cnt = 1;
                                }
                            } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                                Cali_Status = FAIL; //CAN Read error
                            }
                        }
                        else if (communication_found == MODBUS)
                        {
                            // Read Calibration Type
                            Modbus_TxProcess_Read(MOD_CALIBRATION_TYPE);
                            // Reveice(Cali_type)
                            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                            // check receive data complete
                            if (Mod_bytes_read > 0) { // CRC 2 bytes
                                PSU_Calibration_Type = response_data[4];
                                receive_cnt = 1;
                            } else if (Mod_bytes_read == -2) {
                                //Cali_Status = FAIL;
                            }
                        }
                        break;
                    }
                        
                    case 1:
                    {
                        if (communication_found == CANBUS) 
                        {
                            // Read Calibration point
                            Canbus_TxProcess_Read(CAN_CALIBRATION_POINT);
                            // Reveice(Cali_point)
                            Can_read_bytes = read(can_socket, &frame, sizeof(frame));
                            if (Can_read_bytes > 0) {
                                if((frame.data[0] == 0x14) && (frame.data[1] == 0xD0)){
                                    Cali_Type_Point = frame.data[2];
                                    receive_cnt = 2;
                                }
                            } else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                                Cali_Status = FAIL; //CAN Read error
                            }
                        }
                        else if (communication_found == MODBUS)
                        {
                            // Read Calibration Type
                            Modbus_TxProcess_Read(MOD_CALIBRATION_POINT);
                            // Reveice(Cali_point)
                            Mod_bytes_read = Modbus_RxProcess(response_data, sizeof(response_data));
                            // check receive data complete
                            if (Mod_bytes_read > 0) { // CRC 2 bytes
                                Cali_Type_Point = response_data[4];
                                receive_cnt = 2;
                            } else if (Mod_bytes_read == -2) {
                                //Cali_Status = FAIL;
                            }
                        }
                        break;
                    }

                    default:
                        break;
                }

                if (((PSU_Calibration_Type & 0xF1) != 0) && (receive_cnt == 2)) 
                {
                    Cali_type_polling = 1;
                } 
                else 
                {
                    Cali_type_polling = 0;
                }
            }
            if(End_polling == 1)
            {
                if (communication_found == CANBUS) {
                    Canbus_TxProcess_Write(CAN_CALI_STEP);
                } else if (communication_found == MODBUS) {
                    Modbus_TxProcess_Write(MOD_CALI_STEP);
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

const char* find_keyboard_device(void)
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

uint8_t Get_PSU_Calibration_Type(void) {
    return PSU_Calibration_Type;
}

uint8_t Get_Calibration_Status(void) {
    return Cali_Status;
}

uint8_t Get_Calibration_Type_Point(void) {
    return Cali_Type_Point;
}

void Parameter_Init(void) {
    memset(machine_type, ' ', sizeof(char) * 12);
    machine_type[12] = '\0';
    communication_found = 0;
    space_pressed = 0;
    PSU_Calibration_Type = 0;
    Cali_Status = 0;
    Cali_Type_Point = 0;
}

