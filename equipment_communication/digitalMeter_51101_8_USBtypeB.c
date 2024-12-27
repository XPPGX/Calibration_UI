
/**
 * DC LOAD JT6336A RS232 => test OK
 * 
 * Note!!
 * It is needed to promise the time interval 1 second between each command send from raspberry pi to JT6336A
 * */ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int init_serial(const char *portname){
	int fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
	//~ int fd = open(portname, O_RDWR | O_NOCTTY);
	if(fd == -1){
		perror("Error opening serial port");
		return -1;
	}
	struct termios options;
	tcgetattr(fd, &options);
	
	cfsetispeed(&options, B115200);
	cfsetospeed(&options, B115200);
	
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	
	options.c_cflag |= (CLOCAL | CREAD);
	
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;
	
	tcflush(fd, TCIFLUSH);
	if(tcsetattr(fd, TCSANOW, &options) != 0){
		perror("Error configuring serial port");
		close(fd);
		return -1;
		
	}
	
	return fd;
}

int send_command(int fd, char *command){
	int len = write(fd, command, strlen(command));
	if(len < 0){
		perror("Error writing to serial port");
		return -1;
	}
	return len;
}

int read_response(int fd, char *buffer, size_t size){
	memset(buffer, 0, size);
	int len = read(fd, buffer, size - 1);
	if(len < 0){
		perror("Error reading from serial port");
		return -1;
	}
	buffer[len] = '\0';
	return len;
}

unsigned int CRC(unsigned char *start_of_packet, unsigned char *end_of_packet){
	unsigned int crc;
	unsigned char bit_count;
	unsigned char *char_ptr;
	char_ptr = start_of_packet;
	crc = 0xffff;
	
	do{
		crc ^= (unsigned int)*char_ptr;
		bit_count = 0;
		do{
			if(crc & 0x0001){
				crc >>= 1;
				crc ^= 0xA001;
			}
			else{
				crc >>=1;
			}
			
		}while(bit_count++ < 7);
	}while(char_ptr++ < end_of_packet);
	
	return crc;
}

int main(){
	const char *portname = "/dev/ttyUSB0";
	char response[256];
	int fd = init_serial(portname);
	if(fd == -1){
		return EXIT_FAILURE;
	}
	//~ //Read Existence
	printf("\nRead Existence : \n");
	unsigned char* packet_queryExist = (char*)malloc(sizeof(char) * 10);
	int start_index = 0;
	int end_index = 1;
	packet_queryExist[0] = 0xA5;
	packet_queryExist[1] = 0x10;
	unsigned int crc_value = CRC(packet_queryExist + start_index, packet_queryExist + end_index);
	printf("\tCRC = 0x%04X\n", crc_value);
	unsigned char crc_L_byte = crc_value & 0xff;
	unsigned char crc_H_byte = (crc_value >> 8) & 0xff;
	printf("\tCRC_L = 0x%04X, CRC_H = 0x%04X\n", crc_L_byte, crc_H_byte);
	packet_queryExist[2] = crc_L_byte;
	packet_queryExist[3] = crc_H_byte;
	
	write(fd, packet_queryExist, 4);
	sleep(1);
	if(read_response(fd, response, sizeof(response)) > 0){
		printf("\treceive data : 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X", response[0], response[1], response[2], response[3], response[4]);
	}
	
	//~ //Read Channel 1 Input Type
	sleep(1);
	printf("\nRead Channel 1 Input Type : \n");
	unsigned char* packet_ReadInputType = (char*)malloc(sizeof(char) * 10);
	start_index = 0;
	end_index = 2;
	packet_ReadInputType[0] = 0xA5;
	packet_ReadInputType[1] = 0x02;
	packet_ReadInputType[2] = 0x00;
	unsigned int crc_value2 = CRC(packet_ReadInputType + start_index, packet_ReadInputType + end_index);
	printf("\tCRC = 0x%04X\n", crc_value2);
	crc_L_byte = crc_value2 & 0xff;
	crc_H_byte = (crc_value2 >> 8) & 0xff;
	printf("\tCRC_L = 0x%04X, CRC_H = 0x%04X\n", crc_L_byte, crc_H_byte);
	packet_ReadInputType[3] = crc_L_byte;
	packet_ReadInputType[4] = crc_H_byte;
	write(fd, packet_ReadInputType, 5);
	sleep(1);
	memset(response, 0, sizeof(response));
	if(read_response(fd, response, sizeof(response)) > 0){
		printf("\treceive data : 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X", response[0], response[1], response[2], response[3], response[4]);
	}
	
	//~ //Read Channel 1 data
	sleep(1);
	printf("\nRead Channel 1 data : \n");
	unsigned char* packet_ReadData = (char*)malloc(sizeof(char) * 10);
	start_index = 0;
	end_index = 2;
	packet_ReadData[0] = 0xA5;
	packet_ReadData[1] = 0x00;
	packet_ReadData[2] = 0x00;
	unsigned int crc_value3 = CRC(packet_ReadData + start_index, packet_ReadData + end_index);
	printf("\tCRC = 0x%04X\n", crc_value3);
	crc_L_byte = crc_value3 & 0xff;
	crc_H_byte = (crc_value3 >> 8) & 0xff;
	printf("\tCRC_L = 0x%04X, CRC_H = 0x%04X\n", crc_L_byte, crc_H_byte);
	packet_ReadData[3] = crc_L_byte;
	packet_ReadData[4] = crc_H_byte;
	write(fd, packet_ReadData, 5);
	sleep(1);
	int recv_len = read_response(fd, response, sizeof(response));
	if(recv_len > 0){
		printf("\treceive data : ");
		for(int i = 0 ; i < recv_len ; i ++){
			printf("0x%04X, ", response[i]);
		}
		printf("\n");
		//~ printf("\treceive data : 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7]);
	}
	
	//~ //Set Channel 1 input type
	sleep(1);
	printf("Set Channel 1 Input Type : \n");
	unsigned char* packet_SetInputType = (char*)malloc(sizeof(char) * 10);
	start_index = 0;
	end_index = 3;
	packet_SetInputType[0] = 0xA5;
	packet_SetInputType[1] = 0x82;
	packet_SetInputType[2] = 0x00;
	packet_SetInputType[3] = 0x00;
	unsigned int crc_value4 = CRC(packet_SetInputType + start_index, packet_SetInputType + end_index);
	printf("\tCRC = 0x%04X\n", crc_value2);
	crc_L_byte = crc_value4 & 0xff;
	crc_H_byte = (crc_value4 >> 8) & 0xff;
	printf("\tCRC_L = 0x%04X, CRC_H = 0x%04X\n", crc_L_byte, crc_H_byte);
	packet_SetInputType[4] = crc_L_byte;
	packet_SetInputType[5] = crc_H_byte;
	write(fd, packet_SetInputType, 6);
	sleep(1);
	if(read_response(fd, response, sizeof(response)) > 0){
		printf("\treceive data : 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X\n", response[0], response[1], response[2], response[3], response[4]);
	}
	

	close(fd);
	return EXIT_SUCCESS;
}
