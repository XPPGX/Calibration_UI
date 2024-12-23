#include <stdio.h>
#include <libusb-1.0/libusb.h>

#define VENDOR_ID 0x04d8
#define PRODUCT_ID 0x0053

int main(){
	libusb_device_handle *dev_handle;
	libusb_context *ctx = NULL;
	
	if(libusb_init(NULL) < 0){
		fprintf(stderr, "Failed to initialize libusb\n");
		return 1;
	}
	
	dev_handle = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
	if(!dev_handle){
		fprintf(stderr, "Failed to open CANalyst-II device\n");
		libusb_exit(ctx);
		return 1;
	}
	
	unsigned char buffer[64];
	int transferred;
	int endpoint_in = 0x81;
	int timeout = 1000;
	
	int ret = libusb_bulk_transfer(dev_handle, endpoint_in, buffer, sizeof(buffer), &transferred, timeout);
	
	if(ret == 0 && transferred > 0){
		printf("Received CAN frame");
		for(int i = 0 ; i < transferred ; i ++){
			printf("%02X ", buffer[i]);
		}
		printf("\n");
	}
	else{
		fprintf(stderr, "Failed to read CAN frame: %s\n", libusb_error_name(ret));
	}
	
	libusb_close(dev_handle);
	libusb_exit(ctx);
	
	return 0;
}
