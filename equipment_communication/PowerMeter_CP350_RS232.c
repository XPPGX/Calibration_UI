
/***
 * Power meter IDCR CP350 RS232 test OK
 * 
 * *********************************/

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
	
	//~ /*VOLT gear setting (without response)*/
	const char *comm_VOLT_Gear = "V1;A1;?S\n";
	if(send_command(fd, comm_VOLT_Gear) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	sleep(3);
	if(read_response(fd, response, sizeof(response)) > 0){
		printf("Response: %s\n", response);
	}
	
	
	
	
	//~ /*********************************/
	//After sending "Set command", the device will automatically return your setting value in the buffer.
	//And the time interval is better be 1 second.
	//~ sleep(1);
	//~ const char *comm_SetFreq = "SFRE 60\n";
	//~ if(send_command(fd, comm_SetFreq) < 0){
		//~ close(fd);
		//~ return EXIT_FAILURE;
	//~ }
	
	//~ sleep(1);
	//~ const char *comm_SetVolt = "SVOL 230\n";  
	//~ if(send_command(fd, comm_SetVolt) < 0){
		//~ close(fd);
		//~ return EXIT_FAILURE;
	//~ }
	//~ sleep(1);
	
	//~ const char *comm_SetSourceOn = "PON\n";
	//~ if(send_command(fd, comm_SetSourceOn) < 0){
		//~ close(fd);
		//~ return EXIT_FAILURE;
	//~ }
	//~ sleep(5);
	
	//~ const char *comm_SetSourceOff  = "POFF\n";
	//~ if(send_command(fd, comm_SetSourceOff) < 0){
		//~ close(fd);
		//~ return EXIT_FAILURE;
	//~ }
	//~ const char *comm_ReadVolt = "?SVOL\n";
	//~ if(send_command(fd, comm_ReadVolt) < 0){
		//~ close(fd);
		//~ return EXIT_FAILURE;
	//~ }
	
	//~ sleep(1);
	//~ if(read_response(fd, response, sizeof(response)) > 0){
		//~ printf("Response : %s\n", response);
	//~ }
	//~ if(read_response(fd, response, sizeof(response)) > 0){
		//~ printf("Response: $s\n", response);
	//~ }
	
	//~ /*************************************/
	

	close(fd);
	return EXIT_SUCCESS;
}
