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

int send_command(int fd, const char *command){
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

int main(){
	const char *portname = "/dev/ttyUSB0";
	char response[256];
	int fd = init_serial(portname);
	if(fd == -1){
		return EXIT_FAILURE;
	}
	
	//~ IDN query
	const char *comm_IDN = "*IDN?\n"; //note that do not add "\r" before \n
	if(send_command(fd, comm_IDN) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	usleep(100000);
	if(read_response(fd, response, sizeof(response)) > 0){
		printf("Response: %s\n", response);
	}
	sleep(1);
	
	//MODE CC
	const char *comm_ModeCC = "MODE CURR\n";
	if(send_command(fd, comm_ModeCC) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	//~ usleep(100000);
	//~ if(read_response(fd, response, sizeof(response)) > 0){
		//~ printf("Response: %s\n", response);
	//~ }
	sleep(1);
	////////////
	
	const char *comm_SetCC_Value = "CURR 42\n";
	if(send_command(fd, comm_SetCC_Value) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	sleep(1);
	
	const char *comm_ReadCC_Value = "CURR?\n";
	if(send_command(fd, comm_ReadCC_Value) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	usleep(100000);
	if(read_response(fd, response, sizeof(response)) > 0){
		printf("Response: %s\n", response);
	}
	sleep(1);
	
	//~ const char *comm_LoadON = "INP ON\n";
	//~ if(send_command(fd, comm_LoadON) < 0){
		//~ close(fd);
		//~ return EXIT_FAILURE;
	//~ }
	//~ sleep(3);
	//~ const char *comm_LoadOFF = "INP OFF\n";
	//~ if(send_command(fd, comm_LoadOFF) < 0){
		//~ close(fd);
		//~ return EXIT_FAILURE;
	//~ }
	
	//~ i
	
	//~ /*************************************/
	

	close(fd);
	return EXIT_SUCCESS;
}
