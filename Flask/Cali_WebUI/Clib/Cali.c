/* Includes ------------------------------------------------------------------*/
#include "Cali.h"

/* Parameter -----------------------------------------------*/
int s;
struct sockaddr_can addr;
struct ifreq ifr;
struct can_frame frame;
uint8_t space_pressed = 0; //1:Coarse 0:Fine
volatile char machine_type[13]; //Receive the machine type returned by the command 0x0082 and 0x0083
int SVR_value = 0;
uint8_t PSU_Calibration_Type;
uint8_t Cali_Status;
uint8_t Cali_Type_Point;

volatile uint8_t communication_found = 0; // 0: No COMM, 1: CAN, 2: MODBUS
pthread_mutex_t lock; // Protect Shared variables
pthread_t Cali_thread; // Main Cali thread

/* function -----------------------------------------------*/
void Manual_Calibration(void);
void CAN_TX(uint32_t CAN_Address, uint8_t CAN_DLC);
void CAN_RX(void);
char* Get_Machine_Name(void);
uint8_t Get_Communication_Type(void);
uint8_t Get_Keyboard_Adjustment(void);
uint8_t Get_PSU_Calibration_Type(void);
uint8_t Get_Calibration_Status(void);
uint8_t Get_Calibration_Type_Point(void);
void Parameter_Init(void);

// CRC16
uint16_t calculateCRC(uint8_t *data, int length) {
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < length; pos++) {
        crc ^= (uint16_t)data[pos];  // XOR byte into least sig. byte of crc
        for (int i = 8; i != 0; i--) {  // Loop over each bit
            if ((crc & 0x0001) != 0) {  // If the LSB is set
                crc >>= 1;              // Shift right and XOR 0xA001
                crc ^= 0xA001;
            } else {
                crc >>= 1;              // Just shift right
            }
        }
    }
    return crc;
}

void CAN_TX(uint32_t CAN_Address, uint8_t CAN_DLC)
{
    // Setting CAN Frame
    frame.can_id = TX_ID | CAN_EFF_FLAG;
    frame.can_dlc = CAN_DLC;    // Data Length
    if(CAN_DLC == 2)
    {
        frame.data[0] = CAN_Address;
        frame.data[1] = CAN_Address>>8;
    }
    else
    {
        frame.data[0] = CAN_Address;
        frame.data[1] = CAN_Address>>8;
        switch(CAN_Address){
            case CALIBRATION:
                frame.data[2] = 0x57;
                frame.data[3] = 0x4D;
            break;
            case CALI_STEP:
                frame.data[2] = 01;     // Low byte
                frame.data[3] = 00;     // High byte
            break;
            case WRITE_SVR:
                frame.data[2] = SVR_value & 0xFF;        // Low byte
                frame.data[3] = (SVR_value >> 8) & 0x0F; // High byte
            break;
            default:
            break;
        }
    }

    int Retry_cnt = 0;
    while(write(s, &frame, sizeof(frame)) != sizeof(frame)) {
        Retry_cnt++;
        perror("CAN Write error");
        usleep(10000); //delay 10ms
        if(Retry_cnt >= 10)
        {
            Cali_Status = FAIL;
            break;
        }
    }
}
void CAN_Init(void)
{
    // Init CAN Port
    system("sudo ifconfig can0 down");
    usleep(10000);
    system("sudo ip link set can0 type can bitrate 250000");
    system("sudo ifconfig can0 up");

    // Init CANBUS Socket
    // 1. Create CAN Socket
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("CAN Socket error");
        pthread_exit(NULL);
    }
    // 2. Specify can0 device
    strcpy(ifr.ifr_name, "can0");
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("CAN IOCTL error");
        close(s);
        pthread_exit(NULL);
    }
    // 3. Bind socket to can0
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("CAN Bind error");
        close(s);
        pthread_exit(NULL);
    }
    
    struct can_filter rfilter[1];               //Define CAN data filter
    rfilter[0].can_id = RX_ID | CAN_EFF_FLAG;   //Specifies ID of the received CAN message、CAN_EFF_FLAG->29bits
    rfilter[0].can_mask = CAN_EFF_MASK;
    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
    
    // Set non-blocking mode
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

