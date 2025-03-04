/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Canbus.h"

/* Parameter -----------------------------------------------*/
//CAN
int can_socket;
struct sockaddr_can addr;
struct ifreq ifr;
struct can_frame frame;

int Can_Retry_cnt = 0;
uint8_t Can_no_receive_cnt = 0;
uint8_t Can_receive_data_error_cnt = 0;

void Canbus_Init(void);
void Canbus_TxProcess_Read(uint32_t CAN_Address);
void Canbus_TxProcess_Write(uint32_t CAN_Address);
void Canbus_RxProcess(uint16_t Commmand);

void Canbus_Init(void)
{
    // Init CAN Port
    system("sudo ifconfig can0 down");
    usleep(10000);
    system("sudo ip link set can0 type can bitrate 250000");
    system("sudo ifconfig can0 up");

    struct can_filter rfilter[1];               //Define CAN data filter
    int flags;

    // Init CANBUS Socket
    // 1. Create CAN Socket
    can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_socket < 0) {
        Cali_Result = DUT_FAIL; //CAN Socket error
        pthread_exit(NULL);
    }
    // 2. Specify can0 device
    strcpy(ifr.ifr_name, "can0");
    if (ioctl(can_socket, SIOCGIFINDEX, &ifr) < 0)
    {
        Cali_Result = DUT_FAIL; //CAN IOCTL error
        close(can_socket);
        pthread_exit(NULL);
    }
    // 3. Bind socket to can0
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        Cali_Result = DUT_FAIL; //CAN Bind error
        close(can_socket);
        pthread_exit(NULL);
    }
    
    rfilter[0].can_id = CAN_RX_ID | CAN_EFF_FLAG;   //Specifies ID of the received CAN message、CAN_EFF_FLAG->29bits
    rfilter[0].can_mask = CAN_EFF_MASK;
    setsockopt(can_socket, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
    
    // Set non-blocking mode
    flags = fcntl(can_socket, F_GETFL, 0);
    fcntl(can_socket, F_SETFL, flags | O_NONBLOCK);
}

void Canbus_TxProcess_Read(uint32_t CAN_Address)
{
    // Setting CAN Frame
    frame.can_id = CAN_TX_ID | CAN_EFF_FLAG;
    frame.can_dlc = 2;    // Data Length
    frame.data[0] = CAN_Address;
    frame.data[1] = CAN_Address>>8;

    Can_Retry_cnt = 0;
    while(write(can_socket, &frame, sizeof(frame)) != sizeof(frame)) {
        Can_Retry_cnt++;
        usleep(10000); //delay 10ms
        if(Can_Retry_cnt >= 10)
        {
            Cali_Result = DUT_FAIL; //CAN Write error
            break;
        }
    }
    usleep(10000);
}

void Canbus_TxProcess_Write(uint32_t CAN_Address)
{
    // Setting CAN Frame
    frame.can_id = CAN_TX_ID | CAN_EFF_FLAG;
    frame.can_dlc = 4;    // Data Length
    frame.data[0] = CAN_Address;
    frame.data[1] = CAN_Address>>8;
    switch(CAN_Address){
        case CAN_CALIBRATION_KEY:
            frame.data[2] = 0x57;
            frame.data[3] = 0x4D;
        break;
        case CAN_CALI_STATUS:
            frame.data[2] = 0x01;     // Low byte
            frame.data[3] = 0x00;     // High byte
        break;
        case CAN_WRITE_SVR:
            frame.data[2] = SVR_value & 0xFF;        // Low byte
            frame.data[3] = (SVR_value >> 8) & 0x0F; // High byte
        break;
        case CAN_CALIBRATION_POINT:
            frame.data[2] = UI_Calibration_Point & 0xFF;        // Low byte
            frame.data[3] = (UI_Calibration_Point >> 8) & 0xFF; // High byte
        break;
        case CAN_CALI_RESULT:
            frame.data[2] = 0xFF;       // Low byte
            frame.data[3] = 0xFF;       // High byte
        break;
        default:
        break;
    }

    Can_Retry_cnt = 0;
    while(write(can_socket, &frame, sizeof(frame)) != sizeof(frame)) {
        Can_Retry_cnt++;
        usleep(10000); //delay 10ms
        if(Can_Retry_cnt >= 10)
        {
            Cali_Result = DUT_FAIL; //CAN Write error
            break;
        }
    }
}

