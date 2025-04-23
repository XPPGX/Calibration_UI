/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Modbus.h"

/* Parameter -----------------------------------------------*/
int serial_fd;

uint8_t Mod_no_receive_cnt = 0;
uint8_t Mod_receive_data_error_cnt = 0;

uint16_t calculateCRC(uint8_t *data, int length);
void Modbus_Init(void);
void Modbus_TxProcess_Read(uint32_t MOD_ID, uint16_t StartAddress);
void Modbus_TxProcess_Write(uint32_t MOD_ID, uint16_t StartAddress);
void Modbus_RxProcess(uint16_t StartAddress);

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

void Modbus_Init(void)
{
    // Init RS485 Port
    if ((serial_fd = serialOpen("/dev/ttyS0", 115200)) < 0) { // if use /dev/ttyS0
        perror("RS485 Initialization error");
        Cali_Result = DUT_FAIL;
    }
    
    if (wiringPiSetupGpio() < 0) { // BCM2835 IO number
        perror("WiringPi Setup GPIO error");
        Cali_Result = DUT_FAIL;
        serialClose(serial_fd);
    }

    pinMode(EN_485, OUTPUT);
    digitalWrite(EN_485, LOW); //Default receive mode
}

void Modbus_TxProcess_Read(uint32_t MOD_ID, uint16_t StartAddress)
{
    uint8_t polling_command[6] = {0};
    uint8_t full_polling_command[8] = {0}; // Add CRC

    polling_command[0] = MOD_ID;
    polling_command[1] = 0x03;
    polling_command[2] = (StartAddress>>8) & 0xFF;
    polling_command[3] = StartAddress & 0xFF;

    if( (StartAddress == MOD_MODEL_NAME_B0B5)       || (StartAddress == MOD_MODEL_NAME_B6B11)
        || (StartAddress == MOD_AC_SOURCE_SET_B0B5) || (StartAddress == MOD_DC_SOURCE_SET_B0B5)
        || (StartAddress == MOD_DC_LOAD_SET_B0B5)   || (StartAddress == MOD_POWERMETER_SET_B0B5)
        || (StartAddress == MOD_METER_SET_B0B5)     || (StartAddress == MOD_SCALING_FACTOR) )
    {
        polling_command[4] = 0x00;
        polling_command[5] = 0x03;
    }
    else
    {
        polling_command[4] = 0x00;
        polling_command[5] = 0x01;
    }

    // Calculate CRC
    memcpy(full_polling_command, polling_command, sizeof(polling_command));
    uint16_t crc = calculateCRC(polling_command, sizeof(polling_command));
    full_polling_command[6] = crc & 0xFF;          // CRC 低位
    full_polling_command[7] = (crc >> 8) & 0xFF;   // CRC 高位

    digitalWrite(EN_485, HIGH); // switch send mode
    usleep(1000); // wait 1ms
    for (int i = 0; i < sizeof(full_polling_command); i++) {
        serialPutchar(serial_fd, full_polling_command[i]);
    }
    usleep(1000); // wait 1ms
}

