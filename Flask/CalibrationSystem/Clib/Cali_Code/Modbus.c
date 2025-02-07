/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Modbus.h"

/* Parameter -----------------------------------------------*/
int serial_fd;

void Modbus_Init(void);
void Modbus_TxProcess_Read(uint16_t StartAddress);
void Modbus_TxProcess_Write(uint16_t StartAddress);
int Modbus_RxProcess(uint8_t *response, size_t max_response_size);

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
    }
    
    if (wiringPiSetupGpio() < 0) { // BCM2835 IO number
        perror("WiringPi Setup GPIO error");
        serialClose(serial_fd);
    }

    pinMode(EN_485, OUTPUT);
    digitalWrite(EN_485, LOW); //Default receive mode
}

void Modbus_TxProcess_Read(uint16_t StartAddress)
{
    uint8_t polling_command[6] = {0};
    uint8_t full_polling_command[8] = {0}; // Add CRC

    polling_command[0] = MOD_TX_ID;
    polling_command[1] = 0x03;
    polling_command[2] = (StartAddress>>8) & 0xFF;
    polling_command[3] = StartAddress & 0xFF;

    if( (StartAddress == MOD_MODEL_NAME_B0B5)       || (StartAddress == MOD_MODEL_NAME_B6B11)
        || (StartAddress == MOD_AC_SOURCE_SET_B0B5) || (StartAddress == MOD_DC_SOURCE_SET_B0B5)
        || (StartAddress == MOD_DC_LOAD_SET_B0B5)   || (StartAddress == MOD_POWERMETER_SET_B0B5)
        || (StartAddress == MOD_METER_SET_B0B5) )
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
    full_polling_command[6] = crc & 0xFF;       // CRC 低位
    full_polling_command[7] = (crc >> 8) & 0xFF; // CRC 高位

    digitalWrite(EN_485, HIGH); // switch send mode
    usleep(1000); // wait 1ms
    for (int i = 0; i < sizeof(full_polling_command); i++) {
        serialPutchar(serial_fd, full_polling_command[i]);
    }
    usleep(1000); // wait 1ms
}

void Modbus_TxProcess_Write(uint16_t StartAddress)
{
    uint8_t polling_command[6] = {0};
    uint8_t full_polling_command[8] = {0}; // Add CRC

    polling_command[0] = MOD_TX_ID;
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
            polling_command[5] = 0x01;
        break;
        case MOD_WRITE_SVR:
            polling_command[4] = (SVR_value >> 8) & 0x0F;
            polling_command[5] = SVR_value & 0xFF;
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

int Modbus_RxProcess(uint8_t *response, size_t max_response_size)
{
    int bytes_read = 0;
    int timeout_485 = 5;
    uint16_t received_crc;
    uint16_t calculated_crc;

    tcflush(serial_fd, TCIFLUSH); //clear receive buffer

    digitalWrite(EN_485, LOW); // receive mode
    // receive data
    usleep(5000); // wait device response
    while (timeout_485 > 0) {
        if (serialDataAvail(serial_fd)) {
            response[bytes_read++] = serialGetchar(serial_fd);
            if (bytes_read >= max_response_size) break;
        } else {
            usleep(1000);
            timeout_485--;
        }
    }

    if (bytes_read > 2) { // CRC 2 bytes
        received_crc = (response[bytes_read - 1] << 8) | response[bytes_read - 2];
        calculated_crc = calculateCRC(response, bytes_read - 2);

        for (int i = 0; i < bytes_read; i++) {
            printf("0x%02X ", response[i]);
        }
        printf("\n");

        if ((received_crc == calculated_crc) && (response[1] == 0x03)) {
            return bytes_read;
        } else {
            printf("CRC error: received=0x%04X, calculated=0x%04X\n", received_crc, calculated_crc);
            return -2; // CRC error
        }
    }
}
