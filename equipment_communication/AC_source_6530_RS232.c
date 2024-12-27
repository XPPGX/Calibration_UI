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
	
	cfsetispeed(&options, B9600);
	cfsetospeed(&options, B9600);
	
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
	return len;
}

int main(){
	const char *portname = "/dev/ttyUSB0";
	char response[256];
	int fd = init_serial(portname);
	if(fd == -1){
		return EXIT_FAILURE;
	}
	
	//~ /*VOLT setting (without response)*/
	const char *comm_setVolt = "VOLT 130\r\n";
	if(send_command(fd, comm_setVolt) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	usleep(10000);
	//~ /*********************************/
	
	
	
	//Set Frequency
	const char *setFreq = "FREQ 60\r\n";
	if(send_command(fd, setFreq) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	usleep(10000);
	
	//Query Frequency
	//~ /*************************************/
	const char *command = "FREQ?\r\n";
	if(send_command(fd, command) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	//~ //sleep 1ms
	sleep(1); // This is needed if we want to read some parameter from AC source
	if(read_response(fd, response, sizeof(response)) > 0){
		printf("Response: %s\n", response);
	}
	//~ /*************************************/
	
	//~ const char *command = "MEAS:CURR:AC?\r\n";
	//~ if(send_command(fd, command) < 0){
			//~ close(fd);
			//~ return EXIT_FAILURE;
	//~ }
	//~ sleep(1);
	//~ char response[256];
	//~ if(read_response(fd, response, sizeof(response)) > 0){
		//~ printf("Response: %s\n", response);
	//~ }
	
	const char *comm_outputOn = "OUTPUT ON\r\n";
	if(send_command(fd, comm_outputOn) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	sleep(3);
	
	const char *comm_measureVolt = "MEAS:VOLT:AC?\r\n";
	if(send_command(fd, comm_measureVolt) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	usleep(100000);
	if(read_response(fd, response, sizeof(response)) > 0){
		printf("MEAS:%s\n", response);
	}
	close(fd);
	return EXIT_SUCCESS;
}