void *can_polling(void *arg) {
    int ask_name = 0; // 0 : machine type is not asked yet, 2 : receive answer by asking 0x0082 and 0x0083
    memset(&frame, 0, sizeof(struct can_frame));
    CAN_Init();

    while (!communication_found) {
        // Send 0x0082
        CAN_TX(MODEL_NAME_B0B5, 2);

        // Try to read
        int read_bytes = read(s, &frame, sizeof(frame));
        if (read_bytes > 0) {
            if((frame.data[0] == 0x82) && (frame.data[1] == 0x00))
            {
                ask_name++;
                machine_type[0] = frame.data[2];
                machine_type[1] = frame.data[3];
                machine_type[2] = frame.data[4];
                machine_type[3] = frame.data[5];
                machine_type[4] = frame.data[6];
                machine_type[5] = frame.data[7];
                machine_type[6] = '\0';
            }
            else
            {
                ask_name = 0;
            }

            while(ask_name == 1){ //Send 0x0083
                CAN_TX(MODEL_NAME_B6B10, 2);
                int read_bytes2 = read(s, &frame, sizeof(frame));
                if(read_bytes2 > 0){
                    if((frame.data[0] == 0x83) && (frame.data[1] == 0x00))
                    {
                        ask_name ++;
                        machine_type[6] = frame.data[2];
                        machine_type[7] = frame.data[3];
                        machine_type[8] = frame.data[4];
                        machine_type[9] = frame.data[5];
                        machine_type[10] = frame.data[6];
                        machine_type[11] = frame.data[7];
                        machine_type[12] = '\0';
                    }
                    else
                    {
                        ask_name = 1;
                    }
                }
                if(ask_name == 2){
                    break;
                }
                usleep(20000); //Delay 20ms
            }

            pthread_mutex_lock(&lock);
            if (!communication_found) {
                communication_found = CANBUS; // Set COMM -> CANBUS
                printf("Detected CANBUS communication\n");
            }
            pthread_mutex_unlock(&lock);
            break;
        } else if (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("CAN Read error");
        }
        usleep(20000); // Delay 20ms
    }

    close(s);
    system("sudo ifconfig can0 down");
    pthread_exit(NULL);
}

void *rs485_polling(void *arg) {
    int serial_fd;
    uint8_t polling_command[] = {0xC0, 0x03, 0x00, 0x86, 0x00, 0x03}; // 基本輪詢指令
    uint8_t full_command[8]; // 包含 CRC 的完整指令
    uint8_t response[256];   // 暫存接收的回應
    int bytes_read;

    // 初始化 RS485 串口
    if ((serial_fd = serialOpen("/dev/ttyS0", 115200)) < 0) { // 假設使用 /dev/ttyS0
        perror("RS485 Initialization error");
        pthread_exit(NULL);
    }
    
    if (wiringPiSetupGpio() < 0) { // 使用 BCM2835 腳位編號
        perror("WiringPi Setup GPIO error");
        serialClose(serial_fd);
        pthread_exit(NULL);
    }

    pinMode(EN_485, OUTPUT);
    digitalWrite(EN_485, LOW); //默認接收模式

    // 計算 CRC 並準備完整指令
    memcpy(full_command, polling_command, sizeof(polling_command));
    uint16_t crc = calculateCRC(polling_command, sizeof(polling_command));
    full_command[6] = crc & 0xFF;       // CRC 低位
    full_command[7] = (crc >> 8) & 0xFF; // CRC 高位

    while (!communication_found) {
        // 發送輪詢指令
        digitalWrite(EN_485, HIGH); // 切換為發送模式
        usleep(1000); // 等待模式切換（視硬體調整）
        for (int i = 0; i < sizeof(full_command); i++) {
            serialPutchar(serial_fd, full_command[i]);
        }
        usleep(1000); // 等待硬體完成發送
        digitalWrite(EN_485, LOW); // 切換為接收模式

        // 接收資料
        usleep(2000); // 等待設備回應
        bytes_read = 0;
        int timeout_485 = 5;
        while (timeout_485 > 0) {
            if (serialDataAvail(serial_fd)) {
                response[bytes_read++] = serialGetchar(serial_fd);
                if (bytes_read >= sizeof(response)) break;
            } else {
                usleep(1000);
                timeout_485--;
            }
        }

         // 檢查回應是否完整
        if (bytes_read > 2) { // 至少需要 CRC 兩個位元組
            uint16_t received_crc = (response[bytes_read - 1] << 8) | response[bytes_read - 2];
            uint16_t calculated_crc = calculateCRC(response, bytes_read - 2);

            if (received_crc == calculated_crc) {
                pthread_mutex_lock(&lock);
                if (!communication_found) {
                    communication_found = MODBUS; // 設定為 RS485 通訊
                }
                pthread_mutex_unlock(&lock);
                break;
            }
        }
        usleep(20000);
    }

    serialClose(serial_fd);
    pthread_exit(NULL);
}

