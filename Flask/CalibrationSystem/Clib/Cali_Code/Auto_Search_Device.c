/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Auto_Search_Device.h"

/* Parameter -----------------------------------------------*/
char Device_information[MAX_DEVICES][MAX_NAME_LENGTH];
int device_count = 0;

//--------------Meter 51101-8---------------//
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

void Store_Device_information(const char *device_name) {
    if (device_count < MAX_DEVICES) {
        strncpy(Device_information[device_count], device_name, MAX_NAME_LENGTH - 1);
        Device_information[device_count][MAX_NAME_LENGTH - 1] = '\0';
        printf("Stored Device[%d]: %s\n", device_count, Device_information[device_count]);
        device_count++;
    } else {
        printf("Device storage full! Cannot store more devices.\n");
    }
}

int send_packet(int fd, unsigned char *packet, int length) {
    int len = write(fd, packet, length);
    if (len < 0) {
        perror("Error writing to serial port");
        return -1;
    }
    return len;
}

int read_response(int fd, char *buffer, size_t size) {
    memset(buffer, 0, size);
    int len = read(fd, buffer, size - 1);
    if (len < 0) {
        perror("Error reading from serial port");
        return -1;
    }
    buffer[len] = '\0';
    return len;
}

void Read_Packet(int fd, unsigned char command, char param, int response_length)
{
    unsigned char packet[10];
    int crc_value;
    char response[256];
    int start_index = 0;
    int end_index = 0;

    packet[0] = 0xA5;
    packet[1] = command;

    switch(response_length)
    {
        case 0:
        {
            end_index = 1;
            crc_value = CRC(packet + start_index, packet + end_index);
            packet[end_index + 1] = crc_value & 0xff; // CRC low byte
            packet[end_index + 2] = (crc_value >> 8) & 0xff; // CRC high byte
            break;
        }

        case 1:
        {
            packet[2] = param;
            end_index = 2;
            crc_value = CRC(packet + start_index, packet + end_index);
            packet[end_index + 1] = crc_value & 0xff; // CRC low byte
            packet[end_index + 2] = (crc_value >> 8) & 0xff; // CRC high byte
            break;
        }

        default:
            break;
    }

    if (send_packet(fd, packet, end_index + 3) < 0) {
        return;
    }
    sleep(1);

    int recv_len = read_response(fd, response, sizeof(response));
    if (recv_len > 0) {
        printf("Received data: ");
        for (int i = 0; i < recv_len; i++) {
            printf("0x%02X, ", (unsigned char)response[i]);
        }
        printf("\n");
    }

    if((response[1] == 0x50) && (response[2] == 0xA5))
    {
        Store_Device_information("Chroma 51101-8");
    }
}
//--------------Meter 51101-8---------------//

// Determine USB port is usbtmc*
int Device_usbtmc(const char *device_path) {
    char write_buffer[] = SCPI_IDN;
    int fd = open(device_path, O_RDWR);
    if (fd < 0) {
        perror("Failed to open SCPI device");
        return 1;
    }

    // send IDN
    write(fd, write_buffer, strlen(write_buffer));
    printf("[Device information]\n");
    usleep(20000); // wait respone

    // read respone
    ssize_t bytes_read = read(fd, Device_information[device_count], MAX_NAME_LENGTH - 1);
    close(fd);

    if (bytes_read > 0) {
        Device_information[device_count][bytes_read] = '\0';
        printf("Response from device: %s\n", Device_information[device_count]);
        device_count++;
    }
    return 0;
}

// Determine USB port is ttyUSB*
int Device_ttyUSB(const char *device_path) {
    int fd = init_serial(device_path);
    if (fd == -1) {
        return 1;
    }

    printf("\nRead Existence:\n");
    Read_Packet(fd, GET_DEVICE_ADDRESS, 0x00, 0);

    close(fd);
    return 0;
}

void scan_usb_devices() {
    DIR *dir;
    struct dirent *entry;
    char device_path[256];

    // Scan /dev Table usbtmc and ttyUSB devices
    dir = opendir("/dev");
    if (!dir) {
        perror("Failed to open /dev directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "usbtmc", 6) == 0) {
            snprintf(device_path, sizeof(device_path), "/dev/%s", entry->d_name);
            printf("Device: %s\n", device_path);
            if (Device_usbtmc(device_path) == 0) {
                printf("Device: %s identified\n", device_path);
            } else {
                printf("Failed to communicate with: %s\n", device_path);
            }
        } else if (strncmp(entry->d_name, "ttyUSB", 6) == 0) {
            snprintf(device_path, sizeof(device_path), "/dev/%s", entry->d_name);
            printf("Device: %s\n", device_path);
            if (Device_ttyUSB(device_path) == 0) {
                printf("Device: %s identified\n", device_path);
            } else {
                printf("Failed to communicate with: %s\n", device_path);
            }
        }
    }
    closedir(dir);
}

const char* Get_Device_information(int index) {
    if (index >= 0 && index < device_count) {
        return Device_information[index];
    } else {
        return "Invalid Index";
    }
}

/*
int main() {
    printf("Starting USB device scan...\n");
    scan_usb_devices();
    printf("Scan complete.\n");
    Get_Device_information(0);
    Get_Device_information(1);
    return 0;
}
*/
