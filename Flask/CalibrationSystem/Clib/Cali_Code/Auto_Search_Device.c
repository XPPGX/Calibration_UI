/* Includes ------------------------------------------------------------------*/
#include "Cali.h"
#include "Auto_Search_Device.h"
#include "Chroma_meter_51101_8.h"

/* Parameter -----------------------------------------------*/
char Device_information[MAX_DEVICES][MAX_NAME_LENGTH];
int device_count = 0;
Device Device_USB_Port[MAX_DEVICES];
int device_usb_port_count = 0;

/* function -----------------------------------------------*/
void Store_Device_information(const char *device_path, const char *device_name);
int SCPI_Read_Process(const char *device_path, const char *command);
int Chroma_51101_Read_Process(const char *device_path, unsigned char command);
void scan_usb_devices(void);
const char* Get_Device_information(int index);
int Device_Open_Init(const char *device_path);
void Device_Close(void);

int Device_Open_Init(const char *device_path) {
    int fd;

    if (strncmp(device_path, "usbtmc", 6) == 0)
    {
        fd = open(device_path, O_RDWR);
        Device_USB_Port[device_usb_port_count].fd = fd; 
        strncpy(Device_USB_Port[device_usb_port_count].device_path, device_path, MAX_NAME_LENGTH - 1);
        Device_USB_Port[device_usb_port_count].device_path[MAX_NAME_LENGTH - 1] = '\0';
        device_usb_port_count++;
        if (fd < 0) {
            perror("Failed to open SCPI device");
            return -1;
        }
    }
    else if (strncmp(device_path, "ttyUSB", 6) == 0)
    {
        fd = init_serial(device_path);
        Device_USB_Port[device_usb_port_count].fd = fd; 
        strncpy(Device_USB_Port[device_usb_port_count].device_path, device_path, MAX_NAME_LENGTH - 1);
        Device_USB_Port[device_usb_port_count].device_path[MAX_NAME_LENGTH - 1] = '\0';
        device_usb_port_count++;
        if (fd < 0) {
            perror("Failed to open SCPI device");
            return -1;
        }
    }
    else{
        return -1;
    }
    return 0;
}

void Device_Close(void) {
    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        close(Device_USB_Port[device_usb_port_count].fd);
    }
}

void Store_Device_information(const char *device_path, const char *device_name) {
    char temp[MAX_NAME_LENGTH];
    if (device_count < MAX_DEVICES) {
        snprintf(temp, sizeof(temp), "%s %s", device_path, device_name);
        strncpy(Device_information[device_count], temp, MAX_NAME_LENGTH - 1);
        Device_information[device_count][MAX_NAME_LENGTH - 1] = '\0';

        printf("Stored Device[%d]: %s\n", device_count, Device_information[device_count]);
        device_count++;
    } else {
        printf("Device storage full! Cannot store more devices.\n");
    }
}

// SCPI read data process
int SCPI_Read_Process(const char *device_path, const char *command) {
    char write_buffer[MAX_NAME_LENGTH];
    char read_buffer[MAX_NAME_LENGTH];
    char temp[MAX_NAME_LENGTH];
    int receive_len = 0;
    int fd;

    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        if (strcmp(device_path, Device_USB_Port[i].device_path) == 0)
        {
            fd = Device_USB_Port[i].fd;
            break;
        }
    }

    snprintf(write_buffer, sizeof(write_buffer), "%s", command);
    // send IDN
    if (write(fd, write_buffer, strlen(write_buffer)) < 0) {
        return -1;
    }
    usleep(20000); // wait respone

    // read respone
    memset(read_buffer, 0, sizeof(read_buffer));
    receive_len = read(fd, read_buffer, MAX_NAME_LENGTH - 1);

    if (receive_len > 0) {
        read_buffer[receive_len] = '\0';

        if(strcmp(command, SCPI_IDN) == 0) //Determine USB port is usbtmc*
        {
            snprintf(temp, sizeof(temp), "%s %s", device_path, read_buffer);
            strncpy(Device_information[device_count], temp, MAX_NAME_LENGTH - 1);
            Device_information[device_count][MAX_NAME_LENGTH - 1] = '\0';
            printf("Response from device: %s\n", Device_information[device_count]);
            device_count++;
        }
    }
    else if (receive_len < 0) {
        perror("Error reading from SCPI device");
        return -1;
    }

    return 0;
}

// Determine USB port is ttyUSB*
int Chroma_51101_Read_Process(const char *device_path, unsigned char command) {
    char Receive_respone[MAX_NAME_LENGTH];
    int receive_datalen;
    char Device_name[MAX_NAME_LENGTH];
    int fd;

    for (uint8_t i = 0; i < device_usb_port_count; i++)
    {
        if (strcmp(device_path, Device_USB_Port[i].device_path) == 0)
        {
            fd = Device_USB_Port[i].fd;
            break;
        }
    }

    printf("\nRead Existence:\n");
    receive_datalen = Read_Packet(fd, command, 0x00, 0 ,Receive_respone);
    if (receive_datalen > 0) {
        if((Receive_respone[1] == 0x50) && (Receive_respone[2] == 0xA5))
        {
            strncpy(Device_name, "Chroma 51101-8", MAX_NAME_LENGTH - 1);
            Device_name[MAX_NAME_LENGTH - 1] = '\0';
            Store_Device_information(device_path, Device_name);
        }
    }
    else if (receive_datalen < 0)
    {
        return -1;
    }

    return 0;
}

void scan_usb_devices(void) {
    DIR *dir;
    struct dirent *entry;
    char device_path[MAX_NAME_LENGTH];

    memset(Device_information, 0, sizeof(Device_information));
    
    // Scan /dev Table usbtmc and ttyUSB devices
    dir = opendir("/dev");
    if (!dir) {
        perror("Failed to open /dev directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "usbtmc", 6) == 0 || strncmp(entry->d_name, "ttyUSB", 6) == 0) {
            snprintf(device_path, sizeof(device_path), "/dev/%s", entry->d_name);
            printf("Device: %s\n", device_path);

            if (Device_Open_Init(device_path) == 0) {
                if ((strncmp(entry->d_name, "usbtmc", 6) == 0 && SCPI_Read_Process(device_path, SCPI_IDN) == 0) ||
                    (strncmp(entry->d_name, "ttyUSB", 6) == 0 && Chroma_51101_Read_Process(device_path, GET_DEVICE_ADDRESS) == 0)) 
                {
                    printf("Device: %s identified\n", device_path);
                } else {
                    printf("Failed to communicate with: %s\n", device_path);
                }
            } else {
                printf("Failed to open: %s\n", device_path);
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
