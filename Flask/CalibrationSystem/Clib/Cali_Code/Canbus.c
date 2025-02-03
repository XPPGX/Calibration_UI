/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Canbus.h"

/* Parameter -----------------------------------------------*/
//CAN
int can_socket;
struct sockaddr_can addr;
struct ifreq ifr;
struct can_frame frame;

void Canbus_Init(void);
void Canbus_TxProcess_Read(uint32_t CAN_Address);
void Canbus_TxProcess_Write(uint32_t CAN_Address);
void Canbus_RxProcess(void);

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
        Cali_Status = FAIL; //CAN Socket error
        pthread_exit(NULL);
    }
    // 2. Specify can0 device
    strcpy(ifr.ifr_name, "can0");
    if (ioctl(can_socket, SIOCGIFINDEX, &ifr) < 0)
    {
        Cali_Status = FAIL; //CAN IOCTL error
        close(can_socket);
        pthread_exit(NULL);
    }
    // 3. Bind socket to can0
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(can_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        Cali_Status = FAIL; //CAN Bind error
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

    int Retry_cnt = 0;
    while(write(can_socket, &frame, sizeof(frame)) != sizeof(frame)) {
        Retry_cnt++;
        usleep(10000); //delay 10ms
        if(Retry_cnt >= 10)
        {
            //Cali_Status = FAIL; //CAN Write error
            break;
        }
    }
}

void Canbus_TxProcess_Write(uint32_t CAN_Address)
{
    // Setting CAN Frame
    frame.can_id = CAN_TX_ID | CAN_EFF_FLAG;
    frame.can_dlc = 4;    // Data Length
    frame.data[0] = CAN_Address;
    frame.data[1] = CAN_Address>>8;
    switch(CAN_Address){
        case CAN_CALIBRATION:
            frame.data[2] = 0x57;
            frame.data[3] = 0x4D;
        break;
        case CAN_CALI_STEP:
            frame.data[2] = 0x01;     // Low byte
            frame.data[3] = 0x00;     // High byte
        break;
        case CAN_WRITE_SVR:
            frame.data[2] = SVR_value & 0xFF;        // Low byte
            frame.data[3] = (SVR_value >> 8) & 0x0F; // High byte
        break;
        default:
        break;
    }

    int Retry_cnt = 0;
    while(write(can_socket, &frame, sizeof(frame)) != sizeof(frame)) {
        Retry_cnt++;
        usleep(10000); //delay 10ms
        if(Retry_cnt >= 10)
        {
            Cali_Status = FAIL; //CAN Write error
            break;
        }
    }
}

void Canbus_RxProcess(void)
{
    
}
