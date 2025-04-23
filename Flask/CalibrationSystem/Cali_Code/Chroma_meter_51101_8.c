/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Auto_Search_Device.h"
#include "Chroma_meter_51101_8.h"
#include "Modbus.h"

uint8_t CRC_error_cnt = 0;
/* function -----------------------------------------------*/
int init_serial(const char *portname);
int Read_Packet(int fd, unsigned char command, uint8_t param, uint8_t response_length, char *response);
int Write_Packet(int fd, unsigned char command, uint8_t param1, uint8_t param2, uint8_t response_length);

int init_serial(const char *portname) {
    struct termios options;
    int fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);

    if (fd == -1) {
        perror("Error opening serial port");
        return -1;
    }
    //Get port setting
    if (tcgetattr(fd, &options) != 0) {
        perror("Error getting serial attributes");
        close(fd);
        return -1;
    }
    //Baudrate：115200
    if (cfsetispeed(&options, B115200) < 0 || cfsetospeed(&options, B115200) < 0) {
        perror("Error setting baud rate");
        close(fd);
        return -1;
    }

    //c_cflag：control flags
    //PARENB：Parity Enable、CSTOPB：stop 2-bit、CSIZE：character size
    //CS8：character 8-bit、CLOCAL：Local Mode、CREAD：Enable Read
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    options.c_cflag |= (CS8 | CLOCAL | CREAD);

    //c_lflag：Local flags
    //ICANON：Canonical Mode、ECHO：Echo Input、ECHOE：Echo Erase、ISIG：Signal Characters
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    //c_oflag：Output Flags
    //OPOST：Enable Output Processing、ONLCR：Convert \n to \r\n、OCRNL：Convert \r to \n
    options.c_oflag &= ~(OPOST | ONLCR | OCRNL);

    //c_iflag：Input Flags
    //IXON：Enable start and stop output control、IXOFF：Enable start and stop input control
    //IXANY：Enable any Characters restart output
    //INLCR：Convert \n to \r、ICRNL：Convert \r to \n、IGNCR：Ignore \r
    options.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL | IGNCR);

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("Error configuring serial port");
        close(fd);
        return -1;
    }

    tcflush(fd, TCIFLUSH);

    return fd;
}
/*
unsigned int CRC(unsigned char *start_of_packet, unsigned char *end_of_packet) {
    unsigned int crc = 0xffff;
    unsigned char bit_count;
    unsigned char *char_ptr = start_of_packet;

    do {
        crc ^= (unsigned int)*char_ptr;
        bit_count = 0;
        do {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        } while (bit_count++ < 7);
    } while (char_ptr++ < end_of_packet);

    return crc;
}
*/

int Read_Packet(int fd, unsigned char command, uint8_t param, uint8_t response_length, char *response)
{
    unsigned char packet[10];
    int crc_value;
    int receive_len = 0;
    uint16_t received_crc;
    uint16_t calculated_crc;

    packet[0] = 0xA5;
    packet[1] = command;

    switch(response_length)
    {
        case 0:
        {
            break;
        }
        case 1:
        {
            packet[2] = param;
            break;
        }
        default:
            break;
    }
    crc_value = calculateCRC(packet, (response_length + 2));
    packet[response_length + 2] = crc_value & 0xff;             // CRC low byte
    packet[response_length + 3] = (crc_value >> 8) & 0xff;      // CRC high byte

    if (write(fd, packet, response_length + 4) < 0) {
        perror("Error writing from serial port");
        return -1;
    }
    usleep(20000);

    memset(response, 0, MAX_NAME_LENGTH);
    receive_len = read(fd, response, MAX_NAME_LENGTH - 1);
    if(receive_len > 2)
    {
        received_crc = (response[receive_len - 1] << 8) | response[receive_len - 2];
        calculated_crc = calculateCRC(response, receive_len - 2);
        response[receive_len] = '\0';

        // printf("Received data: ");
        // for (int i = 0; i < receive_len; i++) {
        //     printf("0x%02X, ", (unsigned char)response[i]);
        // }
        // printf("\n");
        if(received_crc == calculated_crc)
        {
            return receive_len;
        }
        else
        {
            printf("CRC ERROR\n");
            return -1;
        }
    }
    else if (receive_len < 0) {
        perror("Error reading from serial port");
        return -1;
    }
    else{
        return -1;
    }
}

int Write_Packet(int fd, unsigned char command, uint8_t param1, uint8_t param2, uint8_t response_length)
{
    unsigned char packet[10];
    int crc_value;
    int receive_len = 0;
    char response[256];
    uint16_t received_crc;
    uint16_t calculated_crc;

    packet[0] = 0xA5;
    packet[1] = (command | 0x80);

    switch(response_length)
    {
        case 1:
        {
            packet[2] = param1;
            break;
        }
        case 2:
        {
            packet[2] = param1;
            packet[3] = param2;
            break;
        }
        default:
            break;
    }
    crc_value = calculateCRC(packet, (response_length + 2));
    packet[response_length + 2] = crc_value & 0xff;             // CRC low byte
    packet[response_length + 3] = (crc_value >> 8) & 0xff;      // CRC high byte

    if (write(fd, packet, response_length + 4) < 0) {
        perror("Error writing from serial port");
        return -1;
    }
    usleep(20000);

    memset(response, 0, MAX_NAME_LENGTH);
    receive_len = read(fd, response, MAX_NAME_LENGTH - 1);
    if(receive_len > 2)
    {
        received_crc = (response[receive_len - 1] << 8) | response[receive_len - 2];
        calculated_crc = calculateCRC(response, receive_len - 2);
        response[receive_len] = '\0';

        // printf("Received data: ");
        // for (int i = 0; i < receive_len; i++) {
        //     printf("0x%02X, ", (unsigned char)response[i]);
        // }
        // printf("\n");
        if(received_crc == calculated_crc)
        {
            CRC_error_cnt = 0;
            return receive_len;
        }
        else
        {
            CRC_error_cnt++;
            if(CRC_error_cnt > 5)
            {
                printf("CRC ERROR\n");
                return -1;
            }
            else
            {
                return 0;
            }
        }
    }
    else if (receive_len < 0) {
        perror("Error reading from serial port");
        return -1;
    }
    else{
        return -1;
    }
}
