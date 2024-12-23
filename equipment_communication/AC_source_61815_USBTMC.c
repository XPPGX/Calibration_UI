#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define USBTMC_DEVICE "/dev/usbtmc0"  // USBTMC 設備路徑

// 發送指令到 USBTMC 設備
int send_command(int fd, const char *cmd) {
    int n = write(fd, cmd, strlen(cmd));
    if (n < 0) {
        perror("Failed to send command");
        return -1;
    }
    printf("Sent: %s\n", cmd);
    return 0;
}

// 讀取設備回應
int read_response(int fd, char *buffer, size_t length) {
    int n = read(fd, buffer, length - 1);
    if (n > 0) {
        buffer[n] = '\0';  // 確保字符串結尾
        printf("Response: %s\n", buffer);
    } else if (n == 0) {
        printf("No response received.\n");
    } else {
        perror("Failed to read response");
        return -1;
    }
    return 0;
}

int main() {
    int fd;
    char response[256];

    // 打開 USBTMC 設備
    fd = open(USBTMC_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Unable to open USBTMC device");
        return 1;
    }

    printf("Connected to USBTMC device: %s\n", USBTMC_DEVICE);

    // 1. Query device ID
    send_command(fd, "*IDN?\n");//can be executed
    usleep(20000);
    read_response(fd, response, sizeof(response));
    
    // 2. Query current frequency
    send_command(fd, "FREQ?\n");//can be executed
    usleep(20000);
    read_response(fd, response, sizeof(response));
    sleep(1);
    
    send_command(fd, "VOLT 230\n");//can be executed
    usleep(20000);
    
    send_command(fd, "FREQ 60\n");//can be executed
    usleep(20000);
    
    sleep(1);
    send_command(fd, "OUTP ON\n");//can be executed
    //~ send_command(fd, "OUTP OFF\n"); //can be executed
    usleep(20000);
    
    // 關閉 USBTMC 設備
    close(fd);
    printf("Disconnected from USBTMC device.\n");

    return 0;
}