void Modbus_TxProcess_Write(uint32_t MOD_ID, uint16_t StartAddress)
{
    uint8_t polling_command[6] = {0};
    uint8_t full_polling_command[8] = {0}; // Add CRC

    polling_command[0] = MOD_ID;
    polling_command[1] = 0x06;
    polling_command[2] = (StartAddress>>8) & 0xFF;
    polling_command[3] = StartAddress & 0xFF;

     switch(StartAddress){
        case MOD_CALIBRATION_KEY:
            polling_command[4] = 0x4D;
            polling_command[5] = 0x57;
        break;
        case MOD_CALI_STATUS:
            polling_command[4] = 0x00;
            polling_command[5] = wCali_Status;
        break;
        case MOD_WRITE_SVR:
            polling_command[4] = (SVR_value >> 8) & 0x0F;
            polling_command[5] = SVR_value & 0xFF;
        break;
        case MOD_CALIBRATION_POINT:
            polling_command[4] = (UI_Calibration_Point >> 8) & 0xFF;    // High byte
            polling_command[5] = UI_Calibration_Point & 0xFF;           // Low byte
        break;
        case MOD_CALI_RESULT:
            polling_command[4] = 0xFF;
            polling_command[5] = 0xFF;
        break;
        case MOD_OPERATION:
            polling_command[4] = 0x00;
            polling_command[5] = DCS_Remote_Switch & 0xFF;
        break;
        case MOD_SYSTEM_CONFIG:
            polling_command[4] = 0x00;       // High byte
            polling_command[5] = 0x03;       // Low byte(CAN control)
        break;
        case MOD_VOUT_SET:
            polling_command[4] = (DCS_Volt_Set_Value >> 8) & 0xFF;       // High byte
            polling_command[5] = DCS_Volt_Set_Value & 0xFF;              // Low byte
        break;
        case MOD_IOUT_SET:
            polling_command[4] = (DCS_Curr_Set_Value >> 8) & 0xFF;       // High byte
            polling_command[5] = DCS_Curr_Set_Value & 0xFF;              // Low byte
        break;
        case MOD_CALI_DEFAULT_SET:
            polling_command[4] = (Cali_Default_Value >> 8) & 0xFF;       // High byte
            polling_command[5] = Cali_Default_Value & 0xFF;              // Low byte
        break;
        default:
        break;
    }

    // Calculate CRC
    memcpy(full_polling_command, polling_command, sizeof(polling_command));
    uint16_t crc = calculateCRC(polling_command, sizeof(polling_command));
    full_polling_command[6] = crc & 0xFF;       // CRC 低位
    full_polling_command[7] = (crc >> 8) & 0xFF; // CRC 高位

    digitalWrite(EN_485, HIGH); // switch send mode
    usleep(1000); // wait 1ms
    for (int i = 0; i < sizeof(full_polling_command); i++) {
        serialPutchar(serial_fd, full_polling_command[i]);
    }
    usleep(1000); // wait 1ms
}