void CAN_CALI(void){
    memset(&frame, 0, sizeof(struct can_frame));
    CAN_Init();
    uint8_t Manual_Cali_step = 1;
    while(1){
        if (Manual_Cali_step == 1){ //Send KEY repeatedly、Ask calibration mode?
            // Send Key
            CAN_TX(CALIBRATION, 4);
            usleep(20000); //Delay 20ms

            // Read PSU mode 0：Normal Mode  1：Calibrate Mode
            CAN_TX(READ_PSU_MODE, 2);

            // Receive(Cali_mode)
            int read_bytes = read(s, &frame, sizeof(frame));
            if (read_bytes > 0) {
                if((frame.data[0] == 0x11) && (frame.data[1] == 0xD0)){
                    if(frame.data[2] == 0x01){
                        Manual_Cali_step = 2;
                    }
                }
            } else if (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("CAN Read error");
            }
            usleep(20000); // Delay 20ms
        }
        else if (Manual_Cali_step == 2){
            digitalWrite(LED_PIN, HIGH); // LED ON
            printf("LED ON\n");
            usleep(500000);

            digitalWrite(LED_PIN, LOW); // LED OFF
            printf("LED OFF\n");
            usleep(500000);
            // Read Cali Step
            CAN_TX(CALI_STEP, 2);

            // 接收資料(Cali_Step)
            int read_bytes = read(s, &frame, sizeof(frame));
            if (read_bytes > 0) {
                if((frame.data[0] == 0x01) && (frame.data[1] == 0xD0)){
                    if(frame.data[2] == 0x00){
                        digitalWrite(LED_PIN, HIGH); // LED ON
                        Manual_Calibration();
                    }
                    else if(frame.data[2] == 0x01){
                        Manual_Cali_step = 2;
                    }
                    else if(frame.data[2] == 0xFF){
                        Cali_Status = FINISH;
                        printf("PASS\n");
                        break;
                    }
                }
            } else if (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("CAN Read error");
            }
            usleep(20000); // delay 20ms
        }
    }
    close(s);
}

void MOD_CALI(void){
    
}

void Manual_Calibration(void){
    int polling_cnt = 0;
    uint8_t End_polling = 0;
    uint8_t Cali_type_polling = 0;
    uint8_t receive_cnt = 0;
    SVR_value = 2048;
    // Set keyboard device path
    const char *device = "/dev/input/event2"; //choose the eventX corresponding to the keyboard
    int fd = open(device, O_RDONLY | O_NONBLOCK);
    
    if (fd == -1) {
        perror("Unable to open input device");
    }

    // Set terminal properties to disable input echoing
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);           // Get current terminal settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);         // Disable line buffering and echoing
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // Set new terminal settings

    struct input_event ev;
    printf("開始監控鍵盤輸入（使用上下左右鍵進行控制，按 Ctrl+C 結束）\n");

    while (1) {
        polling_cnt++;
        // Read events
        if (read(fd, &ev, sizeof(struct input_event)) > 0){
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
                printf("0x%X\n", SVR_value);
            }
        }

        if (polling_cnt >= 200) {
            polling_cnt = 0;
            
            if (Cali_type_polling == 1) {    //Need polling SVR value
                if (communication_found == CANBUS) {
                    //Polling SVR value
                    CAN_TX(WRITE_SVR, 4);
                } else if (communication_found == MODBUS) {
                    
                }
            } else {
                int read_bytes;
                switch (receive_cnt)
                {
                case 0:
                    // Read Calibration Type
                    CAN_TX(CALIBRATION_TYPE, 2);
                    // Reveice(Cali_type)
                    read_bytes = read(s, &frame, sizeof(frame));
                    if (read_bytes > 0) {
                        if((frame.data[0] == 0x10) && (frame.data[1] == 0xD0)){
                            PSU_Calibration_Type = frame.data[2];
                            receive_cnt = 1;
                        }
                    } else if (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("CAN Read error");
                    }
                    break;

                case 1:
                    // Read Calibration point
                    CAN_TX(CALIBRATION_POINT, 2);
                    // Reveice(Cali_point)
                    read_bytes = read(s, &frame, sizeof(frame));
                    if (read_bytes > 0) {
                        if((frame.data[0] == 0x14) && (frame.data[1] == 0xD0)){
                            Cali_Type_Point = frame.data[2];
                            printf("Cali_Type_Point = %x", Cali_Type_Point);
                            receive_cnt = 2;
                        }
                    } else if (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("CAN Read error");
                    }
                    break;
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
                    CAN_TX(CALI_STEP, 4);
                } else if (communication_found == MODBUS) {
                    
                }
                delay(1000);
                break;
            }
        }
         
        usleep(100); 
    }
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    close(fd);
}

void* Cali_routine(void* arg) {
    Parameter_Init();
    pthread_t can_thread, rs485_thread;

    // Init Mutex
    pthread_mutex_init(&lock, NULL);

    // Create CANBUS 、 RS485 polling thread
    pthread_create(&can_thread, NULL, can_polling, NULL);
    pthread_create(&rs485_thread, NULL, rs485_polling, NULL);

    // Wait thread end
    pthread_join(can_thread, NULL);
    pthread_join(rs485_thread, NULL);

    if (communication_found == CANBUS) {
        printf("Using CANBUS for communication\n");
        pthread_cancel(rs485_thread); // Cancel RS485 polling
    } else if (communication_found == MODBUS) {
        printf("Using MODBUS for communication\n");
        pthread_cancel(can_thread); // Cancel CAN polling
    } else {
        printf("No communication detected\n");
    }

    pthread_mutex_destroy(&lock);

    // Set GPIO 17 output mode
    pinMode(LED_PIN, OUTPUT);

    if (communication_found == CANBUS) {
        CAN_CALI();
    } else if (communication_found == MODBUS) {
        MOD_CALI();
    }
    digitalWrite(LED_PIN, LOW); // LED OFF

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
