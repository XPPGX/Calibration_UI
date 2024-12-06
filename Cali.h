#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <termios.h>

#ifndef GUI_GTK_LIB
#define GUI_GTK_LIB
#include <gtk/gtk.h>
#endif

#define EN_485 4  // 根據實際 RS485 Enable 腳位設置
#define CANBUS 1
#define MODBUS 2
#define PMBUS 3

#define LED_PIN 17

#define TX_ID 0x000C0300
#define RX_ID 0x000C0200

//參數
int s;
struct sockaddr_can addr;
struct ifreq ifr;
struct can_frame frame;

volatile int communication_found = 0; // 0: 未確定, 1: CAN, 2: MODBUS

pthread_mutex_t lock; // 保護共享變數

pthread_mutex_t button_lock;    //used to lock flow_control_thread at the beginning
pthread_cond_t button_cond;     //used to inform flow_control thread to start the calibration process

char adjustment_status; // 0 means coarse adjustment, 1 means fine adjustment.

int space_pressed; //1:Coarse 0:Fine

struct Label_UpdateInfo{
    GtkWidget *target_label;
    char *text;
};

void flow_control_task();