void Modbus_RxProcess(uint16_t StartAddress)
{
    uint8_t response_data[256] = {0};
    int Mod_read_bytes = 0;
    int timeout_485 = 5;
    uint16_t received_crc;
    uint16_t calculated_crc;

    tcflush(serial_fd, TCIFLUSH); //clear receive buffer

    digitalWrite(EN_485, LOW); // receive mode
    // receive data
    usleep(20000); // wait device response
    while (timeout_485 > 0) {
        if (serialDataAvail(serial_fd)) {
            response_data[Mod_read_bytes++] = serialGetchar(serial_fd);
            if (Mod_read_bytes >= MAX_STRING_LENGTH) break;
        } else {
            usleep(1000);
            timeout_485--;
        }
    }

    if (Mod_read_bytes > 2) { // CRC 2 bytes
        received_crc = (response_data[Mod_read_bytes - 1] << 8) | response_data[Mod_read_bytes - 2];
        calculated_crc = calculateCRC(response_data, Mod_read_bytes - 2);

        for (int i = 0; i < Mod_read_bytes; i++) {
            printf("0x%02X ", response_data[i]);
        }
        printf("\n");

        if ((received_crc == calculated_crc) && (response_data[1] == 0x03)) {
            Mod_receive_data_error_cnt = 0;
            Mod_no_receive_cnt = 0;

            switch (StartAddress)
            {
            case MOD_MODEL_NAME_B0B5:
                Modbus_ask_name = 1;
                memcpy(machine_type, &response_data[3], 6);
                machine_type[6] = '\0';
                break;
            
            case MOD_MODEL_NAME_B6B11:
                Modbus_ask_name = 2;
                memcpy(&machine_type[6], &response_data[3], 6);
                machine_type[12] = '\0';
                break;
                
            case MOD_READ_PSU_MODE:
                if(response_data[4] == 0x01){
                    Manual_Cali_step = READ_PSU_SCALING_FACTOR;
                }
                else{
                    Manual_Cali_step = SEND_KEY_READ_MODE;
                }
                break;
            
            case MOD_SCALING_FACTOR:
                PSU_ACV_Factor_Comm = (response_data[4] & 0xF);
                PSU_ACI_Factor_Comm = (response_data[6] & 0xF);
                PSU_DCV_Factor_Comm = (response_data[3] & 0xF);
                PSU_DCI_Factor_Comm = ((response_data[3]>>4) & 0xF);
                PSU_ACV_Factor = Scaling_Factor_Convert(PSU_ACV_Factor_Comm);
                PSU_ACI_Factor = Scaling_Factor_Convert(PSU_ACI_Factor_Comm);
                PSU_DCV_Factor = Scaling_Factor_Convert(PSU_DCV_Factor_Comm);
                PSU_DCI_Factor = Scaling_Factor_Convert(PSU_DCI_Factor_Comm);
                Manual_Cali_step = UI_RESET_CALI;
                break;
            
            case MOD_CALI_STATUS:
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
                    printf("校正點不匹配\n");
                    Cali_Result = CALI_POINT_FAIL;
                    break;
                
                case CALI_POINT_SETTING:    //0x0003
                    Modbus_TxProcess_Write(MOD_TX_ID_DUT, MOD_CALIBRATION_POINT);
                    usleep(20000); // Delay 20ms
                    break;
                
                case CALI_FINISH:           //0xFFFF
                    Manual_Cali_step = READ_CALI_RESULT;
                    break;
                
                default:
                    break;
                }
                break;

            case MOD_CALI_RESULT:
                if(response_data[4] == 0x01)
                {
                    Cali_Result = PASS;
                }
                else if(response_data[4] == 0x02)
                {
                    Cali_Result = NOT_COMPLETE;
                }
                break;

            case MOD_SVR_POLLING:
                Cali_type_polling = response_data[4];
                Cali_read_step++;
                break;

            case MOD_READ_HIGH_LIMIT:
                PSU_High_Limit_Comm = ((response_data[3] << 8) | response_data[4]);
                PSU_High_Limit_uComm = ((response_data[3] << 8) | response_data[4]);
                if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                {
                    PSU_High_Limit = PSU_High_Limit_uComm * PSU_ACV_Factor;
                } 
                else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                {
                    PSU_High_Limit = PSU_High_Limit_Comm * PSU_ACI_Factor;
                } 
                else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                {
                    PSU_High_Limit = PSU_High_Limit_uComm * PSU_DCV_Factor;
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

            case MOD_READ_LOW_LIMIT:
                PSU_Low_Limit_Comm = ((response_data[3] << 8) | response_data[4]);
                PSU_Low_Limit_uComm = ((response_data[3] << 8) | response_data[4]);
                if (strcmp(UI_Scaling_Factor, "ACV") == 0) 
                {
                    PSU_Low_Limit = PSU_Low_Limit_uComm * PSU_ACV_Factor;
                } 
                else if (strcmp(UI_Scaling_Factor, "ACI") == 0) 
                {
                    PSU_Low_Limit = PSU_Low_Limit_Comm * PSU_ACI_Factor;
                } 
                else if (strcmp(UI_Scaling_Factor, "DCV") == 0) 
                {
                    PSU_Low_Limit = PSU_Low_Limit_uComm * PSU_DCV_Factor;
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

            case MOD_AC_SOURCE_SET_B0B5:
                Cali_read_step++;
                break;

            case MOD_DC_SOURCE_SET_B0B5:
                Cali_read_step++;
                break;

            case MOD_DC_LOAD_SET_B0B5:
                Cali_read_step++;
                break;

            default:
                break;
            }
        } else {
            Mod_receive_data_error_cnt++;
            if(Mod_receive_data_error_cnt >= 50)
            {
                Cali_Result = FAIL;         //CRC or Receive Data ERROR
            }
            printf("CRC error: received=0x%04X, calculated=0x%04X\n", received_crc, calculated_crc);
        }
    }
    else{
        Mod_no_receive_cnt++;
        if(Mod_no_receive_cnt >= 50)
        {
            Cali_Result = FAIL;
        }
    }
}
