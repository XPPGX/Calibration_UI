#ifndef Cali
#define Cali
#include "Cali.h"
#endif

#include "UI_layout.c"

void Manual_Calibration();

int space_pressed;
pthread_mutex_t button_lock;
pthread_cond_t button_cond;

// CRC16 計算函數
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

void CAN_Init()
{
    // 初始化 CAN 設備
    system("sudo ifconfig can0 down");
    usleep(10000);
    system("sudo ip link set can0 type can bitrate 250000");
    system("sudo ifconfig can0 up");

    // 初始化 CANBUS Socket
    // 1. 创建 CAN 套接字
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("CAN Socket error");
        pthread_exit(NULL);
    }
    // 2. 指定 can0 设备
    strcpy(ifr.ifr_name, "can0");
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0)
    {
        perror("CAN IOCTL error");
        close(s);
        pthread_exit(NULL);
    }
    // 3. 绑定套接字到 can0
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("CAN Bind error");
        close(s);
        pthread_exit(NULL);
    }
    // 4. 禁用过滤规则，接收和发送数据
    struct can_filter rfilter[1];
    rfilter[0].can_id = RX_ID | CAN_EFF_FLAG;
    rfilter[0].can_mask = CAN_SFF_MASK;
    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
    
    // 設置非阻塞模式
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

void *can_polling(void *arg) {
    memset(&frame, 0, sizeof(struct can_frame));
    CAN_Init();
    // 設定 CAN Frame
    frame.can_id = TX_ID | CAN_EFF_FLAG;
    frame.can_dlc = 2;    // 資料長度
    frame.data[0] = 0x82;
    frame.data[1] = 0x00;

    while (!communication_found) {
        // 發送資料
        if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
            perror("CAN Write error");
            usleep(10000); // 短暫延遲後重試
            continue;
        }

        // 嘗試讀取回應
        int read_bytes = read(s, &frame, sizeof(frame));
        if (read_bytes > 0) {
            pthread_mutex_lock(&lock);
            if (!communication_found) {
                communication_found = CANBUS; // 設定為 CAN 通訊
                printf("Detected CANBUS communication\n");
            }
            pthread_mutex_unlock(&lock);
            break;
        } else if (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("CAN Read error");
        }
        usleep(20000); // 避免過度頻繁的輪詢
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

void CAN_CALI(){
    memset(&frame, 0, sizeof(struct can_frame));
    CAN_Init();
    uint8_t Manual_Cali_step = 1;
    while(1){
        if (Manual_Cali_step == 1){ //重複寫入KEY，詢問是否為校正模式
            // 設定 CAN Frame(開啟校正模式)
            frame.can_id = TX_ID | CAN_EFF_FLAG;
            frame.can_dlc = 4;    // 資料長度
            frame.data[0] = 0x00;
            frame.data[1] = 0xD0;
            frame.data[2] = 0x57;
            frame.data[3] = 0x4D;

            // 發送資料(Send Key)
            if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
                perror("CAN Write error");
                usleep(10000); // 短暫延遲後重試
                continue;
            }
            usleep(20000);

            // 設定 CAN Frame(讀取校正種類)
            frame.can_id = TX_ID | CAN_EFF_FLAG;
            frame.can_dlc = 2;    // 資料長度
            frame.data[0] = 0x11;
            frame.data[1] = 0xD0;

            // 發送資料(Send Key)
            if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
                perror("CAN Write error");
                usleep(10000); // 短暫延遲後重試
                continue;
            }
            // 接收資料(Cali_mode)
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
            usleep(20000); // 避免過度頻繁的輪詢
        }
        else if (Manual_Cali_step == 2){
            digitalWrite(LED_PIN, HIGH); // LED ON
            printf("LED ON\n");
            usleep(500000);

            digitalWrite(LED_PIN, LOW); // LED OFF
            printf("LED OFF\n");
            usleep(500000);
            // 設定 CAN Frame(讀取儀器設定/數值紀錄控制)
            frame.can_id = TX_ID | CAN_EFF_FLAG;
            frame.can_dlc = 2;    // 資料長度
            frame.data[0] = 0x01;
            frame.data[1] = 0xD0;

            // 發送資料
            if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
                perror("CAN Write error");
                usleep(10000); // 短暫延遲後重試
                continue;
            }
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
                        printf("PASS\n");
                        break;
                    }
                }
            } else if (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("CAN Read error");
            }
            usleep(20000); // 避免過度頻繁的輪詢
        }
    }
    close(s); // 確保 CAN 套接字關閉
}

void MOD_CALI(){
    
}

