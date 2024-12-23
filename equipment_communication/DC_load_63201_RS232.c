#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#define BAUDRATE B9600
int init_serial(const char *portname){
	int fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd == -1){
		perror("Error opening serial port");
		return -1;
	}
	struct termios options;
	tcgetattr(fd, &options);
	
	cfsetispeed(&options, BAUDRATE);
	cfsetospeed(&options, BAUDRATE);
	
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
	
	//~ const char *comm = "*IDN?\r\n";
	//~ if(send_command(fd, comm) < 0){
		//~ close(fd);
		//~ return EXIT_FAILURE;
	//~ }
	
	//~ usleep(100000);
	//~ if(read_response(fd, response, sizeof(response)) > 0){
		//~ response[255] = '\0';
		//~ printf("Response : %s\n", response);
	//~ }
	
	const char *comm_RemoteOn = "CONF:REM ON\n";
	if(send_command(fd, comm_RemoteOn) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	
	sleep(1);
	
	
	const char *comm_setCurrent_MAX = "FUNC CURR\n";
	if(send_command(fd, comm_setCurrent_MAX) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	
	sleep(1);
	const char *comm_setCurrent_MIN = "CURR 1.5\n";
	if(send_command(fd, comm_setCurrent_MIN) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	
	sleep(5);
	
	const char *comm_RemoteOff = "CONF:REM OFF\n";
	if(send_command(fd, comm_RemoteOff) < 0){
		close(fd);
		return EXIT_FAILURE;
	}
	
}
