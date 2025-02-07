/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Auto_Search_Device.h"
#include "Chroma_meter_51101_8.h"

/* function -----------------------------------------------*/
int init_serial(const char *portname);
int Read_Packet(int fd, unsigned char command, char param, int response_length, char *response);
void Write_Packet(int fd, unsigned char command, char param1, char param2, int response_length);

int init_serial(const char *portname) {
    int fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("Error opening serial port");
        return -1;
    }
    struct termios options;
    tcgetattr(fd, &options);

    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    options.c_cflag |= CS8 | CLOCAL | CREAD;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;

    tcflush(fd, TCIFLUSH);
    fcntl(fd, F_SETFL, FNDELAY);

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("Error configuring serial port");
        close(fd);
        return -1;
    }

    return fd;
}

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

int Read_Packet(int fd, unsigned char command, char param, int response_length, char *response)
{
    unsigned char packet[10];
    int crc_value;
    int start_index = 0;
    int end_index = 0;
    int receive_len = 0;

    packet[0] = 0xA5;
    packet[1] = command;

    switch(response_length)
    {
        case 0:
        {
            end_index = 1;
            break;
        }
        case 1:
        {
            packet[2] = param;
            end_index = 2;
            break;
        }
        default:
            break;
    }
    crc_value = CRC(packet + start_index, packet + end_index);
    packet[end_index + 1] = crc_value & 0xff; // CRC low byte
    packet[end_index + 2] = (crc_value >> 8) & 0xff; // CRC high byte

    if (write(fd, packet, end_index + 3) < 0) {
        perror("Error reading from serial port");
        return -1;
    }
    usleep(20000);

    memset(response, 0, MAX_NAME_LENGTH);
    receive_len = read(fd, response, MAX_NAME_LENGTH - 1);
    if (receive_len < 0) {
        perror("Error reading from serial port");
        return -1;
    }
    response[receive_len] = '\0';

    printf("Received data: ");
    for (int i = 0; i < receive_len; i++) {
        printf("0x%02X, ", (unsigned char)response[i]);
    }
    printf("\n");

    return receive_len;
}

void Write_Packet(int fd, unsigned char command, char param1, char param2, int response_length)
{
    unsigned char packet[10];
    int crc_value;
    char response[256];
    int start_index = 0;
    int end_index = 0;

    packet[0] = 0xA5;
    packet[1] = (command | 0x80);

    switch(response_length)
    {
        case 1:
        {
            packet[2] = param1;
            end_index = 2;
            break;
        }
        case 2:
        {
            packet[2] = param1;
            packet[3] = param2;
            end_index = 3;
            break;
        }
        default:
            break;
    }

    crc_value = CRC(packet + start_index, packet + end_index);
    packet[end_index + 1] = crc_value & 0xff; // CRC low byte
    packet[end_index + 2] = (crc_value >> 8) & 0xff; // CRC high byte

    if (write(fd, packet, end_index + 3) < 0) {
        perror("Error reading from serial port");
        return;
    }
    sleep(1);

    memset(response, 0, sizeof(response));
    int recv_len = read(fd, response, sizeof(response) - 1);
    if (recv_len < 0) {
        perror("Error reading from serial port");
        return;
    }
    response[recv_len] = '\0';

    printf("Received data: ");
    for (int i = 0; i < recv_len; i++) {
        printf("0x%02X, ", (unsigned char)response[i]);
    }
    printf("\n");
}
