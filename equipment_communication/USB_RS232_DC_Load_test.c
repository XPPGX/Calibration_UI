#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>


int init_serial(const char *port, int buadrate){
	int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1){
		perror("Unable to open port");
		return -1;
	}
	
	struct termios options;
	tcgetattr(fd, &options);
	
	cfsetispeed(&options, buadrate);
	cfsetospeed(&options, buadrate);
	
	options.c_cflag &= ~PARENB; //no parity
	options.c_cflag &= ~CSTOPB; //1 stop bit
	options.c_cflag &= ~CSIZE; //Clear size bits
	options.c_cflag |= CS8; //8 data bits
	options.c_cflag |= (CLOCAL | CREAD); //Enable receiver and set Local mode
	
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); //RAW input
	options.c_oflag &= ~OPOST; // RAW output
	options.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable flow control
	
	tcsetattr(fd, TCSANOW, &options);
	
	return fd;
}

void send_command(int fd, const char *cmd){
	write(fd, cmd, strlen(cmd));
	write(fd, "\r\n", 2); //terminator
}

void receive_response(int fd, char *buffer, size_t size){
	int n = read(fd, buffer, size - 1);
	if(n > 0){
		buffer[n] = '\0'; 
	}
	else{
		buffer[0] = '\0';
	}
}


int main(){
	const char *port = "/dev/ttyUSB0";
	int buadrate = B9600;	
	
	char response[256];
	ssize_t bytes_written;
	
	char CMD_RemoteOn[] = "CONFigure:REMote ON\n";
	char CMD_RemoteOff[] = "CONFigure:REMote OFF\n";
	char CMD_QueryDeviceID[] = "*IDN?\n";
	char CMD_SetModeCCH[] = "MODE CCH\n";
	char CMD_SetCurrent[] = "CURR 1.0\n";
	char CMD_LoadON[] = "LOAD ON\n";
	char CMD_LoadOff[] = "LOAD OFF\n";
	
	int fd = init_serial(port, buadrate);
	if(fd < 0){
		perror("Failed to open USBTMC device");
		return -1;
	}
	printf("[Find Deivce]\n");
	
	
	//~ //0. remote mode on
	bytes_written = write(fd, CMD_RemoteOn, strlen(CMD_RemoteOn));
	if(bytes_written < 0){
		perror("Failed to write to RS232 device");
		close(fd);
		return 1;
	}
	printf("[Set Remote ON]\n");
	
	//1. query device ID
	bytes_written = write(fd, CMD_QueryDeviceID, strlen(CMD_QueryDeviceID));
	receive_response(fd, response, sizeof(response));
	printf("device ID : %s\n", response);
	usleep(20000);
	
	//~ //2. set to be fixed current mode 1A
	//~ bytes_written = write(fd, CMD_SetModeCCH, strlen(CMD_SetModeCCH));
	//~ usleep(20000);
	//~ bytes_written = write(fd, CMD_SetCurrent, strlen(CMD_SetCurrent));
	//~ usleep(20000);
	
	//~ //3. load on
	//~ bytes_written = write(fd, CMD_LoadON, strlen(CMD_LoadON));
	//~ sleep(5);
	//~ bytes_written = write(fd, CMD_LoadOff, strlen(CMD_LoadOff));
	//~ usleep(20000);
	
	//4. Remote mode Off
	bytes_written = write(fd, CMD_RemoteOff, strlen(CMD_RemoteOff));
	
	close(fd);
	
}