void Manual_Calibration(){
    int SVR_value = 2048;
    int polling_cnt = 0;
    uint8_t End_polling = 0;
    uint8_t Cali_type_polling = 0;
    // Set keyboard device path
    const char *device = "/dev/input/event0";
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
    space_pressed = 0;
    printf("開始監控鍵盤輸入（使用上下左右鍵進行控制，按 Ctrl+C 結束）\n");

    while (1) {
        polling_cnt++;
        // Read events
        if (read(fd, &ev, sizeof(struct input_event)) > 0){
            // Only handle key press events
            if (ev.type == EV_KEY) {
                switch (ev.code) {
                    case KEY_SPACE:
                    printf("SPACE");
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
                        break; //Just break the switch case block, not the while block !!!!!!!!!!!!!!!!!!!
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
            if (Cali_type_polling == 1) {    //Need polling
                if (communication_found == CANBUS) {
                    frame.can_id = TX_ID | CAN_EFF_FLAG;
                    frame.can_dlc = 4;    // 資料長度
                    frame.data[0] = 0x02;
                    frame.data[1] = 0xD0;
                    frame.data[2] = SVR_value & 0xFF;        // Low byte
                    frame.data[3] = (SVR_value >> 8) & 0x0F; // High byte
                    // Send
                    if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
                        perror("CAN Write error");
                        usleep(10000);
                        continue;
                    }
                } else if (communication_found == MODBUS) {
                    
                }
            } else {
                // 設定 CAN Frame(讀取校正種類)
                frame.can_id = TX_ID | CAN_EFF_FLAG;
                frame.can_dlc = 2;    // 資料長度
                frame.data[0] = 0x10;
                frame.data[1] = 0xD0;

                // 發送資料(Send Cali_type)
                if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
                    perror("CAN Write error");
                    usleep(10000); // 短暫延遲後重試
                    continue;
                }
                // 接收資料(Cali_type)
                int read_bytes = read(s, &frame, sizeof(frame));
                if (read_bytes > 0) {
                    if((frame.data[0] == 0x10) && (frame.data[1] == 0xD0)){
                        //Here, read frame.data[2] as Cali_type.
                        if ((frame.data[2] & 0xF1) != 0) {
                            Cali_type_polling = 1;
                        } else {
                            Cali_type_polling = 0;
                        }
                    }
                } else if (read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("CAN Read error");
                }
            }
            if(End_polling == 1)
            {
                if (communication_found == CANBUS) {
                    frame.can_id = TX_ID | CAN_EFF_FLAG;
                    frame.can_dlc = 4;    // Data length 
                    frame.data[0] = 0x01;
                    frame.data[1] = 0xD0;
                    frame.data[2] = 01;        // Low byte
                    frame.data[3] = 00; // High byte
                    // Send
                    if (write(s, &frame, sizeof(frame)) != sizeof(frame)) {
                        perror("CAN Write error");
                        usleep(10000);
                        continue;
                    }
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

void flow_control_task(){

    pthread_mutex_lock(&button_lock);
    printf("[flow_control_thread]Waiting for button pressing...\n");
    pthread_cond_wait(&button_cond, &button_lock);
    pthread_mutex_unlock(&button_lock);
    printf("[flow_control_thread]Start\n");    

    pthread_t can_thread, rs485_thread;
    
    // 初始化 Mutex
    pthread_mutex_init(&lock, NULL);

    // 創建 CANBUS 與 RS485 執行緒
    pthread_create(&can_thread, NULL, can_polling, NULL);
    pthread_create(&rs485_thread, NULL, rs485_polling, NULL);

    // 等待執行緒結束
    pthread_join(can_thread, NULL);
    pthread_join(rs485_thread, NULL);

    // 根據結果輸出
    if (communication_found == CANBUS) {
        printf("Using CANBUS for communication\n");
        pthread_cancel(rs485_thread); // 取消 RS485 輪詢
    } else if (communication_found == MODBUS) {
        printf("Using MODBUS for communication\n");
        pthread_cancel(can_thread); // 取消 CAN 輪詢
    } else {
        printf("No communication detected\n");
    }

    pthread_mutex_destroy(&lock);

    // 設定 GPIO 17 為輸出模式
    pinMode(LED_PIN, OUTPUT);

    

    if (communication_found == CANBUS) {
        CAN_CALI();
    } else if (communication_found == MODBUS) {
        MOD_CALI();
    }
    digitalWrite(LED_PIN, LOW); // LED OFF
}

int main(int argc, char *argv[]) {
    
    GtkApplication *app;
    int status;
    app = gtk_application_new("Meanwell.com", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return 0;
}
