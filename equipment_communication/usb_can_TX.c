#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <libusb-1.0/libusb.h>
#include <pthread.h>


#define VENDOR_ID 0x04d8
#define PRODUCT_ID 0x0053
#define TX_ID 0x000C0300 //RS485 CAN HAT
#define RX_ID 0x000C0200 //USB port


void* receive_can_data(void* arg){
	libusb_device_handle *dev_handle;
	libusb_context *ctx = NULL;
	unsigned char buffer[64];
	int transferred;
	int endpoint_in = 0x81;
	int timeout = 1000;
	
	if(libusb_init(&ctx) < 0){
		fprintf(stderr, "Failed to initialized libusb\n");
		return NULL;
	}
	
	dev_handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
	if(!dev_handle){
		fprintf(stderr, "Failed to open CANalyst-II device\n");
		libusb_exit(ctx);
		return NULL;
	}
	
	while(1){
		int ret = libusb_bulk_transfer(dev_handle, endpoint_in, buffer, sizeof(buffer), &transferred, timeout);
		usleep(20000);
		if(ret == 0 && transferred > 0){
			printf("Received CAN frame: ");
			for(int i = 0 ; i < transferred ; i ++){
				printf("%02x ", buffer[i]);
				
				if(buffer[i] > 0){return NULL;}
			}
			printf("\n");
		}
		else{
			fprintf(stderr, "Failed to read CAN frame: %s\n", libusb_error_name(ret));
		}
	}
	libusb_close(dev_handle);
	libusb_exit(ctx);
	return NULL;
	
}

int main(){
	system("sudo ifconfig can0 down");
    usleep(10000);
    system("sudo ip link set can0 type can bitrate 250000");
    system("sudo ifconfig can0 up");
    
	int sock;
	struct sockaddr_can addr;
	struct ifreq ifr;
	
	if((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0 ){
		perror("Socket");
		return 1;
	}
	
	strcpy(ifr.ifr_name, "can0");
	ioctl(sock, SIOCGIFINDEX, &ifr);
	
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	
	if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0){
		perror("Bind");
		return 1;
	}
	
	
	
	struct can_frame frame;
	frame.can_id = TX_ID;
	frame.can_dlc = 8;
	frame.data[0] = 0x11;
	frame.data[1] = 0x22;
	frame.data[2] = 0x33;
	frame.data[3] = 0x44;
	frame.data[4] = 0x55;
	frame.data[5] = 0x66;
	frame.data[6] = 0x77;
	frame.data[7] = 0x88;
	
	//~ int Retry_cnt = 0;
	//~ while(write(sock, &frame, sizeof(frame)) != sizeof(frame)) {
        //~ Retry_cnt++;
        //~ perror("CAN Write error");
        //~ usleep(10000); //delay 10ms
        //~ if(Retry_cnt >= 10)
        //~ {
            //~ printf("CAN frame sent fail\n");
            //~ break;
        //~ }
    //~ }
	if(write(sock, &frame, sizeof(frame)) != sizeof(frame)){
		perror("Write");
		return 1;
	}
	
	pthread_t receive_thread;
	if(pthread_create(&receive_thread, NULL, receive_can_data, NULL) != 0){
		perror("Failed to create receive thread");
		return 1;
	}
	
	pthread_join(receive_thread, NULL);
	close(sock);
	
	return 0;
}
