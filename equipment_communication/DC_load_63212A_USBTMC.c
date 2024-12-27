#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define USBTMC_DEVICE "/dev/usbtmc0"  // USBTMC 設備檔案

// 發送 SCPI 指令到設備
int send_command(int fd, const char *cmd) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s\n", cmd); // 確保指令以 '\n' 結尾
    int n = write(fd, buffer, strlen(buffer));
    if (n < 0) {
        perror("Failed to send command");
        return -1;
    }
    printf("Sent: %s", cmd);
    return 0;
}

// 讀取設備回應
int read_response(int fd, char *response, size_t length) {
    int n = read(fd, response, length - 1);
    if (n > 0) {
        response[n] = '\0';
        printf("Response: %s\n", response);
    } else if (n == 0) {
        printf("No response.\n");
    } else {
        perror("Failed to read response");
        return -1;
    }
    return 0;
}

int main() {
    //Note that the parallel mode should be closed before using USBTMC
    int fd;
    char response[256];
    
    // 打開 USBTMC 設備
    fd = open(USBTMC_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Unable to open USBTMC device");
        return 1;
    }

    printf("Connected to USBTMC device: %s\n", USBTMC_DEVICE);

    // 1. 查詢設備識別資訊
    send_command(fd, "*IDN?"); //can be executed
    usleep(20000);
    read_response(fd, response, sizeof(response));

    sleep(1);
    // 2. 設定為 CC 模式（恆定電流模式）
    send_command(fd, "CONF:PARA:MODE 0"); //can be executed
    //~ sleep(1);
    //~ send_command(fd, "CONF:PARA:INIT 0");
    sleep(1);
    send_command(fd, "CONF:PARA:INIT?"); //can be executed
    //~ send_command(fd, "SYST:ERR?");
    //~ send_command(fd, "CONF:PARA:Init 0");
    //~ send_command(fd, "CONF:PARA:MODE?");
    usleep(20000);
    read_response(fd, response, sizeof(response));
    printf("response : %s\n", response);
    sleep(1);
    
    send_command(fd, "MODE CCH"); //can be exxcuted
    sleep(1);
    //~ send_command(fd, "SYST:LOC");
    //~ sleep(1);
    
    //~ send_command(fd, "SYST:ERR?");
    //~ usleep(20000);
    //~ read_response(fd, response, sizeof(response));
    //~ printf("response : %s\n", response);
    
    //~ // 3. 設定電流，例如 2.5A
    send_command(fd, "CURR:STAT:L2 1A"); //can be executed
    usleep(20000);
    //~ // 4. 啟用輸出
    send_command(fd, "LOAD OFF"); //can be executed
    usleep(20000);
    //~ // 5. 讀取當前電流
    //~ send_command(fd, "MEAS:CURR?");
    //~ usleep(20000);
    //~ read_response(fd, response, sizeof(response));

    // 6. 關閉輸出
    //~ send_command(fd, "OUTPUT OFF");

    // 關閉設備
    close(fd);
    printf("Disconnected from USBTMC device.\n");

    return 0;
}