void Canbus_RxProcess(uint16_t Commmand)
{
    int Can_read_bytes = 0;

    Can_read_bytes = read(can_socket, &frame, sizeof(frame));
    if (Can_read_bytes > 0) {
        if((frame.data[0] == (Commmand & 0xFF)) && (frame.data[1] == ((Commmand>>8) & 0xFF)))
        {
            Can_no_receive_cnt = 0;
            Can_receive_data_error_cnt = 0;

            switch (Commmand)
            {
            case CAN_MODEL_NAME_B0B5:
                Canbus_ask_name = 1;
                memcpy(machine_type, &frame.data[2], 6);
                machine_type[6] = '\0';
                break;

            case CAN_MODEL_NAME_B6B11:
                Canbus_ask_name = 2;
                memcpy(&machine_type[6], &frame.data[2], 6);
                machine_type[12] = '\0';
                break;
            
            case CAN_READ_PSU_MODE:
                if(frame.data[2] == 0x01){
                    Manual_Cali_step = READ_PSU_SCALING_FACTOR;
                }
                else{
                    Manual_Cali_step = SEND_KEY_READ_MODE;
                }
                break;
            
            case CAN_SCALING_FACTOR:
                PSU_ACV_Factor_Comm = (frame.data[3] & 0xF);
                PSU_ACI_Factor_Comm = (frame.data[5] & 0xF);
                PSU_DCV_Factor_Comm = (frame.data[2] & 0xF);
                PSU_DCI_Factor_Comm = ((frame.data[2]>>4) & 0xF);
                PSU_ACV_Factor = Scaling_Factor_Convert(PSU_ACV_Factor_Comm);
                PSU_ACI_Factor = Scaling_Factor_Convert(PSU_ACI_Factor_Comm);
                PSU_DCV_Factor = Scaling_Factor_Convert(PSU_DCV_Factor_Comm);
                PSU_DCI_Factor = Scaling_Factor_Convert(PSU_DCI_Factor_Comm);
                Manual_Cali_step = UI_RESET_CALI;
                break;
            
            case CAN_CALI_STATUS:
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
                    printf("校正點不匹配\n");
                    Cali_Result = CALI_POINT_FAIL;
                    break;
                
                case CALI_POINT_SETTING:    //0x0003
                    Canbus_TxProcess_Write(CAN_CALIBRATION_POINT);
                    usleep(20000); // Delay 20ms
                    break;
                
                case CALI_FINISH:           //0xFFFF
                    Manual_Cali_step = READ_CALI_RESULT;
                    break;
                
                default:
                    break;
                }
                break;
            
            case CAN_CALI_RESULT:
                if(frame.data[2] == 0x01)
                {
                    Cali_Result = FINISH;
                }
                else if(frame.data[2] == 0x02)
                {
                    Cali_Result = NOT_COMPLETE;
                }
                break;
            
            case CAN_SVR_POLLING:
                Cali_type_polling = frame.data[2];
                Cali_read_step++;
                break;

            case CAN_READ_HIGH_LIMIT:
                PSU_High_Limit_Comm = ((frame.data[3] << 8) | frame.data[2]);
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
                printf("上限值:%.2f\n",PSU_High_Limit);
                Cali_read_step++;
                break;

            case CAN_READ_LOW_LIMIT:
                PSU_Low_Limit_Comm = ((frame.data[3] << 8) | frame.data[2]);
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
                printf("下限值:%.2f\n",PSU_Low_Limit);
                Cali_read_step++;
                break;

            case CAN_AC_SOURCE_SET:
                Cali_read_step++;
                break;

            case CAN_DC_SOURCE_SET:
                Cali_read_step++;
                break;

            case CAN_DC_LOAD_SET:
                Cali_read_step++;
                break;

            default:
                break;
            }
        }
        else
        {
            Can_receive_data_error_cnt++;
            if(Can_receive_data_error_cnt >= 10)
            {
                //Cali_Result = FAIL;
            }
        }
    }
    else if (Can_read_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        Cali_Result = DUT_FAIL; //CAN Read error
    }
    else if (Can_read_bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        Can_no_receive_cnt++;
        if(Can_no_receive_cnt >= 10)
        {
            //Cali_Result = FAIL;
        }
    }
}
